// Experimental AMXX module: lets cs-lua's Lua layer call a `public`
// function of any loaded AmxModX plugin by name, through AMXX's forward
// mechanism - the only supported way into Pawn from outside. PoC only,
// branch experiment/amxx-native-bridge, never merged to main: int
// arguments and an int return, at most 4 arguments.
//
// amxmodx loads this via modules.ini, not metamod, hence a second binary
// next to lua_mm.dll - separate loader, separate exports. lua_mm.dll finds
// it in-process by name (see src/lua_amxx.cpp).
//
// Two things cost a whole debugging session, both worth knowing:
//
//   - The module must not have "_amxx" anywhere in its name except the
//     suffix amxmodx appends itself. A modules.ini entry of
//     "lua_amxx_bridge" hung AMXX 1.9.0.5249 solid at startup, spinning
//     before it ever mapped the file. Hence "cslua_bridge".
//
//   - RegisterForward, not RegisterSPForwardByName. The "SP" one targets
//     one specific plugin and wants that plugin's AMX* first; passing NULL
//     to mean "any plugin" crashes the server. RegisterForward matches the
//     public by name across every loaded plugin, which is what this wants.
//
// amxxmodule.cpp is deliberately not compiled in - see CMakeLists.txt.
// HAVE_STDINT_H is set target-wide there too: modern MSVC ships a real
// <stdint.h>, and without it amxxmodule.h's own pre-C99 int32_t/uint32_t
// typedefs (for compilers that lack one) collide with it.
#include "amxxmodule.h"

#include <string>
#include <unordered_map>

static const int kMaxArgs = 4;

// Keyed by "name#argc". A forward registration is looked up across every
// loaded plugin and, for a multi-plugin forward, cannot be handed back at
// all - so register lazily, once per distinct (name, argc), and reuse.
static std::unordered_map<std::string, int> g_forward_ids;

// Requested by hand in AMXX_Attach rather than through amxxmodule.cpp's
// MF_* wrappers. ET_STOP2 is the exec type that returns the plugin's own
// return value instead of discarding it.
static PFN_REGISTER_FORWARD g_RegisterForward;
static PFN_EXECUTE_FORWARD g_ExecuteForward;

static int register_forward(const char *name, int argc)
{
	switch (argc) {
		case 0: return g_RegisterForward(name, ET_STOP2, FP_DONE);
		case 1: return g_RegisterForward(name, ET_STOP2, FP_CELL, FP_DONE);
		case 2: return g_RegisterForward(name, ET_STOP2, FP_CELL, FP_CELL, FP_DONE);
		case 3: return g_RegisterForward(name, ET_STOP2, FP_CELL, FP_CELL, FP_CELL, FP_DONE);
		case 4: return g_RegisterForward(name, ET_STOP2, FP_CELL, FP_CELL, FP_CELL, FP_CELL, FP_DONE);
		default: return -1;
	}
}

static int execute_forward(int id, const long *args, int argc)
{
	switch (argc) {
		case 0: return g_ExecuteForward(id);
		case 1: return g_ExecuteForward(id, (cell)args[0]);
		case 2: return g_ExecuteForward(id, (cell)args[0], (cell)args[1]);
		case 3: return g_ExecuteForward(id, (cell)args[0], (cell)args[1], (cell)args[2]);
		case 4: return g_ExecuteForward(id, (cell)args[0], (cell)args[1], (cell)args[2], (cell)args[3]);
		default: return 0;
	}
}

// ABI sanity check for the lua_mm.dll side, which finds this module by
// GetProcAddress/dlsym rather than linking against it - bump whenever
// cslua_amxx_call_int's signature changes.
extern "C" DLLEXPORT int cslua_amxx_bridge_abi()
{
	return 1;
}

extern "C" DLLEXPORT bool cslua_amxx_call_int(const char *public_name, const long *args, int argc, long *out_result)
{
	if (!public_name || argc < 0 || argc > kMaxArgs || !g_RegisterForward)
		return false;

	std::string key = std::string(public_name) + "#" + std::to_string(argc);
	auto it = g_forward_ids.find(key);
	int id;
	if (it != g_forward_ids.end()) {
		id = it->second;
	} else {
		id = register_forward(public_name, argc);
		if (id == -1)
			return false;	// no loaded plugin has a public of that name/arity
		g_forward_ids[key] = id;
	}

	int result = execute_forward(id, args, argc);
	if (out_result)
		*out_result = result;
	return true;
}

static amxx_module_info_s g_module_info =
{
	MODULE_NAME,
	MODULE_AUTHOR,
	MODULE_VERSION,
	1,				// reload on mapchange
	MODULE_LOGTAG,
	MODULE_LIBRARY,
	MODULE_LIBCLASS,
};

C_DLLEXPORT int AMXX_Query(int *interfaceVersion, amxx_module_info_s *moduleInfo)
{
	if (!interfaceVersion || !moduleInfo)
		return AMXX_PARAM;

	if (*interfaceVersion != AMXX_INTERFACE_VERSION) {
		*interfaceVersion = AMXX_INTERFACE_VERSION;
		return AMXX_IFVERS;
	}

	*moduleInfo = g_module_info;
	return AMXX_OK;
}

C_DLLEXPORT int AMXX_CheckGame(const char *game)
{
	return AMXX_GAME_OK;
}

C_DLLEXPORT int AMXX_Attach(PFN_REQ_FNPTR reqFnptrFunc)
{
	if (!reqFnptrFunc)
		return AMXX_PARAM;

	g_RegisterForward = (PFN_REGISTER_FORWARD)reqFnptrFunc("RegisterForward");
	if (!g_RegisterForward)
		return AMXX_FUNC_NOT_PRESENT;

	g_ExecuteForward = (PFN_EXECUTE_FORWARD)reqFnptrFunc("ExecuteForward");
	if (!g_ExecuteForward)
		return AMXX_FUNC_NOT_PRESENT;

	return AMXX_OK;
}

C_DLLEXPORT int AMXX_Detach()
{
	// Multi-plugin forwards have no unregister call in the module API (only
	// single-plugin ones do), so there is nothing to hand back - just drop
	// the ids so a reload re-registers against the new plugin set.
	g_forward_ids.clear();
	return AMXX_OK;
}
