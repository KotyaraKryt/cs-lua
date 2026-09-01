#include "cslua.h"
#include "lua_natives.h"
#include "lua_engine.h"
#include "lua_events.h"
#include "lua_player.h"
#include "lua_timers.h"
#include "regamedll.h"
#include "players.h"
#include "platform.h"

#include <cstdio>
#include <cctype>

// Creates the global table if it is not there yet, then registers into it.
// `players` is filled from here, from lua_player.cpp and from core/commands.lua.
void cslua_register_namespace(lua_State *L, const char *name, const luaL_Reg *list)
{
	lua_getglobal(L, name);
	if (!lua_istable(L, -1)) {
		lua_pop(L, 1);
		lua_newtable(L);
		lua_pushvalue(L, -1);
		lua_setglobal(L, name);
	}

	for (const luaL_Reg *r = list; r->name; r++) {
		lua_pushcfunction(L, r->func);
		lua_setfield(L, -2, r->name);
	}

	lua_pop(L, 1);
}

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
			return luaL_error(L, "print: 'tostring' must return a string to 'print'");

		if (i > 1)
			out += '\t';
		out += s;
		lua_pop(L, 1);
	}
	lua_pop(L, 1);					// tostring

	cslua_print("%s", out.c_str());
	return 0;
}

// "X.Y.Z" -> three ints. Missing/non-numeric parts read as 0.
static void cslua_parse_version(const char *v, int out[3])
{
	out[0] = out[1] = out[2] = 0;
	if (v)
		sscanf(v, "%d.%d.%d", &out[0], &out[1], &out[2]);
}

static bool cslua_version_less(const char *a, const char *b)
{
	int va[3], vb[3];
	cslua_parse_version(a, va);
	cslua_parse_version(b, vb);

	for (int i = 0; i < 3; i++) {
		if (va[i] != vb[i])
			return va[i] < vb[i];
	}
	return false;
}

// ---------------------------------------------------------------------------
// plugin - both the manifest call (plugin { ... }) and a namespace.

