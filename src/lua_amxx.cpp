#include "cslua.h"
#include "lua_amxx.h"
#include "lua_natives.h"
#include "platform.h"

// AMXX_call surface into src/amxx_bridge.cpp - a separate module amxmodx
// loads (not metamod, not linked against this target). Found in-process by
// name via cslua_module_open/symbol, same trick regamedll.cpp uses for
// ReGameDLL. Experimental, branch experiment/amxx-native-bridge only.

// "cslua_bridge", not "lua_amxx_bridge": a modules.ini entry containing
// "_amxx" anywhere but the suffix amxmodx appends itself hung amxmodx
// 1.9.0.5249 on startup, spinning before it ever mapped the file.
#ifdef _WIN32
static const char *const kBridgeModuleName = "cslua_bridge_amxx.dll";
#else
static const char *const kBridgeModuleName = "cslua_bridge_amxx_i386.so";
#endif

typedef int (*BridgeAbiFn)();
typedef bool (*BridgeCallIntFn)(const char *public_name, const long *args, int argc, long *out_result);

static const int kMaxArgs = 4;
static const int kExpectedAbi = 1;

static bool s_looked_up = false;
static BridgeCallIntFn s_bridge_call_int = nullptr;

// Looked up once and cached: the bridge, once loaded by amxmodx, stays
// mapped for the life of the process (MODULE_RELOAD_ON_MAPCHANGE reruns its
// AMXX_Attach/Detach, not the module load itself), so the pointer stays good.
static bool ensure_bridge()
{
	if (s_looked_up)
		return s_bridge_call_int != nullptr;

	s_looked_up = true;

	void *handle = cslua_module_open(kBridgeModuleName);
	if (!handle)
		return false;

	BridgeAbiFn abi = (BridgeAbiFn)cslua_module_symbol(handle, "cslua_amxx_bridge_abi");
	BridgeCallIntFn call_int = (BridgeCallIntFn)cslua_module_symbol(handle, "cslua_amxx_call_int");
	cslua_module_close(handle);

	if (!abi || !call_int || abi() != kExpectedAbi)
		return false;

	s_bridge_call_int = call_int;
	return true;
}

// amxx.call("Lua_BridgeTest", 2, 3) -> 5, true
// On failure: nil, false, "reason".
static int l_amxx_call(lua_State *L)
{
	const char *name = luaL_checkstring(L, 1);
	int argc = lua_gettop(L) - 1;
	if (argc < 0)
		argc = 0;
	if (argc > kMaxArgs)
		return luaL_error(L, "amxx.call: too many arguments (%d), the bridge PoC caps at %d", argc, kMaxArgs);

	if (!ensure_bridge()) {
		lua_pushnil(L);
		lua_pushboolean(L, 0);
		lua_pushstring(L, "AMXX bridge module not loaded - check modules.ini (cslua_bridge)");
		return 3;
	}

	long args[kMaxArgs];
	for (int i = 0; i < argc; i++)
		args[i] = (long)luaL_checkinteger(L, i + 2);

	long result = 0;
	if (!s_bridge_call_int(name, args, argc, &result)) {
		lua_pushnil(L);
		lua_pushboolean(L, 0);
		lua_pushfstring(L, "amxx.call: no loaded plugin has public '%s' with %d argument(s)", name, argc);
		return 3;
	}

	lua_pushinteger(L, (lua_Integer)result);
	lua_pushboolean(L, 1);
	return 2;
}

void cslua_register_amxx(lua_State *L)
{
	static const luaL_Reg s_api[] =
	{
		{ "call", l_amxx_call },
		{ NULL, NULL }
	};

	cslua_register_namespace(L, "amxx", s_api);
}
