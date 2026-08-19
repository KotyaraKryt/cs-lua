#include "cslua.h"

#include "lua_db.h"
#include "lua_engine.h"
#include "lua_natives.h"

#include <sqlite3.h>

#include <chrono>
#include <string>
#include <vector>

#include <string.h>

// ---------------------------------------------------------------------------
// Handles
//
// Objects are tables carrying an integer id, with a shared metatable out of the
// registry - the same shape as a cvar or an entity object. The sqlite3 * itself
// lives here, in a vector, so nothing that Lua can reach is a raw pointer.
//
// Slots are never reused. An id therefore identifies one database for the whole
// life of the state, and an object left over from a db:close() resolves to a
// closed slot and says so, instead of quietly working on whatever opened next.

struct OpenDb
{
	sqlite3 *handle;
	int plugin;				// index into LuaEngine::plugins()
	std::string path;		// what was opened, for error messages
	bool in_transaction;
};

struct OpenStmt
{
	sqlite3_stmt *stmt;
	int db;					// id of the database it was prepared against
	std::string sql;
};

// Index 0 is unused so an id of 0 is never valid.
static std::vector<OpenDb> s_dbs;
static std::vector<OpenStmt> s_stmts;

static int s_db_mt_ref = LUA_NOREF;
static int s_stmt_mt_ref = LUA_NOREF;

// ---------------------------------------------------------------------------
// Watchdog
//
// The one real danger of running queries on the frame thread: a query over a
// table nobody indexed can run for a minute, and a server that does not answer
// for a minute has dropped every client on it.
//
// SQLite calls the progress handler every N virtual machine instructions;
// returning non-zero there aborts the statement. So a runaway query becomes a
// Lua error with a traceback pointing at the plugin that wrote it, which is a
// bug report instead of an outage.

// cvar_t wants writable strings - the engine writes back into .string when the
// value changes - so these are arrays, not literals.
static char s_timeout_name[] = "cslua_db_timeout_ms";
static char s_timeout_value[] = "200";
static char s_warn_name[] = "cslua_db_warn_ms";
static char s_warn_value[] = "25";

static cvar_t s_cvar_timeout = { s_timeout_name, s_timeout_value, 0, 200.0f, NULL };
static cvar_t s_cvar_warn    = { s_warn_name,    s_warn_value,    0, 25.0f,  NULL };
static bool s_cvars_registered = false;

typedef std::chrono::steady_clock Clock;

struct Watchdog
{
	Clock::time_point start;
	int limit_ms;
};

static int progress_cb(void *ctx)
{
	const Watchdog *w = (const Watchdog *)ctx;
	if (w->limit_ms <= 0)
		return 0;			// 0 or below disables the abort entirely

	long long ms = std::chrono::duration_cast<std::chrono::milliseconds>(
		Clock::now() - w->start).count();

	return ms > w->limit_ms ? 1 : 0;
}

static void watchdog_start(sqlite3 *db, Watchdog &w)
{
	w.start = Clock::now();
	w.limit_ms = (int)CVAR_GET_FLOAT(s_cvar_timeout.name);

	// Every 1000 VM instructions: often enough to catch a runaway inside a
	// millisecond, rare enough that the check itself does not show up.
	sqlite3_progress_handler(db, 1000, progress_cb, &w);
}

static long long watchdog_stop(sqlite3 *db, const Watchdog &w)
{
	sqlite3_progress_handler(db, 0, NULL, NULL);
	return std::chrono::duration_cast<std::chrono::milliseconds>(
		Clock::now() - w.start).count();
}

// Not an error - the query finished - but worth saying out loud, because at
// this length it is already visible as a hitch to everyone on the server.
static void warn_if_slow(long long ms, int plugin, const char *sql)
{
	int limit = (int)CVAR_GET_FLOAT(s_cvar_warn.name);
	if (limit <= 0 || ms < limit)
		return;

	const std::vector<LuaPlugin> &plugins = g_lua.plugins();
	const char *who = (plugin >= 0 && plugin < (int)plugins.size())
		? plugins[plugin].id.c_str() : "core";

	// The whole query would wrap the console; the head of it is enough to find.
	char head[80];
	cslua_snprintf(head, sizeof head, "%s", sql);
	head[sizeof head - 1] = '\0';

	cslua_print("slow query in '%s': %lldms - %s%s", who, ms, head,
		strlen(sql) > sizeof head - 1 ? "..." : "");
}

