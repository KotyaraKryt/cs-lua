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
