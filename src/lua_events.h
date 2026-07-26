#pragma once

#include <lua.hpp>
#include <map>
#include <string>
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

	// Server lifetime - engine level, no ReGameDLL needed.
	CSLUA_EVENT_MAP_CHANGE,			// this map is ending
	CSLUA_EVENT_PLUGIN_UNLOAD,		// the Lua state is going away

	// Gameplay - ReGameDLL only, fire nothing on a vanilla mp.dll.
	CSLUA_EVENT_PLAYER_SPAWN,
	CSLUA_EVENT_PLAYER_HURT,			// pre: change or block the damage
	CSLUA_EVENT_PLAYER_HURT_POST,		// post: damage already applied
	CSLUA_EVENT_PLAYER_DEATH,		// was player_killed
	CSLUA_EVENT_PLAYER_TEAM_CHANGE,
	CSLUA_EVENT_WEAPON_FIRE,
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

// One hook system for everything.
//
//   hook.add("player_hurt", "god.block", function(e) e:cancel() end)
//   hook.add("shop.bought", "stats.count", function(e) ... end)
//   hook.remove("player_hurt", "god.block")
//   local e = hook.run("shop.bought", { player = p, item = it })
//
// Engine events (the enum above) and plugin events share one registry, one
// registration call and one handler shape: every handler takes exactly one
// argument, the event table, and its return value is never read. A handler
// changes the outcome by writing a field (e.damage = 0) or calling e:cancel().
//
// A plugin event name must contain a dot - "shop.bought", not "bought". That
// is what tells a plugin's own event apart from a typo in an engine one, which
// would otherwise register a handler that silently never fires.
class LuaEvents
{
public:
	// Lua entry points, registered as the `hook` namespace.
	static int l_add(lua_State *L);
	static int l_remove(lua_State *L);
	static int l_run(lua_State *L);
	static int l_list(lua_State *L);

	// Builds the two event metatables. Must run before any dispatch.
	void init(lua_State *L);

	void clear();
	void remove_plugin(int plugin_index);
	int count(CsLuaEvent ev) const;

	// True when any script listens for the given event; lets the API skip
	// installing a hookchain nobody uses.
	bool any(CsLuaEvent ev) const;

	// Returns true if a handler rejected the connection; reason is filled in.
	bool fire_client_connect(int id, const char *name, const char *ip, RejectInfo &reject);
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

	// The map is ending, either for the next one or because the server is
	// stopping. The last point where a plugin can undo what it did to the
	// world or write its data out.
	void fire_map_change(const char *map);

	// Something is going away. e.plugin names which one for a single-plugin
	// reload, and is nil when the whole state is being torn down (lua_reload,
	// server shutdown) - same job as map_change, one level lower.
	//
	// Anything holding a reference into another plugin watches this: the core
	// export registry drops that plugin's entries here, and a plugin with a
	// soft dependency can fall back before the calls start failing.
	void fire_plugin_unload(const char *only_id = NULL);

	// ReGameDLL gameplay events.
	void fire_player_spawn(int id);

	// weapon is the killer's weapon classname and distance the gap between the
	// two, both NULL/negative when the world did the killing.
	void fire_player_death(int victim, int killer, int headshot,
		const char *weapon, float distance);

	// old/new are team names ("CT", "T", "SPEC", "NONE").
	void fire_player_team_change(int id, const char *old_team, const char *new_team);

	// A shot left the barrel. Firearms only - see the note in docs/events.md.
	void fire_weapon_fire(int id, const char *weapon, int clip);

	void fire_round_start();
	void fire_round_end(int winner);
	void fire_round_freeze_end();

	// Bomb scenario. planter/defuser are player slots, 0 when unknown.
	void fire_bomb_planted(int planter);
	void fire_bomb_defused(int defuser, bool success);
	void fire_bomb_exploded(float x, float y, float z);

	// TakeDamage passes damage by reference, so a handler can change it or
	// zero it out. Returns the final damage the game should apply: whatever
	// e.damage holds after the chain, or 0 if a handler cancelled.
	//
	// hitgroup is the body region TraceAttack last recorded on the victim, or
	// -1 when the damage did not come from a hit at all (fall, world, gas).
	float fire_player_hurt(int victim, int attacker, float damage, int bits, int hitgroup);

	// Post variant: the damage the game actually applied, for observers.
	void fire_player_hurt_post(int victim, int attacker, float damage, int bits, int hitgroup);

private:
	struct Handler
	{
		int ref;			// registry ref to the Lua function
		int plugin;			// index into LuaEngine::plugins()
		std::string id;		// unique within one plugin, for replace/remove

		// A plugin can be shut down from inside its own handler - blowing the
		// precache budget does exactly that - and erasing from the list being
		// walked would skip whatever moved into the freed slot. Handlers are
		// marked instead and swept once the walk is over.
		bool dead;
	};

	typedef std::vector<Handler> HandlerList;

	// Fills the event's own fields onto the table sitting on top of the stack.
	typedef std::function<void(lua_State *)> FillFields;

	// Runs with the finished event table on top of the stack, for events whose
	// fields the game reads back.
	typedef std::function<void(lua_State *)> ReadFields;

	const char *plugin_id(int plugin_index) const;

	// Drops the handlers marked dead. No-op while a dispatch is in progress.
	void sweep();

	// Pushes a fresh event table: name, cancelled = false, the caller's fields
	// and the metatable carrying cancel().
	void push_event(lua_State *L, const char *name, bool cancellable, const FillFields &fill);

	// Walks one handler list against the event table at `event_index`. Stops
	// early once a handler has cancelled.
	void dispatch_list(lua_State *L, HandlerList &list, int event_index);

	// Notify-style: build the event, run the chain, drop it.
	void notify(CsLuaEvent ev, const FillFields &fill = FillFields());

	// Cancellable: `read` runs with the event table on top, before it is
	// dropped. Returns true when a handler cancelled.
	bool run(CsLuaEvent ev, const FillFields &fill, const ReadFields &read = ReadFields());

	// Engine events, indexed by the enum. Plugin events live in m_custom,
	// keyed by name - there is no fixed set of them to index.
	HandlerList m_handlers[CSLUA_EVENT_COUNT];
	std::map<std::string, HandlerList> m_custom;

	// Depth rather than a flag: one handler firing another event nests, and
	// the sweep must wait for the outermost walk to finish.
	int m_dispatching = 0;

	// The two event metatables, built once in init().
	int m_mt_cancellable = LUA_NOREF;
	int m_mt_notify = LUA_NOREF;
};

extern LuaEvents g_events;

// Registers the `hook` namespace. Separate from init() because the metatables
// have to exist before the first dispatch, not before the first script line.
void cslua_register_hooks(lua_State *L);
