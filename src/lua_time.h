#pragma once

#include <lua.hpp>

// Duration parsing and calendar math, shared so every plugin stops writing
// its own parse_duration. "3d", "45m", and compound strings like
// "60s60m24h32d" (summed) all go through time.parse.
void cslua_register_time(lua_State *L);