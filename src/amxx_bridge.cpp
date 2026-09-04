// Experimental AMXX module: lets cs-lua's Lua layer call a `public`
// function of any loaded AmxModX plugin by name, through AMXX's forward
// mechanism - the only supported way into Pawn from outside. PoC only,
// branch experiment/amxx-native-bridge, never merged to main.
//
// amxmodx loads this via modules.ini, not metamod, hence a second binary
// next to lua_mm.dll - separate loader, separate exports. lua_mm.dll finds
// it in-process by name (see src/lua_amxx.cpp); amxx_bridge.h is the ABI
// the two share.
//
// Three things cost a whole debugging session, all worth knowing:
//
//   - The module must not have "_amxx" anywhere in its name except the
//     suffix amxmodx appends itself. A modules.ini entry of
//     "lua_amxx_bridge" hung AMXX 1.9.0.5249 solid at startup, spinning
//     before it ever mapped the file. Hence "cslua_bridge".
//
//   - RegisterForward, not RegisterSPForwardByName. The "SP" one targets
//     one specific plugin and wants that plugin's AMX* first; passing NULL
//     to mean "any plugin" crashes the server.
//
//   - FP_STRINGEX copies the Pawn string back with no length limit at all
//     (CForward.cpp: amx_GetStringOld straight into the pointer we passed),
//     and the Pawn side always gets a buffer of STRINGEX_MAXLENGTH = 128
//     regardless of what we pass in. So the buffer handed to a forward has
//     to be a full 128 bytes or Pawn writes past it - hence the scratch
//     buffer below rather than writing into the caller's.
//
// amxxmodule.cpp is deliberately not compiled in - see CMakeLists.txt.
// HAVE_STDINT_H is set target-wide there too: modern MSVC ships a real
// <stdint.h>, and without it amxxmodule.h's own pre-C99 int32_t/uint32_t
// typedefs (for compilers that lack one) collide with it.
#include "amxxmodule.h"
#include "amxx_bridge.h"

#include <stdio.h>
#include <string.h>

#include <string>
#include <unordered_map>

// What AMXX allots for an FP_STRINGEX parameter (CForward.cpp). Not in the
// module SDK, so it is repeated here; a forward's out-string buffer must be
// this big.
static const int kStringExMax = 128;
static char s_str_scratch[kStringExMax];

// Keyed by "name/shape". A forward registration is looked up across every
// loaded plugin and, being a multi-plugin forward, cannot be handed back at
// all - so register lazily, once per distinct signature, and reuse.
static std::unordered_map<std::string, int> g_forward_ids;

// Requested by hand in AMXX_Attach rather than through amxxmodule.cpp's
// MF_* wrappers. ET_STOP2 is the exec type that returns the plugin's own
// return value instead of discarding it.
static PFN_REGISTER_FORWARD g_RegisterForward;
static PFN_EXECUTE_FORWARD g_ExecuteForward;
static PFN_GET_AMXSCRIPT g_GetAmxScript;
static PFN_AMX_FINDPUBLIC g_AmxFindPublic;

// RegisterForward takes a name and succeeds whether or not anybody
// implements it - a multi-plugin forward with no implementers is perfectly
// legal, it just calls nothing and returns 0. That would turn a typo in a
// public's name into a silent zero, so check the loaded plugins first.
// Not cached: the answer changes as plugins load and unload.
static bool any_plugin_has_public(const char *name)
{
	for (int i = 0; ; i++) {
		AMX *amx = g_GetAmxScript(i);
		if (!amx)
			return false;

		int index;
		if (g_AmxFindPublic(amx, name, &index) == AMX_ERR_NONE)
			return true;
	}
}

enum Shape
{
	SHAPE_CELLS,		// (cell ...)
	SHAPE_CELLS_OUT,	// (cell ..., out[])
	SHAPE_IN_CELLS,		// (const in[], cell ...)
	SHAPE_BAD,
};

// Sorts an argument list into one of the shapes above, and reports how many
// plain cells it carries.
static Shape classify(const CsluaAmxxArg *args, int argc, int *ncells)
{
	int cells = 0, str_in = 0, str_out = 0;

	for (int i = 0; i < argc; i++) {
		switch (args[i].type) {
			case CSLUA_AMXX_INT:
				cells++;
				break;
			case CSLUA_AMXX_STR_IN:
				if (i != 0 || !args[i].sval)
					return SHAPE_BAD;	// only as the first argument
				str_in++;
				break;
			case CSLUA_AMXX_STR_OUT:
				if (i != argc - 1 || !args[i].out || args[i].outlen <= 0)
					return SHAPE_BAD;	// only as the last argument
				str_out++;
				break;
			default:
				return SHAPE_BAD;
		}
	}

	*ncells = cells;

	if (str_in && str_out)
		return SHAPE_BAD;
	if (str_out)
		return cells <= 3 ? SHAPE_CELLS_OUT : SHAPE_BAD;
	if (str_in)
		return cells <= 3 ? SHAPE_IN_CELLS : SHAPE_BAD;
	return cells <= CSLUA_AMXX_MAX_ARGS ? SHAPE_CELLS : SHAPE_BAD;
}

