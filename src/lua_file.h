#pragma once

#include <lua.hpp>

// Raw byte-string file I/O, sandboxed to the calling plugin's own data
// directory (plugin.data_dir()). Filenames only - one optional extension, no
// path separators. Synchronous, like db and store.
void cslua_register_file(lua_State *L);

// log.write(msg): one text file per plugin per day
// (plugin.data_dir()/logs/<YYYY-MM-DD>.log). No held handle - the path is
// recomputed on every call. Old files are never cleaned up.
void cslua_register_log(lua_State *L);
