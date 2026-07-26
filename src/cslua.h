#pragma once

// metamod's mutil.h includes <util.h>, which under cssdk is the full CS one:
// it uses CBaseEntity as a default template argument long before cbase.h
// declares it. Declaring it here fixes every translation unit at once.
class CBaseEntity;

#include <extdll.h>
#include <meta_api.h>

#include "platform.h"

#include <string>

#define CSLUA_NAME		"cs-lua"
#define CSLUA_VERSION	"2.0.0"
#define CSLUA_TAG		"[cs-lua] "

// The scripting API version. A plain integer that bumps on every breaking
// change to the Lua-facing API - independent of CSLUA_VERSION, which tracks
// the whole project. Plugins declare the version they were written against
// via plugin{ api_version = N }.
//
// v1 -> v2 renamed the whole Lua surface into namespaces and gave every event
// handler one table instead of positional arguments. A v1 plugin is refused at
// its plugin{} line rather than left to die on the first missing global; see
// docs/migration.md.
#define CSLUA_API_VERSION 2

// Max players an HLDS server can hold, +1 because edict indices are 1-based.
#define CSLUA_MAXPLAYERS 33

// Prints to the server console and log with our tag. Everything the user is
// meant to see (script output, errors, command replies) goes through these.
void cslua_print(const char *fmt, ...);
void cslua_error(const char *fmt, ...);

// <gamedir>/addons/lua, resolved from metamod's GINFO_GAMEDIR.
const std::string &cslua_base_dir();

// Monotonic seconds, independent of the server clock: gpGlobals->time restarts
// on every map, which would make any measurement spanning one meaningless.
double cslua_now_seconds();

// Whether the cslua_profile cvar is on. Checked once per dispatch rather than
// per handler: reading a cvar costs more than a small handler does.
bool cslua_profiling();

// Registers the cslua_profile cvar. Called once at startup.
void cslua_profile_init();

// True once the map's entities exist and until the map ends. Anything that
// reaches into the engine's edict table has to ask first: plugins load during
// Meta_Attach, long before there is a world, and the engine does not check -
// it walks a table that is not there yet and takes the server down.
bool cslua_world_ready();
