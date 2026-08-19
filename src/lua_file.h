#pragma once

#include <lua.hpp>

// Raw byte-string file I/O, sandboxed to the calling plugin's own data
// directory (plugin.data_dir(), same root db.open()/store already write
// into). Filenames only - one optional extension, no path separators, same
// shape as db.open()'s name plus a dot. Synchronous, like db and store: this
// runs on the game thread, and local disk I/O on a size-capped file does not
// block long enough to justify a worker thread and a callback-shaped API.
void cslua_register_file(lua_State *L);

// log.write(msg): one rotating text file per plugin, dated by the day it is
// written on (plugin.data_dir()/logs/<YYYY-MM-DD>.log). No held handle, no
// in-memory rotation state - the path is recomputed from the current date on
// every call, same pattern as cslua_netwatch.cpp's log_line(). Old dated
// files are never cleaned up; that is a deliberate v1 scope decision, not an
// oversight.
void cslua_register_log(lua_State *L);
