#pragma once

#include <lua.hpp>

// SQLite, compiled into the module. For the cases store.lua's read-all/write-all
// stops working - stats, balances, ban lists, anything counted in thousands of
// rows.
//
// Synchronous on purpose: a local file in WAL mode answers in fractions of a
// millisecond. A runaway query is handled by a watchdog (the progress handler
// in the .cpp).
void cslua_register_db(lua_State *L);

// Closes every open database. Owns an OS resource, so it must run before
// lua_close().
void cslua_db_shutdown();

// Drops what one plugin left open.
void cslua_db_remove_plugin(int plugin_index);

// For lua_list.
int cslua_db_open_count();
int cslua_db_count_for_plugin(int plugin_index);
