#pragma once

#include <lua.hpp>

// command("name", fn) registers a server console command handled in Lua. The
// engine keeps command pointers forever, so one trampoline is registered per
// name and dispatch is by name; the Lua handler can be swapped on reload.
void cslua_register_command_api(lua_State *L);

// Drops the Lua handler table; engine registrations survive as no-ops.
void cslua_command_shutdown();

// Makes one plugin's commands dormant until it re-registers them.
void cslua_command_remove_plugin(int plugin_index);