// ---------------------------------------------------------------------------
// Resolving objects
//
// Every error path here has to finish its cleanup before raising: luaL_error
// jumps out of the function, and anything left half-done stays that way.

static OpenDb *self_db(lua_State *L, int index = 1)
{
	luaL_checktype(L, index, LUA_TTABLE);

	lua_getfield(L, index, "id");
	if (!lua_isnumber(L, -1))
		luaL_error(L, "db: expected a database object, use db:method() and not db.method()");
	int id = (int)lua_tointeger(L, -1);
	lua_pop(L, 1);

	if (id <= 0 || id >= (int)s_dbs.size() || !s_dbs[id].handle)
		luaL_error(L, "db: this database is closed");

	return &s_dbs[id];
}

static OpenStmt *self_stmt(lua_State *L)
{
	luaL_checktype(L, 1, LUA_TTABLE);

	lua_getfield(L, 1, "id");
	if (!lua_isnumber(L, -1))
		luaL_error(L, "st: expected a statement object, use s:method() and not s.method()");
	int id = (int)lua_tointeger(L, -1);
	lua_pop(L, 1);

	if (id <= 0 || id >= (int)s_stmts.size() || !s_stmts[id].stmt)
		luaL_error(L, "st: this statement is closed");

	OpenStmt *s = &s_stmts[id];
	if (s->db <= 0 || s->db >= (int)s_dbs.size() || !s_dbs[s->db].handle)
		luaL_error(L, "st: the database this statement belongs to is closed");

	return s;
}

static void push_handle(lua_State *L, int id, int mt_ref)
{
	lua_newtable(L);
	lua_pushinteger(L, id);
	lua_setfield(L, -2, "id");
	lua_rawgeti(L, LUA_REGISTRYINDEX, mt_ref);
	lua_setmetatable(L, -2);
}

// ---------------------------------------------------------------------------
// Binding and reading

// Fills `err` and returns false on a type Lua cannot hand to SQLite.
static bool bind_args(lua_State *L, sqlite3_stmt *stmt, int first, char *err, size_t errlen)
{
	int given = lua_gettop(L) - first + 1;
	if (given < 0)
		given = 0;

	int wanted = sqlite3_bind_parameter_count(stmt);
	if (given != wanted) {
		cslua_snprintf(err, errlen, "query takes %d parameter(s), %d given", wanted, given);
		err[errlen - 1] = '\0';
		return false;
	}

	for (int i = 0; i < given; i++) {
		int arg = first + i;
		int slot = i + 1;

		switch (lua_type(L, arg)) {
		case LUA_TNIL:
			sqlite3_bind_null(stmt, slot);
			break;

		case LUA_TBOOLEAN:
			// SQLite has no boolean; 0/1 is the convention its own docs use.
			sqlite3_bind_int(stmt, slot, lua_toboolean(L, arg) ? 1 : 0);
			break;

		case LUA_TNUMBER: {
			double n = lua_tonumber(L, arg);
			// Whole numbers go in as INTEGER so they compare and sort the way
			// the plugin author expects; everything else stays a double.
			if (n == (double)(sqlite3_int64)n)
				sqlite3_bind_int64(stmt, slot, (sqlite3_int64)n);
			else
				sqlite3_bind_double(stmt, slot, n);
			break;
		}

		case LUA_TSTRING: {
			// With the length, so a string with an embedded zero survives.
			size_t len = 0;
			const char *s = lua_tolstring(L, arg, &len);
			sqlite3_bind_text(stmt, slot, s, (int)len, SQLITE_TRANSIENT);
			break;
		}

		default:
			cslua_snprintf(err, errlen, "cannot bind a %s (parameter %d)",
				lua_typename(L, lua_type(L, arg)), slot);
			err[errlen - 1] = '\0';
			return false;
		}
	}

	return true;
}

