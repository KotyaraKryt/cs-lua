#include "cslua.h"
#include "lua_sound.h"
#include "lua_engine.h"
#include "lua_natives.h"
#include "lua_events.h"
#include "lua_timers.h"
#include "rehlds.h"
#include "players.h"

#include <rehlds_api.h>

#include <string.h>
#include <string>
#include <vector>

// GoldSrc indexes precached resources with 9 bits, so 512 each for sounds and
// models. Going over does not fail gracefully - the engine throws Host_Error
// and the server dies. That is the classic AMXX footgun: nobody knows how much
// room is left until the server drops.
//
// PRECACHE_* returns the slot it used, which is effectively the live count for
// the whole server (the game DLL and other plugins included). We watch that
// number and refuse before it can kill anyone, naming the plugin that asked.
static const int PRECACHE_LIMIT = 512;

// Left free for the game and other plugins that precache after us.
static const int PRECACHE_RESERVE = 24;

struct Entry
{
	std::string name;
	int plugin;			// who asked, so we can disable the right one
};

static std::vector<Entry> s_sounds;
static std::vector<Entry> s_models;

// Highest slot we have seen, per table. -1 until the first precache.
static int s_sound_used = -1;
static int s_model_used = -1;

static bool s_window_open = false;

// True once a map has finished loading. Before that, a closed window just
// means "the server is still starting" - the registry gets replayed at
// worldspawn, so there is nothing to warn about.
static bool s_map_seen = false;

static bool contains(const std::vector<Entry> &list, const char *name)
{
	for (size_t i = 0; i < list.size(); i++)
		if (list[i].name == name)
			return true;
	return false;
}

// With ReHLDS we see every precache the server makes - the game DLL's, other
// plugins', ours - so the counters below are the real thing and not a guess
// frozen at the moment we happened to look.
static int hook_precache_sound(IRehldsHook_PF_precache_sound_I *chain, const char *name)
{
	int slot = chain->callNext(name);
	if (slot > s_sound_used)
		s_sound_used = slot;
	return slot;
}

static int hook_precache_model(IRehldsHook_PF_precache_model_I *chain, const char *name)
{
	int slot = chain->callNext(name);
	if (slot > s_model_used)
		s_model_used = slot;
	return slot;
}

void cslua_sound_install_hooks()
{
	IRehldsHookchains *hooks = cslua_rehlds_hooks();
	if (!hooks)
		return;

	hooks->PF_precache_sound_I()->registerHook(&hook_precache_sound);
	hooks->PF_precache_model_I()->registerHook(&hook_precache_model);
}

void cslua_sound_set_window(bool open)
{
	s_window_open = open;
	if (!open)
		s_map_seen = true;
}

void cslua_sound_clear()
{
	s_sounds.clear();
	s_models.clear();
}

int cslua_precache_used(bool models)
{
	return models ? s_model_used : s_sound_used;
}

int cslua_precache_limit()
{
	return PRECACHE_LIMIT;
}

int cslua_precache_registered(bool models)
{
	return (int)(models ? s_models.size() : s_sounds.size());
}

// Shuts a plugin down without taking the server with it: its handlers, timers
// and remaining resources stop mattering, everything else keeps running.
static void disable_plugin(int index, const char *why, const char *what)
{
	const std::vector<LuaPlugin> &plugins = g_lua.plugins();
	const char *id = (index >= 0 && index < (int)plugins.size())
		? plugins[index].id.c_str() : "?";

	cslua_error("precache limit: %s while '%s' asked for '%s'", why, id, what);
	cslua_error("  disabling plugin '%s' - the server keeps running", id);

	g_events.remove_plugin(index);
	cslua_timers_remove_plugin(index);
	g_lua.mark_failed(index);
}

// Returns false when the table is too full to take another entry.
static bool has_room(int used)
{
	return used < 0 || used < PRECACHE_LIMIT - PRECACHE_RESERVE;
}

