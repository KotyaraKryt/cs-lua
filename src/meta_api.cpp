#include "cslua.h"
#include "cslua_bootstrap.h"
#include "lua_http.h"
#include "lua_mysql.h"
#include "lua_httpserver.h"

#include <time.h>
#include "lua_engine.h"
#include "commands.h"
#include "regamedll.h"
#include "rehlds.h"
#include "lua_sound.h"
#include "lua_message.h"
#include "cslua_corpse.h"
#include "cslua_netwatch.h"

#include <stdarg.h>
#include <stdio.h>

meta_globals_t *gpMetaGlobals;
gamedll_funcs_t *gpGamedllFuncs;
mutil_funcs_t *gpMetaUtilFuncs;

plugin_info_t Plugin_info =
{
	META_INTERFACE_VERSION,		// ifvers
	CSLUA_NAME,					// name
	CSLUA_VERSION,				// version
	__DATE__,					// date
	"kotyarakryt",				// author
	"https://github.com/KotyaraKryt/cs-lua",	// url
	"CSLUA",					// logtag
	PT_STARTUP,					// (when) loadable
	PT_NEVER,					// (when) unloadable - we hand function pointers
								// to the engine via AddServerCommand
};

static void cslua_vprint(bool is_error, const char *fmt, va_list ap)
{
	char msg[1024];
	int n = cslua_vsnprintf(msg, sizeof msg - 1, fmt, ap);
	// _vsnprintf (MSVC) returns -1 on overflow; vsnprintf (C99) returns the
	// wanted length. Clamp for both.
	if (n < 0 || n > (int)sizeof msg - 1)
		n = (int)sizeof msg - 1;
	msg[n] = '\0';

	// pfnServerPrint reaches the interactive console, an rcon caller and a game
	// panel's stdout alike; metamod's LOG_MESSAGE does not.
	char raw[1024 + 32];
	cslua_snprintf(raw, sizeof raw, "%s%s\n", CSLUA_TAG, msg);
	raw[sizeof raw - 1] = '\0';
	if (g_engfuncs.pfnServerPrint)
		g_engfuncs.pfnServerPrint(raw);

	// An error also goes to the metamod log, so a load-time failure leaves a
	// trace after the console has scrolled.
	if (is_error && gpMetaUtilFuncs)
		LOG_ERROR(PLID, "%s", msg);
}

static cvar_t s_cvar_profile = { "cslua_profile", "0", 0, 0.0f, nullptr };
static cvar_t *s_profile = nullptr;

void cslua_profile_init()
{
	CVAR_REGISTER(&s_cvar_profile);
	s_profile = CVAR_GET_POINTER("cslua_profile");
}

bool cslua_profiling()
{
	return s_profile && s_profile->value != 0.0f;
}

double cslua_now_seconds()
{
	return (double)clock() / (double)CLOCKS_PER_SEC;
}

void cslua_print(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	cslua_vprint(false, fmt, ap);
	va_end(ap);
}

void cslua_error(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	cslua_vprint(true, fmt, ap);
	va_end(ap);
}

static const std::string &game_dir()
{
	static std::string dir;
	if (dir.empty()) {
		const char *gamedir = GET_GAME_INFO(PLID, GINFO_GAMEDIR);
		dir = gamedir ? gamedir : ".";
	}
	return dir;
}

const std::string &cslua_base_dir()
{
	static std::string dir;
	if (dir.empty())
		dir = game_dir() + "/addons/lua";
	return dir;
}

const std::string &cslua_maps_dir()
{
	static std::string dir;
	if (dir.empty())
		dir = game_dir() + "/maps";
	return dir;
}

