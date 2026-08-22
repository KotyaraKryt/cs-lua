#include "cslua.h"
#include "rehlds.h"
#include "lua_events.h"

#include <rehlds_api.h>

#include "platform.h"

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

	// Which file the engine lives in depends on the platform and on how the
	// server was started; the list is in platform.cpp.
	void *engine = NULL;
	for (int i = 0; CSLUA_ENGINE_MODULES[i] && !engine; i++)
		engine = cslua_module_open(CSLUA_ENGINE_MODULES[i]);

	if (!engine) {
		cslua_print("engine module not found, precache accounting will be approximate");
		return false;
	}

	CreateInterfaceFn factory =
		(CreateInterfaceFn)cslua_module_symbol(engine, "CreateInterface");
	cslua_module_close(engine);

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
	cslua_snprintf(s_version, sizeof s_version, "%d.%d", major, minor);
	s_version[sizeof s_version - 1] = '\0';

	cslua_print("ReHLDS API %s connected", s_version);
	return true;
}

// Any cvar changing - the engine's own, another plugin's, this module's,
// through any path (console, rcon, a server.cfg exec) - goes through here.
// Pre: cancelling skips the chain entirely, so the cvar keeps its old value.
static void hook_cvar_direct_set(IRehldsHook_Cvar_DirectSet *chain, cvar_s *var, const char *value)
{
	if (g_events.fire_server_cvar_change(var && var->name ? var->name : "", value ? value : ""))
		return;

	chain->callNext(var, value);
}

// A bot connected. Notify only - id is the slot it was given, resolved
// after the real CreateFakeClient has already run so the edict exists.
static edict_t *hook_create_fake_client(IRehldsHook_CreateFakeClient *chain, const char *netname)
{
	edict_t *result = chain->callNext(netname);

	int id = result ? g_engfuncs.pfnIndexOfEdict(result) : 0;
	g_events.fire_client_bot_created(id);

	return result;
}

void cslua_rehlds_install_hooks()
{
	if (!s_hooks)
		return;

	s_hooks->Cvar_DirectSet()->registerHook(&hook_cvar_direct_set);
	s_hooks->CreateFakeClient()->registerHook(&hook_create_fake_client);
}
