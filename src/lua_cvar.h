#pragma once

#include <lua.hpp>

// cvar() reads and writes engine console variables; cvar_register() creates a
// plugin's own. Registered cvars live in the engine forever, so their storage
// outlives any lua_State and survives lua_reload.
void cslua_register_cvar(lua_State *L);

// Frees nothing engine-side (cvars cannot be unregistered), just drops our
// bookkeeping so a reload does not report stale duplicates.
void cslua_cvar_shutdown();
