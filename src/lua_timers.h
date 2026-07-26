#pragma once

#include <lua.hpp>

// Deferred and repeating callbacks. Driven by StartFrame, so they tick on
// server time and survive a map change (see cslua_timers_rebase).
void cslua_timers_clear();
void cslua_timers_remove_plugin(int plugin_index);
void cslua_timers_run();
int cslua_timers_count();

// How many live timers one plugin holds, for the lua_list breakdown.
int cslua_timers_count_for_plugin(int plugin_index);

// Called once per map, from ServerActivate. The server clock restarts there,
// so every pending deadline is moved onto the new one.
void cslua_timers_rebase();

// Registers the `timer` namespace: after, every, cancel, create, destroy.
void cslua_register_timers(lua_State *L);
