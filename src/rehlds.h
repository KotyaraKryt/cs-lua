#pragma once

// ReHLDS API, used for engine-level hookchains. Right now it exists for one
// job: counting every precache the server makes - ours, the game DLL's and
// other plugins' - so we always know how close the 512-slot tables are to
// overflowing instead of finding out when the server dies.
//
// Optional at runtime: on a stock HLDS the API is simply absent and the
// counters fall back to what we can observe ourselves.

class IRehldsHookchains;

bool cslua_rehlds_init();
bool cslua_rehlds_ready();
const char *cslua_rehlds_version();
IRehldsHookchains *cslua_rehlds_hooks();

// Cvar_DirectSet/CreateFakeClient - the two ReHLDS hooks general enough that
// they do not belong to any one subsystem (unlike cslua_sound.cpp's
// precache pair or cslua_netwatch.cpp's SV_DropClient). No-op without
// ReHLDS, same fallback as everything else in this file. Called once at
// startup, after cslua_rehlds_init.
void cslua_rehlds_install_hooks();
