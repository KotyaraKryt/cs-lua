#pragma once

#include <lua.hpp>

// amxx.call(public_name, ...int args) -> result, true / nil, false, error.
// Reaches src/amxx_bridge.cpp, a separate AMXX module loaded by amxmodx (not
// metamod) - see third_party/amxmodx-sdk/README.md. Experimental,
// branch experiment/amxx-native-bridge only, never merged to main.
void cslua_register_amxx(lua_State *L);