static int register_forward(const char *name, Shape shape, int ncells)
{
	switch (shape) {
		case SHAPE_CELLS:
			switch (ncells) {
				case 0: return g_RegisterForward(name, ET_STOP2, FP_DONE);
				case 1: return g_RegisterForward(name, ET_STOP2, FP_CELL, FP_DONE);
				case 2: return g_RegisterForward(name, ET_STOP2, FP_CELL, FP_CELL, FP_DONE);
				case 3: return g_RegisterForward(name, ET_STOP2, FP_CELL, FP_CELL, FP_CELL, FP_DONE);
				case 4: return g_RegisterForward(name, ET_STOP2, FP_CELL, FP_CELL, FP_CELL, FP_CELL, FP_DONE);
			}
			break;

		case SHAPE_CELLS_OUT:
			switch (ncells) {
				case 0: return g_RegisterForward(name, ET_STOP2, FP_STRINGEX, FP_DONE);
				case 1: return g_RegisterForward(name, ET_STOP2, FP_CELL, FP_STRINGEX, FP_DONE);
				case 2: return g_RegisterForward(name, ET_STOP2, FP_CELL, FP_CELL, FP_STRINGEX, FP_DONE);
				case 3: return g_RegisterForward(name, ET_STOP2, FP_CELL, FP_CELL, FP_CELL, FP_STRINGEX, FP_DONE);
			}
			break;

		case SHAPE_IN_CELLS:
			switch (ncells) {
				case 0: return g_RegisterForward(name, ET_STOP2, FP_STRING, FP_DONE);
				case 1: return g_RegisterForward(name, ET_STOP2, FP_STRING, FP_CELL, FP_DONE);
				case 2: return g_RegisterForward(name, ET_STOP2, FP_STRING, FP_CELL, FP_CELL, FP_DONE);
				case 3: return g_RegisterForward(name, ET_STOP2, FP_STRING, FP_CELL, FP_CELL, FP_CELL, FP_DONE);
			}
			break;

		default:
			break;
	}

	return -1;
}

// Every non-float forward argument is read back as a cell (CForward.cpp:
// `params[i] = (cell)va_arg(argptr, cell)`), strings included - they travel
// as the pointer value itself.
static int execute_forward(int id, const CsluaAmxxArg *args, int argc, Shape shape, int ncells)
{
	const cell *c = NULL;
	cell cells[CSLUA_AMXX_MAX_ARGS] = { 0, 0, 0, 0 };
	int n = 0;

	for (int i = 0; i < argc; i++)
		if (args[i].type == CSLUA_AMXX_INT && n < CSLUA_AMXX_MAX_ARGS)
			cells[n++] = (cell)args[i].ival;
	c = cells;

	switch (shape) {
		case SHAPE_CELLS:
			switch (ncells) {
				case 0: return g_ExecuteForward(id);
				case 1: return g_ExecuteForward(id, c[0]);
				case 2: return g_ExecuteForward(id, c[0], c[1]);
				case 3: return g_ExecuteForward(id, c[0], c[1], c[2]);
				case 4: return g_ExecuteForward(id, c[0], c[1], c[2], c[3]);
			}
			break;

		case SHAPE_CELLS_OUT: {
			cell buf = (cell)(void *)s_str_scratch;
			switch (ncells) {
				case 0: return g_ExecuteForward(id, buf);
				case 1: return g_ExecuteForward(id, c[0], buf);
				case 2: return g_ExecuteForward(id, c[0], c[1], buf);
				case 3: return g_ExecuteForward(id, c[0], c[1], c[2], buf);
			}
			break;
		}

		case SHAPE_IN_CELLS: {
			cell in = (cell)(void *)args[0].sval;
			switch (ncells) {
				case 0: return g_ExecuteForward(id, in);
				case 1: return g_ExecuteForward(id, in, c[0]);
				case 2: return g_ExecuteForward(id, in, c[0], c[1]);
				case 3: return g_ExecuteForward(id, in, c[0], c[1], c[2]);
			}
			break;
		}

		default:
			break;
	}

	return 0;
}

// ABI sanity check for the lua_mm.dll side, which finds this module by
// GetProcAddress/dlsym rather than linking against it - bump whenever
// cslua_amxx_call's signature or amxx_bridge.h changes.
extern "C" DLLEXPORT int cslua_amxx_bridge_abi()
{
	return 2;
}

extern "C" DLLEXPORT bool cslua_amxx_call(const char *public_name, const CsluaAmxxArg *args, int argc, long *out_result)
{
	if (!public_name || argc < 0 || argc > CSLUA_AMXX_MAX_ARGS || !g_RegisterForward)
		return false;

	int ncells = 0;
	Shape shape = classify(args, argc, &ncells);
	if (shape == SHAPE_BAD)
		return false;

	// The shape is part of the key: the same public called with a different
	// signature is a different forward as far as AMXX is concerned.
	char sig[32];
	snprintf(sig, sizeof sig, "/%d.%d", (int)shape, ncells);
	std::string key = std::string(public_name) + sig;

	if (!any_plugin_has_public(public_name))
		return false;

	auto it = g_forward_ids.find(key);
	int id;
	if (it != g_forward_ids.end()) {
		id = it->second;
	} else {
		id = register_forward(public_name, shape, ncells);
		if (id == -1)
			return false;
		g_forward_ids[key] = id;
	}

	if (shape == SHAPE_CELLS_OUT)
		s_str_scratch[0] = '\0';

	int result = execute_forward(id, args, argc, shape, ncells);

	if (shape == SHAPE_CELLS_OUT) {
		s_str_scratch[kStringExMax - 1] = '\0';
		const CsluaAmxxArg &o = args[argc - 1];
		strncpy(o.out, s_str_scratch, o.outlen - 1);
		o.out[o.outlen - 1] = '\0';
	}

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

	g_GetAmxScript = (PFN_GET_AMXSCRIPT)reqFnptrFunc("GetAmxScript");
	if (!g_GetAmxScript)
		return AMXX_FUNC_NOT_PRESENT;

	g_AmxFindPublic = (PFN_AMX_FINDPUBLIC)reqFnptrFunc("amx_FindPublic");
	if (!g_AmxFindPublic)
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
