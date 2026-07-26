#pragma once

#include <lua.hpp>

// SQLite, compiled into the module. store.lua reads a whole table into memory
// and writes it back; this is for the cases where that stops working - stats,
// balances, per-plugin ban lists, anything counted in thousands of rows.
//
// Synchronous on purpose: a local file in WAL mode answers in fractions of a
// millisecond, and a worker thread would be the first thread in this module and
// would turn the whole API callback-shaped. A runaway query is handled by a
// watchdog instead - see the progress handler in the .cpp.
void cslua_register_db(lua_State *L);

// Closes every open database. Unlike the other modules this owns an OS
// resource, not just a registry ref, so it has to run before lua_close().
void cslua_db_shutdown();

// Drops what one plugin left open. Used when a plugin blows up while loading.
void cslua_db_remove_plugin(int plugin_index);

// For lua_list.
int cslua_db_open_count();
