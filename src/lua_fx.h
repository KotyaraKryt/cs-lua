#pragma once

#include <lua.hpp>

// Temp-entity visuals: explosions, beams, sprite trails. Nothing here reads
// state back - fire and forget, same as res.sound/res.model are precache and
// forget. A sprite passed to any of these must already be precached with
// res.model, same contract as everywhere else in the API.
void cslua_register_fx(lua_State *L);
