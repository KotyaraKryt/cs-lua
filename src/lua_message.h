#pragma once

#include <lua.hpp>

#include <string>

// Text placement and styling for hud()/dhud(). Filled from the options table
// the script passes; every field has a default so `p:hud("text")` works.
struct HudParams
{
	float x = -1.0f;			// -1 centers on that axis
	float y = 0.35f;
	int effect = 0;				// 0 fade, 1 flicker, 2 typewriter
	unsigned char r1 = 255, g1 = 255, b1 = 255, a1 = 0;
	unsigned char r2 = 255, g2 = 255, b2 = 255, a2 = 0;
	float fadein = 0.1f;
	float fadeout = 0.2f;
	float hold = 5.0f;
	float fxtime = 0.25f;
	int channel = 3;			// hud channels are 0..3
};

// Reads an options table at `index` (nil/absent means "all defaults").
void cslua_read_hud_params(lua_State *L, int index, HudParams &out);

// Registers the encoding cvar. Call once at startup, before anything is sent.
void cslua_message_init();

// Registers the `ui` namespace, which for now is the shared colour palette.
// include/color.lua builds the per-channel degrading on top of it.
void cslua_register_ui(lua_State *L);

// The same encoding step chat and HUD text go through, for senders that live
// outside this file (menus). Returns the text as the client wants it: CP1251
// when cslua_cp1251 is on and the input is UTF-8, unchanged otherwise.
std::string cslua_text_for_client(const char *text);

// Resolved user message ids, for the lua_debug command.
int cslua_msg_saytext_id();
int cslua_msg_textmsg_id();

// Sends the same line three different ways so we can see which one a client
// actually renders. Diagnostics only.
void cslua_chat_probe(int id);

// id is a slot 1..32, or 0 to send to everyone connected.
void cslua_send_console(int id, const char *text);

// `from` is the slot whose team {team} resolves to; 0 means "the receiver",
// which is what you want for a message coloured in the reader's own colours.
void cslua_send_chat(int id, const char *text, int from);
void cslua_send_center(int id, const char *text);
void cslua_send_hud(int id, const char *text, const HudParams &p);
void cslua_send_dhud(int id, const char *text, const HudParams &p);
