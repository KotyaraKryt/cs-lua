#pragma once

#include <lua.hpp>

// The entity object and the globals that hand one out. Unlike a player, an
// entity object is made on demand: there is no fixed set of them and the engine
// reuses indices, so an object carries the edict's serial number as well and
// every method checks it before touching anything.
void cslua_register_entity(lua_State *L);
void cslua_entity_shutdown();
