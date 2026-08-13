#pragma once

#include <lua.hpp>

// For edict_t, used by the touch-detonate declarations below.
#include <extdll.h>

// The entity object and the globals that hand one out. Unlike a player, an
// entity object is made on demand: there is no fixed set of them and the engine
// reuses indices, so an object carries the edict's serial number as well and
// every method checks it before touching anything.
void cslua_register_entity(lua_State *L);
void cslua_entity_shutdown();

// Wraps an edict by index the same way ents.find/e:spawn do, for hookchains
// that only hand back an index (or 0 for "no entity").
void cslua_push_entity_index(lua_State *L, int index);

// Backing e:detonate_on_touch() (see lua_entity.cpp for why this exists
// instead of a general touch callback). cslua_touch_detonate_check runs from
// pfnTouch in dllapi.cpp for every entity-to-entity touch in the game, so it
// has to be cheap when nothing is watched - which it is, one size check.
void cslua_touch_detonate_watch(int index, int serial);
void cslua_touch_detonate_check(edict_t *touched);

// Drops everything being watched. Call on map change: an (index, serial)
// pair from the map that just ended means nothing in the next one.
void cslua_touch_detonate_clear();
