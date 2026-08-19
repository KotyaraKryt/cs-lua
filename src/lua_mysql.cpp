#include "cslua.h"
#include "lua_mysql.h"
#include "lua_engine.h"
#include "lua_natives.h"

#include <condition_variable>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Statically linked (third_party/mariadb-connector-c, LGPL - compatible with
// this project's own GPLv3, unlike Oracle's dual GPL/commercial
// libmysqlclient). Built with WITH_SSL=OFF and every auth plugin forced
// STATIC (see CMakeLists.txt): no OpenSSL, no dlopen of a plugin .so at
// connect time, nothing for a shared host with no root and no apt to be
// missing at runtime - the whole reason this used to be dlopen'd instead.
#include <mysql.h>

namespace {

// Text-protocol MySQL sends every value as a string; only the field's
// declared type says whether "42" is the number 42 or the text "42". These
// are the numeric enum_field_types values (mariadb_com.h) - the ones that
// should come back as a Lua number instead of a string, the same convention
// `db` uses for SQLite's INTEGER/REAL. Dates/timestamps stay strings: they
// need parsing, not arithmetic, and that parsing wants the original text.
bool is_numeric_field_type(int type)
{
	switch (type) {
	case 0:		// DECIMAL
	case 1:		// TINY
	case 2:		// SHORT
	case 3:		// LONG
	case 4:		// FLOAT
	case 5:		// DOUBLE
	case 8:		// LONGLONG
	case 9:		// INT24
	case 13:	// YEAR
	case 246:	// NEWDECIMAL
		return true;
	default:
		return false;
	}
}

} // namespace

// ---------------------------------------------------------------------------
// What crosses between the threads. Plain data only - see lua_http.cpp for
// why that matters.

namespace {

enum ParamKind { P_NIL, P_BOOL, P_INT, P_NUM, P_STR };

struct Param
{
	ParamKind kind;
	bool b;
	long long i;
	double d;
	std::string s;
};

struct Cell
{
	std::string name;
	std::string value;
	bool numeric;		// push as a Lua number instead of a string
};

typedef std::vector<Cell> Row;	// NULL columns absent entirely

struct Request
{
	int id;
	int conn;				// index into s_conns
	int plugin;
	int callback;			// registry ref
	int generation;

	std::string sql;
	std::vector<Param> params;
};

struct Response
{
	int id;
	int plugin;
	int callback;
	int generation;

	bool ok;
	std::string error;
	std::vector<Row> rows;
	unsigned long long affected_rows;
	unsigned long long insert_id;
};

std::deque<Request> s_pending;
std::deque<Response> s_done;

std::mutex s_pending_lock;
std::mutex s_done_lock;
std::condition_variable s_wake;

std::vector<std::thread> s_workers;
bool s_stopping = false;
int s_next_id = 1;
int s_generation = 1;

int s_inflight = 0;
std::mutex s_inflight_lock;

// Two workers, same reasoning as http.cpp: enough that one slow site does not
// stall another plugin, few enough that a loop over players cannot spawn a
// thread storm. Two queries against the *same* connection still serialize
// (see Conn::use_lock) - a plugin that wants real parallelism opens more than
// one connection.
const size_t WORKER_COUNT = 2;

// ---------------------------------------------------------------------------
// Connections
//
// A handle Lua can hold (id + metatable, same shape as db.open()), but the
// real socket lives here and is only ever touched by a worker holding
// use_lock - never by the game thread, and never by two workers at once.
// Slots are never reused, so an id identifies one connection for the state's
// whole life.

struct ConnConfig
{
	std::string host;
	int port;
	std::string user;
	std::string password;
	std::string database;
	std::string charset;
};

struct Conn
{
	int id;
	int plugin;
	int generation;
	ConnConfig cfg;

	std::mutex use_lock;		// serializes every worker touching `handle`
	MYSQL *handle;
	bool closed;

	Conn() : id(0), plugin(-1), generation(0), handle(NULL), closed(false) {}
};

// Index 0 unused, so an id of 0 (a handle table with no "id" field) is never
// valid. Pointers, not values: the vector may grow, but a Conn once created
// never moves or is destroyed until process exit.
std::vector<Conn *> s_conns;
std::mutex s_conns_lock;

int s_mysql_mt_ref = LUA_NOREF;

Conn *lookup_conn(int id)
{
	std::lock_guard<std::mutex> guard(s_conns_lock);
	if (id <= 0 || id >= (int)s_conns.size())
		return NULL;
	return s_conns[id];
}

} // namespace

