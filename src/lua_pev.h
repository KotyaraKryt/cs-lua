#pragma once

#include <lua.hpp>

// For entvars_t, used by the declaration below.
#include <extdll.h>

// Shared by e:pev()/e:pev(name, ...) in lua_entity.cpp and p:pev()/p:pev(name,
// ...) in lua_player.cpp - the one place that knows entvars_t's field layout,
// so an entity and a player object read and write the exact same struct
// through the exact same table instead of two hand-maintained copies of it.
//
// `name_arg` is the stack index of the field name; any arguments after it are
// the value(s) to write (absent or nil means read). Follows the project's
// read/write-in-one-call convention: returns the number of values pushed for
// a read, 0 for a write.
int cslua_pev_call(lua_State *L, entvars_t *pev, int name_arg);

// Unwraps an entity object ({index=...}) or a player object ({id=...}) at
// stack index `idx` into its edict, or NULL for a nil argument. Shared with
// ents.trace_line()/ents.trace_hull() in lua_entity.cpp for their `opts.skip`,
// which takes either kind of object the same way an edict-pointer pev field
// (owner, aiment, ...) does.
edict_t *cslua_arg_to_edict(lua_State *L, int idx);
