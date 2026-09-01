#pragma once

#include <lua.hpp>
#include <map>
#include <string>
#include <vector>
#include <functional>

// Prefixed because windows.h claims plain EVENT_* names.
//
// client_* is the connection layer (an edict, maybe no full CBasePlayer yet);
// player_* is a player already in the game.
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

	// Gameplay - ReGameDLL only.
	CSLUA_EVENT_PLAYER_SPAWN,
	CSLUA_EVENT_PLAYER_HURT,			// pre: change or block the damage
	CSLUA_EVENT_PLAYER_HURT_POST,		// post: damage already applied
	CSLUA_EVENT_PLAYER_DEATH,		// was player_killed
	CSLUA_EVENT_PLAYER_TEAM_CHANGE,
	CSLUA_EVENT_WEAPON_FIRE,
	CSLUA_EVENT_WEAPON_DEPLOY,		// view/world model can be swapped
	CSLUA_EVENT_WEAPON_RELOAD,		// a real reload just started
	CSLUA_EVENT_ROUND_START,
	CSLUA_EVENT_ROUND_END,
	CSLUA_EVENT_ROUND_FREEZE_END,
	CSLUA_EVENT_BOMB_PLANTED,
	CSLUA_EVENT_BOMB_DEFUSED,
	CSLUA_EVENT_BOMB_EXPLODED,
	CSLUA_EVENT_GRENADE_THROW,		// about to leave the hand; fuse can change
	CSLUA_EVENT_GRENADE_THROWN,		// just left the hand; entity for reskinning
	CSLUA_EVENT_GRENADE_EXPLODE,		// about to explode; cancel to take over
	CSLUA_EVENT_WEAPON_THROW,		// any grenade-slot throw; cancel to take over
	CSLUA_EVENT_WEAPON_SECONDARY_ATTACK,	// right-click, from a button edge-detect
	CSLUA_EVENT_WEAPON_BUY,			// cancel to block the purchase
	CSLUA_EVENT_AMMO_BUY,
	CSLUA_EVENT_ITEM_BUY,
	CSLUA_EVENT_MONEY_CHANGE,		// AddAccount; cancel to block it
	CSLUA_EVENT_AMMO_PICKUP,			// GiveAmmo; cancel to block it
	CSLUA_EVENT_WEAPON_DROP,			// cancel to block it
	CSLUA_EVENT_PLAYER_JUMP,
	CSLUA_EVENT_PLAYER_DUCK,
	CSLUA_EVENT_PLAYER_SPECTATE,
	CSLUA_EVENT_PLAYER_RADIO,		// cancel to block the sound/message
	CSLUA_EVENT_PLAYER_CAN_RESPAWN,	// pre: cancel to force "no"
	CSLUA_EVENT_BOMB_DEFUSE_START,
	CSLUA_EVENT_PLAYER_TRACE_ATTACK,	// pre: one raw hit before TakeDamage sums it
	CSLUA_EVENT_PLAYER_HEAL,			// pre: change or block healing
	CSLUA_EVENT_ROUND_BALANCE_TEAMS,
	CSLUA_EVENT_ROUND_INTERMISSION,
	CSLUA_EVENT_ITEM_GIVE,			// pre: GiveNamedItem; cancel to block it
	CSLUA_EVENT_PLAYER_STRIP,
	CSLUA_EVENT_PLAYER_CAN_HAVE_ITEM,	// pre: cancel to force "no"
	CSLUA_EVENT_WEAPON_PICKUP,		// picked a weapon up off the ground
	CSLUA_EVENT_PLAYER_DISAPPEAR,
	CSLUA_EVENT_PLAYER_CAN_SWITCH_TEAM,	// pre: cancel to force "no"
	CSLUA_EVENT_PLAYER_SHIELD_GIVE,
	CSLUA_EVENT_PLAYER_SHIELD_DROP,
	CSLUA_EVENT_BOMB_CARRIER,		// GiveC4 just picked who carries the bomb
	CSLUA_EVENT_ROUND_REMOVE_GUNS,
	CSLUA_EVENT_ROUND_DEAD_WEAPONS,	// which drop mode to use, read/write
	CSLUA_EVENT_PLAYER_OBSERVER_NEXT,
	CSLUA_EVENT_PLAYER_OBSERVER_MODE,
	CSLUA_EVENT_PLAYER_SCORE_ADD,		// pre: AddPoints; change or block the score
	CSLUA_EVENT_TEAM_SCORE_ADD,		// pre: AddPointsToTeam
	CSLUA_EVENT_ROUND_CLEANUP,
	CSLUA_EVENT_PLAYER_USERINFO_CHANGE,
	CSLUA_EVENT_PLAYER_CAN_HEAR,		// pre: cancel to force "no"
	CSLUA_EVENT_PLAYER_CHOOSE_MODEL,
	CSLUA_EVENT_PLAYER_CHOOSE_TEAM,	// pre: cancel to block the switch
	CSLUA_EVENT_PLAYER_USE,		// post: DispatchUse just ran
	CSLUA_EVENT_PLAYER_SUICIDE,		// pre: the "kill" console command
	CSLUA_EVENT_ENTS_SHOULD_COLLIDE,	// e.collide, default true, read back
	CSLUA_EVENT_ENTS_FREE,			// entity about to be destroyed
	CSLUA_EVENT_PLAYER_CVAR_VALUE,		// a client answered a cvar query
	CSLUA_EVENT_PLAYER_CVAR_VALUE2,	// same, with a request id and cvar name
	CSLUA_EVENT_SERVER_CVAR_CHANGE,	// pre: ReHLDS Cvar_DirectSet
	CSLUA_EVENT_SERVER_PRECACHE_GENERIC,
	CSLUA_EVENT_CLIENT_BOT_CREATED,
	CSLUA_EVENT_PLAYER_ROUND_RESPAWN,	// pre: cancel to skip it entirely
	CSLUA_EVENT_PLAYER_GIVE_DEFAULT_ITEMS,	// pre: cancel to skip it entirely

	CSLUA_EVENT_COUNT
};

