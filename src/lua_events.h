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
	CSLUA_EVENT_WEAPON_DEPLOY,		// view/world model can be swapped before the game applies them
	CSLUA_EVENT_WEAPON_RELOAD,		// a real reload just started: ammo was available, clip wasn't full
	CSLUA_EVENT_ROUND_START,
	CSLUA_EVENT_ROUND_END,
	CSLUA_EVENT_ROUND_FREEZE_END,	// freeze time over, players can move
	CSLUA_EVENT_BOMB_PLANTED,
	CSLUA_EVENT_BOMB_DEFUSED,
	CSLUA_EVENT_BOMB_EXPLODED,
	CSLUA_EVENT_GRENADE_THROW,		// about to leave the hand; fuse time can be changed
	CSLUA_EVENT_GRENADE_THROWN,		// just left the hand; entity for reskinning
	CSLUA_EVENT_GRENADE_EXPLODE,		// about to explode; cancel to take over
	CSLUA_EVENT_WEAPON_THROW,		// any grenade-slot throw, before HE/flash/smoke is picked; cancel to take over
	CSLUA_EVENT_WEAPON_SECONDARY_ATTACK,	// right-click pressed, synthesized from a button edge-detect

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

	// How many handlers one plugin holds across every event, engine and its
	// own. lua_list breaks its totals down by plugin with this.
	int count_for_plugin(int plugin_index) const;

	// Accumulated time per handler, for lua_profile. Filled only while the
	// cslua_profile cvar is on.
	struct ProfileRow
	{
		std::string plugin;
		std::string event;
		std::string id;
		double seconds;
		int calls;
	};

	void profile_snapshot(std::vector<ProfileRow> &out) const;
	void profile_reset();

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

	// A weapon is about to show its view/world model. view_model/world_model
	// come in holding what the game would use; a handler that changes the
	// field gets that path applied instead, in and out through the same refs.
	void fire_weapon_deploy(int id, const char *weapon,
		std::string &view_model, std::string &world_model);

	// Fires from CBasePlayerWeapon::DefaultReload/DefaultShotgunReload, but
	// only when they report back that a reload is actually starting - both
	// get called on every reload attempt regardless of whether one is
	// possible, and no-op internally when the clip is full or reserve ammo
	// is empty. clip is read before the call, so this is what was left when
	// the reload began, not what came out of it. delay is the animation
	// length the engine is about to run, in seconds from now - same fDelay
	// the game itself passed in, not recomputed.
	void fire_weapon_reload(int id, const char *weapon, int clip, float delay);

	void fire_round_start();
	void fire_round_end(int winner);
	void fire_round_freeze_end();

	// Bomb scenario. planter/defuser are player slots, 0 when unknown.
	void fire_bomb_planted(int planter);
	void fire_bomb_defused(int defuser, bool success);
	void fire_bomb_exploded(float x, float y, float z);

	// weapon is "hegrenade" or "smokegrenade". Fires before the engine creates
	// the projectile; fuse comes in holding the time the game intended to use
	// and a handler that changes it gets that value used instead - the only
	// way to make one explode on (near enough) contact instead of waiting out
	// a multi-second fuse, since neither grenade type has a touch hook.
	void fire_grenade_throw(int owner, const char *weapon, float &fuse);

	// A thrown grenade, right after the engine creates it. entity_index is 0
	// if the throw failed. Scripts get the object through e:model() and the
	// rest of ents rather than a bespoke setter, so reskinning it is exactly
	// like reskinning anything else made with ents.create.
	void fire_grenade_thrown(int owner, const char *weapon, int entity_index);

	// A grenade is about to explode. owner is 0 when the thrower is gone or
	// was never a player, entity_index is 0 if the entity is already gone.
	// Returns true if a handler cancelled: the caller must then skip the
	// game's own explosion (damage, decals, sound) and leave the whole effect
	// - including removing the entity - to Lua.
	bool fire_grenade_explode(int owner, const char *weapon, int entity_index, float x, float y, float z);

	// Fires before the engine decides which real grenade type (HE/flash/
	// smoke) to create and throw - the one dispatcher every grenade-slot
	// weapon's throw goes through, regardless of which weapon it actually
	// is. weapon is the classname of the item being thrown; x,y,z/vx,vy,vz
	// are the position and velocity, time the fuse/spawn delay, all as the
	// engine intended to use them.
	// Returns true if a handler cancelled: the caller must then skip the
	// engine's own throw entirely (return no CGrenade) and leave the whole
	// projectile - typically a hand-built ents.create() entity - up to Lua.
	// ammo_type is the item's m_iPrimaryAmmoType - normally a real ammo
	// index, but a script's p:give(..., { ammo_type = N }) can point it at
	// a spare one instead, which is what lets a handler tell "this throw is
	// my disguised item" apart from a real grenade of the same classname
	// (grenade_thrown/grenade_explode only know the thrown entity's
	// classname, which reads the same for both).
	bool fire_weapon_throw(int player, const char *weapon, int ammo_type,
		float x, float y, float z, float vx, float vy, float vz, float time);

	// The player pressed secondary fire (right-click) while holding this
	// weapon. Fires once per press, not once per frame held - there is no
	// ReGameDLL hookchain for SecondaryAttack (unlike PrimaryAttack, which
	// weapon_fire piggybacks on), so this is synthesized from a button
	// edge-detect in PlayerPreThink (dllapi.cpp). ammo_type is the same
	// m_iPrimaryAmmoType field weapon_throw carries, for telling apart two
	// items that share a classname (see its own comment).
	void fire_weapon_secondary_attack(int player, const char *weapon, int ammo_type);

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

		// Only touched while profiling is on, so an idle server pays nothing
		// for them beyond the two words.
		double spent;
		int calls;

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
