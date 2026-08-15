#pragma once

#include <lua.hpp>

// Player objects are created once per slot at state startup and reused, so
// firing an event costs no allocation. An object only carries its index; every
// method reads live state, exactly like an AMXX player id.
void cslua_player_init(lua_State *L);
void cslua_player_shutdown();

// Pushes the cached object for a slot (1..32).
void cslua_push_player(lua_State *L, int id);

// Pushes the broadcast target used as the global `all`.
void cslua_push_all(lua_State *L);

// sv.hull_free(x, y, z[, ducking]) - registered into `sv` from
// lua_natives.cpp, implemented here for the TRACE_HULL/Vector plumbing
// this file already has.
int cslua_sv_hull_free(lua_State *L);
