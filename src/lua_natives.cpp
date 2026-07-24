#include "cslua.h"
#include "lua_natives.h"
#include "lua_engine.h"
#include "lua_events.h"
#include "lua_player.h"
#include "lua_timers.h"
#include "regamedll.h"
#include "players.h"

// print(...) -> server console and log, tab separated like the stock print.
static int l_print(lua_State *L)
{
	int n = lua_gettop(L);
	std::string out;

	lua_getglobal(L, "tostring");
	for (int i = 1; i <= n; i++) {
		lua_pushvalue(L, -1);		// tostring
		lua_pushvalue(L, i);
		lua_call(L, 1, 1);

		const char *s = lua_tostring(L, -1);
		if (!s)
			return luaL_error(L, "'tostring' must return a string to 'print'");

		if (i > 1)
			out += '\t';
		out += s;
		lua_pop(L, 1);
	}
	lua_pop(L, 1);					// tostring

	cslua_print("%s", out.c_str());
	return 0;
}

// plugin { name = "...", version = "...", author = "...", requires = {...} }
static int l_plugin(lua_State *L)
{
	luaL_checktype(L, 1, LUA_TTABLE);

	LuaPlugin *plugin = g_lua.loading_plugin();
	if (!plugin)
		return luaL_error(L, "plugin{} can only be called while a plugin is loading");

	plugin->declared = true;

	static const char *const fields[] = { "name", "version", "author" };
	std::string *targets[] = { &plugin->name, &plugin->version, &plugin->author };

	for (int i = 0; i < 3; i++) {
		lua_getfield(L, 1, fields[i]);
		if (lua_isstring(L, -1))
			*targets[i] = lua_tostring(L, -1);
		lua_pop(L, 1);
	}

	// api_version = N: the plugin was written against API version N. If it
	// wants a newer API than this build offers, refuse to load it - it would
	// only fail later on a missing function. Older is fine (we stay backward
	// compatible within a major line); a mismatch just gets a note.
	lua_getfield(L, 1, "api_version");
	if (lua_isnumber(L, -1)) {
		int want = (int)lua_tointeger(L, -1);
		if (want > CSLUA_API_VERSION) {
			lua_pop(L, 1);
			return luaL_error(L, "plugin '%s' needs cs-lua API v%d, this build is v%d - update cs-lua",
				plugin->id.c_str(), want, CSLUA_API_VERSION);
		}
		if (want < CSLUA_API_VERSION)
			cslua_print("note: plugin '%s' targets API v%d (current is v%d)",
				plugin->id.c_str(), want, CSLUA_API_VERSION);
	}
	lua_pop(L, 1);

	// requires = { "json", "menu" }: every listed module must resolve now, so
	// a missing dependency fails loudly on the plugin's first line instead of
	// erroring deep inside its code later.
	plugin->required_modules.clear();
	lua_getfield(L, 1, "requires");
	if (lua_istable(L, -1)) {
		int n = (int)lua_objlen(L, -1);
		for (int i = 1; i <= n; i++) {
			lua_rawgeti(L, -1, i);
			if (lua_isstring(L, -1)) {
				const char *mod = lua_tostring(L, -1);
				if (g_lua.resolve_module(g_lua.loading_index(), mod).empty()) {
					lua_pop(L, 2);
					return luaL_error(L, "plugin '%s' requires module '%s', which was not found",
						plugin->id.c_str(), mod);
				}
				plugin->required_modules.push_back(mod);
			}
			lua_pop(L, 1);
		}
	}
	lua_pop(L, 1);

	return 0;
}

// plugin_dir() -> absolute path of the running plugin's folder, or nil for a
// single-file plugin. Lets a plugin load its own data files without guessing
// the server's working directory (which is not the game directory).
static int l_plugin_dir(lua_State *L)
{
	const std::vector<LuaPlugin> &plugins = g_lua.plugins();
	int index = g_lua.current_index();

	if (index < 0 || index >= (int)plugins.size() || plugins[index].dir.empty()) {
		lua_pushnil(L);
		return 1;
	}

	lua_pushstring(L, plugins[index].dir.c_str());
	return 1;
}

