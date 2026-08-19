#pragma once

#include <lua.hpp>

// vec.* - plain vector math with no edict involved, so unlike ents.trace_line
// these take the project's usual x, y, z triples rather than {x,y,z} tables
// (compare p:origin()/e:origin(), which return and accept the same way).
void cslua_register_vector(lua_State *L);