// One row as a table keyed by column name.
//
// A NULL column is pushed as nil, which in a table means the key is simply not
// there. That is the documented behaviour, not an oversight: Lua has no other
// value that round-trips back to NULL.
static void push_row(lua_State *L, sqlite3_stmt *stmt)
{
	int columns = sqlite3_column_count(stmt);
	lua_newtable(L);

	for (int i = 0; i < columns; i++) {
		const char *name = sqlite3_column_name(stmt, i);
		if (!name)
			continue;

		switch (sqlite3_column_type(stmt, i)) {
		case SQLITE_INTEGER:
			// Lua numbers are doubles: exact up to 2^53, and this is where
			// a 64-bit rowid would start losing its tail.
			lua_pushnumber(L, (double)sqlite3_column_int64(stmt, i));
			break;

		case SQLITE_FLOAT:
			lua_pushnumber(L, sqlite3_column_double(stmt, i));
			break;

		case SQLITE_BLOB: {
			const void *blob = sqlite3_column_blob(stmt, i);
			int bytes = sqlite3_column_bytes(stmt, i);
			lua_pushlstring(L, blob ? (const char *)blob : "", (size_t)bytes);
			break;
		}

		case SQLITE_NULL:
			continue;		// leave the key out entirely

		default: {
			const unsigned char *text = sqlite3_column_text(stmt, i);
			int bytes = sqlite3_column_bytes(stmt, i);
			lua_pushlstring(L, text ? (const char *)text : "", (size_t)bytes);
			break;
		}
		}

		lua_setfield(L, -2, name);
	}
}

// ---------------------------------------------------------------------------
// Running statements

enum RunMode
{
	RUN_EXEC,		// no results wanted; returns the number of rows changed
	RUN_QUERY,		// every row, as an array
	RUN_FIRST		// the first row, or nil
};

// Steps an already-bound statement. Never raises: it reports through `err` so
// the caller can finalize first. Leaves its result on the stack for QUERY and
// FIRST, and pushes nothing for EXEC.
static bool step_stmt(lua_State *L, OpenDb *db, sqlite3_stmt *stmt, RunMode mode,
	const char *sql, char *err, size_t errlen)
{
	Watchdog watch;
	watchdog_start(db->handle, watch);

	int rows = 0;
	bool pushed = false;

	if (mode == RUN_QUERY) {
		lua_newtable(L);
		pushed = true;
	}

	int rc;
	while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
		if (mode == RUN_QUERY) {
			push_row(L, stmt);
			lua_rawseti(L, -2, ++rows);
			continue;
		}

		if (mode == RUN_FIRST) {
			push_row(L, stmt);
			pushed = true;
			rc = SQLITE_DONE;		// one row is all that was asked for
			break;
		}

		// RUN_EXEC does not care about rows, but a statement that produces
		// them still has to be stepped to the end to take effect.
	}

	long long ms = watchdog_stop(db->handle, watch);

	if (rc != SQLITE_DONE && rc != SQLITE_OK) {
		if (pushed)
			lua_pop(L, 1);

		if (rc == SQLITE_INTERRUPT)
			cslua_snprintf(err, errlen,
				"query aborted after %lldms (cslua_db_timeout_ms is %d). "
				"Add an index, narrow the query, or raise the limit",
				ms, watch.limit_ms);
		else
			cslua_snprintf(err, errlen, "%s", sqlite3_errmsg(db->handle));

		err[errlen - 1] = '\0';
		return false;
	}

	warn_if_slow(ms, db->plugin, sql);

	if (mode == RUN_FIRST && !pushed)
		lua_pushnil(L);			// no row matched

	if (mode == RUN_EXEC)
		lua_pushinteger(L, sqlite3_changes(db->handle));

	return true;
}

