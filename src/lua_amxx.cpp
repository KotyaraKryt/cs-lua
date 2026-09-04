#include "cslua.h"
#include "lua_amxx.h"
#include "lua_natives.h"
#include "platform.h"
#include "amxx_bridge.h"

#include <string.h>

// The Lua half of the AMXX bridge. The other half is a separate module
// amxmodx loads (src/amxx_bridge.cpp) - not linked against this target,
// found in-process by name instead, the same trick regamedll.cpp uses for
// ReGameDLL. Experimental, branch experiment/amxx-native-bridge.

// "cslua_bridge", not "lua_amxx_bridge": a modules.ini entry containing
// "_amxx" anywhere but the suffix amxmodx appends itself hung amxmodx
// 1.9.0.5249 on startup, spinning before it ever mapped the file.
#ifdef _WIN32
static const char *const kBridgeModuleName = "cslua_bridge_amxx.dll";
#else
static const char *const kBridgeModuleName = "cslua_bridge_amxx_i386.so";
#endif

static const int kExpectedAbi = 2;

// AMXX hands a forward's out-string back through a 128 byte buffer; asking
// for more than that would be a lie.
static const int kOutMax = 128;

static bool s_looked_up = false;
static CsluaAmxxCallFn s_bridge_call = NULL;

// Looked up once and cached: the bridge, once loaded by amxmodx, stays
// mapped for the life of the process (a mapchange reruns its
// AMXX_Attach/Detach, not the module load), so the pointer stays good.
static bool ensure_bridge()
{
	if (s_looked_up)
		return s_bridge_call != NULL;

	s_looked_up = true;

	void *handle = cslua_module_open(kBridgeModuleName);
	if (!handle)
		return false;

	CsluaAmxxBridgeAbiFn abi = (CsluaAmxxBridgeAbiFn)cslua_module_symbol(handle, "cslua_amxx_bridge_abi");
	CsluaAmxxCallFn call = (CsluaAmxxCallFn)cslua_module_symbol(handle, "cslua_amxx_call");
	cslua_module_close(handle);

	if (!abi || !call || abi() != kExpectedAbi)
		return false;

	s_bridge_call = call;
	return true;
}

// amxx.out() - marks the slot a `public Foo(..., out[])` writes into. A
// plain table with one field; nothing else in Lua looks like it.
static int l_amxx_out(lua_State *L)
{
	lua_newtable(L);
	lua_pushboolean(L, 1);
	lua_setfield(L, -2, "__amxx_out");
	return 1;
}

static bool is_out_marker(lua_State *L, int idx)
{
	if (!lua_istable(L, idx))
		return false;

	lua_getfield(L, idx, "__amxx_out");
	bool marker = lua_toboolean(L, -1) != 0;
	lua_pop(L, 1);
	return marker;
}

static int fail(lua_State *L, const char *msg)
{
	lua_pushnil(L);
	lua_pushboolean(L, 0);
	lua_pushstring(L, msg);
	return 3;
}

// amxx.call("SomePublic", 2, 3)              -> 5, true
// amxx.call("RankName", level, amxx.out())   -> ret, true, "Рядовой"
// On failure: nil, false, "reason".
//
// Argument shapes the bridge can build a forward for: cells only (up to
// four), cells then one amxx.out() last, or one string first then cells.
static int l_amxx_call(lua_State *L)
{
	const char *name = luaL_checkstring(L, 1);
	int argc = lua_gettop(L) - 1;
	if (argc < 0)
		argc = 0;
	if (argc > CSLUA_AMXX_MAX_ARGS)
		return luaL_error(L, "amxx.call: %d arguments, the bridge takes at most %d",
			argc, CSLUA_AMXX_MAX_ARGS);

	if (!ensure_bridge())
		return fail(L, "AMXX bridge module not loaded - check modules.ini (cslua_bridge)");

	CsluaAmxxArg args[CSLUA_AMXX_MAX_ARGS];
	char out[kOutMax];
	memset(args, 0, sizeof args);
	bool has_out = false;

	for (int i = 0; i < argc; i++) {
		int slot = i + 2;

		if (is_out_marker(L, slot)) {
			if (has_out)
				return luaL_error(L, "amxx.call: only one amxx.out() per call");
			has_out = true;
			out[0] = '\0';
			args[i].type = CSLUA_AMXX_STR_OUT;
			args[i].out = out;
			args[i].outlen = (int)sizeof out;
		} else if (lua_type(L, slot) == LUA_TSTRING) {
			args[i].type = CSLUA_AMXX_STR_IN;
			args[i].sval = lua_tostring(L, slot);
		} else if (lua_isnumber(L, slot)) {
			args[i].type = CSLUA_AMXX_INT;
			args[i].ival = (long)lua_tointeger(L, slot);
		} else {
			return luaL_error(L, "amxx.call: argument %d is %s, expected a number, a string or amxx.out()",
				i + 1, luaL_typename(L, slot));
		}
	}

	long result = 0;
	if (!s_bridge_call(name, args, argc, &result))
		return fail(L, lua_pushfstring(L,
			"amxx.call: no loaded plugin has public '%s' with this signature, "
			"or the argument shape is not one the bridge can build", name));

	lua_pushinteger(L, (lua_Integer)result);
	lua_pushboolean(L, 1);
	if (has_out) {
		lua_pushstring(L, out);
		return 3;
	}
	return 2;
}

void cslua_register_amxx(lua_State *L)
{
	static const luaL_Reg s_api[] =
	{
		{ "call", l_amxx_call },
		{ "out",  l_amxx_out },
		{ NULL, NULL }
	};

	cslua_register_namespace(L, "amxx", s_api);
}
