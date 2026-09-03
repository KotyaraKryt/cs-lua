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

#include "platform.h"

// Statically linked mariadb-connector-c (LGPL), built with WITH_SSL=OFF and
// every auth plugin forced STATIC: no OpenSSL, no dlopen at connect time.
#include <mysql.h>

namespace {

// enum_field_types values that should come back as a Lua number rather than a
// string. Dates/timestamps stay strings.
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
// What crosses between the threads. Plain data only.

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
	bool numeric;
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

	// When non-empty, worker runs a migration batch. Each entry is
	// already-resolved SQL (files were read on the game thread).
	struct MigrationStep {
		std::string id;
		std::string sql;
	};
	std::vector<MigrationStep> migrations;
	bool is_migrate;

	Request()
		: id(0), conn(0), plugin(-1), callback(LUA_NOREF),
		  generation(0), is_migrate(false) {}
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

	// Filled only for migrate jobs: ids applied in this run.
	std::vector<std::string> applied;
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

// Two workers: enough that one slow site does not stall another plugin, few
// enough that a loop over players cannot spawn a thread storm. Queries against
// the same connection still serialize (Conn::use_lock).
const size_t WORKER_COUNT = 2;

// ---------------------------------------------------------------------------
// Connections
//
// A handle Lua can hold (id + metatable), but the real socket lives here and is
// only touched by a worker holding use_lock. Slots are never reused.

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

// Index 0 unused. Pointers, not values: a Conn once created never moves.
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
// stable ABI the plain functions are. Client-side escaping with
// mysql_real_escape_string is the safer choice here.

namespace {

// Calls `fn` for every unquoted '?'. Tracks '...' and "..." strings with
// backslash escaping inside either.
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

// Substitutes every '?' in order with its escaped SQL literal.
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

		// Without timeouts, a down/firewalled host blocks this worker (and
		// conn:close(), which takes the same use_lock) indefinitely.
		unsigned int connect_timeout_sec = 5;
		unsigned int io_timeout_sec = 10;
		mysql_options(conn->handle, MYSQL_OPT_CONNECT_TIMEOUT, &connect_timeout_sec);
		mysql_options(conn->handle, MYSQL_OPT_READ_TIMEOUT, &io_timeout_sec);
		mysql_options(conn->handle, MYSQL_OPT_WRITE_TIMEOUT, &io_timeout_sec);

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

	// Any failure past this point closes the handle so the next request
	// reconnects - the reliable answer to "the site's MySQL restarted".
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
		// is a real error; on INSERT/UPDATE/DELETE it is "no rows".
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
// Migration runner (whole batch under one use_lock)

static const char *MIGRATIONS_DDL =
	"CREATE TABLE IF NOT EXISTS _cslua_migrations ("
	"  id VARCHAR(128) NOT NULL PRIMARY KEY,"
	"  applied_at TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP"
	")";

bool run_sql_simple(MYSQL *handle, const std::string &sql, std::string &err)
{
	if (mysql_real_query(handle, sql.c_str(), (unsigned long)sql.size()) != 0) {
		err = mysql_error(handle);
		return false;
	}
	MYSQL_RES *result = mysql_store_result(handle);
	if (result)
		mysql_free_result(result);
	else if (mysql_field_count(handle) != 0) {
		err = mysql_error(handle);
		return false;
	}
	return true;
}

void perform_migrate(const Request &req, Response &res)
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
		unsigned int connect_timeout_sec = 5;
		unsigned int io_timeout_sec = 10;
		mysql_options(conn->handle, MYSQL_OPT_CONNECT_TIMEOUT, &connect_timeout_sec);
		mysql_options(conn->handle, MYSQL_OPT_READ_TIMEOUT, &io_timeout_sec);
		mysql_options(conn->handle, MYSQL_OPT_WRITE_TIMEOUT, &io_timeout_sec);

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

	std::string err;
	if (!run_sql_simple(conn->handle, MIGRATIONS_DDL, err)) {
		res.error = std::string("cannot create _cslua_migrations: ") + err;
		mysql_close(conn->handle);
		conn->handle = NULL;
		return;
	}

