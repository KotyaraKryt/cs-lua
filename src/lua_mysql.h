#pragma once

#include <lua.hpp>

// Async MySQL/MariaDB client for plugins backed by a remote site database.
//
//   local site = mysql.connect{ host = "127.0.0.1", user = "root",
//       password = "", database = "gamecms" }
//
//   site:query("SELECT id FROM users WHERE steam_id = ?", steamid, function(res)
//       if not res.ok then return print(res.error) end
//       for _, row in ipairs(res.rows) do print(row.id) end
//   end)
//
// Same shape as http.*: the query runs on a worker, the callback on the game
// thread from the drain that ticks timers. Statically linked (see
// lua_mysql.cpp), no runtime dependency.
//
// Optional helpers (same async + callback shape):
//   conn:find("users", { where = { steam_id = id }, limit = 1 }, cb)
//   conn:create("users", { steam_id = id, name = "x" }, cb)
//   conn:update("users", { where = { id = 1 }, set = { name = "y" } }, cb)
//   conn:delete("users", { where = { id = 1 } }, cb)
//
// Migrations (sequential, one connection lock for the whole batch):
//   conn:migrate({ migrations = {
//     { id = "001_init", sql = "CREATE TABLE ..." },
//     { id = "002_col",  file = "002_col.sql" },  -- plugin data dir only
//   } }, cb)
//   -- res.applied is the list of ids that ran in this call.

void cslua_register_mysql(lua_State *L);

// Runs finished queries' callbacks. Once per frame, next to http and timers.
void cslua_mysql_run();

// Disowns everything in flight for the state being torn down (lua_reload).
// Sockets are left idle until cslua_mysql_shutdown() at process exit.
void cslua_mysql_reset();

// Stops the workers, waits, then closes every connection's socket. Process exit
// only.
void cslua_mysql_shutdown();

// Forgets one plugin's pending queries.
void cslua_mysql_remove_plugin(int plugin_index);

// How many queries are waiting or running, for lua_list.
int cslua_mysql_pending();
