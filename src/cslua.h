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
#define CSLUA_VERSION	"0.5.0"
#define CSLUA_TAG		"[cs-lua] "

// The scripting API version. A plain integer that bumps on every breaking
// change to the Lua-facing API - independent of CSLUA_VERSION, which tracks
// the whole project. Plugins declare the version they were written against
// via plugin{ api_version = N }. Starts at 1 with the first public release.
#define CSLUA_API_VERSION 1

// Max players an HLDS server can hold, +1 because edict indices are 1-based.
#define CSLUA_MAXPLAYERS 33

// Prints to the server console and log with our tag. Everything the user is
// meant to see (script output, errors, command replies) goes through these.
void cslua_print(const char *fmt, ...);
void cslua_error(const char *fmt, ...);

// <gamedir>/addons/lua, resolved from metamod's GINFO_GAMEDIR.
const std::string &cslua_base_dir();