// ---------------------------------------------------------------------------
// Placeholder scanning and query building
//
// No prepared-statement binding: MYSQL_STMT/MYSQL_BIND layouts are not the
// stable, decade-old ABI that the plain functions above are, and getting one
// wrong is memory corruption, not a compile error. Client-side escaping with
// mysql_real_escape_string (itself a plain, stable signature) is the safer
// choice for a library opened without its headers.

namespace {

// Calls `fn` for every unquoted '?' in `sql`. Quoting is tracked well enough
// for ordinary SQL: '...' and "..." strings, with a backslash escaping the
// next character inside either.
template <typename Fn>
void scan_placeholders(const std::string &sql, Fn fn)
{
	char quote = 0;
	for (size_t i = 0; i < sql.size(); i++) {
		char c = sql[i];
		if (quote) {
			if (c == '\\' && i + 1 < sql.size())
				i++;
			else if (c == quote)
				quote = 0;
			continue;
		}
		if (c == '\'' || c == '"')
			quote = c;
		else if (c == '?')
			fn(i);
	}
}

int count_placeholders(const std::string &sql)
{
	int n = 0;
	scan_placeholders(sql, [&n](size_t) { n++; });
	return n;
}

bool append_param(MYSQL *handle, const Param &p, std::string &out)
{
	char buf[64];

	switch (p.kind) {
	case P_NIL:
		out += "NULL";
		break;
	case P_BOOL:
		out += p.b ? "1" : "0";
		break;
	case P_INT:
		cslua_snprintf(buf, sizeof buf, "%lld", p.i);
		out += buf;
		break;
	case P_NUM:
		cslua_snprintf(buf, sizeof buf, "%.17g", p.d);
		out += buf;
		break;
	case P_STR: {
		std::string escaped;
		escaped.resize(p.s.size() * 2 + 1);
		unsigned long n = mysql_real_escape_string(handle, &escaped[0], p.s.data(), (unsigned long)p.s.size());
		out += '\'';
		out.append(escaped.data(), n);
		out += '\'';
		break;
	}
	}
	return true;
}

// Substitutes every '?' in req.sql, in order, with its escaped SQL literal.
std::string build_sql(MYSQL *handle, const Request &req)
{
	std::string out;
	out.reserve(req.sql.size() + req.params.size() * 8);

	size_t next_param = 0, cursor = 0;
	scan_placeholders(req.sql, [&](size_t pos) {
		out.append(req.sql, cursor, pos - cursor);
		if (next_param < req.params.size())
			append_param(handle, req.params[next_param++], out);
		cursor = pos + 1;
	});
	out.append(req.sql, cursor, std::string::npos);

	return out;
}

// ---------------------------------------------------------------------------
// Executing one request on a worker

void perform(const Request &req, Response &res)
{
	res.ok = false;
	res.affected_rows = 0;
	res.insert_id = 0;

	Conn *conn = lookup_conn(req.conn);
	if (!conn) {
		res.error = "this connection no longer exists";
		return;
	}

	std::lock_guard<std::mutex> guard(conn->use_lock);

	if (conn->closed) {
		res.error = "this connection is closed";
		return;
	}

	if (!conn->handle) {
		conn->handle = mysql_init(NULL);
		MYSQL *ok = mysql_real_connect(conn->handle, conn->cfg.host.c_str(),
			conn->cfg.user.c_str(), conn->cfg.password.c_str(),
			conn->cfg.database.empty() ? NULL : conn->cfg.database.c_str(),
			(unsigned int)conn->cfg.port, NULL, 0);

		if (!ok) {
			res.error = std::string("connect failed: ") + mysql_error(conn->handle);
			mysql_close(conn->handle);
			conn->handle = NULL;
			return;
		}

		if (!conn->cfg.charset.empty())
			mysql_set_character_set(conn->handle, conn->cfg.charset.c_str());
	}

	std::string sql = build_sql(conn->handle, req);

	// Any failure past this point closes the handle so the *next* request on
	// this connection reconnects from scratch - the simple, reliable answer to
	// "the site's MySQL restarted underneath us", at the cost of one wasted
	// reconnect on a plain SQL mistake too. Cheap enough for how rarely a query
	// actually fails.
	if (mysql_real_query(conn->handle, sql.c_str(), (unsigned long)sql.size()) != 0) {
		res.error = mysql_error(conn->handle);
		mysql_close(conn->handle);
		conn->handle = NULL;
		return;
	}

	MYSQL_RES *result = mysql_store_result(conn->handle);
	if (result) {
		unsigned int nfields = mysql_num_fields(result);
		std::vector<std::string> names(nfields);
		std::vector<bool> numeric(nfields);
		for (unsigned int i = 0; i < nfields; i++) {
			MYSQL_FIELD *f = mysql_fetch_field_direct(result, i);
			names[i] = f ? f->name : "";
			numeric[i] = f && is_numeric_field_type((int)f->type);
		}

		char **row;
		while ((row = mysql_fetch_row(result)) != NULL) {
			Row r;
			for (unsigned int i = 0; i < nfields; i++) {
				if (row[i]) {
					Cell c;
					c.name = names[i];
					c.value = row[i];
					c.numeric = numeric[i];
					r.push_back(c);
				}
			}
			res.rows.push_back(r);
		}

		mysql_free_result(result);
	} else if (mysql_field_count(conn->handle) != 0) {
		// store_result() failing on a statement that DOES produce a result set
		// is a real error; on INSERT/UPDATE/DELETE it is simply "no rows".
		res.error = mysql_error(conn->handle);
		mysql_close(conn->handle);
		conn->handle = NULL;
		return;
	}

	res.affected_rows = mysql_affected_rows(conn->handle);
	res.insert_id = mysql_insert_id(conn->handle);
	res.ok = true;
}

// ---------------------------------------------------------------------------
// Workers

void worker_loop()
{
	for (;;) {
		Request req;

		{
			std::unique_lock<std::mutex> guard(s_pending_lock);
			s_wake.wait(guard, [] { return s_stopping || !s_pending.empty(); });

			if (s_stopping)
				return;

			req = s_pending.front();
			s_pending.pop_front();
		}

		Response res;
		res.id = req.id;
		res.plugin = req.plugin;
		res.callback = req.callback;
		res.generation = req.generation;

		perform(req, res);

		{
			std::lock_guard<std::mutex> guard(s_done_lock);
			s_done.push_back(res);
		}
	}
}

void start_workers()
{
	if (!s_workers.empty())
		return;

	s_stopping = false;
	for (size_t i = 0; i < WORKER_COUNT; i++)
		s_workers.push_back(std::thread(worker_loop));
}

// ---------------------------------------------------------------------------
// Lua side

Conn *self_conn(lua_State *L, int index = 1)
{
	luaL_checktype(L, index, LUA_TTABLE);

	lua_getfield(L, index, "id");
	if (!lua_isnumber(L, -1))
		luaL_error(L, "conn: expected a mysql connection, use conn:method() and not conn.method()");
	int id = (int)lua_tointeger(L, -1);
	lua_pop(L, 1);

	Conn *conn = lookup_conn(id);
	if (!conn)
		luaL_error(L, "conn: invalid mysql connection");

	return conn;
}

Param to_param(lua_State *L, int index)
{
	Param p;
	switch (lua_type(L, index)) {
	case LUA_TNIL:
		p.kind = P_NIL;
		break;
	case LUA_TBOOLEAN:
		p.kind = P_BOOL;
		p.b = lua_toboolean(L, index) != 0;
		break;
	case LUA_TNUMBER: {
		double n = lua_tonumber(L, index);
		if (n == (double)(long long)n) {
			p.kind = P_INT;
			p.i = (long long)n;
		} else {
			p.kind = P_NUM;
			p.d = n;
		}
		break;
	}
	case LUA_TSTRING: {
		size_t len = 0;
		const char *s = lua_tolstring(L, index, &len);
		p.kind = P_STR;
		p.s.assign(s, len);
		break;
	}
	default:
		luaL_error(L, "conn: cannot bind a %s as a mysql parameter (argument %d)",
			luaL_typename(L, index), index);
	}
	return p;
}

// conn:query(sql[, params...], fn) / conn:exec(...) - identical, both names
// exist because "query" reads naturally for a SELECT and "exec" for a write.
int l_query(lua_State *L)
{
	Conn *conn = self_conn(L, 1);
	const char *sql = luaL_checkstring(L, 2);

	int top = lua_gettop(L);
	luaL_checktype(L, top, LUA_TFUNCTION);

	int given = top - 3;			// args strictly between sql (2) and the callback (top)
	if (given < 0)
		given = 0;

	int wanted = count_placeholders(sql);
	if (given != wanted)
		return luaL_error(L, "conn: query takes %d parameter(s), %d given", wanted, given);

	Request req;
	req.id = s_next_id++;
	req.conn = conn->id;
	req.plugin = g_lua.current_index();
	req.generation = s_generation;
	req.sql = sql;

	for (int i = 3; i < top; i++)
		req.params.push_back(to_param(L, i));

	lua_pushvalue(L, top);
	req.callback = luaL_ref(L, LUA_REGISTRYINDEX);

	start_workers();

	{
		std::lock_guard<std::mutex> guard(s_inflight_lock);
		s_inflight++;
	}
	{
		std::lock_guard<std::mutex> guard(s_pending_lock);
		s_pending.push_back(req);
	}
	s_wake.notify_one();

	lua_pushinteger(L, req.id);
	return 1;
}

// conn:close() - marks the connection unusable. The socket itself is not
// closed here (a worker may be using it right now); it is left idle until
// cslua_mysql_shutdown() at process exit, same tradeoff as lua_reload.
int l_close(lua_State *L)
{
	Conn *conn = self_conn(L, 1);
	std::lock_guard<std::mutex> guard(conn->use_lock);
	conn->closed = true;
	return 0;
}

int l_readonly(lua_State *L)
{
	return luaL_error(L, "conn: mysql connections are read-only, keep your own state in a table");
}

const char *opt_field_str(lua_State *L, int index, const char *key, const char *def)
{
	lua_getfield(L, index, key);
	const char *v = lua_isstring(L, -1) ? lua_tostring(L, -1) : def;
	static std::string hold;		// lua_tostring's pointer only needs to outlive this call
	hold = v ? v : "";
	lua_pop(L, 1);
	return hold.c_str();
}

double opt_field_num(lua_State *L, int index, const char *key, double def)
{
	lua_getfield(L, index, key);
	double v = lua_isnumber(L, -1) ? lua_tonumber(L, -1) : def;
	lua_pop(L, 1);
	return v;
}

// mysql.connect{ host=, port=, user=, password=, database=, charset= }
//
// Never blocks: the handle comes back immediately and the real TCP connect
// happens lazily, on a worker, the first time something is queried on it.
int l_connect(lua_State *L)
{
	luaL_checktype(L, 1, LUA_TTABLE);

	Conn *conn = new Conn();
	conn->cfg.host = opt_field_str(L, 1, "host", "127.0.0.1");
	conn->cfg.port = (int)opt_field_num(L, 1, "port", 3306);
	conn->cfg.user = opt_field_str(L, 1, "user", "");
	conn->cfg.password = opt_field_str(L, 1, "password", "");
	conn->cfg.database = opt_field_str(L, 1, "database", "");
	conn->cfg.charset = opt_field_str(L, 1, "charset", "utf8");
	conn->plugin = g_lua.current_index();
	conn->generation = s_generation;

	{
		std::lock_guard<std::mutex> guard(s_conns_lock);
		if (s_conns.empty())
			s_conns.push_back(NULL);		// slot 0: never a valid id
		conn->id = (int)s_conns.size();
		s_conns.push_back(conn);
	}

	lua_newtable(L);
	lua_pushinteger(L, conn->id);
	lua_setfield(L, -2, "id");
	lua_rawgeti(L, LUA_REGISTRYINDEX, s_mysql_mt_ref);
	lua_setmetatable(L, -2);

	return 1;
}

const luaL_Reg s_conn_methods[] =
{
	{ "query", l_query },
	{ "exec",  l_query },
	{ "close", l_close },
	{ NULL, NULL }
};

const luaL_Reg s_api_ns[] =
{
	{ "connect", l_connect },
	{ NULL, NULL }
};

} // namespace

