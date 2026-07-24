#include "cslua.h"
#include "lua_events.h"
#include "lua_engine.h"
#include "lua_player.h"

#include <string.h>
#include <functional>

LuaEvents g_events;

// Order must match the CsLuaEvent enum exactly.
static const char *const s_event_names[CSLUA_EVENT_COUNT] =
{
	"client_connect",
	"client_disconnect",
	"player_authorized",
	"player_ready",
	"player_chat",
	"menu_select",
	"player_spawn",
	"player_hurt",
	"player_hurt_post",
	"player_death",
	"round_start",
	"round_end",
	"round_freeze_end",
	"bomb_planted",
	"bomb_defused",
	"bomb_exploded",
};

int LuaEvents::l_on(lua_State *L)
{
	const char *name = luaL_checkstring(L, 1);
	luaL_checktype(L, 2, LUA_TFUNCTION);

	int ev = -1;
	for (int i = 0; i < CSLUA_EVENT_COUNT; i++) {
		if (!strcmp(name, s_event_names[i])) {
			ev = i;
			break;
		}
	}

	if (ev < 0) {
		std::string known;
		for (int i = 0; i < CSLUA_EVENT_COUNT; i++) {
			if (i) known += ", ";
			known += s_event_names[i];
		}
		return luaL_error(L, "unknown event '%s' (known events: %s)", name, known.c_str());
	}

	Handler h;
	lua_pushvalue(L, 2);
	h.ref = luaL_ref(L, LUA_REGISTRYINDEX);
	h.plugin = g_lua.current_index();
	h.dead = false;
	g_events.m_handlers[ev].push_back(h);
	return 0;
}

void LuaEvents::clear()
{
	// Called right before lua_close(), so the refs die with the state.
	for (int i = 0; i < CSLUA_EVENT_COUNT; i++)
		m_handlers[i].clear();
	m_dispatching = 0;
}

int LuaEvents::count(CsLuaEvent ev) const
{
	int n = 0;
	for (size_t i = 0; i < m_handlers[ev].size(); i++)
		n += m_handlers[ev][i].dead ? 0 : 1;
	return n;
}

bool LuaEvents::any(CsLuaEvent ev) const
{
	return count(ev) > 0;
}

// Marks a plugin's handlers dead and drops their refs. Safe to call from
// inside a handler: nothing is erased until the walk unwinds.
void LuaEvents::remove_plugin(int plugin_index)
{
	lua_State *L = g_lua.state();

	for (int i = 0; i < CSLUA_EVENT_COUNT; i++) {
		std::vector<Handler> &list = m_handlers[i];
		for (size_t j = 0; j < list.size(); j++) {
			if (list[j].plugin != plugin_index || list[j].dead)
				continue;
			if (L)
				luaL_unref(L, LUA_REGISTRYINDEX, list[j].ref);
			list[j].ref = LUA_NOREF;
			list[j].dead = true;
		}
	}

	sweep();
}

void LuaEvents::sweep()
{
	if (m_dispatching > 0)
		return;

	for (int i = 0; i < CSLUA_EVENT_COUNT; i++) {
		std::vector<Handler> &list = m_handlers[i];
		for (size_t j = list.size(); j-- > 0; )
			if (list[j].dead)
				list.erase(list.begin() + j);
	}
}

// Holds the dispatch depth for the length of one walk, and sweeps on the way
// out of the outermost one.
namespace {
struct DispatchScope
{
	explicit DispatchScope(int &depth) : m_depth(depth) { m_depth++; }
	~DispatchScope() { m_depth--; }

private:
	int &m_depth;
};
}

const char *LuaEvents::plugin_id(int plugin_index) const
{
	// -1 is the core layer, which registers handlers outside any plugin.
	if (plugin_index == -1)
		return "core";

	const std::vector<LuaPlugin> &plugins = g_lua.plugins();
	if (plugin_index < 0 || plugin_index >= (int)plugins.size())
		return "?";
	return plugins[plugin_index].id.c_str();
}

