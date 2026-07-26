#include "cslua.h"
#include "lua_menu.h"
#include "lua_events.h"
#include "lua_natives.h"
#include "lua_message.h"
#include "players.h"

#include <string.h>
#include <string>

// ShowMenu is a game DLL user message, resolved on first use.
static int msg_showmenu()
{
	static int id = -1;
	if (id < 0) {
		id = GET_USER_MSG_ID(PLID, "ShowMenu", NULL);
		if (!id)
			cslua_error("user message 'ShowMenu' is not registered, menus are unavailable");
	}
	return id;
}

// One menu open per player at most, which is all the engine can draw anyway.
// Storing the key mask lets us ignore a stale answer: the client will happily
// send menuselect for a menu that has already been replaced.
static bool s_menu_open[CSLUA_MAXPLAYERS];
static int s_menu_keys[CSLUA_MAXPLAYERS];

// When the panel stops being ours, in server time. CS draws its own team and
// buy menus with the same `menuselect` command, so a menu we keep claiming
// after it left the screen would eat the player's buy keys. 0 = no deadline,
// which is what a menu shown with time < 0 gets.
static float s_menu_until[CSLUA_MAXPLAYERS];

static void forget_menu(int id)
{
	s_menu_open[id] = false;
	s_menu_keys[id] = 0;
	s_menu_until[id] = 0.0f;
}

// The panel body is capped per packet; longer text is split across follow-up
// messages with the "more" flag set, exactly as the engine expects.
static const size_t MENU_CHUNK = 175;

static void send_menu(int id, int keys, int time, const char *raw)
{
	int msg = msg_showmenu();
	if (!msg)
		return;

	edict_t *e = g_engfuncs.pfnPEntityOfEntIndex(id);
	if (!e || e->free)
		return;

	// Encode first, split second: the conversion changes the byte length, so
	// chunking the UTF-8 could cut a character in half.
	std::string encoded = cslua_text_for_client(raw);
	const char *text = encoded.c_str();

	size_t len = encoded.size();
	size_t sent = 0;

	do {
		size_t chunk = len - sent;
		bool more = chunk > MENU_CHUNK;
		if (more)
			chunk = MENU_CHUNK;

		std::string part(text + sent, chunk);

		MESSAGE_BEGIN(MSG_ONE, msg, NULL, e);
		WRITE_SHORT(keys);
		WRITE_CHAR(time);
		WRITE_BYTE(more ? 1 : 0);
		WRITE_STRING(part.c_str());
		MESSAGE_END();

		sent += chunk;
	} while (sent < len);
}

// menu._show(playerId, keys, time, text)
//   keys: bitmask of allowed slots, bit 0 = key "1"
//   time: seconds to keep it up, -1 for until answered
static int l_show_menu(lua_State *L)
{
	int id = (int)luaL_checkinteger(L, 1);
	int keys = (int)luaL_checkinteger(L, 2);
	int time = (int)luaL_optinteger(L, 3, -1);
	const char *text = luaL_checkstring(L, 4);

	if (id < 1 || id >= CSLUA_MAXPLAYERS)
		return luaL_error(L, "show_menu: player index %d out of range", id);

	if (!g_players.is_connected(id))
		return 0;

	send_menu(id, keys, time, text);

	s_menu_open[id] = true;
	s_menu_keys[id] = keys;
	// Match the deadline the client draws with, plus a second of slack for the
	// round trip, so a key pressed on the last frame still counts.
	s_menu_until[id] = time > 0 ? gpGlobals->time + (float)time + 1.0f : 0.0f;
	return 0;
}

// menu._close(playerId) - an empty panel with no keys dismisses it.
static int l_close_menu(lua_State *L)
{
	int id = (int)luaL_checkinteger(L, 1);

	if (id < 1 || id >= CSLUA_MAXPLAYERS)
		return luaL_error(L, "close_menu: player index %d out of range", id);

	if (g_players.is_connected(id))
		send_menu(id, 0, 0, " ");

	forget_menu(id);
	return 0;
}

// A slot changing hands must not inherit the previous occupant's menu.
void cslua_menu_reset(int id)
{
	if (id >= 1 && id < CSLUA_MAXPLAYERS)
		forget_menu(id);
}

bool cslua_menu_handle_select(int id, const char *cmd)
{
	if (strcmp(cmd, "menuselect"))
		return false;

	if (id < 1 || id >= CSLUA_MAXPLAYERS || !s_menu_open[id])
		return false;			// not ours: let the game have it

	// Timed out and gone from the screen - the key belongs to whatever the
	// game is showing now.
	if (s_menu_until[id] != 0.0f && gpGlobals->time > s_menu_until[id]) {
		forget_menu(id);
		return false;
	}

	int key = CMD_ARGC() > 1 ? atoi(CMD_ARGV(1)) : 0;
	if (key < 1 || key > 10)
		return false;

	// Ignore a key the menu never offered - a client can send anything.
	if (!(s_menu_keys[id] & (1 << (key - 1))))
		return false;

	// One answer per menu. Clearing first means a handler is free to open the
	// next menu without this one swallowing its reply.
	forget_menu(id);

	g_events.fire_menu_select(id, key);
	return true;
}

void cslua_register_menu(lua_State *L)
{
	// Reset on every state: a reload drops the Lua handlers, so a menu still
	// on someone's screen has nothing left to answer to.
	for (int i = 0; i < CSLUA_MAXPLAYERS; i++)
		forget_menu(i);

	// Raw panel drawing, not the menu API. core/ui.lua picks these up into
	// locals and clears them before any plugin runs, so what a plugin sees on
	// `menu` is menu.new() and nothing else.
	static const luaL_Reg s_internal[] =
	{
		{ "_show",  l_show_menu },
		{ "_close", l_close_menu },
		{ NULL, NULL }
	};

	cslua_register_namespace(L, "menu", s_internal);
}