void cslua_register_mysql(lua_State *L)
{
	lua_newtable(L);

	lua_newtable(L);
	for (const luaL_Reg *r = s_conn_methods; r->name; r++) {
		lua_pushcfunction(L, r->func);
		lua_setfield(L, -2, r->name);
	}
	lua_setfield(L, -2, "__index");

	lua_pushcfunction(L, l_readonly);
	lua_setfield(L, -2, "__newindex");

	lua_pushboolean(L, 0);
	lua_setfield(L, -2, "__metatable");

	s_mysql_mt_ref = luaL_ref(L, LUA_REGISTRYINDEX);

	cslua_register_namespace(L, "mysql", s_api_ns);
}

// ---------------------------------------------------------------------------
// Draining, on the game thread

void cslua_mysql_run()
{
	lua_State *L = g_lua.state();
	if (!L)
		return;

	std::deque<Response> batch;
	{
		std::lock_guard<std::mutex> guard(s_done_lock);
		if (s_done.empty())
			return;
		batch.swap(s_done);
	}

	{
		std::lock_guard<std::mutex> guard(s_inflight_lock);
		s_inflight -= (int)batch.size();
		if (s_inflight < 0)
			s_inflight = 0;
	}

	for (size_t i = 0; i < batch.size(); i++) {
		Response &res = batch[i];

		if (res.callback == LUA_NOREF)
			continue;
		if (res.generation != s_generation)
			continue;

		PluginScope scope(res.plugin);
		int errfunc = g_lua.push_errfunc();

		lua_rawgeti(L, LUA_REGISTRYINDEX, res.callback);

		lua_newtable(L);

		lua_pushboolean(L, res.ok);
		lua_setfield(L, -2, "ok");

		if (!res.error.empty()) {
			lua_pushstring(L, res.error.c_str());
			lua_setfield(L, -2, "error");
		}

		lua_pushnumber(L, (double)res.affected_rows);
		lua_setfield(L, -2, "affected_rows");

		lua_pushnumber(L, (double)res.insert_id);
		lua_setfield(L, -2, "insert_id");

		lua_newtable(L);
		for (size_t r = 0; r < res.rows.size(); r++) {
			lua_newtable(L);
			const Row &row = res.rows[r];
			for (size_t c = 0; c < row.size(); c++) {
				const Cell &cell = row[c];
				if (cell.numeric)
					lua_pushnumber(L, strtod(cell.value.c_str(), NULL));
				else
					lua_pushlstring(L, cell.value.data(), cell.value.size());
				lua_setfield(L, -2, cell.name.c_str());
			}
			lua_rawseti(L, -2, (int)(r + 1));
		}
		lua_setfield(L, -2, "rows");

		if (lua_pcall(L, 1, 0, errfunc) != 0)
			g_lua.report_error("mysql callback");

		lua_remove(L, errfunc);

		luaL_unref(L, LUA_REGISTRYINDEX, res.callback);
	}
}

