#pragma once

#include <lua.hpp>
#include <vector>
#include <functional>

// Prefixed because windows.h claims plain EVENT_* names.
//
// Two families: client_* is the connection layer (an edict, maybe no full
// CBasePlayer yet), player_* is a player already in the game.
enum CsLuaEvent
{
	// Connection layer - work on a vanilla mp.dll too.
	CSLUA_EVENT_CLIENT_CONNECT,
	CSLUA_EVENT_CLIENT_DISCONNECT,
	CSLUA_EVENT_PLAYER_AUTHORIZED,	// steamid finally known
	CSLUA_EVENT_PLAYER_READY,		// was client_put_in_server
	CSLUA_EVENT_PLAYER_CHAT,		// was client_say
	CSLUA_EVENT_MENU_SELECT,		// answered a menu opened from Lua

	// Gameplay - ReGameDLL only, fire nothing on a vanilla mp.dll.
	CSLUA_EVENT_PLAYER_SPAWN,
	CSLUA_EVENT_PLAYER_HURT,			// pre: change or block the damage
	CSLUA_EVENT_PLAYER_HURT_POST,		// post: damage already applied
	CSLUA_EVENT_PLAYER_DEATH,		// was player_killed
	CSLUA_EVENT_ROUND_START,
	CSLUA_EVENT_ROUND_END,
	CSLUA_EVENT_ROUND_FREEZE_END,	// freeze time over, players can move
	CSLUA_EVENT_BOMB_PLANTED,
	CSLUA_EVENT_BOMB_DEFUSED,
	CSLUA_EVENT_BOMB_EXPLODED,

	CSLUA_EVENT_COUNT
};

struct RejectInfo
{
	char reason[128];
};

class LuaEvents
{
public:
	// Lua: on("client_connect", function(p, name, ip) end)
	static int l_on(lua_State *L);

	void clear();
	void remove_plugin(int plugin_index);
	int count(CsLuaEvent ev) const;

	// Returns true if a handler rejected the connection; reason is filled in.
	bool fire_client_connect(int id, const char *name, const char *ip, RejectInfo &reject);
	void fire_client_put_in_server(int id);
	void fire_client_disconnect(int id, const char *name);

	// Fires once per client, the moment Steam stops answering
	// STEAM_ID_PENDING. Anything keyed on the steamid (access rights, stats)
	// belongs here rather than in client_connect.
	void fire_player_authorized(int id, const char *authid);

	void fire_player_ready(int id);

	// Returns true if a handler swallowed the message, so it never reaches
	// the chat - that is how "!command" style input stays invisible.
	bool fire_player_chat(int id, const char *text, bool team);

	// A player pressed a key on a menu opened from Lua. Only the key is
	// passed; which menu it belonged to is core/menu.lua's business.
	void fire_menu_select(int id, int key);

	// ReGameDLL gameplay events.
	void fire_player_spawn(int id);
	void fire_player_death(int victim, int killer, int headshot);
	void fire_round_start();
	void fire_round_end(int winner);
	void fire_round_freeze_end();

	// Bomb scenario. planter/defuser are player slots, 0 when unknown.
	void fire_bomb_planted(int planter);
	void fire_bomb_defused(int defuser, bool success);
	void fire_bomb_exploded(float x, float y, float z);

	// TakeDamage passes damage by reference, so a handler can change it or
	// zero it out. Returns the final damage the game should apply; a handler
	// returns a number to replace it or false to block it entirely.
	float fire_player_hurt(int victim, int attacker, float damage, int bits);

	// Post variant: the damage the game actually applied, for observers.
	void fire_player_hurt_post(int victim, int attacker, float damage, int bits);

	// True when any script listens for the given event; lets the API skip
	// installing a hookchain nobody uses.
	bool any(CsLuaEvent ev) const;

private:
	struct Handler
	{
		int ref;			// registry ref to the Lua function
		int plugin;			// index into LuaEngine::plugins()

		// A plugin can be shut down from inside its own handler - blowing the
		// precache budget does exactly that - and erasing from the list being
		// walked would skip whatever moved into the freed slot. Handlers are
		// marked instead and swept once the walk is over.
		bool dead;
	};

	const char *plugin_id(int plugin_index) const;

	// Drops the handlers marked dead. No-op while a dispatch is in progress.
	void sweep();

	// Runs every handler of a notify-style event (no return value inspected).
	// `push` pushes the event's arguments; `nargs` is how many it pushed.
	typedef std::function<void(lua_State *)> PushArgs;
	void dispatch(CsLuaEvent ev, int nargs, const PushArgs &push);

	std::vector<Handler> m_handlers[CSLUA_EVENT_COUNT];

	// Depth rather than a flag: one handler firing another event nests, and
	// the sweep must wait for the outermost walk to finish.
	int m_dispatching = 0;
};

extern LuaEvents g_events;
