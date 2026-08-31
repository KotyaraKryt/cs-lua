#pragma once

#include <lua.hpp>

// Async MySQL/MariaDB client for plugins that live on a remote site database
// (the GameCMS-style "admins/services/users live in the site's MySQL, not on
// the game server" integration).
//
//   local site = mysql.connect{ host = "127.0.0.1", user = "root",
//       password = "", database = "gamecms" }
//
//   site:query("SELECT id, shilings FROM users WHERE steam_id = ?", steamid,
//       function(res)
//           if not res.ok then return print(res.error) end
//           for _, row in ipairs(res.rows) do print(row.id, row.shilings) end
//       end)
//
// Same shape as http.*: the query runs on a worker thread, the callback runs
// on the game thread from the same drain that ticks timers. Nothing a plugin
// writes ever touches a socket, and nothing a worker touches ever touches the
// Lua state.
//
// Statically linked, same as SQLite (third_party/mariadb-connector-c, LGPL -
// compatible with this project's own GPLv3, unlike Oracle's dual
// GPL/commercial libmysqlclient). Built with TLS and every dynamic auth
// plugin turned off, so the result has no runtime dependency of its own
// either - see CMakeLists.txt. A bare dlopen() of the system's own
// libmysqlclient was tried first and abandoned: which SONAME exists, and
// what *that* itself needs (OpenSSL 1.0 vs 1.1 vs 3.0, ...), varies enough
// across hosts that it was a losing game, especially on shared hosting with
// no root and no apt to fix it with.
//
// A connection is a handle object (id + metatable, like db.open()), but the
// real TCP connect happens lazily on a worker the first time something is
// queried on it - mysql.connect() itself never blocks the frame.
//
// Optional helpers (same async + callback shape as query/exec):
//
//   conn:find("users", { where = { steam_id = id }, limit = 1 }, cb)
//   conn:create("users", { steam_id = id, name = "x" }, cb)
//   conn:update("users", { where = { id = 1 }, set = { name = "y" } }, cb)
//   conn:delete("users", { where = { id = 1 } }, cb)
//
// Migrations (sequential, one connection lock for the whole batch):
//
//   conn:migrate({
//     migrations = {
//       { id = "001_init", sql = "CREATE TABLE ..." },
//       { id = "002_col",  file = "002_col.sql" },  -- plugin data dir only
//     },
//   }, cb)
//
// res.applied is the list of migration ids that ran in this call.

void cslua_register_mysql(lua_State *L);

// Runs finished queries' callbacks. Called once per frame, next to http and
// timers.
void cslua_mysql_run();

// Disowns everything in flight for the state being torn down (lua_reload).
// Mirrors http: sockets are not closed here - a worker mid-query cannot be
// interrupted, and waiting for it would stall the reload. They are left idle
// until cslua_mysql_shutdown() at process exit. A reply landing afterwards
// belongs to a past generation and is dropped by the drain.
void cslua_mysql_reset();

// Stops the workers, waits for them, and only then closes every connection's
// socket - safe now that nothing can be using them. Process exit only.
void cslua_mysql_shutdown();

// Forgets one plugin's pending queries. Its callbacks reference an
// environment that is being torn down.
void cslua_mysql_remove_plugin(int plugin_index);

// How many queries are waiting or running, for lua_list.
int cslua_mysql_pending();