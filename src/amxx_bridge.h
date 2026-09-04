#pragma once

// The C ABI between lua_mm.dll and cslua_bridge_amxx.dll. Both are built
// from this tree and ship together; cslua_amxx_bridge_abi() guards against
// a stale pairing. Experimental, branch experiment/amxx-native-bridge.

enum CsluaAmxxArgType
{
	CSLUA_AMXX_INT = 0,		// plain cell, in
	CSLUA_AMXX_STR_IN = 1,	// const string, in
	CSLUA_AMXX_STR_OUT = 2,	// string the Pawn side writes into
};

struct CsluaAmxxArg
{
	int type;
	long ival;			// CSLUA_AMXX_INT
	const char *sval;	// CSLUA_AMXX_STR_IN
	char *out;			// CSLUA_AMXX_STR_OUT: where to put the result
	int outlen;			// CSLUA_AMXX_STR_OUT: size of `out`, result truncated to fit
};

// Argument shapes the bridge knows how to build a forward for. AMXX takes
// the parameter types at registration through a C variadic, so the list
// cannot be assembled at run time - each shape is one hand-written call.
//
//   only cells                          up to 4
//   cells then one out-string           up to 3 cells
//   one in-string then cells            up to 3 cells
//
// Anything else is rejected rather than guessed at.
#define CSLUA_AMXX_MAX_ARGS 4

typedef int (*CsluaAmxxBridgeAbiFn)();
typedef bool (*CsluaAmxxCallFn)(const char *public_name, const CsluaAmxxArg *args, int argc, long *out_result);
