#pragma once

#include <extdll.h>

// Post-hook table (see meta_api.cpp's GetEngineFunctions_Post): the only
// entry that matters is pfnRegUserMsg, so this module can learn a custom
// usermessage's real name instead of just logging its numeric id. Has to be
// a *post* hook - the assigned id doesn't exist until the real engine
// function has run, unlike the pre-hook table in cslua_corpse.cpp.
extern enginefuncs_t g_CsluaNetwatchPostEngineFuncs;

// Explains "Overflow" (and any other) client drop after the fact: which
// message types put how many bytes on a client's reliable queue in the
// seconds before the engine gave up on them.
//
// cslua_corpse.cpp's enginefuncs_t hook is the only pfnMessageBegin/Write*
// interception point this module has (metamod only lets one plugin occupy
// each pre-hook slot at a time via GetEngineFunctions), so this module does
// not install its own copy of that table - cslua_corpse.cpp's Hook_* bodies
// call into cslua_netwatch_message_begin/add_bytes/message_end as they run.
// That also means every reliable message reaching a client is counted here
// regardless of who wrote it - our own scripts, amxmodx, or the game DLL -
// since they all end up going through the same engine entry points.

// Registers the ReHLDS SV_DropClient hookchain, where the engine's own drop
// reason text is available. No-op without ReHLDS (same fallback as
// cslua_sound_install_hooks). Called once at startup, after cslua_rehlds_init.
void cslua_netwatch_install_hooks();

void cslua_netwatch_message_begin(edict_t *dest, int msg_type);
void cslua_netwatch_add_bytes(int bytes);
void cslua_netwatch_message_end();

// Resolves a msg_type to its name - a builtin svc_* opcode, a custom
// usermessage learned from pfnRegUserMsg, or "usermsg#<n>" as a fallback for
// one this module has not seen registered yet. `buf` only gets written for
// that fallback case; the builtin/custom cases return a pointer into this
// module's own tables. Shared with cslua_corpse.cpp's hook.add("msg:Name",
// ...) dispatch, which needs the same id->name resolution this module
// already built for its own logging.
const char *cslua_netwatch_msg_name(int msg_type, char *buf, size_t buflen);

// Drops the accumulated history for a slot so it cannot be attributed to
// whoever reconnects into it next.
void cslua_netwatch_forget(int id);
