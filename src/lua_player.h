#pragma once

#include <lua.hpp>

// For TraceResult and Vector, used by the declaration below.
#include <extdll.h>

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

// Turns a finished TraceResult into the { kind, player, classname, x, y, z,
// distance, hitgroup } table p:trace()/p:trace_to() hand back. Shared with
// ents.trace_line()/ents.trace_hull() in lua_entity.cpp, which fire from
// arbitrary points rather than a player's eyes but want the same result
// shape. `start` is only needed to compute distance.
void cslua_push_trace_result(lua_State *L, const TraceResult &tr, const Vector &start);
