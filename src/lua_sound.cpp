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

// GoldSrc indexes precached resources with 9 bits: 512 each for sounds and
// models. Going over throws Host_Error and the server dies. PRECACHE_* returns
// the slot it used, which is the live count for the whole server. We watch it
// and refuse before it can kill anyone.
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

// Highest slot seen, per table. -1 until the first precache.
static int s_sound_used = -1;
static int s_model_used = -1;

static bool s_window_open = false;

// True once a map has finished loading. Before that a closed window just means
// the server is still starting - the registry gets replayed at worldspawn.
static bool s_map_seen = false;

static bool contains(const std::vector<Entry> &list, const char *name)
{
	for (size_t i = 0; i < list.size(); i++)
		if (list[i].name == name)
			return true;
	return false;
}

// With ReHLDS we see every precache the server makes, so the counters are real.
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

// No overflow accounting: nothing here calls precache_generic, so there is no
// budget of our own to protect. Just the notify.
static int hook_precache_generic(IRehldsHook_PF_precache_generic_I *chain, const char *name)
{
	int index = chain->callNext(name);
	g_events.fire_server_precache_generic(name, index);
	return index;
}

void cslua_sound_install_hooks()
{
	IRehldsHookchains *hooks = cslua_rehlds_hooks();
	if (!hooks)
		return;

	hooks->PF_precache_sound_I()->registerHook(&hook_precache_sound);
	hooks->PF_precache_model_I()->registerHook(&hook_precache_model);
	hooks->PF_precache_generic_I()->registerHook(&hook_precache_generic);
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

// Shuts a plugin down without taking the server with it.
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

	// PRECACHE_* Host_Errors on an empty name - the whole process. Catch it.
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
		if (s_map_seen)
			cslua_print("%s('%s') arrived mid-map; it will be precached on the next map",
				models ? "res.model" : "res.sound", name);
		return 0;
	}

	int used = models ? s_model_used : s_sound_used;
	if (!has_room(used)) {
		disable_plugin(g_lua.current_index(),
			models ? "model table is full" : "sound table is full", name);
		return luaL_error(L, "res: precache limit reached (%d/%d)", used + 1, PRECACHE_LIMIT);
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

	// id 0 (broadcast): EMIT_SOUND reaches whoever is in PAS of the source, and
	// attenuation may not be 0, so emit from every connected player in turn.
	for (int slot = 1; slot < CSLUA_MAXPLAYERS; slot++) {
		if (!g_players.is_connected(slot))
			continue;
		edict_t *e = g_engfuncs.pfnPEntityOfEntIndex(slot);
		if (e && !e->free)
			EMIT_SOUND_DYN2(e, channel, sample, volume, attenuation, 0, pitch);
	}
}

void cslua_play_sound_private(int id, const char *sample)
{
	if (strlen(sample) > 120)
		return;

	if (id == 0) {
		for (int slot = 1; slot < CSLUA_MAXPLAYERS; slot++) {
			if (!g_players.is_connected(slot))
				continue;

			edict_t *e = g_engfuncs.pfnPEntityOfEntIndex(slot);
			if (!e || e->free)
				continue;

			CLIENT_COMMAND(e, "spk %s\n", sample);
		}

		return;
	}

	if (!g_players.is_connected(id))
		return;

	edict_t *e = g_engfuncs.pfnPEntityOfEntIndex(id);
	if (!e || e->free)
		return;

	CLIENT_COMMAND(e, "spk %s\n", sample);
}

// res - map resources. Declared once at the top of a plugin; the engine
// replays the registry on every map.
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
