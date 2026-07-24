#pragma once

#include <lua.hpp>

// Sound and precache.
//
// GoldSrc only accepts precache calls while a map is loading. A plugin, though,
// is loaded once at server start and lives across map changes, so it cannot
// simply call precache at the right moment. Instead precache_sound() records
// what is needed in a registry that outlives the Lua state, and the engine hook
// replays that registry on every worldspawn. Scripts never think about timing.

void cslua_register_sound(lua_State *L);

// Hooks the engine's precache calls so the counters cover the whole server,
// not just what we asked for. No-op without ReHLDS. Called once at startup.
void cslua_sound_install_hooks();

// Forgets the registry (on lua_reload; plugins re-register as they load).
void cslua_sound_clear();

// Precaches everything in the registry. Called from the worldspawn hook, which
// is the one point where the engine allows it.
void cslua_precache_all();

// Opens/closes the window where an immediate precache is legal.
void cslua_sound_set_window(bool open);

// Plays a sound. id is a player slot, or 0 for everyone connected.
void cslua_play_sound(int id, const char *sample, int channel, float volume,
	float attenuation, int pitch);

// For the lua_precache report: how full each table is, and how much of it we
// registered ourselves. used is -1 before anything has been precached.
int cslua_precache_used(bool models);
int cslua_precache_limit();
int cslua_precache_registered(bool models);
