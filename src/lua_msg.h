#pragma once

#include <lua.hpp>

// Raw networked user messages: msg.send(dest, name, target, ...fields).
//
// Deliberately not a multi-call begin/write/end API. MESSAGE_BEGIN/
// MESSAGE_END bracket an open write into the engine's network stream; if a
// script got to call back into Lua between them and something errored there
// (nil access, bad argument, anything), the message would be left half
// written - not a plugin crash, a corrupt packet or a wedged send buffer for
// every player on the server. So msg.send takes the whole message - dest,
// message name, recipient/origin, and every field - in one call, validates
// all of it before MESSAGE_BEGIN ever runs, and only then does the
// begin/write/end sequence with no Lua in the loop. Nothing between
// MESSAGE_BEGIN and MESSAGE_END can fail.
void cslua_register_msg(lua_State *L);