// Notify-style events all share the same loop: push args, pcall with no
// return, log an error and keep going. Only the argument list differs, so it
// comes in as a callback. Events that inspect a return value (connect, chat,
// hurt) stay hand-written below.
void LuaEvents::dispatch(CsLuaEvent ev, int nargs, const PushArgs &push)
{
	lua_State *L = g_lua.state();
	if (!L)
		return;

	{
		DispatchScope walking(m_dispatching);

		std::vector<Handler> &list = m_handlers[ev];
		for (size_t i = 0; i < list.size(); i++) {
			if (list[i].dead)
				continue;

			PluginScope scope(list[i].plugin);
			int errfunc = g_lua.push_errfunc();

			lua_rawgeti(L, LUA_REGISTRYINDEX, list[i].ref);
			push(L);

			if (lua_pcall(L, nargs, 0, errfunc) != 0)
				g_lua.report_error(plugin_id(list[i].plugin));

			lua_remove(L, errfunc);
		}
	}

	sweep();
}

bool LuaEvents::fire_client_connect(int id, const char *name, const char *ip, RejectInfo &reject)
{
	lua_State *L = g_lua.state();
	if (!L)
		return false;

	bool rejected = false;

	{
		DispatchScope walking(m_dispatching);

		std::vector<Handler> &list = m_handlers[CSLUA_EVENT_CLIENT_CONNECT];
		for (size_t i = 0; i < list.size() && !rejected; i++) {
			if (list[i].dead)
				continue;

			PluginScope scope(list[i].plugin);
			int errfunc = g_lua.push_errfunc();

			lua_rawgeti(L, LUA_REGISTRYINDEX, list[i].ref);
			cslua_push_player(L, id);
			lua_pushstring(L, name ? name : "");
			lua_pushstring(L, ip ? ip : "");

			if (lua_pcall(L, 3, 2, errfunc) != 0) {
				g_lua.report_error(plugin_id(list[i].plugin));
				lua_remove(L, errfunc);
				continue;
			}

			// Only an explicit false blocks; nil/no return means "don't care".
			bool blocked = lua_isboolean(L, -2) && !lua_toboolean(L, -2);
			if (blocked) {
				const char *why = lua_isstring(L, -1) ? lua_tostring(L, -1) : NULL;
				cslua_snprintf(reject.reason, sizeof reject.reason, "%s",
					why ? why : "Rejected by a Lua plugin");
				reject.reason[sizeof reject.reason - 1] = '\0';
				rejected = true;
			}

			lua_pop(L, 2);
			lua_remove(L, errfunc);
		}
	}

	sweep();
	return rejected;
}

void LuaEvents::fire_client_disconnect(int id, const char *name)
{
	std::string nm = name ? name : "";
	dispatch(CSLUA_EVENT_CLIENT_DISCONNECT, 2, [=](lua_State *L) {
		cslua_push_player(L, id);
		lua_pushstring(L, nm.c_str());
	});
}

void LuaEvents::fire_player_authorized(int id, const char *authid)
{
	std::string sid = authid ? authid : "";
	dispatch(CSLUA_EVENT_PLAYER_AUTHORIZED, 2, [=](lua_State *L) {
		cslua_push_player(L, id);
		lua_pushstring(L, sid.c_str());
	});
}

void LuaEvents::fire_player_ready(int id)
{
	dispatch(CSLUA_EVENT_PLAYER_READY, 1, [=](lua_State *L) {
		cslua_push_player(L, id);
	});
}

bool LuaEvents::fire_player_chat(int id, const char *text, bool team)
{
	lua_State *L = g_lua.state();
	if (!L)
		return false;

	bool swallowed = false;

	{
		DispatchScope walking(m_dispatching);

		std::vector<Handler> &list = m_handlers[CSLUA_EVENT_PLAYER_CHAT];
		for (size_t i = 0; i < list.size() && !swallowed; i++) {
			if (list[i].dead)
				continue;

			PluginScope scope(list[i].plugin);
			int errfunc = g_lua.push_errfunc();

			lua_rawgeti(L, LUA_REGISTRYINDEX, list[i].ref);
			cslua_push_player(L, id);
			lua_pushstring(L, text ? text : "");
			lua_pushboolean(L, team);

			if (lua_pcall(L, 3, 1, errfunc) != 0) {
				g_lua.report_error(plugin_id(list[i].plugin));
				lua_remove(L, errfunc);
				continue;
			}

			swallowed = lua_isboolean(L, -1) && !lua_toboolean(L, -1);
			lua_pop(L, 1);
			lua_remove(L, errfunc);
		}
	}

	sweep();
	return swallowed;
}