// Prepares exactly one statement out of `sql` and refuses anything trailing.
// Silently running only the first of several statements is the kind of thing
// that is found months later.
static sqlite3_stmt *prepare_one(OpenDb *db, const char *sql, char *err, size_t errlen)
{
	sqlite3_stmt *stmt = NULL;
	const char *tail = NULL;

	if (sqlite3_prepare_v2(db->handle, sql, -1, &stmt, &tail) != SQLITE_OK) {
		cslua_snprintf(err, errlen, "%s", sqlite3_errmsg(db->handle));
		err[errlen - 1] = '\0';
		return NULL;
	}

	if (!stmt) {
		cslua_snprintf(err, errlen, "no statement in that SQL");
		err[errlen - 1] = '\0';
		return NULL;
	}

	while (tail && (*tail == ';' || *tail == ' ' || *tail == '\t' ||
			*tail == '\n' || *tail == '\r'))
		tail++;

	if (tail && *tail) {
		sqlite3_finalize(stmt);
		cslua_snprintf(err, errlen,
			"more than one statement; run them one at a time (or use db:exec with no parameters)");
		err[errlen - 1] = '\0';
		return NULL;
	}

	return stmt;
}

// The shared body of db:exec/query/first.
static int db_run(lua_State *L, RunMode mode)
{
	OpenDb *db = self_db(L);
	const char *sql = luaL_checkstring(L, 2);

	char err[256] = "";
	sqlite3_stmt *stmt = prepare_one(db, sql, err, sizeof err);

	if (stmt) {
		if (bind_args(L, stmt, 3, err, sizeof err))
			step_stmt(L, db, stmt, mode, sql, err, sizeof err);
		sqlite3_finalize(stmt);
	}

	if (err[0])
		return luaL_error(L, "db: %s", err);

	return 1;
}

// ---------------------------------------------------------------------------
// Database methods

static int l_db_exec(lua_State *L)
{
	// With no parameters this accepts a whole script - a schema is naturally
	// several statements, and making people split it would be pointless. Bound
	// parameters need exactly one statement, which db_run enforces.
	if (lua_gettop(L) == 2) {
		OpenDb *db = self_db(L);
		const char *sql = luaL_checkstring(L, 2);

		char err[256] = "";
		char *msg = NULL;

		Watchdog watch;
		watchdog_start(db->handle, watch);
		int rc = sqlite3_exec(db->handle, sql, NULL, NULL, &msg);
		long long ms = watchdog_stop(db->handle, watch);

		if (rc != SQLITE_OK) {
			if (rc == SQLITE_INTERRUPT)
				cslua_snprintf(err, sizeof err,
					"query aborted after %lldms (cslua_db_timeout_ms is %d)",
					ms, watch.limit_ms);
			else
				cslua_snprintf(err, sizeof err, "%s", msg ? msg : sqlite3_errmsg(db->handle));
			err[sizeof err - 1] = '\0';
		}

		if (msg)
			sqlite3_free(msg);

		if (err[0])
			return luaL_error(L, "db:exec: %s", err);

		warn_if_slow(ms, db->plugin, sql);
		lua_pushinteger(L, sqlite3_changes(db->handle));
		return 1;
	}

	return db_run(L, RUN_EXEC);
}

static int l_db_query(lua_State *L)
{
	return db_run(L, RUN_QUERY);
}

static int l_db_first(lua_State *L)
{
	return db_run(L, RUN_FIRST);
}

static int l_db_prepare(lua_State *L)
{
	OpenDb *db = self_db(L);
	const char *sql = luaL_checkstring(L, 2);

	// The id is needed before the pointer, so resolve it the same way self_db
	// did rather than carrying it around.
	lua_getfield(L, 1, "id");
	int db_id = (int)lua_tointeger(L, -1);
	lua_pop(L, 1);

	char err[256] = "";
	sqlite3_stmt *stmt = prepare_one(db, sql, err, sizeof err);
	if (!stmt)
		return luaL_error(L, "db:prepare: %s", err);

	OpenStmt entry;
	entry.stmt = stmt;
	entry.db = db_id;
	entry.sql = sql;
	s_stmts.push_back(entry);

	push_handle(L, (int)s_stmts.size() - 1, s_stmt_mt_ref);
	return 1;
}

