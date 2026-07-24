/*
 * What leaves the module, and nothing else.
 *
 * Without this the whole of LuaJIT ends up in the dynamic symbol table -
 * lua_pushstring, lua_pcall, some 450 more. Metamod plugins all live in one
 * process, so a second Lua-based plugin would find our symbols before its own
 * and start driving our lua_State. That crashes, and it crashes far away from
 * the cause.
 *
 * -fvisibility=hidden does not cover this: it applies to our translation
 * units, not to the static library linked into them.
 *
 * The cost is that binary Lua modules (package.loadlib) cannot bind against
 * us. Plugins wanting C should use the FFI, which does not need exports.
 */
CSLUA_1.0 {
	global:
		GiveFnptrsToDll;

		Meta_Query;
		Meta_Attach;
		Meta_Detach;

		GetEntityAPI2;
		GetEntityAPI2_Post;
		GetNewDLLFunctions;
		GetNewDLLFunctions_Post;

	local:
		*;
};
