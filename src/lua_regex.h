#pragma once

#include <lua.hpp>

// regex.match/find/replace - real regular expressions (ECMAScript/PCRE-ish
// syntax: character classes, quantifiers, alternation, groups), on top of
// the standard library's <regex> rather than a vendored engine. This project
// already requires C++14, so std::regex costs nothing to add: no new
// third-party dependency, no 32-bit build to wire up (see how much
// CMakeLists.txt already carries just for MariaDB's static link). Lua's own
// patterns cover simple cases; this is for the rest - validating a chat
// command's shape, pulling fields out of a log line, that kind of thing.
void cslua_register_regex(lua_State *L);
