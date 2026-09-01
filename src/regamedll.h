#pragma once

// ReGameDLL gives typed access to CS state plain HLSDK cannot reach (team,
// money, deaths, weapons) plus real gameplay hooks. All optional at runtime: on
// a vanilla mp.dll the API stays unavailable and the CS-specific natives report
// that instead of crashing.

class CBasePlayer;

bool cslua_regamedll_init();

bool cslua_regamedll_ready();
const char *cslua_regamedll_version();

// NULL when the API is missing. GetWeaponInfo(classname) backs p:give's
// disguise opts.
class IReGameApi;
IReGameApi *cslua_regamedll_api();

// NULL when the API is missing, the slot is empty, or the entity is not a
// player yet.
CBasePlayer *cslua_player_entity(int id);

// Thin predicates so callers that must not pull in the heavy CS headers can
// still filter players. Both need ReGameDLL: alive is false and team is "" when
// unavailable.
bool cslua_player_alive(int id);
const char *cslua_player_team_name(int id);

// classname of the weapon in hand, "" if none/unavailable. Backs
// weapon_secondary_attack's edge-detect in dllapi.cpp.
const char *cslua_player_active_weapon(int id);

// m_iPrimaryAmmoType of the weapon in hand, -1 if none/unavailable. Tells a
// real grenade apart from a p:give(..., {ammo_type=N}) disguised one.
int cslua_player_active_weapon_ammo_type(int id);

// Installs / removes the ReGameDLL hookchains that feed gameplay events to Lua.
// Safe when the API is unavailable. Install runs after scripts register
// handlers; remove runs before teardown so a reload does not leave the game DLL
// calling into a dead lua_State.
void cslua_regamedll_install_hooks();
void cslua_regamedll_remove_hooks();