struct RejectInfo
{
	char reason[128];
};

// One hook system for everything:
//   hook.add("player:hurt", "god.block", function(e) e:cancel() end)
//   hook.add("shop.bought", "stats.count", function(e) ... end)
//   local e = hook.run("shop.bought", { player = p, item = it })
//
// Engine events (the enum above) and plugin events share one registry. Every
// handler takes one argument, the event table, and its return value is never
// read - a handler changes the outcome by writing a field or calling e:cancel().
// A plugin event name must contain a dot.
class LuaEvents
{
public:
	// Fills the event's fields onto the table on top of the stack. Public so
	// run_custom() can take one from callers outside this class.
	typedef std::function<void(lua_State *)> FillFields;

	static int l_add(lua_State *L);
	static int l_remove(lua_State *L);
	static int l_run(lua_State *L);
	static int l_list(lua_State *L);

	// Builds the two event metatables. Must run before any dispatch.
	void init(lua_State *L);

	void clear();
	void remove_plugin(int plugin_index);
	int count(CsLuaEvent ev) const;
	int count_for_plugin(int plugin_index) const;

	// Accumulated time per handler, for lua_profile.
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

	// True when any script listens for the event; lets the API skip installing
	// a hookchain nobody uses.
	bool any(CsLuaEvent ev) const;

	// Same, for a custom-named event ("msg:TextMsg" etc). Checked on every
	// network message, so it stays a cheap map lookup with no table built.
	bool any_custom(const std::string &name) const;

	// Fires a custom-named event. Unlike hook.run, this is for events the
	// engine sourced, so no dot is required in the name.
	bool run_custom(const std::string &name, const FillFields &fill);

	// Returns true if a handler rejected the connection; reason is filled in.
	bool fire_client_connect(int id, const char *name, const char *ip, RejectInfo &reject);

	// forced is true for the synthesized disconnect from a ClientConnect
	// rejection rather than a real engine ClientDisconnect.
	void fire_client_disconnect(int id, const char *name, const char *reason, bool forced);

	// Fires the moment Steam stops answering STEAM_ID_PENDING. Anything keyed
	// on the steamid belongs here rather than in client_connect.
	void fire_player_authorized(int id, const char *authid);

	void fire_player_ready(int id);

	// Returns true if a handler swallowed the message (so "!command" input
	// stays invisible).
	bool fire_player_chat(int id, const char *text, bool team);

	void fire_menu_select(int id, int key);
	void fire_map_change(const char *map);

	// e.plugin names which one for a single-plugin reload, nil for a full
	// teardown.
	void fire_plugin_unload(const char *only_id = NULL);

	void fire_player_spawn(int id);

	// weapon/distance are NULL/negative when the world did the killing.
	void fire_player_death(int victim, int killer, int headshot,
		const char *weapon, float distance);

	// old/new are team names ("CT", "T", "SPEC", "NONE").
	void fire_player_team_change(int id, const char *old_team, const char *new_team);

	// Firearms only.
	void fire_weapon_fire(int id, const char *weapon, int clip);

	// view_model/world_model come in holding what the game would use; a handler
	// that changes the field gets that path applied instead.
	void fire_weapon_deploy(int id, const char *weapon,
		std::string &view_model, std::string &world_model);

