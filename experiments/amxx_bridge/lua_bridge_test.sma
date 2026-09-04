// PoC test plugin for the cs-lua <-> AmxModX bridge, branch
// experiment/amxx-native-bridge. Not a real cs-lua plugin - compile with
// amxxpc and drop into addons/amxmodx/plugins/ to verify amxx.call() reaches
// a public function and gets its return value back into Lua.
//
// Expected: amxx.call("Lua_BridgeTest", 2, 3) -> 5, true

#include <amxmodx>

public plugin_init()
{
    register_plugin("Lua Bridge Test", "0.1", "cs-lua")
}

public Lua_BridgeTest(a, b)
{
    return a + b
}