// db:transaction(fn) - one COMMIT for the whole block. Without this every
// insert is its own transaction and pays a disk sync, which is the difference
// between a thousand rows in a moment and a thousand rows in a minute.
static int l_db_transaction(lua_State *L)
{
	OpenDb *db = self_db(L);
	luaL_checktype(L, 2, LUA_TFUNCTION);

	if (db->in_transaction)
		return luaL_error(L, "db:transaction: already inside db:transaction()");

	// Keep the id, not the pointer: the callback may open another database and
	// grow the vector, which moves every element of it.
	lua_getfield(L, 1, "id");
	int db_id = (int)lua_tointeger(L, -1);
	lua_pop(L, 1);

	if (sqlite3_exec(db->handle, "BEGIN", NULL, NULL, NULL) != SQLITE_OK) {
		char err[256];
		cslua_snprintf(err, sizeof err, "%s", sqlite3_errmsg(db->handle));
		err[sizeof err - 1] = '\0';
		return luaL_error(L, "db:transaction: %s", err);
	}

	s_dbs[db_id].in_transaction = true;

	lua_pushvalue(L, 2);
	int rc = lua_pcall(L, 0, 1, 0);

	s_dbs[db_id].in_transaction = false;

	// The callback is free to have called db:close() on us. Nothing left to
	// commit or roll back in that case - the close already dealt with it.
	sqlite3 *handle = s_dbs[db_id].handle;

	if (rc != 0) {
		if (handle)
			sqlite3_exec(handle, "ROLLBACK", NULL, NULL, NULL);
		// The callback's error object is on top; let it travel on unchanged so
		// the message still names what actually went wrong.
		return lua_error(L);
	}

	if (handle && sqlite3_exec(handle, "COMMIT", NULL, NULL, NULL) != SQLITE_OK) {
		char err[256];
		cslua_snprintf(err, sizeof err, "commit failed: %s", sqlite3_errmsg(handle));
		err[sizeof err - 1] = '\0';
		sqlite3_exec(handle, "ROLLBACK", NULL, NULL, NULL);
		return luaL_error(L, "db:transaction: %s", err);
	}

	return 1;
}

static int l_db_last_id(lua_State *L)
{
	lua_pushnumber(L, (double)sqlite3_last_insert_rowid(self_db(L)->handle));
	return 1;
}

static int l_db_changes(lua_State *L)
{
	lua_pushinteger(L, sqlite3_changes(self_db(L)->handle));
	return 1;
}

static int l_db_path(lua_State *L)
{
	lua_pushstring(L, self_db(L)->path.c_str());
	return 1;
}

// Finalizes every statement prepared against a database. sqlite3_close refuses
// while any of them are alive, and a plugin is not going to track them all.
static void close_stmts_of(int db_id)
{
	for (size_t i = 1; i < s_stmts.size(); i++) {
		if (s_stmts[i].db != db_id || !s_stmts[i].stmt)
			continue;
		sqlite3_finalize(s_stmts[i].stmt);
		s_stmts[i].stmt = NULL;
	}
}

static void close_db(int id)
{
	if (id <= 0 || id >= (int)s_dbs.size() || !s_dbs[id].handle)
		return;

	close_stmts_of(id);
	sqlite3_close(s_dbs[id].handle);
	s_dbs[id].handle = NULL;
	s_dbs[id].in_transaction = false;
}

// Closing twice is not an error: cleanup code should not have to remember.
static int l_db_close(lua_State *L)
{
	luaL_checktype(L, 1, LUA_TTABLE);

	lua_getfield(L, 1, "id");
	int id = (int)lua_tointeger(L, -1);
	lua_pop(L, 1);

	close_db(id);
	return 0;
}

// ---------------------------------------------------------------------------
// Statement methods

static int stmt_run(lua_State *L, RunMode mode)
{
	OpenStmt *s = self_stmt(L);
	OpenDb *db = &s_dbs[s->db];

	// A statement is reused, so last run's bindings and cursor have to go.
	sqlite3_reset(s->stmt);
	sqlite3_clear_bindings(s->stmt);

	char err[256] = "";
	if (bind_args(L, s->stmt, 2, err, sizeof err))
		step_stmt(L, db, s->stmt, mode, s->sql.c_str(), err, sizeof err);

	sqlite3_reset(s->stmt);

	if (err[0])
		return luaL_error(L, "st: %s", err);

	return 1;
}

static int l_stmt_run(lua_State *L)   { return stmt_run(L, RUN_EXEC); }
static int l_stmt_query(lua_State *L) { return stmt_run(L, RUN_QUERY); }
static int l_stmt_first(lua_State *L) { return stmt_run(L, RUN_FIRST); }