void cslua_mysql_remove_plugin(int plugin_index)
{
	lua_State *L = g_lua.state();

	{
		std::lock_guard<std::mutex> guard(s_pending_lock);
		for (std::deque<Request>::iterator it = s_pending.begin(); it != s_pending.end(); ) {
			if (it->plugin != plugin_index) {
				++it;
				continue;
			}
			if (L)
				luaL_unref(L, LUA_REGISTRYINDEX, it->callback);
			it = s_pending.erase(it);
		}
	}

	std::lock_guard<std::mutex> guard(s_done_lock);
	for (size_t i = 0; i < s_done.size(); i++) {
		if (s_done[i].plugin == plugin_index) {
			if (L)
				luaL_unref(L, LUA_REGISTRYINDEX, s_done[i].callback);
			s_done[i].callback = LUA_NOREF;
		}
	}
}

int cslua_mysql_pending()
{
	std::lock_guard<std::mutex> guard(s_inflight_lock);
	return s_inflight;
}

void cslua_mysql_reset()
{
	s_generation++;

	{
		std::lock_guard<std::mutex> guard(s_pending_lock);
		s_pending.clear();
	}
	{
		std::lock_guard<std::mutex> guard(s_done_lock);
		s_done.clear();
	}
	{
		std::lock_guard<std::mutex> guard(s_inflight_lock);
		s_inflight = 0;
	}

	s_mysql_mt_ref = LUA_NOREF;

	// Registry refs on still-open connections are not unref'd here - the state
	// they belong to is being closed, which frees the whole registry at once.
	// The connections themselves (and their sockets) are left alone; see
	// cslua_mysql_shutdown().
}

void cslua_mysql_shutdown()
{
	cslua_mysql_reset();

	{
		std::lock_guard<std::mutex> guard(s_pending_lock);
		s_stopping = true;
	}
	s_wake.notify_all();

	for (size_t i = 0; i < s_workers.size(); i++) {
		if (s_workers[i].joinable())
			s_workers[i].join();
	}
	s_workers.clear();

	// Safe now: every worker that could have been touching a handle is gone.
	std::lock_guard<std::mutex> guard(s_conns_lock);
	for (size_t i = 1; i < s_conns.size(); i++) {
		if (s_conns[i] && s_conns[i]->handle)
			mysql_close(s_conns[i]->handle);
	}
}
