#pragma once

#include <lua.hpp>

#include <string>

// Creates (or reuses) the global table `name` and registers `list` into it.
// The one door through which the module adds anything to _G.
void cslua_register_namespace(lua_State *L, const char *name, const luaL_Reg *list);

// Builds { __index = methods, __metatable = false } and leaves it on the
// stack. A caller that needs more on top - __newindex, __tostring - adds it
// before ref'ing the table itself; cslua_register_metatable below is the
// shortcut for the common case that needs nothing else.
void cslua_push_metatable(lua_State *L, const luaL_Reg *list);

// cslua_push_metatable() then luaL_ref() in one call, for a metatable that
// needs nothing beyond __index and the __metatable guard.
int cslua_register_metatable(lua_State *L, const luaL_Reg *list);

// Installs the base globals: print, the plugin{} manifest, and the `sv`,
// `plugin` and `players` namespaces.
void cslua_register_natives(lua_State *L);

// addons/lua/data/<plugin_id>/ for the given plugin, created if absent. Empty
// on failure or when the index names no plugin. Behind plugin.data_dir(), and
// used directly by the database layer.
std::string cslua_plugin_data_dir(int plugin_index);
