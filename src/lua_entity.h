#pragma once

#include <lua.hpp>

// For edict_t, used below.
#include <extdll.h>

// The entity object and the globals that hand one out. Unlike a player, an
// entity object is made on demand and the engine reuses indices, so it carries
// the edict's serial number and every method checks it first.
void cslua_register_entity(lua_State *L);
void cslua_entity_shutdown();

// Wraps an edict by index, for hookchains that only hand back an index (or 0).
void cslua_push_entity_index(lua_State *L, int index);

// Backing e:detonate_on_touch(). cslua_touch_detonate_check runs from pfnTouch
// for every entity-to-entity touch, so it has to be cheap when nothing is
// watched - one size check.
void cslua_touch_detonate_watch(int index, int serial);
void cslua_touch_detonate_check(edict_t *touched);

// Drops everything being watched. Call on map change.
void cslua_touch_detonate_clear();
