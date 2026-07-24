#pragma once

#include <lua.hpp>

// Deferred and repeating callbacks. Driven by StartFrame, so they tick on
// server time and survive a map change (see cslua_timers_rebase).
void cslua_timers_clear();
void cslua_timers_remove_plugin(int plugin_index);
void cslua_timers_run();
int cslua_timers_count();

// Called once per map, from ServerActivate. The server clock restarts there,
// so every pending deadline is moved onto the new one.
void cslua_timers_rebase();

// Lua: after(seconds, fn) / every(seconds, fn) -> id, cancel(id)
int cslua_l_after(lua_State *L);
int cslua_l_every(lua_State *L);
int cslua_l_cancel(lua_State *L);