void cslua_precache_all()
{
	int blocked = -1;		// plugin already disabled during this pass

	for (size_t i = 0; i < s_sounds.size(); i++) {
		if (s_sounds[i].plugin == blocked)
			continue;

		if (!has_room(s_sound_used)) {
			blocked = s_sounds[i].plugin;
			disable_plugin(blocked, "sound table is full", s_sounds[i].name.c_str());
			continue;
		}

		int slot = PRECACHE_SOUND(s_sounds[i].name.c_str());
		if (slot > s_sound_used)
			s_sound_used = slot;
	}

	blocked = -1;
	for (size_t i = 0; i < s_models.size(); i++) {
		if (s_models[i].plugin == blocked)
			continue;

		if (!has_room(s_model_used)) {
			blocked = s_models[i].plugin;
			disable_plugin(blocked, "model table is full", s_models[i].name.c_str());
			continue;
		}

		int slot = PRECACHE_MODEL(s_models[i].name.c_str());
		if (slot > s_model_used)
			s_model_used = slot;
	}

	if (s_sound_used >= 0 || s_model_used >= 0)
		cslua_print("precache: sounds %d/%d, models %d/%d (%d+%d ours)",
			s_sound_used + 1, PRECACHE_LIMIT, s_model_used + 1, PRECACHE_LIMIT,
			(int)s_sounds.size(), (int)s_models.size());
}

static int add_resource(lua_State *L, std::vector<Entry> &list, bool models)
{
	const char *name = luaL_checkstring(L, 1);

	// PRECACHE_SOUND/PRECACHE_MODEL do not fail gracefully on an empty name -
	// Host_Error takes the whole process down, not just the offending plugin.
	// A config with a stray "" (nil is the correct "nothing here", but a
	// blank string is an easy typo) must not be able to do that - catch it
	// here as an ordinary Lua error, same as any other bad argument.
	if (name[0] == '\0') {
		return luaL_error(L, models ? "res.model: empty name" : "res.sound: empty name");
	}

	if (!contains(list, name)) {
		Entry e;
		e.name = name;
		e.plugin = g_lua.current_index();
		list.push_back(e);
	}

	if (!s_window_open) {
		// Only worth a word if a map is already up (a reload mid-game): the
		// engine won't take it now, it goes in on the next map. During startup
		// this is the normal path and needs no warning.
		if (s_map_seen)
			cslua_print("%s('%s') arrived mid-map; it will be precached on the next map",
				models ? "res.model" : "res.sound", name);
		return 0;
	}

	int used = models ? s_model_used : s_sound_used;
	if (!has_room(used)) {
		disable_plugin(g_lua.current_index(),
			models ? "model table is full" : "sound table is full", name);
		return luaL_error(L, "precache limit reached (%d/%d)", used + 1, PRECACHE_LIMIT);
	}

	int slot = models ? PRECACHE_MODEL(name) : PRECACHE_SOUND(name);
	if (models) {
		if (slot > s_model_used) s_model_used = slot;
	} else {
		if (slot > s_sound_used) s_sound_used = slot;
	}
	return 0;
}

static int l_precache_sound(lua_State *L)
{
	return add_resource(L, s_sounds, false);
}

static int l_precache_model(lua_State *L)
{
	return add_resource(L, s_models, true);
}

void cslua_play_sound(int id, const char *sample, int channel, float volume,
	float attenuation, int pitch)
{
	if (id > 0) {
		edict_t *e = g_engfuncs.pfnPEntityOfEntIndex(id);
		if (e && !e->free)
			EMIT_SOUND_DYN2(e, channel, sample, volume, attenuation, 0, pitch);
		return;
	}

	// id 0: EMIT_SOUND's recipients are whoever is in PAS of the source
	// entity, not "the players we name" - there is no per-listener send
	// through this call. Emitting once from any connected player is enough:
	// this is always called with attenuation 0 (no distance falloff) by
	// every caller in the tree, so PAS reach from one live player already
	// covers everyone else standing anywhere near the map's playable space.
	// Looping and emitting from every player in turn - the previous
	// behaviour - does not extend that reach; it just means a listener
	// within PAS of more than one source hears the same clip stacked once
	// per source it is in range of.
	for (int slot = 1; slot < CSLUA_MAXPLAYERS; slot++) {
		if (!g_players.is_connected(slot))
			continue;
		edict_t *e = g_engfuncs.pfnPEntityOfEntIndex(slot);
		if (e && !e->free) {
			EMIT_SOUND_DYN2(e, channel, sample, volume, attenuation, 0, pitch);
			return;
		}
	}
}

// res - map resources. Declared once at the top of a plugin; the engine
// replays the registry on every map, so there is no precache timing to get
// wrong.
static const luaL_Reg s_res[] =
{
	{ "sound", l_precache_sound },
	{ "model", l_precache_model },
	{ NULL, NULL }
};

void cslua_register_sound(lua_State *L)
{
	cslua_register_namespace(L, "res", s_res);
}
