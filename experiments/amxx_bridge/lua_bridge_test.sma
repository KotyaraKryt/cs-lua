// PoC test plugin for the cs-lua <-> AmxModX bridge, branch
// experiment/amxx-native-bridge. Not a real cs-lua plugin - compile with
// amxxpc and drop into addons/amxmodx/plugins/ to verify amxx.call()
// reaches a public function and gets its results back.
//
// Expected from Lua:
//   amxx.call("Lua_BridgeTest", 2, 3)              -> 5, true
//   amxx.call("Lua_BridgeName", 7, amxx.out())     -> 7, true, "level 7"
//   amxx.call("Lua_BridgeLen", "hello")            -> 5, true

#include <amxmodx>

// AMXX allots exactly STRINGEX_MAXLENGTH (128) cells for a forward's
// out-string, but a `name[]` parameter carries no size the compiler can
// see, so charsmax() on it is indeterminate - spell the size out.
#define BRIDGE_STRLEN 127

public plugin_init()
{
    register_plugin("Lua Bridge Test", "0.2", "cs-lua")
}

public Lua_BridgeTest(a, b)
{
    return a + b
}

// out-string: always the last argument, buffer is a real 128 cells
public Lua_BridgeName(level, name[])
{
    formatex(name, BRIDGE_STRLEN, "level %d", level)
    return level
}

// in-string: always the first argument
public Lua_BridgeLen(const text[])
{
    return strlen(text)
}
