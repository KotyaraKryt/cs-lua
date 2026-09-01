#include "cslua.h"
#include "lua_timers.h"
#include "lua_engine.h"
#include "lua_natives.h"

#include <string>
#include <vector>

struct Timer
{
	int id;
	int ref;			// registry ref to the callback
	int plugin;			// index into LuaEngine::plugins()
	float next;			// server time of the next run
	float interval;		// 0 for a one-shot
	bool dead;

	// Set by timer.create(); empty for the anonymous after/every ones.
	std::string name;

	// Survives a changelevel. On by default: a timer set up in a plugin's body
	// is infrastructure. Turn it off for anything tied to the current map.
	bool persist;
};

static std::vector<Timer> s_timers;
static int s_next_id = 1;

// Set while running callbacks: a timer must not reshuffle the vector we walk.
static bool s_running = false;

// The clock as of the last frame. gpGlobals->time restarts from zero every map.
static float s_last_now = 0.0f;

static int add_timer(lua_State *L, float delay, float interval, bool persist)
{
	if (delay < 0.0f)
		delay = 0.0f;

	lua_pushvalue(L, 2);

	Timer t;
	t.id = s_next_id++;
	t.ref = luaL_ref(L, LUA_REGISTRYINDEX);
	t.plugin = g_lua.current_index();
	t.next = gpGlobals->time + delay;
	t.interval = interval;
	t.dead = false;
	t.persist = persist;

	s_timers.push_back(t);
	return t.id;
}

// persist defaults to true; an explicit `persist = false` drops it at the next
// changelevel.
static bool opt_persist(lua_State *L, int index)
{
	if (!lua_istable(L, index))
		return true;

	lua_getfield(L, index, "persist");
	bool value = lua_isnil(L, -1) ? true : lua_toboolean(L, -1) != 0;
	lua_pop(L, 1);
	return value;
}

// timer.after(seconds, fn[, opts]) -> id
static int l_after(lua_State *L)
{
	float delay = (float)luaL_checknumber(L, 1);
	luaL_checktype(L, 2, LUA_TFUNCTION);

	lua_pushinteger(L, add_timer(L, delay, 0.0f, opt_persist(L, 3)));
	return 1;
}

// timer.every(seconds, fn[, opts]) -> id
static int l_every(lua_State *L)
{
	float interval = (float)luaL_checknumber(L, 1);
	luaL_checktype(L, 2, LUA_TFUNCTION);

	if (interval <= 0.0f)
		return luaL_error(L, "timer.every: needs a positive interval, got %f", interval);

	lua_pushinteger(L, add_timer(L, interval, interval, opt_persist(L, 3)));
	return 1;
}

// timer.cancel(id) -> was it still running
static int l_cancel(lua_State *L)
{
	int id = (int)luaL_checkinteger(L, 1);

	for (size_t i = 0; i < s_timers.size(); i++) {
		if (s_timers[i].id != id || s_timers[i].dead)
			continue;
		s_timers[i].dead = true;
		lua_pushboolean(L, 1);
		return 1;
	}

	lua_pushboolean(L, 0);
	return 1;
}

static Timer *find_named(int plugin, const char *name)
{
	for (size_t i = 0; i < s_timers.size(); i++) {
		Timer &t = s_timers[i];
		if (!t.dead && t.plugin == plugin && t.name == name)
			return &t;
	}
	return NULL;
}

// timer.create(name, seconds, fn [, { once = true, persist = false }])
//
// Registering the same name twice replaces the timer instead of stacking one.
// The name only has to be unique within the plugin.
static int l_create(lua_State *L)
{
	const char *name = luaL_checkstring(L, 1);
	float seconds = (float)luaL_checknumber(L, 2);
	luaL_checktype(L, 3, LUA_TFUNCTION);

	bool once = false;
	if (lua_istable(L, 4)) {
		lua_getfield(L, 4, "once");
		once = lua_toboolean(L, -1) != 0;
		lua_pop(L, 1);
	}
	bool persist = opt_persist(L, 4);

	if (seconds <= 0.0f)
		return luaL_error(L, "timer.create: '%s' needs a positive interval, got %f",
			name, seconds);

	int plugin = g_lua.current_index();

	if (Timer *existing = find_named(plugin, name)) {
		luaL_unref(L, LUA_REGISTRYINDEX, existing->ref);
		lua_pushvalue(L, 3);
		existing->ref = luaL_ref(L, LUA_REGISTRYINDEX);
		existing->next = gpGlobals->time + seconds;
		existing->interval = once ? 0.0f : seconds;
		existing->persist = persist;
		return 0;
	}

	// add_timer reads the callback from stack slot 2.
	lua_pushvalue(L, 3);
	lua_replace(L, 2);
	int id = add_timer(L, seconds, once ? 0.0f : seconds, persist);

	for (size_t i = 0; i < s_timers.size(); i++) {
		if (s_timers[i].id == id) {
			s_timers[i].name = name;
			break;
		}
	}

	return 0;
}