	std::vector<std::string> applied_set;
	{
		const char *q = "SELECT id FROM _cslua_migrations";
		if (mysql_real_query(conn->handle, q, (unsigned long)strlen(q)) != 0) {
			res.error = mysql_error(conn->handle);
			mysql_close(conn->handle);
			conn->handle = NULL;
			return;
		}
		MYSQL_RES *result = mysql_store_result(conn->handle);
		if (result) {
			MYSQL_ROW row;
			while ((row = mysql_fetch_row(result)) != NULL) {
				if (row[0])
					applied_set.push_back(row[0]);
			}
			mysql_free_result(result);
		}
	}

	auto already = [&](const std::string &id) -> bool {
		for (size_t i = 0; i < applied_set.size(); i++)
			if (applied_set[i] == id)
				return true;
		return false;
	};

	for (size_t i = 0; i < req.migrations.size(); i++) {
		const Request::MigrationStep &step = req.migrations[i];
		if (already(step.id))
			continue;

		if (!run_sql_simple(conn->handle, step.sql, err)) {
			res.error = std::string("migration '") + step.id + "': " + err;
			return;
		}

		std::string ins = "INSERT INTO _cslua_migrations (id) VALUES ('";
		ins += step.id;
		ins += "')";
		if (!run_sql_simple(conn->handle, ins, err)) {
			res.error = std::string("migration '") + step.id +
				"' applied but failed to record: " + err;
			return;
		}

		res.applied.push_back(step.id);
	}

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

