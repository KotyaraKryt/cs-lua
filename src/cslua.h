#pragma once

// mutil.h -> <util.h> (cssdk) uses CBaseEntity as a default template argument
// before cbase.h declares it.
class CBaseEntity;

#include <extdll.h>
#include <meta_api.h>

#include "platform.h"

#include <string>

#define CSLUA_NAME		"cs-lua"
#define CSLUA_VERSION	"1.0.0"
#define CSLUA_TAG		"[cs-lua] "

// Scripting API version. Bumps on every breaking change to the Lua-facing API,
// independent of CSLUA_VERSION. Plugins declare it via plugin{ api_version = N }.
#define CSLUA_API_VERSION 1

// Max players (32) + 1: edict indices are 1-based.
#define CSLUA_MAXPLAYERS 33

// A real player slot: 1..32. The same range check was independently
// hand-rolled at 20+ call sites across the module; this is the one copy.
inline bool cslua_valid_player_id(int id)
{
	return id >= 1 && id < CSLUA_MAXPLAYERS;
}

// A real player slot, or 0 - the players.broadcast target, not a player.
// A handful of call sites (the ones dealing with a message/ping cache
// indexed the same way broadcast writes go) accept that too.
inline bool cslua_valid_player_or_broadcast_id(int id)
{
	return id >= 0 && id < CSLUA_MAXPLAYERS;
}

void cslua_print(const char *fmt, ...);
void cslua_error(const char *fmt, ...);

const std::string &cslua_base_dir();

// The mod's maps/ directory, e.g. "cstrike/maps" - where .bsp files live.
const std::string &cslua_maps_dir();

// Monotonic seconds, independent of gpGlobals->time (which restarts every map).
double cslua_now_seconds();

bool cslua_profiling();
void cslua_profile_init();

// True once the map's entities exist and until the map ends. Anything reaching
// into the edict table must check: plugins load during Meta_Attach, before
// there is a world, and the engine walks the table without checking.
bool cslua_world_ready();

void cslua_set_game_desc(const char *text);
