#include "cslua.h"
#include "lua_engine.h"
#include "commands.h"
#include "regamedll.h"
#include "rehlds.h"
#include "lua_sound.h"
#include "lua_message.h"

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

META_FUNCTIONS gMetaFunctionTable =
{
	NULL,						// pfnGetEntityAPI			HL SDK; called before game DLL
	NULL,						// pfnGetEntityAPI_Post		META; called after game DLL
	GetEntityAPI2,				// pfnGetEntityAPI2			HL SDK2; called before game DLL
	GetEntityAPI2_Post,			// pfnGetEntityAPI2_Post	META; called after game DLL
	GetNewDLLFunctions,			// pfnGetNewDLLFunctions	HL SDK2; called before game DLL
	GetNewDLLFunctions_Post,	// pfnGetNewDLLFunctions_Post	META; called after game DLL
	NULL,						// pfnGetEngineFunctions	META; called before HL engine
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
	cslua_regamedll_init();

	// Engine-level accounting: with ReHLDS we can watch every precache on the
	// server and stop plugins before the 512-slot tables overflow.
	if (cslua_rehlds_init())
		cslua_sound_install_hooks();

	cslua_register_commands();
	g_lua.init();

	memcpy(pFunctionTable, &gMetaFunctionTable, sizeof(META_FUNCTIONS));
	return TRUE;
}

C_DLLEXPORT int Meta_Detach(PLUG_LOADTIME now, PL_UNLOAD_REASON reason)
{
	g_lua.shutdown();
	return TRUE;
}
