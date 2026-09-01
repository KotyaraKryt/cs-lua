#pragma once

#include <extdll.h>

// Post-hook table (meta_api.cpp's GetEngineFunctions_Post): the only entry that
// matters is pfnRegUserMsg, so this module can learn a custom usermessage's
// name. Must be a post hook - the id doesn't exist until the real engine
// function has run.
extern enginefuncs_t g_CsluaNetwatchPostEngineFuncs;

// Explains an "Overflow" (or any) client drop after the fact: which message
// types put how many bytes on the client's reliable queue in the seconds
// before the drop.
//
// cslua_corpse.cpp's enginefuncs_t hook is the only pfnMessageBegin/Write*
// interception point (metamod allows one plugin per pre-hook slot), so its
// Hook_* bodies call into cslua_netwatch_message_begin/add_bytes/message_end.
// Every reliable message reaching a client is counted regardless of who wrote
// it.

// Registers the ReHLDS SV_DropClient hookchain. No-op without ReHLDS. Called
// once at startup, after cslua_rehlds_init.
void cslua_netwatch_install_hooks();

void cslua_netwatch_message_begin(edict_t *dest, int msg_type);
void cslua_netwatch_add_bytes(int bytes);
void cslua_netwatch_message_end();

// Resolves a msg_type to its name - a builtin svc_* opcode, a custom
// usermessage learned from pfnRegUserMsg, or "usermsg#<n>". `buf` is written
// only for the fallback. Also used by cslua_corpse.cpp's hook.add("msg:Name")
// dispatch.
const char *cslua_netwatch_msg_name(int msg_type, char *buf, size_t buflen);

// Drops the accumulated history for a slot.
void cslua_netwatch_forget(int id);