		if (req.is_migrate)
			perform_migrate(req, res);
		else
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

// conn:query(sql[, params...], fn) / conn:exec(...) - identical.
int l_query(lua_State *L)
{
	Conn *conn = self_conn(L, 1);
	const char *sql = luaL_checkstring(L, 2);

	int top = lua_gettop(L);
	luaL_checktype(L, top, LUA_TFUNCTION);

	int wanted = count_placeholders(sql);

	Request req;
	req.id = s_next_id++;
	req.conn = conn->id;
	req.plugin = g_lua.current_index();
	req.generation = s_generation;
	req.sql = sql;

	int given = 0;

	if (top == 4 && lua_istable(L, 3)) {
		// conn:query(sql, { value1, value2, ... }, fn)
		given = (int)lua_objlen(L, 3);

		for (int i = 1; i <= given; i++) {
			lua_rawgeti(L, 3, i);
			req.params.push_back(to_param(L, -1));
			lua_pop(L, 1);
		}
	} else {
		// conn:query(sql, value1, value2, ..., fn)
		given = top - 3;

		if (given < 0)
			given = 0;

		for (int i = 3; i < top; i++)
			req.params.push_back(to_param(L, i));
	}

	if (given != wanted)
		return luaL_error(
			L,
			"conn: query takes %d parameter(s), %d given",
			wanted,
			given
		);

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

// ---------------------------------------------------------------------------
// Identifier / file helpers (game thread)

bool valid_ident(const char *s)
{
	if (!s || !*s)
		return false;
	for (const char *p = s; *p; p++) {
		if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z')
			|| (*p >= '0' && *p <= '9') || *p == '_'))
			return false;
	}
	return true;
}

bool valid_migration_id(const char *s)
{
	if (!s || !*s || strlen(s) > 128)
		return false;
	for (const char *p = s; *p; p++) {
		if (!((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z')
			|| (*p >= '0' && *p <= '9') || *p == '_' || *p == '-' || *p == '.'))
			return false;
	}
	return true;
}

// Same rules as valid_file_name() in lua_file.cpp (stem[.ext], no path).
bool valid_migrate_file_name(const char *name)
{
	if (!name || !*name || strlen(name) > 64)
		return false;
	if (!strcmp(name, ".") || !strcmp(name, ".."))
		return false;
	const char *dot = strrchr(name, '.');
	if (dot) {
		if (dot == name)
			return false;
		if (strchr(name, '.') != dot)
			return false;
		if (!*(dot + 1))
			return false;
	}
	for (const char *p = name; *p; p++) {
		if (p == dot)
			continue;
		bool alnum = (*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z')
			|| (*p >= '0' && *p <= '9');
		if (dot && p > dot) {
			if (!alnum)
				return false;
		} else {
			if (!alnum && *p != '_' && *p != '-')
				return false;
		}
	}
	return true;
}

bool read_plugin_file(int plugin_index, const char *name, std::string &out, std::string &err)
{
	if (!valid_migrate_file_name(name)) {
		err = std::string("'") + name + "' is not a usable file name";
		return false;
	}
	std::string dir = cslua_plugin_data_dir(plugin_index);
	if (dir.empty()) {
		err = "no data directory (migrate has to be called from a plugin)";
		return false;
	}
	std::string path = dir + "/" + name;
	FILE *fh = fopen(path.c_str(), "rb");
	if (!fh) {
		err = std::string("cannot open '") + name + "'";
		return false;
	}
	fseek(fh, 0, SEEK_END);
	long size = ftell(fh);
	fseek(fh, 0, SEEK_SET);
	if (size < 0 || size > 4 * 1024 * 1024) {
		fclose(fh);
		err = std::string("'") + name + "' is larger than the 4 MiB limit";
		return false;
	}
	out.resize((size_t)size);
	size_t got = size > 0 ? fread(&out[0], 1, (size_t)size, fh) : 0;
	fclose(fh);
	out.resize(got);
	return true;
}

void enqueue_request(Request &req)
{
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
}

// ---------------------------------------------------------------------------
// CRUD helpers

// Appends "<sep>col = ?" per key/value pair of the table on top of the
// stack - `first` for the first pair, `rest` for every one after - and one
// Param per value, in the same order find/create/update/delete each
// hand-rolled this loop to build a WHERE or SET clause from a Lua table.
// Table is left on the stack; caller's job to pop it. Returns how many pairs
// got appended, so the caller can decide whether zero counts as an error.
int append_assignments(lua_State *L, const char *first, const char *rest,
	std::string &sql, std::vector<Param> &params, const char *fn_name, const char *what)
{
	int table_idx = lua_gettop(L);
	int n = 0;

	lua_pushnil(L);
	while (lua_next(L, table_idx) != 0) {
		if (!lua_isstring(L, -2))
			luaL_error(L, "%s: %s keys must be strings", fn_name, what);
		const char *col = lua_tostring(L, -2);
		if (!valid_ident(col))
			luaL_error(L, "%s: invalid column '%s'", fn_name, col);

		sql += (n == 0) ? first : rest;
		sql += col;
		sql += " = ?";
		params.push_back(to_param(L, -1));
		lua_pop(L, 1);
		n++;
	}

	return n;
}

// conn:find(table, opts, fn)
int l_find(lua_State *L)
{
	Conn *conn = self_conn(L, 1);
	const char *table = luaL_checkstring(L, 2);
	luaL_checktype(L, 3, LUA_TTABLE);
	luaL_checktype(L, 4, LUA_TFUNCTION);

	if (!valid_ident(table))
		return luaL_error(L, "conn:find: invalid table name '%s'", table);

	std::string sql = "SELECT ";
	std::vector<Param> params;

	lua_getfield(L, 3, "select");
	if (lua_istable(L, -1)) {
		int n = (int)lua_objlen(L, -1);
		if (n == 0)
			sql += "*";
		else {
			for (int i = 1; i <= n; i++) {
				lua_rawgeti(L, -1, i);
				const char *col = luaL_checkstring(L, -1);
				if (!valid_ident(col))
					return luaL_error(L, "conn:find: invalid column '%s'", col);
				if (i > 1)
					sql += ", ";
				sql += col;
				lua_pop(L, 1);
			}
		}
	} else {
		sql += "*";
	}
	lua_pop(L, 1);

	sql += " FROM ";
	sql += table;

	lua_getfield(L, 3, "where");
	if (lua_istable(L, -1))
		append_assignments(L, " WHERE ", " AND ", sql, params, "conn:find", "where");
	lua_pop(L, 1);

	lua_getfield(L, 3, "order");
	if (lua_istable(L, -1)) {
		int n = (int)lua_objlen(L, -1);
		for (int i = 1; i <= n; i++) {
			lua_rawgeti(L, -1, i);
			const char *ord = luaL_checkstring(L, -1);
			std::string piece = ord;
			size_t sp = piece.find(' ');
			std::string col = sp == std::string::npos ? piece : piece.substr(0, sp);
			std::string dir = sp == std::string::npos ? "" : piece.substr(sp + 1);
			if (!valid_ident(col.c_str()))
				return luaL_error(L, "conn:find: invalid order column '%s'", col.c_str());
			if (!dir.empty() && dir != "ASC" && dir != "DESC"
				&& dir != "asc" && dir != "desc")
				return luaL_error(L, "conn:find: order direction must be ASC or DESC");
			sql += (i == 1) ? " ORDER BY " : ", ";
			sql += col;
			if (!dir.empty()) {
				sql += " ";
				sql += dir;
			}
			lua_pop(L, 1);
		}
	}
	lua_pop(L, 1);

	lua_getfield(L, 3, "limit");
	if (lua_isnumber(L, -1)) {
		int lim = (int)lua_tointeger(L, -1);
		if (lim < 0)
			return luaL_error(L, "conn:find: limit must be >= 0");
		char buf[32];
		cslua_snprintf(buf, sizeof buf, " LIMIT %d", lim);
		sql += buf;
	}
	lua_pop(L, 1);

	lua_getfield(L, 3, "offset");
	if (lua_isnumber(L, -1)) {
		int off = (int)lua_tointeger(L, -1);
		if (off < 0)
			return luaL_error(L, "conn:find: offset must be >= 0");
		char buf[32];
		cslua_snprintf(buf, sizeof buf, " OFFSET %d", off);
		sql += buf;
	}
	lua_pop(L, 1);

	Request req;
	req.id = s_next_id++;
	req.conn = conn->id;
	req.plugin = g_lua.current_index();
	req.generation = s_generation;
	req.sql = sql;
	req.params = params;

	lua_pushvalue(L, 4);
	req.callback = luaL_ref(L, LUA_REGISTRYINDEX);

	enqueue_request(req);
	lua_pushinteger(L, req.id);
	return 1;
}

// conn:create(table, data, fn)
int l_create(lua_State *L)
{
	Conn *conn = self_conn(L, 1);
	const char *table = luaL_checkstring(L, 2);
	luaL_checktype(L, 3, LUA_TTABLE);
	luaL_checktype(L, 4, LUA_TFUNCTION);

	if (!valid_ident(table))
		return luaL_error(L, "conn:create: invalid table name '%s'", table);

	std::string cols, placeholders;
	std::vector<Param> params;

	lua_pushnil(L);
	while (lua_next(L, 3) != 0) {
		if (!lua_isstring(L, -2))
			return luaL_error(L, "conn:create: column names must be strings");
		const char *col = lua_tostring(L, -2);
		if (!valid_ident(col))
			return luaL_error(L, "conn:create: invalid column '%s'", col);
		if (!cols.empty()) {
			cols += ", ";
			placeholders += ", ";
		}
		cols += col;
		placeholders += "?";
		params.push_back(to_param(L, -1));
		lua_pop(L, 1);
	}

	if (cols.empty())
		return luaL_error(L, "conn:create: data table is empty");

	std::string sql = "INSERT INTO ";
	sql += table;
	sql += " (";
	sql += cols;
	sql += ") VALUES (";
	sql += placeholders;
	sql += ")";

	Request req;
	req.id = s_next_id++;
	req.conn = conn->id;
	req.plugin = g_lua.current_index();
	req.generation = s_generation;
	req.sql = sql;
	req.params = params;

	lua_pushvalue(L, 4);
	req.callback = luaL_ref(L, LUA_REGISTRYINDEX);

	enqueue_request(req);
	lua_pushinteger(L, req.id);
	return 1;
}

// conn:update(table, opts, fn)
int l_update(lua_State *L)
{
	Conn *conn = self_conn(L, 1);
	const char *table = luaL_checkstring(L, 2);
	luaL_checktype(L, 3, LUA_TTABLE);
	luaL_checktype(L, 4, LUA_TFUNCTION);

	if (!valid_ident(table))
		return luaL_error(L, "conn:update: invalid table name '%s'", table);

	std::string sql = "UPDATE ";
	sql += table;
	sql += " SET ";
	std::vector<Param> params;

	lua_getfield(L, 3, "set");
	if (!lua_istable(L, -1))
		return luaL_error(L, "conn:update: opts.set table required");
	if (append_assignments(L, "", ", ", sql, params, "conn:update", "set") == 0)
		return luaL_error(L, "conn:update: opts.set is empty");
	lua_pop(L, 1);

	lua_getfield(L, 3, "where");
	if (!lua_istable(L, -1))
		return luaL_error(L, "conn:update: opts.where table required");
	if (append_assignments(L, " WHERE ", " AND ", sql, params, "conn:update", "where") == 0)
		return luaL_error(L, "conn:update: opts.where is empty");
	lua_pop(L, 1);

	Request req;
	req.id = s_next_id++;
	req.conn = conn->id;
	req.plugin = g_lua.current_index();
	req.generation = s_generation;
	req.sql = sql;
	req.params = params;

	lua_pushvalue(L, 4);
	req.callback = luaL_ref(L, LUA_REGISTRYINDEX);

	enqueue_request(req);
	lua_pushinteger(L, req.id);
	return 1;
}

// conn:delete(table, opts, fn)
int l_delete(lua_State *L)
{
	Conn *conn = self_conn(L, 1);
	const char *table = luaL_checkstring(L, 2);
	luaL_checktype(L, 3, LUA_TTABLE);
	luaL_checktype(L, 4, LUA_TFUNCTION);

	if (!valid_ident(table))
		return luaL_error(L, "conn:delete: invalid table name '%s'", table);

	std::string sql = "DELETE FROM ";
	sql += table;
	std::vector<Param> params;

	lua_getfield(L, 3, "where");
	if (!lua_istable(L, -1))
		return luaL_error(L, "conn:delete: opts.where table required");
	if (append_assignments(L, " WHERE ", " AND ", sql, params, "conn:delete", "where") == 0)
		return luaL_error(L, "conn:delete: opts.where is empty");
	lua_pop(L, 1);

	Request req;
	req.id = s_next_id++;
	req.conn = conn->id;
	req.plugin = g_lua.current_index();
	req.generation = s_generation;
	req.sql = sql;
	req.params = params;

	lua_pushvalue(L, 4);
	req.callback = luaL_ref(L, LUA_REGISTRYINDEX);

	enqueue_request(req);
	lua_pushinteger(L, req.id);
	return 1;
}

// conn:migrate(opts, fn)
int l_migrate(lua_State *L)
{
	Conn *conn = self_conn(L, 1);
	luaL_checktype(L, 2, LUA_TTABLE);
	luaL_checktype(L, 3, LUA_TFUNCTION);

	int plugin = g_lua.current_index();
	Request req;
	req.id = s_next_id++;
	req.conn = conn->id;
	req.plugin = plugin;
	req.generation = s_generation;
	req.is_migrate = true;

	lua_getfield(L, 2, "migrations");
	if (lua_istable(L, -1)) {
		int n = (int)lua_objlen(L, -1);
		for (int i = 1; i <= n; i++) {
			lua_rawgeti(L, -1, i);
			if (!lua_istable(L, -1))
				return luaL_error(L, "conn:migrate: migrations[%d] must be a table", i);

			lua_getfield(L, -1, "id");
			const char *id = lua_isstring(L, -1) ? lua_tostring(L, -1) : NULL;
			if (!id || !valid_migration_id(id))
				return luaL_error(L, "conn:migrate: migrations[%d].id invalid", i);
			std::string mid = id;
			lua_pop(L, 1);

			std::string sql;
			lua_getfield(L, -1, "sql");
			if (lua_isstring(L, -1)) {
				size_t len = 0;
				const char *s = lua_tolstring(L, -1, &len);
				sql.assign(s, len);
			}
			lua_pop(L, 1);

			if (sql.empty()) {
				lua_getfield(L, -1, "file");
				const char *fname = lua_isstring(L, -1) ? lua_tostring(L, -1) : NULL;
				if (!fname)
					return luaL_error(L, "conn:migrate: migrations[%d] needs sql or file", i);
				std::string err;
				if (!read_plugin_file(plugin, fname, sql, err))
					return luaL_error(L, "conn:migrate: %s", err.c_str());
				lua_pop(L, 1);
			}

			Request::MigrationStep step;
			step.id = mid;
			step.sql = sql;
			req.migrations.push_back(step);
			lua_pop(L, 1);
		}
	}
	lua_pop(L, 1);

	lua_getfield(L, 2, "files");
	if (lua_istable(L, -1)) {
		int n = (int)lua_objlen(L, -1);
		for (int i = 1; i <= n; i++) {
			lua_rawgeti(L, -1, i);
			const char *fname = luaL_checkstring(L, -1);
			std::string sql, err;
			if (!read_plugin_file(plugin, fname, sql, err))
				return luaL_error(L, "conn:migrate: %s", err.c_str());
			if (!valid_migration_id(fname))
				return luaL_error(L, "conn:migrate: file name '%s' is not a valid migration id", fname);
			Request::MigrationStep step;
			step.id = fname;
			step.sql = sql;
			req.migrations.push_back(step);
			lua_pop(L, 1);
		}
	}
	lua_pop(L, 1);

	if (req.migrations.empty())
		return luaL_error(L, "conn:migrate: no migrations provided");

	lua_pushvalue(L, 3);
	req.callback = luaL_ref(L, LUA_REGISTRYINDEX);

	enqueue_request(req);
	lua_pushinteger(L, req.id);
	return 1;
}

// conn:close() - marks the connection unusable and closes the socket. Taking
// use_lock first guarantees no worker is touching `handle`.
int l_close(lua_State *L)
{
	Conn *conn = self_conn(L, 1);
	std::lock_guard<std::mutex> guard(conn->use_lock);
	conn->closed = true;
	if (conn->handle) {
		mysql_close(conn->handle);
		conn->handle = NULL;
	}
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
	static std::string hold;		// only needs to outlive this call
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
// Never blocks: the real TCP connect happens lazily on a worker.
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
	{ "query",   l_query },
	{ "exec",    l_query },
	{ "find",    l_find },
	{ "create",  l_create },
	{ "update",  l_update },
	{ "delete",  l_delete },
	{ "migrate", l_migrate },
	{ "close",   l_close },
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
	cslua_push_metatable(L, s_conn_methods);

	lua_pushcfunction(L, l_readonly);
	lua_setfield(L, -2, "__newindex");

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

		if (!res.applied.empty()) {
			lua_newtable(L);
			for (size_t a = 0; a < res.applied.size(); a++) {
				lua_pushlstring(L, res.applied[a].data(), res.applied[a].size());
				lua_rawseti(L, -2, (int)(a + 1));
			}
			lua_setfield(L, -2, "applied");
		}

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

	{
		std::lock_guard<std::mutex> guard(s_done_lock);
		for (size_t i = 0; i < s_done.size(); i++) {
			if (s_done[i].plugin == plugin_index) {
				if (L)
					luaL_unref(L, LUA_REGISTRYINDEX, s_done[i].callback);
				s_done[i].callback = LUA_NOREF;
			}
		}
	}

	// Connections this plugin opened and never closed would otherwise sit idle
	// until process exit. try_lock rather than lock: a worker mid-query must not
	// stall a reload on the game thread. Losing that race leaves the connection
	// to the shutdown-time cleanup.
	std::lock_guard<std::mutex> guard(s_conns_lock);
	for (size_t i = 1; i < s_conns.size(); i++) {
		Conn *conn = s_conns[i];
		if (!conn || conn->plugin != plugin_index || conn->closed)
			continue;
		if (conn->use_lock.try_lock()) {
			conn->closed = true;
			if (conn->handle) {
				mysql_close(conn->handle);
				conn->handle = NULL;
			}
			conn->use_lock.unlock();
		} else {
			conn->closed = true;
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

	// Registry refs on still-open connections are not unref'd: the whole
	// registry is freed at once. The connections themselves are left alone;
	// see cslua_mysql_shutdown().
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