static int l_plugin_call(lua_State *L)
{
	luaL_checktype(L, 2, LUA_TTABLE);

	LuaPlugin *plugin = g_lua.loading_plugin();
	if (!plugin)
		return luaL_error(L, "plugin: plugin{} can only be called while a plugin is loading");

	plugin->declared = true;

	static const char *const fields[] = { "name", "version", "author" };
	std::string *targets[] = { &plugin->name, &plugin->version, &plugin->author };

	for (int i = 0; i < 3; i++) {
		lua_getfield(L, 2, fields[i]);
		if (lua_isstring(L, -1))
			*targets[i] = lua_tostring(L, -1);
		lua_pop(L, 1);
	}

	// api_version = N: written against API version N.
	lua_getfield(L, 2, "api_version");
	if (lua_isnumber(L, -1)) {
		int want = (int)lua_tointeger(L, -1);
		lua_pop(L, 1);

		if (want > CSLUA_API_VERSION)
			return luaL_error(L, "plugin: '%s' needs cs-lua API v%d, this build is v%d - update cs-lua",
				plugin->id.c_str(), want, CSLUA_API_VERSION);
	} else {
		lua_pop(L, 1);
	}

	// min/max_engine_version = "1.1.0": pins to a specific build, for a plugin
	// using a native that shipped later under the same api_version.
	lua_getfield(L, 2, "min_engine_version");
	if (lua_isstring(L, -1)) {
		std::string want = lua_tostring(L, -1);
		lua_pop(L, 1);
		if (cslua_version_less(CSLUA_VERSION, want.c_str()))
			return luaL_error(L, "plugin: '%s' needs cs-lua v%s or newer, this build is v%s",
				plugin->id.c_str(), want.c_str(), CSLUA_VERSION);
	} else {
		lua_pop(L, 1);
	}

	lua_getfield(L, 2, "max_engine_version");
	if (lua_isstring(L, -1)) {
		std::string want = lua_tostring(L, -1);
		lua_pop(L, 1);
		if (cslua_version_less(want.c_str(), CSLUA_VERSION))
			return luaL_error(L, "plugin: '%s' targets cs-lua up to v%s, this build is v%s - "
				"it may rely on behavior that changed since", plugin->id.c_str(), want.c_str(), CSLUA_VERSION);
	} else {
		lua_pop(L, 1);
	}

	// requires = { "json", "menu" }: every listed module must resolve now.
	plugin->required_modules.clear();
	lua_getfield(L, 2, "requires");
	if (lua_istable(L, -1)) {
		int n = (int)lua_objlen(L, -1);
		for (int i = 1; i <= n; i++) {
			lua_rawgeti(L, -1, i);
			if (lua_isstring(L, -1)) {
				const char *mod = lua_tostring(L, -1);
				if (g_lua.resolve_module(g_lua.loading_index(), mod).empty()) {
					lua_pop(L, 2);
					return luaL_error(L, "plugin: '%s' requires module '%s', which was not found",
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

// plugin.dir() -> absolute path of the running plugin's folder, or nil.
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

// plugin.id() -> the folder name of the running plugin, or nil for the core layer.
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

// plugin.data_dir() -> a folder the plugin owns under addons/lua/data/, meant
// to outlive an update (unlike plugin.dir()). Created on first call.
std::string cslua_plugin_data_dir(int plugin_index)
{
	const std::vector<LuaPlugin> &plugins = g_lua.plugins();

	if (plugin_index < 0 || plugin_index >= (int)plugins.size())
		return std::string();

	std::string root = cslua_base_dir() + "/data";
	std::string dir = root + "/" + plugins[plugin_index].id;

	if (!cslua_make_dir(root) || !cslua_make_dir(dir))
		return std::string();

	return dir;
}

static int l_plugin_data_dir(lua_State *L)
{
	int index = g_lua.current_index();

	const std::vector<LuaPlugin> &plugins = g_lua.plugins();
	if (index < 0 || index >= (int)plugins.size()) {
		lua_pushnil(L);
		return 1;
	}

	std::string dir = cslua_plugin_data_dir(index);
	if (dir.empty())
		return luaL_error(L, "plugin.data_dir: cannot create the data directory for '%s'",
			plugins[index].id.c_str());

	lua_pushstring(L, dir.c_str());
	return 1;
}

// plugin.on_unload(fn) - run fn when this plugin goes away.
static int l_plugin_on_unload(lua_State *L)
{
	luaL_checktype(L, 1, LUA_TFUNCTION);

	int index = g_lua.current_index();
	if (index < 0)
		return luaL_error(L, "plugin.on_unload: no plugin is running "
			"(the core layer uses the plugin_unload event instead)");

	lua_pushvalue(L, 1);
	g_lua.add_unload_handler(index, luaL_ref(L, LUA_REGISTRYINDEX));
	return 0;
}

// plugin.list() -> one entry per loaded plugin: id, name, version, author, dir,
// failed.
static int l_plugin_list(lua_State *L)
{
	const std::vector<LuaPlugin> &plugins = g_lua.plugins();

	lua_newtable(L);
	for (size_t i = 0; i < plugins.size(); i++) {
		const LuaPlugin &p = plugins[i];

		lua_newtable(L);

		lua_pushstring(L, p.id.c_str());
		lua_setfield(L, -2, "id");
		lua_pushstring(L, (p.name.empty() ? p.id : p.name).c_str());
		lua_setfield(L, -2, "name");
		lua_pushstring(L, p.version.c_str());
		lua_setfield(L, -2, "version");
		lua_pushstring(L, p.author.c_str());
		lua_setfield(L, -2, "author");
		lua_pushstring(L, p.dir.c_str());
		lua_setfield(L, -2, "dir");
		lua_pushboolean(L, p.failed);
		lua_setfield(L, -2, "failed");

		lua_rawseti(L, -2, (int)i + 1);
	}

	return 1;
}

// plugin.reload(id) -> true if `id` names a loaded plugin. The reload runs from
// the top of the next frame, not on the spot.
static int l_plugin_reload(lua_State *L)
{
	const char *id = luaL_checkstring(L, 1);
	lua_pushboolean(L, g_lua.request_plugin_reload(id));
	return 1;
}

// plugin.reload_all() - same deferral, for every plugin at once.
static int l_plugin_reload_all(lua_State *L)
{
	g_lua.request_full_reload();
	return 0;
}

// ---------------------------------------------------------------------------
// sv - the server itself

// sv.cmd("changelevel %s", map) - queue a command on the server console. With a
// single argument the string is passed through untouched.
static int l_sv_cmd(lua_State *L)
{
	const char *cmd;

	if (lua_gettop(L) > 1) {
		lua_getglobal(L, "string");
		lua_getfield(L, -1, "format");
		lua_remove(L, -2);
		lua_insert(L, 1);
		lua_call(L, lua_gettop(L) - 1, 1);
		cmd = luaL_checkstring(L, -1);
	} else {
		cmd = luaL_checkstring(L, 1);
	}

	// A truncated command would run as something else; refuse instead of cutting.
	if (strlen(cmd) > 250)
		return luaL_error(L, "sv.cmd: command is too long (%d chars, 250 max)", (int)strlen(cmd));

	char line[256];
	cslua_snprintf(line, sizeof line, "%s\n", cmd);
	line[sizeof line - 1] = '\0';
	SERVER_COMMAND(line);
	return 0;
}

// sv.time() -> the server clock in seconds. Same clock timers run on.
static int l_sv_time(lua_State *L)
{
	lua_pushnumber(L, gpGlobals ? gpGlobals->time : 0.0f);
	return 1;
}

// sv.map() -> the current map, "de_dust2".
static int l_sv_map(lua_State *L)
{
	lua_pushstring(L, gpGlobals && gpGlobals->mapname ? STRING(gpGlobals->mapname) : "");
	return 1;
}

// sv.set_game_desc(name) -> overrides the "game" column a server browser shows.
// "" (or nil) drops back to the engine's own description.
static int l_sv_set_game_desc(lua_State *L)
{
	const char *text = lua_isnoneornil(L, 1) ? "" : luaL_checkstring(L, 1);
	cslua_set_game_desc(text);
	return 0;
}

// ---------------------------------------------------------------------------
// players

// players.get(id) -> the object for a slot, or nil if nobody is in it.
static int l_players_get(lua_State *L)
{
	int id = (int)luaL_checkinteger(L, 1);
	if (!g_players.is_connected(id)) {
		lua_pushnil(L);
		return 1;
	}

	cslua_push_player(L, id);
	return 1;
}

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

// players.list() -> everyone connected.
// players.list{ alive =, team =, bot =, hltv =, name = } -> only those matching.
// alive/team need ReGameDLL.
static int l_players_list(lua_State *L)
{
	int filter_alive = -1;			// -1 = don't care, 0 = dead, 1 = alive
	int filter_bot = -1;
	int filter_hltv = -1;
	std::string filter_team;
	std::string filter_name;

	if (lua_istable(L, 1)) {
		lua_getfield(L, 1, "alive");
		if (lua_isboolean(L, -1))
			filter_alive = lua_toboolean(L, -1) ? 1 : 0;
		lua_pop(L, 1);

		lua_getfield(L, 1, "bot");
		if (lua_isboolean(L, -1))
			filter_bot = lua_toboolean(L, -1) ? 1 : 0;
		lua_pop(L, 1);

		lua_getfield(L, 1, "hltv");
		if (lua_isboolean(L, -1))
			filter_hltv = lua_toboolean(L, -1) ? 1 : 0;
		lua_pop(L, 1);

		lua_getfield(L, 1, "team");
		if (lua_isstring(L, -1))
			filter_team = lua_tostring(L, -1);
		lua_pop(L, 1);

		lua_getfield(L, 1, "name");
		if (lua_isstring(L, -1))
			filter_name = lua_tostring(L, -1);
		lua_pop(L, 1);
	}

	bool needs_cs = filter_alive >= 0 || !filter_team.empty();
	if (needs_cs && !cslua_regamedll_ready())
		return luaL_error(L, "players.list{alive=..., team=...} needs ReGameDLL");

	lua_newtable(L);

	int n = 0;
	for (int id = 1; id < CSLUA_MAXPLAYERS; id++) {
		if (!g_players.is_connected(id))
			continue;
		if (filter_alive >= 0 && (cslua_player_alive(id) ? 1 : 0) != filter_alive)
			continue;
		if (!filter_team.empty() && filter_team != cslua_player_team_name(id))
			continue;

		if (filter_bot >= 0 || filter_hltv >= 0) {
			edict_t *e = g_engfuncs.pfnPEntityOfEntIndex(id);
			if (!e || e->free)
				continue;
			const int flags = e->v.flags;
			if (filter_bot >= 0 && ((flags & FL_FAKECLIENT) ? 1 : 0) != filter_bot)
				continue;
			if (filter_hltv >= 0 && ((flags & FL_PROXY) ? 1 : 0) != filter_hltv)
				continue;
		}

		if (!filter_name.empty() &&
		    !name_contains_ci(g_players.name(id), filter_name.c_str()))
			continue;

		cslua_push_player(L, id);
		lua_rawseti(L, -2, ++n);
	}

	return 1;
}

// ---------------------------------------------------------------------------

static const luaL_Reg s_plugin[] =
{
	{ "dir",         l_plugin_dir },
	{ "id",          l_plugin_id },
	{ "data_dir",    l_plugin_data_dir },
	{ "on_unload",   l_plugin_on_unload },
	{ "list",        l_plugin_list },
	{ "reload",      l_plugin_reload },
	{ "reload_all",  l_plugin_reload_all },
	{ NULL, NULL }
};

static const luaL_Reg s_sv[] =
{
	{ "cmd",           l_sv_cmd },
	{ "time",          l_sv_time },
	{ "map",           l_sv_map },
	{ "hull_free",     cslua_sv_hull_free },
	{ "set_game_desc", l_sv_set_game_desc },
	{ NULL, NULL }
};

static const luaL_Reg s_players[] =
{
	{ "get",  l_players_get },
	{ "list", l_players_list },
	{ NULL, NULL }
};

void cslua_register_natives(lua_State *L)
{
	// print overrides the stdlib one.
	lua_pushcfunction(L, l_print);
	lua_setglobal(L, "print");

	cslua_register_namespace(L, "plugin", s_plugin);
	cslua_register_namespace(L, "sv", s_sv);
	cslua_register_namespace(L, "players", s_players);

	// plugin{ ... } is a call on the namespace table.
	lua_getglobal(L, "plugin");
	lua_newtable(L);
	lua_pushcfunction(L, l_plugin_call);
	lua_setfield(L, -2, "__call");
	lua_setmetatable(L, -2);
	lua_pop(L, 1);

	lua_getglobal(L, "sv");

	lua_pushstring(L, CSLUA_VERSION);
	lua_setfield(L, -2, "version");

	lua_pushinteger(L, CSLUA_API_VERSION);
	lua_setfield(L, -2, "api");

	lua_pushstring(L, cslua_base_dir().c_str());
	lua_setfield(L, -2, "dir");

	lua_pop(L, 1);
}
