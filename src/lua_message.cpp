#include "cslua.h"
#include "lua_message.h"
#include "players.h"

#include <string.h>
#include <vector>

// Spelled out rather than pulled from the SDK: dlls/util.h and common/hltv.h
// drag in the whole CBaseEntity hierarchy for three integers.
#define SVC_TEMPENTITY		23
#define SVC_DIRECTOR		51
#define DRC_CMD_MESSAGE		6

// SayText is a game DLL user message, so it only exists once the game DLL has
// registered it. Resolved on first use and cached.
static int msg_saytext()
{
	static int id = -1;
	if (id < 0) {
		id = GET_USER_MSG_ID(PLID, "SayText", NULL);
		if (!id)
			cslua_error("user message 'SayText' is not registered, chat output is unavailable");
	}
	return id;
}

static int msg_textmsg()
{
	static int id = -1;
	if (id < 0) {
		id = GET_USER_MSG_ID(PLID, "TextMsg", NULL);
		if (!id)
			cslua_error("user message 'TextMsg' is not registered");
	}
	return id;
}

int cslua_msg_saytext_id() { return msg_saytext(); }
int cslua_msg_textmsg_id() { return msg_textmsg(); }

// Text encoding, and why this is a switch rather than a rule.
//
// Modern CS 1.6 clients (current Steam builds) render chat as UTF-8, which is
// also what .lua files normally are - so the right thing is to send the text
// untouched. Older clients expect single-byte CP1251 and show UTF-8 as
// garbage; for those, cslua_cp1251 1 turns on the conversion below.
//
// Default is off: it matches current clients, and a wrong conversion is
// invisible on the server - you only find out from players.
static cvar_t s_cvar_cp1251 = { "cslua_cp1251", "0", 0, 0.0f, nullptr };
static cvar_t *s_cp1251 = nullptr;

void cslua_message_init()
{
	CVAR_REGISTER(&s_cvar_cp1251);
	s_cp1251 = CVAR_GET_POINTER("cslua_cp1251");
}

static bool cp1251_enabled()
{
	return s_cp1251 && s_cp1251->value != 0.0f;
}