	// Fires only when DefaultReload/DefaultShotgunReload report a real reload
	// starting. clip is read before the call; delay is the animation length;
	// max_clip is GetItemInfo's iMaxClip, -1 if unreadable.
	void fire_weapon_reload(int id, const char *weapon, int clip, float delay, int max_clip);

	void fire_round_start();
	void fire_round_end(int winner);
	void fire_round_freeze_end();

	// planter/defuser are player slots, 0 when unknown.
	void fire_bomb_planted(int planter);
	void fire_bomb_defused(int defuser, bool success);
	void fire_bomb_exploded(float x, float y, float z);

	// fuse comes in holding the intended time; a handler that changes it gets
	// that value used instead.
	void fire_grenade_throw(int owner, const char *weapon, float &fuse);

	// entity_index is 0 if the throw failed.
	void fire_grenade_thrown(int owner, const char *weapon, int entity_index);

	// Returns true if a handler cancelled: the caller then skips the game's own
	// explosion and leaves the whole effect (entity removal included) to Lua.
	bool fire_grenade_explode(int owner, const char *weapon, int entity_index, float x, float y, float z);

	// Fires before the engine picks HE/flash/smoke. ammo_type is m_iPrimaryAmmoType,
	// which a p:give(..., { ammo_type = N }) can point at a spare index to tell
	// a disguised item apart from a real grenade of the same classname.
	// Returns true if cancelled: skip the engine's throw entirely.
	bool fire_weapon_throw(int player, const char *weapon, int ammo_type,
		float x, float y, float z, float vx, float vy, float vz, float time);

	void fire_weapon_secondary_attack(int player, const char *weapon, int ammo_type);

	// weapon is the classname GetItemInfo() had for the id. Returns true if
	// cancelled: skip the chain, nothing charged, nothing given.
	bool fire_weapon_buy(int player, const char *weapon);
	bool fire_ammo_buy(int player, const char *weapon);

	// item is one of "vest", "vesthelm", "flashbang", "hegrenade",
	// "smokegrenade", "nvg", "defusekit", "shield".
	bool fire_item_buy(int player, const char *item);

	// amount can be negative. Cancel to block; the caller then skips AddAccount.
	bool fire_money_change(int player, int amount, const char *reason);

	// Cancel -> the caller returns -1 (GiveAmmo's own "failed" value).
	bool fire_ammo_pickup(int player, const char *weapon, int count, int max);

	bool fire_weapon_drop(int player, const char *weapon);

	// Notify only - blocking mid-flight would leave movement in a bad state.
	void fire_player_jump(int player);
	void fire_player_duck(int player);

	void fire_player_spectate(int player);

	// Cancel to block the sound and the console line.
	bool fire_player_radio(int player, const char *sentence, const char *sample);

	// Cancel to force "no". Cannot grant a "yes" the game would not have given.
	bool fire_player_can_respawn(int player);

	// defuser: using a defuse kit (faster) or bare hands.
	void fire_bomb_defuse_start(int player, bool defuser);

	// damage is by reference in TakeDamage. Returns the final damage: e.damage
	// after the chain, or 0 if a handler cancelled. hitgroup is -1 when the
	// damage did not come from a hit (fall, world, gas).
	float fire_player_hurt(int victim, int attacker, float damage, int bits, int hitgroup);

	// Post: the damage the game actually applied, for observers.
	void fire_player_hurt_post(int victim, int attacker, float damage, int bits, int hitgroup);

	// The exact hit before TakeDamage sums same-frame hits. damage is the raw
	// per-hit amount before armor/multipliers. x,y,z is the trace position.
	// Returns the (possibly changed) damage, 0 if cancelled.
	float fire_player_trace_attack(int victim, int attacker, float damage, int bits,
		int hitgroup, float x, float y, float z);

	// TakeHealth. Returns the (possibly changed) amount, 0 if cancelled.
	float fire_player_heal(int player, float amount, int bits);

	void fire_round_balance_teams();

	// Earlier than server:map_change - the world is still there.
	void fire_round_intermission();

	// Cancel -> the caller returns NULL, nothing created.
	bool fire_item_give(int player, const char *item);

	void fire_player_strip(int player, bool remove_suit);

	// Cancel to force "no"; cannot grant a "yes".
	bool fire_player_can_have_item(int player, const char *item);

	void fire_weapon_pickup(int player, const char *weapon);
	void fire_player_disappear(int player);

	// Cancel to force "no"; cannot grant a "yes".
	bool fire_player_can_switch_team(int player, const char *team);

	// deploy: drawn immediately.
	void fire_player_shield_give(int player, bool deploy);
	void fire_player_shield_drop(int player, bool deploy);

	// player is 0 if the game did not choose anyone. Notify only.
	void fire_bomb_carrier(int player);

