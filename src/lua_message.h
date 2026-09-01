#pragma once

#include <lua.hpp>

#include <string>

// Text placement and styling for hud()/dhud(). Every field has a default.
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

// Registers the encoding cvar. Call once at startup.
void cslua_message_init();

// Registers the `ui` namespace (the shared colour palette).
void cslua_register_ui(lua_State *L);

// The same encoding step chat/HUD text go through, for senders outside this
// file (menus). CP1251 when cslua_cp1251 is on and the input is UTF-8.
std::string cslua_text_for_client(const char *text);

// Resolved user message ids, for the lua_debug command.
int cslua_msg_saytext_id();
int cslua_msg_textmsg_id();

// Sends the same line three ways to see which one a client renders. Diagnostics.
void cslua_chat_probe(int id);

// id is a slot 1..32, or 0 for everyone connected.
void cslua_send_console(int id, const char *text);

// `from` is the slot whose team {team} resolves to; 0 means "the receiver".
void cslua_send_chat(int id, const char *text, int from);
void cslua_send_center(int id, const char *text);
void cslua_send_hud(int id, const char *text, const HudParams &p);

// The MOTD window - the one multi-line surface the game has. Renders HTML on
// every client; raw = false wraps the text so it arrives readable, raw = true
// sends it untouched.
void cslua_send_motd(int id, const char *text, bool raw);
void cslua_send_dhud(int id, const char *text, const HudParams &p);

void cslua_send_screen_shake(int id, float amplitude, float frequency, float duration);

// Options for p:screen_fade(). Defaults: the classic "flash and clear" damage
// effect - opaque at t=0, fading away over `duration`.
struct ScreenFadeParams
{
	float duration = 1.0f;
	float hold = 0.0f;
	unsigned char r = 0, g = 0, b = 0, a = 200;
	bool fade_out = false;		// FFADE_OUT: fade FROM normal TO the colour
	bool modulate = false;		// FFADE_MODULATE: blend instead of a solid colour
	bool stay = false;		// FFADE_STAYOUT: hold at the end instead of clearing
};

void cslua_read_screen_fade_params(lua_State *L, int index, ScreenFadeParams &out);
void cslua_send_screen_fade(int id, const ScreenFadeParams &p);