static int l_stmt_sql(lua_State *L)
{
	lua_pushstring(L, self_stmt(L)->sql.c_str());
	return 1;
}

static int l_stmt_close(lua_State *L)
{
	luaL_checktype(L, 1, LUA_TTABLE);

	lua_getfield(L, 1, "id");
	int id = (int)lua_tointeger(L, -1);
	lua_pop(L, 1);

	if (id > 0 && id < (int)s_stmts.size() && s_stmts[id].stmt) {
		sqlite3_finalize(s_stmts[id].stmt);
		s_stmts[id].stmt = NULL;
	}
	return 0;
}

// ---------------------------------------------------------------------------
// Opening

// A database name is a name, not a path. Anything with a separator in it could
// put a file outside the plugin's own directory, which is exactly the mistake
// plugin_data_dir() exists to prevent.
static bool valid_db_name(const char *name)
{
	if (!name || !*name || strlen(name) > 64)
		return false;

	if (!strcmp(name, ".") || !strcmp(name, ".."))
		return false;

	for (const char *p = name; *p; p++) {
		bool ok = (*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z')
			|| (*p >= '0' && *p <= '9') || *p == '_' || *p == '-';
		if (!ok)
			return false;
	}

	return true;
}

// db.open("stats") -> a database in this plugin's own data directory.
// db.open(":memory:") -> a scratch database that dies with the state.
//
// Returns nil plus a reason on failure, the way create_entity() does: a missing
// directory or a locked file is a situation, not a programming error.
static int l_sqlite(lua_State *L)
{
	const char *name = luaL_checkstring(L, 1);
	int plugin = g_lua.current_index();

	std::string path;

	if (!strcmp(name, ":memory:")) {
		path = ":memory:";
	} else {
		if (!valid_db_name(name)) {
			lua_pushnil(L);
			lua_pushfstring(L, "'%s' is not a usable database name: letters, digits, "
				"'_' and '-' only, and no path separators", name);
			return 2;
		}

		std::string dir = cslua_plugin_data_dir(plugin);
		if (dir.empty()) {
			lua_pushnil(L);
			lua_pushstring(L, "no data directory (db.open() has to be called from a plugin)");
			return 2;
		}

		path = dir + "/" + name + ".db";

		// Already open for this plugin: hand back the same handle rather than a
		// second connection to the same file. Same idea as cvar_register().
		for (size_t i = 1; i < s_dbs.size(); i++) {
			if (s_dbs[i].handle && s_dbs[i].plugin == plugin && s_dbs[i].path == path) {
				push_handle(L, (int)i, s_db_mt_ref);
				return 1;
			}
		}
	}

	sqlite3 *handle = NULL;
	int rc = sqlite3_open_v2(path.c_str(), &handle,
		SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, NULL);

	if (rc != SQLITE_OK) {
		// sqlite3_open_v2 hands back a handle even on failure, purely so the
		// message can be read off it.
		std::string why = handle ? sqlite3_errmsg(handle) : "cannot open the database";
		if (handle)
			sqlite3_close(handle);

		lua_pushnil(L);
		lua_pushfstring(L, "%s: %s", path.c_str(), why.c_str());
		return 2;
	}

	// WAL is what makes the synchronous design work: writers stop blocking
	// readers, and a commit no longer waits on a full disk flush. NORMAL trades
	// "a crash of the machine may lose the last transactions" for not fsyncing
	// on every commit - the right trade for game statistics.
	sqlite3_exec(handle, "PRAGMA journal_mode = WAL", NULL, NULL, NULL);
	sqlite3_exec(handle, "PRAGMA synchronous = NORMAL", NULL, NULL, NULL);
	sqlite3_exec(handle, "PRAGMA foreign_keys = ON", NULL, NULL, NULL);

	// Deliberately short. This is time spent frozen if another process holds
	// the file, and the watchdog cannot interrupt a busy wait.
	sqlite3_busy_timeout(handle, 100);

	OpenDb entry;
	entry.handle = handle;
	entry.plugin = plugin;
	entry.path = path;
	entry.in_transaction = false;
	s_dbs.push_back(entry);

	push_handle(L, (int)s_dbs.size() - 1, s_db_mt_ref);
	return 1;
}

// ---------------------------------------------------------------------------
// Registration and teardown

static int l_readonly(lua_State *L)
{
	return luaL_error(L, "database objects are read-only, keep your own state in a table");
}

static const luaL_Reg s_db_methods[] =
{
	{ "exec",        l_db_exec },
	{ "query",       l_db_query },
	{ "first",       l_db_first },
	{ "prepare",     l_db_prepare },
	{ "transaction", l_db_transaction },
	{ "last_id",     l_db_last_id },
	{ "changes",     l_db_changes },
	{ "path",        l_db_path },
	{ "close",       l_db_close },
	{ NULL, NULL }
};

static const luaL_Reg s_stmt_methods[] =
{
	{ "run",   l_stmt_run },
	{ "query", l_stmt_query },
	{ "first", l_stmt_first },
	{ "sql",   l_stmt_sql },
	{ "close", l_stmt_close },
	{ NULL, NULL }
};

static int push_metatable(lua_State *L, const luaL_Reg *methods)
{
	lua_newtable(L);

	lua_newtable(L);
	for (const luaL_Reg *r = methods; r->name; r++) {
		lua_pushcfunction(L, r->func);
		lua_setfield(L, -2, r->name);
	}
	lua_setfield(L, -2, "__index");

	lua_pushcfunction(L, l_readonly);
	lua_setfield(L, -2, "__newindex");

	lua_pushboolean(L, 0);
	lua_setfield(L, -2, "__metatable");

	return luaL_ref(L, LUA_REGISTRYINDEX);
}

void cslua_register_db(lua_State *L)
{
	// Slot 0 is a placeholder so that an id of 0 - what a table without the
	// field reads as - is never a real handle.
	if (s_dbs.empty()) {
		OpenDb blank;
		blank.handle = NULL;
		blank.plugin = -1;
		blank.in_transaction = false;
		s_dbs.push_back(blank);
	}
	if (s_stmts.empty()) {
		OpenStmt blank;
		blank.stmt = NULL;
		blank.db = 0;
		s_stmts.push_back(blank);
	}

	// Once per process: the engine keeps the pointer, and a reload must not
	// hand it a second copy.
	if (!s_cvars_registered) {
		s_cvars_registered = true;

		// SQLITE_OMIT_AUTOINIT means no API call bootstraps the library on our
		// behalf - including sqlite3_open_v2, which would then allocate through
		// an allocator that was never set up. That is a crash, not an error
		// code, so this line is not optional.
		sqlite3_initialize();

		if (!CVAR_GET_POINTER(s_cvar_timeout.name))
			CVAR_REGISTER(&s_cvar_timeout);
		if (!CVAR_GET_POINTER(s_cvar_warn.name))
			CVAR_REGISTER(&s_cvar_warn);
	}

	s_db_mt_ref = push_metatable(L, s_db_methods);
	s_stmt_mt_ref = push_metatable(L, s_stmt_methods);

	static const luaL_Reg s_api[] =
	{
		{ "open", l_sqlite },
		{ NULL, NULL }
	};

	cslua_register_namespace(L, "db", s_api);
}

void cslua_db_shutdown()
{
	// Files, not registry refs: these have to be closed for real, and before
	// lua_close, not left for the state to take with it.
	for (size_t i = 1; i < s_dbs.size(); i++)
		close_db((int)i);

	s_dbs.clear();
	s_stmts.clear();

	s_db_mt_ref = LUA_NOREF;
	s_stmt_mt_ref = LUA_NOREF;
}

void cslua_db_remove_plugin(int plugin_index)
{
	for (size_t i = 1; i < s_dbs.size(); i++) {
		if (s_dbs[i].handle && s_dbs[i].plugin == plugin_index)
			close_db((int)i);
	}
}

int cslua_db_count_for_plugin(int plugin_index)
{
	int n = 0;
	for (size_t i = 1; i < s_dbs.size(); i++)
		if (s_dbs[i].handle && s_dbs[i].plugin == plugin_index)
			n++;
	return n;
}

int cslua_db_open_count()
{
	int n = 0;
	for (size_t i = 1; i < s_dbs.size(); i++)
		if (s_dbs[i].handle)
			n++;
	return n;
}
