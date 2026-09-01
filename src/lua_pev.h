#pragma once

#include <lua.hpp>

// For entvars_t, used below.
#include <extdll.h>

// Shared by e:pev() in lua_entity.cpp and p:pev() in lua_player.cpp - the one
// place that knows entvars_t's field layout, so both objects read and write the
// same struct through the same table.
//
// `name_arg` is the stack index of the field name; arguments after it are the
// value(s) to write (absent or nil means read). Returns the number of values
// pushed for a read, 0 for a write.
int cslua_pev_call(lua_State *L, entvars_t *pev, int name_arg);

// Unwraps an entity object ({index=...}) or a player object ({id=...}) into its
// edict, or NULL for nil. Shared with ents.trace_*'s `opts.skip`.
edict_t *cslua_arg_to_edict(lua_State *L, int idx);
