#pragma once

#include <lua.hpp>

// Installs the cs-lua globals (print override, plugin, on, player_*) into L.
void cslua_register_natives(lua_State *L);
