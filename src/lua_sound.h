#pragma once

#include <lua.hpp>

// Sound and precache.
//
// GoldSrc only accepts precache calls while a map is loading, but a plugin
// lives across map changes. So res.sound()/res.model() record what is needed in
// a registry that outlives the Lua state, and the worldspawn hook replays it.

void cslua_register_sound(lua_State *L);

// Hooks the engine's precache calls so the counters cover the whole server.
// No-op without ReHLDS. Called once at startup.
void cslua_sound_install_hooks();

// Forgets the registry (on lua_reload; plugins re-register as they load).
void cslua_sound_clear();

// Precaches everything in the registry. Called from the worldspawn hook.
void cslua_precache_all();

// Opens/closes the window where an immediate precache is legal.
void cslua_sound_set_window(bool open);

// Plays a sound. id is a player slot, or 0 for everyone connected.
void cslua_play_sound(int id, const char *sample, int channel, float volume,
	float attenuation, int pitch);

// Plays a sound locally on one client via its own "spk" console command - no
// EMIT_SOUND, no PAS, nobody nearby hears it. Player slot only (id > 0); still
// needs res.sound().
void cslua_play_sound_private(int id, const char *sample);

// For the lua_precache report. used is -1 before anything has been precached.
int cslua_precache_used(bool models);
int cslua_precache_limit();
int cslua_precache_registered(bool models);