	void fire_round_remove_guns();

	// mode is one of gamerules.h's GR_PLR_DROP_GUN_*. Returns the (possibly
	// changed) mode. Not cancellable.
	int fire_round_dead_weapons(int player, int mode);

	// target is a specific name to jump to, NULL/empty when none.
	void fire_player_observer_next(int player, bool reverse, const char *target);

	// mode is one of pm_shared.h's OBS_*.
	void fire_player_observer_mode(int player, int mode);

	// score/allow_negative come in holding what the game intended and are
	// written back from e.score/e.allow_negative. Returns true if cancelled:
	// the caller skips AddPoints entirely.
	bool fire_player_score_add(int player, int &score, bool &allow_negative);

	// player is just who triggered the call; read e.player:team() for whose.
	bool fire_team_score_add(int player, int &score, bool &allow_negative);

	void fire_round_cleanup();

	// Read a specific key with p:info(key); the raw buffer never reaches Lua.
	void fire_player_userinfo_change(int player);

	// Cancel to force "no"; cannot grant a "yes".
	bool fire_player_can_hear(int listener, int speaker);

	// slot is the menu item picked, not a model name.
	void fire_player_choose_model(int player, int slot);

	// slot is the menu item, not a TeamName. Cancel to block the switch.
	bool fire_player_choose_team(int player, int slot);

	// entity is 0 if it is already gone by the time this fires. Notify only.
	void fire_player_use(int player, int entity);

	// The "kill" console command. Cancel -> nothing happens, not even the
	// rate-limit timer.
	bool fire_player_suicide(int player);

	// pfnShouldCollide - the only implementation. collide comes in true and
	// goes back out however e.collide was left; only an explicit false makes
	// two colliding entities skip touching.
	bool fire_ents_should_collide(int entity, int other, bool collide);

	// Fires before ReGameDLL's cleanup - the last point the entity is readable.
	// Notify only.
	void fire_ents_free(int entity);

	// The context-free QueryClientCvarValue answer (no name, no request id).
	void fire_player_cvar_value(int player, const char *value);

	// The QueryClientCvarValue2 answer: request_id + cvar name.
	void fire_player_cvar_value2(int player, int request_id, const char *cvar, const char *value);

	// ReHLDS Cvar_DirectSet. Cancel to keep the old value. A handler that sets
	// a cvar from inside this event re-enters it - do not set the same cvar
	// unconditionally.
	bool fire_server_cvar_change(const char *name, const char *value);

	void fire_server_precache_generic(const char *name, int index);
	void fire_client_bot_created(int id);

	// RoundRespawn. Cancel -> the player is left exactly as they were.
	bool fire_player_round_respawn(int player);

	// GiveDefaultItems starts by stripping the current inventory, so cancelling
	// leaves it untouched rather than being cleared first.
	bool fire_player_give_default_items(int player);

private:
	struct Handler
	{
		int ref;			// registry ref to the Lua function
		int plugin;			// index into LuaEngine::plugins()
		std::string id;		// unique within one plugin, for replace/remove

		// Only touched while profiling is on.
		double spent;
		int calls;

		// A plugin can be shut down from inside its own handler; handlers are
		// marked dead and swept once the walk is over.
		bool dead;
	};

	typedef std::vector<Handler> HandlerList;

	// Runs with the finished event table on top, for events the game reads back.
	typedef std::function<void(lua_State *)> ReadFields;

	const char *plugin_id(int plugin_index) const;

	// Drops the handlers marked dead. No-op while a dispatch is in progress.
	void sweep();

	void push_event(lua_State *L, const char *name, bool cancellable, const FillFields &fill);

	// Walks one handler list against the event table at `event_index`. Stops
	// early once a handler has cancelled.
	void dispatch_list(lua_State *L, HandlerList &list, int event_index);

	void notify(CsLuaEvent ev, const FillFields &fill = FillFields());

	// `read` runs with the event table on top, before it is dropped. Returns
	// true when a handler cancelled.
	bool run(CsLuaEvent ev, const FillFields &fill, const ReadFields &read = ReadFields());

	// Engine events indexed by the enum; plugin events keyed by name.
	HandlerList m_handlers[CSLUA_EVENT_COUNT];
	std::map<std::string, HandlerList> m_custom;

	// Depth rather than a flag: one handler firing another event nests.
	int m_dispatching = 0;

	int m_mt_cancellable = LUA_NOREF;
	int m_mt_notify = LUA_NOREF;
};

extern LuaEvents g_events;

// Registers the `hook` namespace. Separate from init() because the metatables
// have to exist before the first dispatch.
void cslua_register_hooks(lua_State *L);
