#pragma once

// Registers the lua_reload / lua_list server console commands.
// Must be called once, at Meta_Attach.
void cslua_register_commands();
