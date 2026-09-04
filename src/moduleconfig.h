// Own copy of amxmodx's public/sdk/moduleconfig.in.h, filled in for this
// module - see third_party/amxmodx-sdk/README.md for why this lives here
// and not in the vendored SDK.
#ifndef __MODULECONFIG_H__
#define __MODULECONFIG_H__

#define MODULE_NAME "cslua_bridge"
#define MODULE_VERSION "0.1-experiment"
#define MODULE_AUTHOR "kotyarakryt"
#define MODULE_URL "https://github.com/KotyaraKryt/cs-lua"
#define MODULE_LOGTAG "LUABR"
#define MODULE_LIBRARY "cslua_bridge"
#define MODULE_LIBCLASS ""
#define MODULE_RELOAD_ON_MAPCHANGE

#ifdef __DATE__
#define MODULE_DATE __DATE__
#else
#define MODULE_DATE "Unknown"
#endif

// Not a metamod plugin - lua_mm.dll already is one (see meta_api.cpp).
// #define USE_METAMOD

// No FN_AMXX_* hooks: src/amxx_bridge.cpp implements AMXX_Query/CheckGame/
// Attach/Detach directly rather than going through amxxmodule.cpp's
// dispatcher - see the comment at the top of that file for why.

#endif // __MODULECONFIG_H__