// Converts UTF-8 to CP1251. Only used when the switch above is on; a string
// that is not valid multi-byte UTF-8 (plain ASCII, or text already CP1251) is
// passed through untouched.
static unsigned short utf8_decode(const unsigned char *s, int &len)
{
	if (s[0] < 0x80) {
		len = 1;
		return s[0];
	}

	if ((s[0] & 0xE0) == 0xC0 && (s[1] & 0xC0) == 0x80) {
		len = 2;
		unsigned short cp = ((s[0] & 0x1F) << 6) | (s[1] & 0x3F);
		return cp < 0x80 ? 0 : cp;			// reject overlong
	}

	if ((s[0] & 0xF0) == 0xE0 && (s[1] & 0xC0) == 0x80 && (s[2] & 0xC0) == 0x80) {
		len = 3;
		unsigned short cp = ((s[0] & 0x0F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F);
		return cp < 0x800 ? 0 : cp;
	}

	len = 0;
	return 0;
}

static unsigned char cp1251_of(unsigned short cp)
{
	if (cp >= 0x0410 && cp <= 0x044F)		// А..я, contiguous in both
		return (unsigned char)(0xC0 + (cp - 0x0410));

	switch (cp) {
	case 0x0401: return 0xA8;				// Ё
	case 0x0451: return 0xB8;				// ё
	case 0x0490: return 0xA5;				// Ґ
	case 0x0491: return 0xB4;				// ґ
	case 0x00AB: return 0xAB;				// «
	case 0x00BB: return 0xBB;				// »
	case 0x2013: return 0x96;				// –
	case 0x2014: return 0x97;				// —
	case 0x2026: return 0x85;				// …
	case 0x2116: return 0xB9;				// №
	case 0x00A0: return 0x20;				// nbsp
	default:     return 0;
	}
}

static void to_cp1251(const char *in, char *out, size_t outsz)
{
	const unsigned char *s = (const unsigned char *)in;

	// First pass: is this actually UTF-8 with something to convert?
	bool multibyte = false;
	for (size_t i = 0; s[i]; ) {
		if (s[i] < 0x80) {
			i++;
			continue;
		}
		int len = 0;
		if (!utf8_decode(s + i, len)) {
			cslua_snprintf(out, outsz, "%s", in);	// not UTF-8, leave it alone
			out[outsz - 1] = '\0';
			return;
		}
		multibyte = true;
		i += len;
	}

	if (!multibyte) {
		cslua_snprintf(out, outsz, "%s", in);
		out[outsz - 1] = '\0';
		return;
	}

	size_t w = 0;
	for (size_t i = 0; s[i] && w + 1 < outsz; ) {
		if (s[i] < 0x80) {
			out[w++] = (char)s[i++];
			continue;
		}
		int len = 0;
		unsigned short cp = utf8_decode(s + i, len);
		unsigned char mapped = cp1251_of(cp);
		out[w++] = mapped ? (char)mapped : '?';
		i += len;
	}

	out[w] = '\0';
}

static edict_t *player_edict(int id)
{
	if (!g_players.is_connected(id))
		return NULL;

	edict_t *e = g_engfuncs.pfnPEntityOfEntIndex(id);
	if (!e || e->free)
		return NULL;

	return e;
}

// Runs `fn` for one player, or for everyone connected when id is 0.
template <typename Fn>
static void for_targets(int id, Fn fn)
{
	if (id > 0) {
		edict_t *e = player_edict(id);
		if (e)
			fn(id, e);
		return;
	}

	for (int i = 1; i < CSLUA_MAXPLAYERS; i++) {
		edict_t *e = player_edict(i);
		if (e)
			fn(i, e);
	}
}

// The HUD protocol stores floats as fixed point; same conversion the SDK uses.
static short fixed_signed16(float value, float scale)
{
	int output = (int)(value * scale);
	if (output > 32767) output = 32767;
	else if (output < -32768) output = -32768;
	return (short)output;
}

static short fixed_unsigned16(float value, float scale)
{
	int output = (int)(value * scale);
	if (output < 0) output = 0;
	else if (output > 0xFFFF) output = 0xFFFF;
	return (short)output;
}

static int float_bits(float f)
{
	int bits;
	memcpy(&bits, &f, sizeof bits);
	return bits;
}

// LuaJIT is 5.1, which has no lua_absindex.
static int abs_index(lua_State *L, int index)
{
	return index > 0 || index <= LUA_REGISTRYINDEX ? index : lua_gettop(L) + index + 1;
}

static float opt_number(lua_State *L, int table, const char *key, float def)
{
	lua_getfield(L, table, key);
	float value = lua_isnumber(L, -1) ? (float)lua_tonumber(L, -1) : def;
	lua_pop(L, 1);
	return value;
}

// color = {r, g, b} or {r, g, b, a}
static void opt_color(lua_State *L, int table, const char *key,
	unsigned char &r, unsigned char &g, unsigned char &b, unsigned char &a)
{
	lua_getfield(L, table, key);
	if (lua_istable(L, -1)) {
		const int comps = 4;
		unsigned char *out[comps] = { &r, &g, &b, &a };
		for (int i = 0; i < comps; i++) {
			lua_rawgeti(L, -1, i + 1);
			if (lua_isnumber(L, -1)) {
				int v = (int)lua_tointeger(L, -1);
				if (v < 0) v = 0;
				else if (v > 255) v = 255;
				*out[i] = (unsigned char)v;
			}
			lua_pop(L, 1);
		}
	}
	lua_pop(L, 1);
}

void cslua_read_hud_params(lua_State *L, int index, HudParams &out)
{
	if (lua_isnoneornil(L, index))
		return;

	luaL_checktype(L, index, LUA_TTABLE);
	index = abs_index(L, index);

	out.x = opt_number(L, index, "x", out.x);
	out.y = opt_number(L, index, "y", out.y);
	out.effect = (int)opt_number(L, index, "effect", (float)out.effect);
	out.fadein = opt_number(L, index, "fadein", out.fadein);
	out.fadeout = opt_number(L, index, "fadeout", out.fadeout);
	out.hold = opt_number(L, index, "hold", out.hold);
	out.fxtime = opt_number(L, index, "fxtime", out.fxtime);

	int channel = (int)opt_number(L, index, "channel", (float)out.channel);
	out.channel = channel & 0xFF;

	opt_color(L, index, "color", out.r1, out.g1, out.b1, out.a1);
	// Effect 2 fades from color to color2; default it to color so a script
	// that only sets one colour still looks right.
	out.r2 = out.r1; out.g2 = out.g1; out.b2 = out.b1; out.a2 = out.a1;
	opt_color(L, index, "color2", out.r2, out.g2, out.b2, out.a2);
}

// Copies the text out, converting only if the server asked for it, and appends
// the newline where the protocol needs one.
static void prepare(const char *text, char *out, size_t outsz, bool newline)
{
	size_t room = newline ? outsz - 1 : outsz;

	if (cp1251_enabled()) {
		to_cp1251(text, out, room);
	} else {
		// Copied, not formatted: the truncation here is the point - `room` is
		// what the channel carries - and spelling it out this way says so,
		// where snprintf only looked like an oversight the compiler warned about.
		size_t len = strlen(text);
		if (len > room - 1)
			len = room - 1;
		memcpy(out, text, len);
		out[len] = '\0';
	}

	if (newline) {
		size_t len = strlen(out);
		out[len] = '\n';
		out[len + 1] = '\0';
	}
}

// Menus build their own packets but need the same encoding treatment. They can
// also be a lot longer than one chat line, so this one sizes itself: CP1251 is
// single-byte, so the result never grows.
std::string cslua_text_for_client(const char *text)
{
	if (!text)
		return std::string();
	if (!cp1251_enabled())
		return std::string(text);

	std::vector<char> buf(strlen(text) + 1);
	to_cp1251(text, &buf[0], buf.size());
	return std::string(&buf[0]);
}

void cslua_send_console(int id, const char *text)
{
	char buf[256];
	prepare(text, buf, sizeof buf, true);

	for_targets(id, [&](int, edict_t *e) {
		CLIENT_PRINTF(e, print_console, buf);
	});
}

void cslua_send_center(int id, const char *text)
{
	char buf[256];
	prepare(text, buf, sizeof buf, true);

	for_targets(id, [&](int, edict_t *e) {
		CLIENT_PRINTF(e, print_center, buf);
	});
}

// SayText understands three control bytes. There is no free RGB: {team} takes
// its colour from the player whose slot is in the message's first byte, which
// is how you get blue and red at all.
static const struct { const char *tag; char code; } s_chat_tags[] =
{
	{ "{default}", '\x01' },
	{ "{yellow}",  '\x01' },
	{ "{team}",    '\x03' },
	{ "{green}",   '\x04' },
};

static void expand_chat_tags(const char *in, char *out, size_t outsz)
{
	size_t w = 0;

	for (size_t i = 0; in[i] && w + 1 < outsz; ) {
		bool matched = false;

		if (in[i] == '{') {
			for (size_t t = 0; t < sizeof s_chat_tags / sizeof s_chat_tags[0]; t++) {
				size_t len = strlen(s_chat_tags[t].tag);
				if (strncmp(in + i, s_chat_tags[t].tag, len) != 0)
					continue;
				out[w++] = s_chat_tags[t].code;
				i += len;
				matched = true;
				break;
			}
		}

		if (!matched)
			out[w++] = in[i++];
	}

	out[w] = '\0';
}

void cslua_send_chat(int id, const char *text, int from)
{
	int msg = msg_saytext();
	if (!msg)
		return;

	char tagged[256];
	expand_chat_tags(text, tagged, sizeof tagged);

	// SayText's buffer is 192 bytes and it needs the trailing newline,
	// otherwise the next line runs into this one.
	char buf[190];
	prepare(tagged, buf, sizeof buf, true);

	for_targets(id, [&](int slot, edict_t *e) {
		// The first byte is the "speaker" slot and decides what \x03 shows as.
		MESSAGE_BEGIN(MSG_ONE, msg, NULL, e);
		WRITE_BYTE(from > 0 ? from : slot);
		WRITE_STRING(buf);
		MESSAGE_END();
	});
}

void cslua_chat_probe(int id)
{
	int say = msg_saytext();
	int text = msg_textmsg();

	for_targets(id, [&](int slot, edict_t *e) {
		if (say) {
			MESSAGE_BEGIN(MSG_ONE, say, NULL, e);
			WRITE_BYTE(slot);
			WRITE_STRING("\x01[probe 1] SayText with \\x01 prefix\n");
			MESSAGE_END();

			MESSAGE_BEGIN(MSG_ONE, say, NULL, e);
			WRITE_BYTE(slot);
			WRITE_STRING("[probe 2] SayText, no prefix\n");
			MESSAGE_END();
		}

		if (text) {
			MESSAGE_BEGIN(MSG_ONE, text, NULL, e);
			WRITE_BYTE(HUD_PRINTTALK);
			WRITE_STRING("[probe 3] TextMsg HUD_PRINTTALK\n");
			MESSAGE_END();
		}

		// Plain engine call, no user message involved.
		CLIENT_PRINTF(e, print_chat, "[probe 4] ClientPrintf print_chat\n");
	});
}

void cslua_send_hud(int id, const char *text, const HudParams &p)
{
	char buf[512];
	prepare(text, buf, sizeof buf, false);

	for_targets(id, [&](int, edict_t *e) {
		MESSAGE_BEGIN(MSG_ONE, SVC_TEMPENTITY, NULL, e);
		WRITE_BYTE(TE_TEXTMESSAGE);
		WRITE_BYTE(p.channel & 0xFF);

		WRITE_SHORT(fixed_signed16(p.x, 1 << 13));
		WRITE_SHORT(fixed_signed16(p.y, 1 << 13));
		WRITE_BYTE(p.effect);

		WRITE_BYTE(p.r1); WRITE_BYTE(p.g1); WRITE_BYTE(p.b1); WRITE_BYTE(p.a1);
		WRITE_BYTE(p.r2); WRITE_BYTE(p.g2); WRITE_BYTE(p.b2); WRITE_BYTE(p.a2);

		WRITE_SHORT(fixed_unsigned16(p.fadein, 1 << 8));
		WRITE_SHORT(fixed_unsigned16(p.fadeout, 1 << 8));
		WRITE_SHORT(fixed_unsigned16(p.hold, 1 << 8));

		if (p.effect == 2)
			WRITE_SHORT(fixed_unsigned16(p.fxtime, 1 << 8));

		WRITE_STRING(buf);
		MESSAGE_END();
	});
}

void cslua_send_dhud(int id, const char *text, const HudParams &p)
{
	// The directed HUD is an engine message, not a game DLL user message, so
	// it needs no registration. The client caps it at 128 chars.
	char buf[128];
	prepare(text, buf, sizeof buf, false);

	int length = (int)strlen(buf);

	for_targets(id, [&](int, edict_t *e) {
		MESSAGE_BEGIN(MSG_ONE_UNRELIABLE, SVC_DIRECTOR, NULL, e);
		WRITE_BYTE(length + 31);				// payload size, header included
		WRITE_BYTE(DRC_CMD_MESSAGE);
		WRITE_BYTE(p.effect);
		WRITE_LONG(p.b1 + (p.g1 << 8) + (p.r1 << 16));
		WRITE_LONG(float_bits(p.x));
		WRITE_LONG(float_bits(p.y));
		WRITE_LONG(float_bits(p.fadein));
		WRITE_LONG(float_bits(p.fadeout));
		WRITE_LONG(float_bits(p.hold));
		WRITE_LONG(float_bits(p.fxtime));
		WRITE_STRING(buf);
		MESSAGE_END();
	});
}
