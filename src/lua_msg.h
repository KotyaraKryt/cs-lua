#pragma once

#include <lua.hpp>

// Raw networked user messages: msg.send(dest, name, target, ...fields).
//
// Not a begin/write/end API on purpose: if a script could call back into Lua
// between MESSAGE_BEGIN and MESSAGE_END and error there, the message would be
// left half-written - a corrupt packet for every player. msg.send takes the
// whole message in one call, validates all of it, and only then runs the
// begin/write/end sequence with no Lua in the loop.
void cslua_register_msg(lua_State *L);

// Shared with cslua_corpse.cpp's receive-side hook (hook.add("msg:Name", ...)):
// names both the send-side {kind, value} wrappers and the receive-side decode.
enum FieldKind
{
	MSGF_BYTE = 1,
	MSGF_CHAR,
	MSGF_SHORT,
	MSGF_LONG,
	MSGF_ANGLE,
	MSGF_COORD,
	MSGF_STRING,
	MSGF_ENTITY,
};
