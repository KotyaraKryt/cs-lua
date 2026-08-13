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

// Creates the global table if it is not there yet, then registers into it.
// Reusing an existing table is the point: `players` is filled from here, from
// lua_player.cpp and from core/commands.lua, and none of them has to know
// whether it got there first.
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

// "X.Y.Z" -> three ints. Missing or non-numeric parts read as 0, so a
// fat-fingered version string fails safe (compares as the oldest possible
// build) instead of crashing min_version/max_version below.
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
// plugin
//
// `plugin` is both the manifest call and a namespace:
//
//   plugin { name = "Shop", api_version = 2 }
//   plugin.data_dir()
//
// The call is manifest.lua's only job - the engine runs that file before
// init.lua and requires it to have made this call - and reads best as a
// call, so the table carries __call rather than growing a plugin.declare().

// The manifest itself. Called through __call, so the spec table is argument 2.
static int l_plugin_call(lua_State *L)
{
	luaL_checktype(L, 2, LUA_TTABLE);

	LuaPlugin *plugin = g_lua.loading_plugin();
	if (!plugin)
		return luaL_error(L, "plugin{} can only be called while a plugin is loading");

	plugin->declared = true;

	static const char *const fields[] = { "name", "version", "author" };
	std::string *targets[] = { &plugin->name, &plugin->version, &plugin->author };

	for (int i = 0; i < 3; i++) {
		lua_getfield(L, 2, fields[i]);
		if (lua_isstring(L, -1))
			*targets[i] = lua_tostring(L, -1);
		lua_pop(L, 1);
	}

	// api_version = N: the plugin was written against API version N. A newer
	// one than this build offers would only fail later on a missing function.
	//
	// v1 is not "older but fine" - it is a different API. Everything was
	// renamed, so a v1 plugin would die on its first call with a confusing
	// "attempt to call a nil value". Say what actually happened instead.
	lua_getfield(L, 2, "api_version");
	if (lua_isnumber(L, -1)) {
		int want = (int)lua_tointeger(L, -1);
		lua_pop(L, 1);

		if (want > CSLUA_API_VERSION)
			return luaL_error(L, "plugin '%s' needs cs-lua API v%d, this build is v%d - update cs-lua",
				plugin->id.c_str(), want, CSLUA_API_VERSION);

		if (want < 2)
			return luaL_error(L, "plugin '%s' targets cs-lua API v%d, which this build no longer speaks. "
				"v2 renamed the whole Lua API (on -> hook.add, after -> timer.after, ...); "
				"see docs/migration.md", plugin->id.c_str(), want);
	} else {
		lua_pop(L, 1);
	}

	// min_engine_version / max_engine_version = "2.1.0": pins to a specific
	// build, not just an API generation. api_version only moves on a breaking
	// rename - a plugin using a native that shipped later under the same
	// api_version (http_server, say) has no other way to say "at least this
	// build". Named "engine", not "version", so it can't be mistaken for
	// api_version sitting right above it.
	lua_getfield(L, 2, "min_engine_version");
	if (lua_isstring(L, -1)) {
		std::string want = lua_tostring(L, -1);
		lua_pop(L, 1);
		if (cslua_version_less(CSLUA_VERSION, want.c_str()))
			return luaL_error(L, "plugin '%s' needs cs-lua v%s or newer, this build is v%s",
				plugin->id.c_str(), want.c_str(), CSLUA_VERSION);
	} else {
		lua_pop(L, 1);
	}

	lua_getfield(L, 2, "max_engine_version");
	if (lua_isstring(L, -1)) {
		std::string want = lua_tostring(L, -1);
		lua_pop(L, 1);
		if (cslua_version_less(want.c_str(), CSLUA_VERSION))
			return luaL_error(L, "plugin '%s' targets cs-lua up to v%s, this build is v%s - "
				"it may rely on behavior that changed since", plugin->id.c_str(), want.c_str(), CSLUA_VERSION);
	} else {
		lua_pop(L, 1);
	}

	// requires = { "json", "menu" }: every listed module must resolve now, so
	// a missing dependency fails loudly on the plugin's first line instead of
	// erroring deep inside its code later.
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

// plugin.dir() -> absolute path of the running plugin's folder, or nil when
// no plugin is running (the core layer). Lets a plugin load its own data
// files without guessing the server's working directory (which is not the
// game directory).
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

// plugin.id() -> the folder name of the plugin whose code is running, or nil
// for the core layer. What the export layer keys its registry on.
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

// plugin.data_dir() -> absolute path of a folder the plugin owns, under
// addons/lua/data/, created on the first call. plugin.dir() points at the
// plugin's code, which the installer overwrites on every update; this is the
// place whose contents are meant to outlive one.
std::string cslua_plugin_data_dir(int plugin_index)
{
	const std::vector<LuaPlugin> &plugins = g_lua.plugins();

	if (plugin_index < 0 || plugin_index >= (int)plugins.size())
		return std::string();

	// One level at a time: data/ itself may not exist yet on a fresh install.
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

// plugin.on_unload(fn) - run fn when this plugin goes away, whether that is a
// full lua_reload or `lua_reload <this plugin>`. The plugin_unload event fires
// for everybody; this fires for you, which is what cleanup actually wants.
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

// plugin.list() -> one entry per loaded plugin: id, name, version, author,
// dir (the plugin's folder), failed. Read-only
// introspection of what `lua_list` already prints to the console - built for
// the web panel's resource manager, but not specific to it.
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

// plugin.reload(id) -> true if `id` names a loaded plugin. Does not reload it
// on the spot: the request is only recorded, and carried out from the top of
// the next frame, well after this call - and the route handler it runs
// in - have returned. See LuaEngine::request_plugin_reload for why: doing it
// synchronously would tear down the very Lua state this call is running on.
static int l_plugin_reload(lua_State *L)
{
	const char *id = luaL_checkstring(L, 1);
	lua_pushboolean(L, g_lua.request_plugin_reload(id));
	return 1;
}

// plugin.reload_all() - same deferral, for every plugin at once (what the
// bare `lua_reload` console command does).
static int l_plugin_reload_all(lua_State *L)
{
	g_lua.request_full_reload();
	return 0;
}

// ---------------------------------------------------------------------------
// sv - the server itself

// sv.cmd("changelevel %s", map) - queue a command on the server console.
// The module goes there itself for kick and ban; without this a plugin cannot,
// and everything from sv_restart to another mod's commands is out of reach.
//
// Formatting happens here rather than at the call site so the common case reads
// like print(). With a single argument the string is passed through untouched,
// so a lone % in a player's nick cannot turn into a format directive.
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

	// The engine's command buffer takes this whole; a truncated command would
	// run as something else entirely, so refuse instead of cutting.
	if (strlen(cmd) > 250)
		return luaL_error(L, "sv.cmd: command is too long (%d chars, 250 max)", (int)strlen(cmd));

	char line[256];
	cslua_snprintf(line, sizeof line, "%s\n", cmd);
	line[sizeof line - 1] = '\0';
	SERVER_COMMAND(line);
	return 0;
}

// sv.time() -> the server clock in seconds, with fractions. This is the same
// clock timers run on. Note os.clock() is CPU time, not wall time, and drifts
// badly on a server that sleeps between frames - use this instead.
static int l_sv_time(lua_State *L)
{
	lua_pushnumber(L, gpGlobals ? gpGlobals->time : 0.0f);
	return 1;
}

// sv.map() -> the map running right now, "de_dust2". Map-scoped access rights
// need it, and it saves every plugin a cvar lookup.
static int l_sv_map(lua_State *L)
{
	lua_pushstring(L, gpGlobals && gpGlobals->mapname ? STRING(gpGlobals->mapname) : "");
	return 1;
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

// players.list() -> array of everyone connected.
// players.list{ alive = true, team = "CT" } -> only those matching. The
// alive/team filters read live CS state, so they need ReGameDLL.
static int l_players_list(lua_State *L)
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
	{ "cmd",  l_sv_cmd },
	{ "time", l_sv_time },
	{ "map",  l_sv_map },
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
	// print stays a bare global: it overrides the stdlib one rather than
	// adding to the cs-lua API.
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

	// addons/lua. Scripts that keep data files (the access layer, stats) need
	// an absolute path: the server's working directory is not the game dir.
	lua_pushstring(L, cslua_base_dir().c_str());
	lua_setfield(L, -2, "dir");

	lua_pop(L, 1);
}