void LuaEvents::fire_menu_select(int id, int key)
{
	dispatch(CSLUA_EVENT_MENU_SELECT, 2, [=](lua_State *L) {
		cslua_push_player(L, id);
		lua_pushinteger(L, key);
	});
}

void LuaEvents::fire_player_spawn(int id)
{
	dispatch(CSLUA_EVENT_PLAYER_SPAWN, 1, [=](lua_State *L) {
		cslua_push_player(L, id);
	});
}

float LuaEvents::fire_player_hurt(int victim, int attacker, float damage, int bits)
{
	lua_State *L = g_lua.state();
	if (!L)
		return damage;

	{
		DispatchScope walking(m_dispatching);

		std::vector<Handler> &list = m_handlers[CSLUA_EVENT_PLAYER_HURT];
		for (size_t i = 0; i < list.size(); i++) {
			if (list[i].dead)
				continue;

			PluginScope scope(list[i].plugin);
			int errfunc = g_lua.push_errfunc();

			lua_rawgeti(L, LUA_REGISTRYINDEX, list[i].ref);
			cslua_push_player(L, victim);
			if (attacker > 0)
				cslua_push_player(L, attacker);
			else
				lua_pushnil(L);
			lua_pushnumber(L, damage);
			lua_pushinteger(L, bits);

			if (lua_pcall(L, 4, 1, errfunc) != 0) {
				g_lua.report_error(plugin_id(list[i].plugin));
				lua_remove(L, errfunc);
				continue;
			}

			// false blocks the hit; a number replaces the damage; nil/true leaves
			// the current value for the next handler in the chain.
			if (lua_isboolean(L, -1) && !lua_toboolean(L, -1))
				damage = 0.0f;
			else if (lua_isnumber(L, -1))
				damage = (float)lua_tonumber(L, -1);

			lua_pop(L, 1);
			lua_remove(L, errfunc);

			if (damage <= 0.0f) {
				damage = 0.0f;
				break;			// nothing left to scale once it is cancelled
			}
		}
	}

	sweep();
	return damage;
}

// killer/attacker 0 means "no player" (world, fall damage): push nil, not a
// bogus object, so scripts test it plainly.
static void push_player_or_nil(lua_State *L, int id)
{
	if (id > 0)
		cslua_push_player(L, id);
	else
		lua_pushnil(L);
}

void LuaEvents::fire_player_hurt_post(int victim, int attacker, float damage, int bits)
{
	dispatch(CSLUA_EVENT_PLAYER_HURT_POST, 4, [=](lua_State *L) {
		cslua_push_player(L, victim);
		push_player_or_nil(L, attacker);
		lua_pushnumber(L, damage);
		lua_pushinteger(L, bits);
	});
}

void LuaEvents::fire_player_death(int victim, int killer, int headshot)
{
	dispatch(CSLUA_EVENT_PLAYER_DEATH, 3, [=](lua_State *L) {
		cslua_push_player(L, victim);
		push_player_or_nil(L, killer);
		lua_pushboolean(L, headshot);
	});
}

void LuaEvents::fire_round_start()
{
	dispatch(CSLUA_EVENT_ROUND_START, 0, [](lua_State *) {});
}

void LuaEvents::fire_round_end(int winner)
{
	dispatch(CSLUA_EVENT_ROUND_END, 1, [=](lua_State *L) {
		lua_pushinteger(L, winner);
	});
}

void LuaEvents::fire_round_freeze_end()
{
	dispatch(CSLUA_EVENT_ROUND_FREEZE_END, 0, [](lua_State *) {});
}

void LuaEvents::fire_bomb_planted(int planter)
{
	dispatch(CSLUA_EVENT_BOMB_PLANTED, 1, [=](lua_State *L) {
		push_player_or_nil(L, planter);
	});
}

void LuaEvents::fire_bomb_defused(int defuser, bool success)
{
	dispatch(CSLUA_EVENT_BOMB_DEFUSED, 2, [=](lua_State *L) {
		push_player_or_nil(L, defuser);
		lua_pushboolean(L, success);
	});
}

void LuaEvents::fire_bomb_exploded(float x, float y, float z)
{
	dispatch(CSLUA_EVENT_BOMB_EXPLODED, 3, [=](lua_State *L) {
		lua_pushnumber(L, x);
		lua_pushnumber(L, y);
		lua_pushnumber(L, z);
	});
}
