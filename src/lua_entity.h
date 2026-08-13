#pragma once

#include <lua.hpp>

// The entity object and the globals that hand one out. Unlike a player, an
// entity object is made on demand: there is no fixed set of them and the engine
// reuses indices, so an object carries the edict's serial number as well and
// every method checks it before touching anything.
void cslua_register_entity(lua_State *L);
void cslua_entity_shutdown();

// Wraps an edict by index the same way ents.find/e:spawn do, for hookchains
// that only hand back an index (or 0 for "no entity").
void cslua_push_entity_index(lua_State *L, int index);
