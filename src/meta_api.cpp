#include "cslua.h"
#include "cslua_bootstrap.h"
#include "lua_http.h"
#include "lua_httpserver.h"

#include <time.h>
#include "lua_engine.h"
#include "commands.h"
#include "regamedll.h"
#include "rehlds.h"
#include "lua_sound.h"
#include "lua_message.h"
#include "cslua_corpse.h"

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
	PT_NEVER,					// (when) unloadable - we hand out function pointers
								// to the engine via AddServerCommand, so unloading
								// the DLL would leave them dangling.
};

static void cslua_vprint(bool is_error, const char *fmt, va_list ap)
{
	char msg[1024];
	int n = cslua_vsnprintf(msg, sizeof msg - 1, fmt, ap);
	// _vsnprintf (MSVC) returns -1 on overflow; vsnprintf (C99) returns the
	// length it WANTED, which can be past the buffer. Clamp for both.
	if (n < 0 || n > (int)sizeof msg - 1)
		n = (int)sizeof msg - 1;
	msg[n] = '\0';

	// Straight to the server console. Con_Printf is what a command reply has to
	// go through: it reaches the interactive console, an rcon caller (it honours
	// the rcon redirect), and a game panel's stdout alike. metamod's LOG_MESSAGE
	// does not - it lands in the metamod log file and nowhere a person watching
	// a console would see it, which reads as "the command printed nothing".
	char raw[1024 + 32];
	cslua_snprintf(raw, sizeof raw, "%s%s\n", CSLUA_TAG, msg);
	raw[sizeof raw - 1] = '\0';
	if (g_engfuncs.pfnServerPrint)
		g_engfuncs.pfnServerPrint(raw);

	// An error also goes to the metamod log, so a plugin blowing up at load
	// leaves a trace after the console has scrolled away.
	if (is_error && gpMetaUtilFuncs)
		LOG_ERROR(PLID, "%s", msg);
}

// Profiling is off by default: the clock read per handler is cheap but not
// free, and a server that is not being investigated should not pay for it.
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

const std::string &cslua_base_dir()
{
	static std::string dir;
	if (dir.empty()) {
		const char *gamedir = GET_GAME_INFO(PLID, GINFO_GAMEDIR);
		dir = gamedir ? gamedir : ".";
		dir += "/addons/lua";
	}
	return dir;
}

// pfnMessageBegin and its Write*/pfnMessageEnd siblings, so a script asking
// to hide a death's native ragdoll (hook.add("player_corpse", ...) +
// e:cancel()) can have that message swallowed before any client sees it -
// see cslua_corpse.cpp for why blocking has to happen at this level and not
// after the fact.
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

META_FUNCTIONS gMetaFunctionTable =
{
	NULL,						// pfnGetEntityAPI			HL SDK; called before game DLL
	NULL,						// pfnGetEntityAPI_Post		META; called after game DLL
	GetEntityAPI2,				// pfnGetEntityAPI2			HL SDK2; called before game DLL
	GetEntityAPI2_Post,			// pfnGetEntityAPI2_Post	META; called after game DLL
	GetNewDLLFunctions,			// pfnGetNewDLLFunctions	HL SDK2; called before game DLL
	GetNewDLLFunctions_Post,	// pfnGetNewDLLFunctions_Post	META; called after game DLL
	GetEngineFunctions,			// pfnGetEngineFunctions	META; called before HL engine
	NULL,						// pfnGetEngineFunctions_Post	META; called after HL engine
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

	// Engine-level accounting: with ReHLDS we can watch every precache on the
	// server and stop plugins before the 512-slot tables overflow.
	if (cslua_rehlds_init())
		cslua_sound_install_hooks();

	// Before anything reads addons/lua/core: a module dropped in on its own,
	// with no scripts/ copied over by hand, gets the runtime layer fetched
	// here instead of failing to find a single core function.
	cslua_bootstrap_if_missing();

	cslua_register_commands();
	g_lua.init();

	memcpy(pFunctionTable, &gMetaFunctionTable, sizeof(META_FUNCTIONS));
	return TRUE;
}

C_DLLEXPORT int Meta_Detach(PLUG_LOADTIME now, PL_UNLOAD_REASON reason)
{
	g_lua.shutdown();

	// Only here, never on a reload: this is the one place where waiting for a
	// worker costs nothing, because the process is on its way out.
	cslua_http_shutdown();
	cslua_httpserver_shutdown();
	return TRUE;
}
