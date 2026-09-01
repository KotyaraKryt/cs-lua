#pragma once

#include <lua.hpp>

// regex.match/find/replace - real regular expressions (ECMAScript syntax) on
// top of the standard library's <regex>. Lua's own patterns cover simple
// cases; this is for the rest - validating a chat command's shape, pulling
// fields out of a log line.
void cslua_register_regex(lua_State *L);
