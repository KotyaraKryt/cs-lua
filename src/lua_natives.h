#pragma once

#include <lua.hpp>

#include <string>

// Creates (or reuses) the global table `name` and registers `list` into it.
// The one door through which the module adds anything to _G.
void cslua_register_namespace(lua_State *L, const char *name, const luaL_Reg *list);

// Installs the base globals: print, the plugin{} manifest, and the `sv`,
// `plugin` and `players` namespaces.
void cslua_register_natives(lua_State *L);

// addons/lua/data/<plugin_id>/ for the given plugin, created if absent. Empty
// on failure or when the index names no plugin. Behind plugin.data_dir(), and
// used directly by the database layer.
std::string cslua_plugin_data_dir(int plugin_index);