// timer.destroy(name) -> was it running. Only your own plugin's.
static int l_destroy(lua_State *L)
{
	const char *name = luaL_checkstring(L, 1);

	Timer *t = find_named(g_lua.current_index(), name);
	if (t)
		t->dead = true;

	lua_pushboolean(L, t != NULL);
	return 1;
}

static const luaL_Reg s_timer[] =
{
	{ "after",   l_after },
	{ "every",   l_every },
	{ "cancel",  l_cancel },
	{ "create",  l_create },
	{ "destroy", l_destroy },
	{ NULL, NULL }
};

void cslua_register_timers(lua_State *L)
{
	cslua_register_namespace(L, "timer", s_timer);
}

void cslua_timers_clear()
{
	s_timers.clear();
	s_next_id = 1;
	s_last_now = 0.0f;
}

// A new map restarts the server clock. Carry the remaining wait over to the new
// clock so after(600, ...) still means "ten minutes from when it was set". A
// `persist = false` timer does not make the trip.
void cslua_timers_rebase()
{
	lua_State *L = g_lua.state();
	float now = gpGlobals->time;

	for (size_t i = s_timers.size(); i-- > 0; ) {
		if (!s_timers[i].persist) {
			if (L)
				luaL_unref(L, LUA_REGISTRYINDEX, s_timers[i].ref);
			s_timers.erase(s_timers.begin() + i);
			continue;
		}

		float left = s_timers[i].next - s_last_now;
		if (left < 0.0f)
			left = 0.0f;
		s_timers[i].next = now + left;
	}

	s_last_now = now;
}

void cslua_timers_remove_plugin(int plugin_index)
{
	lua_State *L = g_lua.state();

	for (size_t i = s_timers.size(); i-- > 0; ) {
		if (s_timers[i].plugin != plugin_index)
			continue;
		if (L)
			luaL_unref(L, LUA_REGISTRYINDEX, s_timers[i].ref);
		s_timers.erase(s_timers.begin() + i);
	}
}

int cslua_timers_count()
{
	int n = 0;
	for (size_t i = 0; i < s_timers.size(); i++)
		n += s_timers[i].dead ? 0 : 1;
	return n;
}

int cslua_timers_count_for_plugin(int plugin_index)
{
	int n = 0;
	for (size_t i = 0; i < s_timers.size(); i++)
		if (!s_timers[i].dead && s_timers[i].plugin == plugin_index)
			n++;
	return n;
}

static const char *timer_owner(int plugin_index)
{
	const std::vector<LuaPlugin> &plugins = g_lua.plugins();
	if (plugin_index < 0 || plugin_index >= (int)plugins.size())
		return "?";
	return plugins[plugin_index].id.c_str();
}

void cslua_timers_run()
{
	// Tracked every frame so a timer created later still has a sane baseline
	// for the next rebase.
	float now = gpGlobals->time;
	if (!s_running)
		s_last_now = now;

	if (s_timers.empty() || s_running)
		return;

	lua_State *L = g_lua.state();
	if (!L)
		return;

	s_running = true;

	// Index-based: a callback may append to s_timers.
	for (size_t i = 0; i < s_timers.size(); i++) {
		if (s_timers[i].dead || s_timers[i].next > now)
			continue;

		int ref = s_timers[i].ref;
		int plugin = s_timers[i].plugin;

		if (s_timers[i].interval > 0.0f)
			s_timers[i].next = now + s_timers[i].interval;
		else
			s_timers[i].dead = true;

		PluginScope scope(plugin);
		int errfunc = g_lua.push_errfunc();
		lua_rawgeti(L, LUA_REGISTRYINDEX, ref);

		if (lua_pcall(L, 0, 0, errfunc) != 0) {
			g_lua.report_error(timer_owner(plugin));
			// A repeating timer that throws every frame would flood the log.
			s_timers[i].dead = true;
		}

		lua_remove(L, errfunc);
	}

	s_running = false;

	for (size_t i = s_timers.size(); i-- > 0; ) {
		if (!s_timers[i].dead)
			continue;
		luaL_unref(L, LUA_REGISTRYINDEX, s_timers[i].ref);
		s_timers.erase(s_timers.begin() + i);
	}
}