// plugin_id() -> the folder name (or bare file name) of the plugin whose code
// is running, or nil for the core layer. Unlike plugin_dir() a single-file
// plugin has one too, which is what makes it usable as an identity - the
// export layer keys its registry on it.
static int l_plugin_id(lua_State *L)
{
	const std::vector<LuaPlugin> &plugins = g_lua.plugins();
	int index = g_lua.current_index();

	if (index < 0 || index >= (int)plugins.size()) {
		lua_pushnil(L);
		return 1;
	}

	lua_pushstring(L, plugins[index].id.c_str());
	return 1;
}

// server_time() -> the server clock in seconds, with fractions. This is the
// same clock timers run on. Note os.clock() is CPU time, not wall time, and
// drifts badly on a server that sleeps between frames - use this instead.
static int l_server_time(lua_State *L)
{
	lua_pushnumber(L, gpGlobals ? gpGlobals->time : 0.0f);
	return 1;
}

// map() -> the map running right now, "de_dust2". Map-scoped access rights
// need it, and it saves every plugin a cvar lookup.
static int l_map(lua_State *L)
{
	lua_pushstring(L, gpGlobals && gpGlobals->mapname ? STRING(gpGlobals->mapname) : "");
	return 1;
}

// player(id) -> the object for a slot, or nil if nobody is in it.
static int l_player(lua_State *L)
{
	int id = (int)luaL_checkinteger(L, 1);
	if (!g_players.is_connected(id)) {
		lua_pushnil(L);
		return 1;
	}

	cslua_push_player(L, id);
	return 1;
}

// players() -> array of everyone connected.
// players{ alive = true, team = "CT" } -> only those matching. The alive/team
// filters read live CS state, so they need ReGameDLL.
static int l_players(lua_State *L)
{
	int filter_alive = -1;			// -1 = don't care, 0 = dead, 1 = alive
	std::string filter_team;

	if (lua_istable(L, 1)) {
		lua_getfield(L, 1, "alive");
		if (lua_isboolean(L, -1))
			filter_alive = lua_toboolean(L, -1) ? 1 : 0;
		lua_pop(L, 1);

		lua_getfield(L, 1, "team");
		if (lua_isstring(L, -1))
			filter_team = lua_tostring(L, -1);
		lua_pop(L, 1);
	}

	bool needs_cs = filter_alive >= 0 || !filter_team.empty();
	if (needs_cs && !cslua_regamedll_ready())
		return luaL_error(L, "players{alive=..., team=...} needs ReGameDLL");

	lua_newtable(L);

	int n = 0;
	for (int id = 1; id < CSLUA_MAXPLAYERS; id++) {
		if (!g_players.is_connected(id))
			continue;
		if (filter_alive >= 0 && (cslua_player_alive(id) ? 1 : 0) != filter_alive)
			continue;
		if (!filter_team.empty() && filter_team != cslua_player_team_name(id))
			continue;
		cslua_push_player(L, id);
		lua_rawseti(L, -2, ++n);
	}

	return 1;
}

static const luaL_Reg s_natives[] =
{
	{ "print",   l_print },
	{ "plugin",  l_plugin },
	{ "on",      LuaEvents::l_on },
	{ "player",      l_player },
	{ "players",     l_players },
	{ "plugin_dir",  l_plugin_dir },
	{ "plugin_id",   l_plugin_id },
	{ "server_time", l_server_time },
	{ "map",         l_map },
	{ "after",   cslua_l_after },
	{ "every",   cslua_l_every },
	{ "cancel",  cslua_l_cancel },
	{ NULL, NULL }
};

void cslua_register_natives(lua_State *L)
{
	for (const luaL_Reg *r = s_natives; r->name; r++) {
		lua_pushcfunction(L, r->func);
		lua_setglobal(L, r->name);
	}

	lua_pushstring(L, CSLUA_VERSION);
	lua_setglobal(L, "_CSLUA_VERSION");

	lua_pushinteger(L, CSLUA_API_VERSION);
	lua_setglobal(L, "_CSLUA_API");

	// addons/lua. Scripts that keep data files (the access layer, stats) need
	// an absolute path: the server's working directory is not the game dir.
	lua_pushstring(L, cslua_base_dir().c_str());
	lua_setglobal(L, "_CSLUA_DIR");
}
