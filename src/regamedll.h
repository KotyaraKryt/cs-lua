#pragma once

// ReGameDLL gives typed access to CS state that plain HLSDK cannot reach -
// team, money, deaths, weapons - plus real gameplay hooks. Everything here is
// optional at runtime: on a vanilla mp.dll the API simply stays unavailable
// and the CS-specific natives report that instead of crashing.

class CBasePlayer;

bool cslua_regamedll_init();

bool cslua_regamedll_ready();
const char *cslua_regamedll_version();

// NULL when the API is missing, the slot is empty, or the entity is not a
// player yet.
CBasePlayer *cslua_player_entity(int id);

// Thin predicates so callers that must not pull in the heavy CS headers (like
// the natives file) can still filter players. Both need ReGameDLL: alive is
// false and team is "" when the API or the player is unavailable.
bool cslua_player_alive(int id);
const char *cslua_player_team_name(int id);

// Installs / removes the ReGameDLL hookchains that feed gameplay events
// (spawn, death, round end) to Lua. Safe to call when the API is unavailable -
// they do nothing. Install runs after scripts register handlers; remove runs
// before the state is torn down, so a reload does not leave the game DLL
// calling into a dead lua_State.
void cslua_regamedll_install_hooks();
void cslua_regamedll_remove_hooks();
