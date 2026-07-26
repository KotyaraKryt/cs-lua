#pragma once

#include <lua.hpp>

// command("name", fn) registers a server console command handled in Lua.
// The engine keeps command pointers forever, so we register one trampoline per
// name and route by name at call time; the Lua handler can be swapped on reload
// while the engine registration stays put.
void cslua_register_command_api(lua_State *L);

// Drops the Lua handler table (its refs die with the state). Engine
// registrations survive - a command whose plugin is gone becomes a no-op.
void cslua_command_shutdown();

// Makes one plugin's console commands dormant. The engine registration itself
// is permanent - AddServerCommand keeps our name pointer forever - so the
// command stays in the console's list and simply does nothing until the plugin
// registers it again.
void cslua_command_remove_plugin(int plugin_index);