// pfnMessageBegin and its Write*/pfnMessageEnd siblings, so a script hiding a
// death's native ragdoll can have the message swallowed before any client sees
// it - see cslua_corpse.cpp.
C_DLLEXPORT int GetEngineFunctions(enginefuncs_t *pengfuncsFromEngine, int *interfaceVersion)
{
	if (!pengfuncsFromEngine) {
		ALERT(at_logged, "%s called with null pengfuncsFromEngine", __FUNCTION__);
		return FALSE;
	}
	if (*interfaceVersion != ENGINE_INTERFACE_VERSION) {
		ALERT(at_logged, "%s version mismatch; requested=%d ours=%d", __FUNCTION__, *interfaceVersion, ENGINE_INTERFACE_VERSION);
		*interfaceVersion = ENGINE_INTERFACE_VERSION;
		return FALSE;
	}

	memcpy(pengfuncsFromEngine, &g_CsluaCorpseEngineFuncs, sizeof(enginefuncs_t));
	return TRUE;
}

// pfnRegUserMsg only, so cslua_netwatch.cpp can learn a custom usermessage's
// name from the id the real engine just assigned it.
C_DLLEXPORT int GetEngineFunctions_Post(enginefuncs_t *pengfuncsFromEngine, int *interfaceVersion)
{
	if (!pengfuncsFromEngine) {
		ALERT(at_logged, "%s called with null pengfuncsFromEngine", __FUNCTION__);
		return FALSE;
	}
	if (*interfaceVersion != ENGINE_INTERFACE_VERSION) {
		ALERT(at_logged, "%s version mismatch; requested=%d ours=%d", __FUNCTION__, *interfaceVersion, ENGINE_INTERFACE_VERSION);
		*interfaceVersion = ENGINE_INTERFACE_VERSION;
		return FALSE;
	}

	memcpy(pengfuncsFromEngine, &g_CsluaNetwatchPostEngineFuncs, sizeof(enginefuncs_t));
	return TRUE;
}

META_FUNCTIONS gMetaFunctionTable =
{
	NULL,						// pfnGetEntityAPI
	NULL,						// pfnGetEntityAPI_Post
	GetEntityAPI2,				// pfnGetEntityAPI2
	GetEntityAPI2_Post,			// pfnGetEntityAPI2_Post
	GetNewDLLFunctions,			// pfnGetNewDLLFunctions
	GetNewDLLFunctions_Post,	// pfnGetNewDLLFunctions_Post
	GetEngineFunctions,			// pfnGetEngineFunctions
	GetEngineFunctions_Post,	// pfnGetEngineFunctions_Post
};

C_DLLEXPORT int Meta_Query(char *interfaceVersion, plugin_info_t **plinfo, mutil_funcs_t *pMetaUtilFuncs)
{
	*plinfo = &Plugin_info;
	gpMetaUtilFuncs = pMetaUtilFuncs;
	return TRUE;
}

C_DLLEXPORT int Meta_Attach(PLUG_LOADTIME now, META_FUNCTIONS *pFunctionTable, meta_globals_t *pMGlobals, gamedll_funcs_t *pGamedllFuncs)
{
	gpMetaGlobals = pMGlobals;
	gpGamedllFuncs = pGamedllFuncs;

	cslua_print("%s v%s loading, scripts in %s", CSLUA_NAME, CSLUA_VERSION, cslua_base_dir().c_str());

	cslua_message_init();
	cslua_profile_init();
	cslua_regamedll_init();

	// With ReHLDS we can watch every precache and stop plugins before the
	// 512-slot tables overflow.
	if (cslua_rehlds_init()) {
		cslua_sound_install_hooks();
		cslua_netwatch_install_hooks();
		cslua_rehlds_install_hooks();
	}

	// Fetch the runtime layer if a module was dropped in without scripts/.
	cslua_bootstrap_if_missing();

	cslua_register_commands();
	g_lua.init();

	memcpy(pFunctionTable, &gMetaFunctionTable, sizeof(META_FUNCTIONS));
	return TRUE;
}

C_DLLEXPORT int Meta_Detach(PLUG_LOADTIME now, PL_UNLOAD_REASON reason)
{
	g_lua.shutdown();

	// Only here, never on a reload: waiting for a worker costs nothing when the
	// process is on its way out.
	cslua_http_shutdown();
	cslua_httpserver_shutdown();
	cslua_mysql_shutdown();
	return TRUE;
}
