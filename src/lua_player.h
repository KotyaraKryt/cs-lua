#pragma once

#include <lua.hpp>

// For TraceResult and Vector, used below.
#include <extdll.h>

// Player objects are created once per slot at state startup and reused, so
// firing an event costs no allocation. An object carries only its index; every
// method reads live state.
void cslua_player_init(lua_State *L);
void cslua_player_shutdown();

// Pushes the cached object for a slot (1..32).
void cslua_push_player(lua_State *L, int id);

// Pushes the broadcast target used as the global `all`.
void cslua_push_all(lua_State *L);

// sv.hull_free(x, y, z[, ducking]) - registered into `sv` from lua_natives.cpp,
// implemented here for the TRACE_HULL/Vector plumbing.
int cslua_sv_hull_free(lua_State *L);

// Turns a TraceResult into the { kind, player, classname, x, y, z, distance,
// hitgroup } table p:trace()/ents.trace_line() hand back. `start` is only
// needed for distance.
void cslua_push_trace_result(lua_State *L, const TraceResult &tr, const Vector &start);

// Resets the fallback ping/loss cache for a slot - called on disconnect so a
// reused slot doesn't briefly report the previous occupant's numbers.
void cslua_player_reset_ping(int id);
