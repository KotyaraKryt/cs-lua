#include "cslua.h"
#include "rehlds.h"

#include <rehlds_api.h>

#include <windows.h>

static IRehldsApi *s_api = nullptr;
static IRehldsHookchains *s_hooks = nullptr;
static char s_version[64] = "unavailable";

bool cslua_rehlds_ready()
{
	return s_api != nullptr;
}

const char *cslua_rehlds_version()
{
	return s_version;
}

IRehldsHookchains *cslua_rehlds_hooks()
{
	return s_hooks;
}

bool cslua_rehlds_init()
{
	if (s_api)
		return true;

	// The engine lives in swds.dll for a dedicated server; the listen-server
	// builds keep it in hw/sw.dll.
	static const char *const candidates[] = { "swds.dll", "hw.dll", "sw.dll" };

	HMODULE engine = NULL;
	for (int i = 0; i < 3 && !engine; i++)
		engine = GetModuleHandleA(candidates[i]);

	if (!engine) {
		cslua_print("engine module not found, precache accounting will be approximate");
		return false;
	}

	CreateInterfaceFn factory = (CreateInterfaceFn)GetProcAddress(engine, "CreateInterface");
	if (!factory) {
		cslua_print("engine exports no CreateInterface: not ReHLDS, precache accounting will be approximate");
		return false;
	}

	int result = 0;
	IRehldsApi *api = (IRehldsApi *)factory(VREHLDS_HLDS_API_VERSION, &result);
	if (!api) {
		cslua_print("ReHLDS API not found, precache accounting will be approximate");
		return false;
	}

	int major = api->GetMajorVersion();
	int minor = api->GetMinorVersion();

	// Refusing on a mismatch is the safe move, not a pedantic one: the hooks we
	// want sit at the very end of the interface's vtable, so a single method
	// added in a newer version shifts every slot and our call would jump into
	// the wrong function - a crash, which is exactly what this module exists to
	// prevent.
	if (major != REHLDS_API_VERSION_MAJOR || minor < REHLDS_API_VERSION_MINOR) {
		cslua_print("ReHLDS API is %d.%d, we build against %d.%d - skipping engine hooks",
			major, minor, REHLDS_API_VERSION_MAJOR, REHLDS_API_VERSION_MINOR);
		cslua_print("  precache counts stay approximate; update ReHLDS for exact ones");
		return false;
	}

	s_api = api;
	s_hooks = api->GetHookchains();
	_snprintf(s_version, sizeof s_version, "%d.%d", major, minor);
	s_version[sizeof s_version - 1] = '\0';

	cslua_print("ReHLDS API %s connected", s_version);
	return true;
}
