#include "cslua.h"
#include "player_filter.h"
#include "players.h"
#include "regamedll.h"

#include <cctype>

// Case-insensitive substring, ASCII only. Empty needle matches anything.
static bool name_contains_ci(const char *hay, const char *needle)
{
	if (!needle || !*needle)
		return true;
	if (!hay)
		return false;

	for (const char *h = hay; *h; h++) {
		const char *a = h;
		const char *b = needle;
		while (*a && *b &&
		       tolower((unsigned char)*a) == tolower((unsigned char)*b)) {
			a++;
			b++;
		}
		if (!*b)
			return true;
	}
	return false;
}

void cslua_read_player_filter(lua_State *L, int idx, PlayerFilter &f)
{
	lua_getfield(L, idx, "alive");
	if (lua_isboolean(L, -1))
		f.alive = lua_toboolean(L, -1) ? 1 : 0;
	lua_pop(L, 1);

	lua_getfield(L, idx, "bot");
	if (lua_isboolean(L, -1))
		f.bot = lua_toboolean(L, -1) ? 1 : 0;
	lua_pop(L, 1);

	lua_getfield(L, idx, "hltv");
	if (lua_isboolean(L, -1))
		f.hltv = lua_toboolean(L, -1) ? 1 : 0;
	lua_pop(L, 1);

	lua_getfield(L, idx, "team");
	if (lua_isstring(L, -1))
		f.team = lua_tostring(L, -1);
	lua_pop(L, 1);

	lua_getfield(L, idx, "name");
	if (lua_isstring(L, -1))
		f.name = lua_tostring(L, -1);
	lua_pop(L, 1);
}

// LuaJIT is 5.1, which has no lua_absindex.
static int abs_index(lua_State *L, int index)
{
	return index > 0 || index <= LUA_REGISTRYINDEX ? index : lua_gettop(L) + index + 1;
}

void cslua_read_broadcast_filter(lua_State *L, int idx, PlayerFilter &f)
{
	idx = abs_index(L, idx);

	lua_pushliteral(L, "alive");
	lua_rawget(L, idx);
	if (lua_isboolean(L, -1))
		f.alive = lua_toboolean(L, -1) ? 1 : 0;
	lua_pop(L, 1);

	lua_pushliteral(L, "bot");
	lua_rawget(L, idx);
	if (lua_isboolean(L, -1))
		f.bot = lua_toboolean(L, -1) ? 1 : 0;
	lua_pop(L, 1);

	lua_pushliteral(L, "hltv");
	lua_rawget(L, idx);
	if (lua_isboolean(L, -1))
		f.hltv = lua_toboolean(L, -1) ? 1 : 0;
	lua_pop(L, 1);

	lua_pushliteral(L, "team");
	lua_rawget(L, idx);
	if (lua_isstring(L, -1))
		f.team = lua_tostring(L, -1);
	lua_pop(L, 1);

	lua_pushliteral(L, "name");
	lua_rawget(L, idx);
	if (lua_isstring(L, -1))
		f.name = lua_tostring(L, -1);
	lua_pop(L, 1);
}

bool cslua_player_matches_filter(int id, const PlayerFilter &f)
{
	if (!g_players.is_connected(id))
		return false;

	if (f.alive >= 0 && (cslua_player_alive(id) ? 1 : 0) != f.alive)
		return false;
	if (!f.team.empty() && f.team != cslua_player_team_name(id))
		return false;

	if (f.bot >= 0 || f.hltv >= 0) {
		edict_t *e = g_engfuncs.pfnPEntityOfEntIndex(id);
		if (!e || e->free)
			return false;
		const int flags = e->v.flags;
		if (f.bot >= 0 && ((flags & FL_FAKECLIENT) ? 1 : 0) != f.bot)
			return false;
		if (f.hltv >= 0 && ((flags & FL_PROXY) ? 1 : 0) != f.hltv)
			return false;
	}

	if (!f.name.empty() && !name_contains_ci(g_players.name(id), f.name.c_str()))
		return false;

	return true;
}
