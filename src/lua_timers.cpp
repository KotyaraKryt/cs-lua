#include "cslua.h"
#include "lua_timers.h"
#include "lua_engine.h"

#include <vector>

struct Timer
{
	int id;
	int ref;			// registry ref to the callback
	int plugin;			// index into LuaEngine::plugins()
	float next;			// server time of the next run
	float interval;		// 0 for a one-shot
	bool dead;
};

static std::vector<Timer> s_timers;
static int s_next_id = 1;

// Set while running callbacks: a timer that starts or cancels another timer
// must not reshuffle the vector we are walking.
static bool s_running = false;

// The clock as of the last frame we saw. gpGlobals->time restarts from zero on
// every map, and the Lua state outlives a map change, so this is what tells
// "the server clock went backwards" apart from "time passed".
static float s_last_now = 0.0f;

static int add_timer(lua_State *L, float delay, float interval)
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

	s_timers.push_back(t);
	return t.id;
}

int cslua_l_after(lua_State *L)
{
	float delay = (float)luaL_checknumber(L, 1);
	luaL_checktype(L, 2, LUA_TFUNCTION);

	lua_pushinteger(L, add_timer(L, delay, 0.0f));
	return 1;
}

int cslua_l_every(lua_State *L)
{
	float interval = (float)luaL_checknumber(L, 1);
	luaL_checktype(L, 2, LUA_TFUNCTION);

	if (interval <= 0.0f)
		return luaL_error(L, "every() needs a positive interval, got %f", interval);

	lua_pushinteger(L, add_timer(L, interval, interval));
	return 1;
}

int cslua_l_cancel(lua_State *L)
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

void cslua_timers_clear()
{
	// The refs die with the state; just forget them.
	s_timers.clear();
	s_next_id = 1;
	s_last_now = 0.0f;
}

// A new map restarts the server clock, which would leave every pending timer
// with a deadline far in the future's past - they would all fire at once on the
// first frame. Carry the remaining wait over to the new clock instead, so
// after(600, ...) still means "ten minutes from when it was set".
void cslua_timers_rebase()
{
	float now = gpGlobals->time;

	for (size_t i = 0; i < s_timers.size(); i++) {
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

static const char *timer_owner(int plugin_index)
{
	const std::vector<LuaPlugin> &plugins = g_lua.plugins();
	if (plugin_index < 0 || plugin_index >= (int)plugins.size())
		return "?";
	return plugins[plugin_index].id.c_str();
}

void cslua_timers_run()
{
	// Tracked every frame, not only when there is something to run: a timer
	// created later still needs a sane baseline for the next rebase.
	float now = gpGlobals->time;
	if (!s_running)
		s_last_now = now;

	if (s_timers.empty() || s_running)
		return;

	lua_State *L = g_lua.state();
	if (!L)
		return;

	s_running = true;

	// Index-based: a callback may append to s_timers and invalidate iterators.
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
