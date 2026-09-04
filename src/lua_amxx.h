#pragma once

#include <lua.hpp>

// amxx.call(public_name, ...) -> result, true[, out_string]
//                             or nil, false, error.
// Arguments are numbers, strings, or amxx.out() for a slot the Pawn side
// writes into.
// Reaches src/amxx_bridge.cpp, a separate AMXX module loaded by amxmodx (not
// metamod) - see third_party/amxmodx-sdk/README.md. Experimental,
// branch experiment/amxx-native-bridge only, never merged to main.
void cslua_register_amxx(lua_State *L);
