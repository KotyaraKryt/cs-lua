#include "cslua.h"
#include "lua_engine.h"
#include "lua_events.h"
#include "lua_natives.h"
#include "lua_player.h"
#include "lua_timers.h"
#include "lua_cvar.h"
#include "lua_command.h"
#include "lua_sound.h"
#include "lua_menu.h"
#include "lua_message.h"
#include "lua_entity.h"
#include "lua_db.h"
#include "lua_http.h"
#include "lua_httpserver.h"
#include "regamedll.h"
#include "platform.h"

#include <algorithm>
#include <map>
#include <time.h>

LuaEngine g_lua;

// pcall message handler: turns "file:line: msg" into a full traceback.
static int cslua_traceback(lua_State *L)
{
	const char *msg = lua_tostring(L, 1);
	if (!msg)
		msg = "(error object is not a string)";

	lua_getglobal(L, "debug");
	if (!lua_istable(L, -1)) {
		lua_pop(L, 1);
		lua_pushstring(L, msg);
		return 1;
	}

	lua_getfield(L, -1, "traceback");
	if (!lua_isfunction(L, -1)) {
		lua_pop(L, 2);
		lua_pushstring(L, msg);
		return 1;
	}

	lua_pushstring(L, msg);
	lua_pushinteger(L, 2);		// skip this handler in the traceback
	lua_call(L, 2, 1);
	return 1;
}

void LuaEngine::mark_failed(int index)
{
	if (index >= 0 && index < (int)m_plugins.size())
		m_plugins[index].failed = true;
}

int LuaEngine::push_errfunc()
{
	lua_pushcfunction(m_L, cslua_traceback);
	return lua_gettop(m_L);
}

// A handler that throws does it every time it runs. In weapon_fire that is
// once per shot on the server, in player_hurt once per hit: a full traceback
// each, thousands of lines a second into the console and the log. The disk
// fills up and everything else worth reading is buried.
//
// So the same error from the same place is printed a few times, then muted for
// a while, and the mute reports how much it swallowed. Keyed by the error text
// as well as the place, so a second, different failure in the same handler is
// not hidden behind the first.
static const int ERROR_BURST = 3;
static const double ERROR_MUTE_SECONDS = 60.0;

struct ErrorRecord
{
	int seen;			// since this record was last reported
	int muted;			// swallowed while muted
	double until;		// server time the mute expires
};

static std::map<std::string, ErrorRecord> s_errors;

// gpGlobals->time restarts on every map, so it cannot be trusted for a
// duration that spans one. This only needs to be monotonic.
static double error_clock()
{
	return (double)clock() / (double)CLOCKS_PER_SEC;
}

void cslua_errors_clear()
{
	s_errors.clear();
}

void LuaEngine::report_error(const char *where)
{
	const char *msg = lua_tostring(m_L, -1);
	std::string text = msg ? msg : "(unknown error)";

	// First line only: the traceback below it repeats per call and would make
	// every occurrence look unique.
	size_t first_break = text.find('\n');
	std::string key = std::string(where) + "\n" +
		(first_break == std::string::npos ? text : text.substr(0, first_break));

	double now = error_clock();
	ErrorRecord &rec = s_errors[key];

	if (rec.until > now) {
		rec.muted++;
		lua_pop(m_L, 1);
		return;
	}

	if (rec.muted > 0) {
		cslua_error("(%d more of the previous error in %s were not printed)",
			rec.muted, where);
		rec.muted = 0;
		rec.seen = 0;
	}

	cslua_error("error in %s:", where);

	// The traceback is multi-line; the console eats everything after the
	// first newline, so print it line by line.
	size_t start = 0;
	while (start <= text.size()) {
		size_t end = text.find('\n', start);
		if (end == std::string::npos)
			end = text.size();
		if (end > start)
			cslua_error("  %s", text.substr(start, end - start).c_str());
		start = end + 1;
	}

	if (++rec.seen >= ERROR_BURST) {
		rec.until = now + ERROR_MUTE_SECONDS;
		cslua_error("  (repeating - muting this one for %d seconds)",
			(int)ERROR_MUTE_SECONDS);
	}

	lua_pop(m_L, 1);
}

// Fills `out` with the paths a module name maps to, in search order, and
// returns how many there are. Plugin-local files win over the shared include/
// directory, so two plugins can both have a lib/util.lua without seeing each
// other's.
int LuaEngine::module_paths(int plugin_index, const char *modname, std::string out[4]) const
{
	// "lib.util" -> "lib/util"
	std::string rel = modname;
	std::replace(rel.begin(), rel.end(), '.', '/');

	int n = 0;
	if (plugin_index >= 0 && plugin_index < (int)m_plugins.size()) {
		const std::string &dir = m_plugins[plugin_index].dir;
		if (!dir.empty()) {
			out[n++] = dir + "/" + rel + ".lua";
			out[n++] = dir + "/" + rel + "/init.lua";
		}
	}
	out[n++] = cslua_base_dir() + "/include/" + rel + ".lua";
	out[n++] = cslua_base_dir() + "/include/" + rel + "/init.lua";
	return n;
}

std::string LuaEngine::resolve_module(int plugin_index, const char *modname) const
{
	std::string candidates[4];
	int n = module_paths(plugin_index, modname, candidates);

	for (int i = 0; i < n; i++)
		if (cslua_file_exists(candidates[i]))
			return candidates[i];

	return std::string();
}

std::string LuaEngine::module_candidates(int plugin_index, const char *modname) const
{
	std::string candidates[4];
	int n = module_paths(plugin_index, modname, candidates);

	std::string text;
	for (int i = 0; i < n; i++) {
		text += "\n\t";
		text += candidates[i];
	}
	return text;
}

int LuaEngine::l_require(lua_State *L)
{
	const char *modname = luaL_checkstring(L, 1);
	int index = (int)lua_tointeger(L, lua_upvalueindex(1));

	if (index < 0 || index >= (int)g_lua.m_plugins.size())
		return luaL_error(L, "require() called outside of a plugin");

	LuaPlugin &plugin = g_lua.m_plugins[index];

	lua_rawgeti(L, LUA_REGISTRYINDEX, plugin.modules_ref);
	lua_getfield(L, -1, modname);
	if (!lua_isnil(L, -1)) {
		if (lua_isboolean(L, -1) && !lua_toboolean(L, -1))
			return luaL_error(L, "circular require of '%s' in plugin '%s'", modname, plugin.id.c_str());
		return 1;
	}
	lua_pop(L, 1);

	std::string path = g_lua.resolve_module(index, modname);
	if (path.empty()) {
		// Say where we looked: nine times out of ten the fix is visible right
		// there (a file sitting in lib/ wants require("lib.name")).
		std::string tried = g_lua.module_candidates(index, modname);
		return luaL_error(L, "module '%s' not found for plugin '%s'. Looked in:%s",
			modname, plugin.id.c_str(), tried.c_str());
	}

	// Mark as in-flight so a cycle reports itself instead of hanging.
	lua_pushboolean(L, 0);
	lua_setfield(L, -2, modname);

	if (luaL_loadfile(L, path.c_str()) != 0)
		return luaL_error(L, "%s", lua_tostring(L, -1));

	// Every file of a plugin shares the plugin's environment.
	lua_rawgeti(L, LUA_REGISTRYINDEX, plugin.env_ref);
	lua_setfenv(L, -2);

	// Not pcall: let the error travel up to whoever is loading the plugin,
	// so the traceback covers the whole require chain.
	lua_call(L, 0, 1);

	if (lua_isnil(L, -1)) {
		lua_pop(L, 1);
		lua_pushboolean(L, 1);
	}

	lua_pushvalue(L, -1);
	lua_setfield(L, -3, modname);
	return 1;
}

void LuaEngine::init()
{
	if (m_L)
		shutdown();

	m_L = luaL_newstate();
	if (!m_L) {
		cslua_error("failed to create Lua state, no plugins will run");
		return;
	}

	luaL_openlibs(m_L);
	cslua_register_natives(m_L);
	cslua_register_hooks(m_L);
	cslua_register_timers(m_L);
	cslua_player_init(m_L);
	cslua_register_cvar(m_L);
	cslua_register_command_api(m_L);
	cslua_register_sound(m_L);
	cslua_register_ui(m_L);
	cslua_register_menu(m_L);
	cslua_register_entity(m_L);
	cslua_register_db(m_L);
	cslua_register_http(m_L);
	cslua_register_httpserver(m_L);

	// Only for code that reaches for the stock require; plugins get their own.
	const std::string &base = cslua_base_dir();
	lua_getglobal(m_L, "package");
	if (lua_istable(m_L, -1)) {
		std::string path = base + "/include/?.lua;" + base + "/include/?/init.lua";
		lua_pushstring(m_L, path.c_str());
		lua_setfield(m_L, -2, "path");
	}
	lua_pop(m_L, 1);

	// The core layer runs before any plugin and defines the shared base API
	// (chat commands and friends). Plugins see it through _G.
	load_core();

	discover_plugins();
	for (size_t i = 0; i < m_plugins.size(); i++)
		load_plugin((int)i);

	// Now that every plugin has declared what it listens for, wire up only the
	// gameplay hookchains that are actually used.
	cslua_regamedll_install_hooks();

	int failed = 0;
	for (size_t i = 0; i < m_plugins.size(); i++)
		failed += m_plugins[i].failed ? 1 : 0;

	if (failed)
		cslua_print("loaded %d plugin(s), %d failed", (int)m_plugins.size() - failed, failed);
	else
		cslua_print("loaded %d plugin(s)", (int)m_plugins.size());
}

void LuaEngine::shutdown()
{
	// Plugins get told first, while everything they might need still works:
	// this is where they undo whatever they did to the world. After this line
	// the state is being taken apart and nothing Lua-side is safe to call.
	//
	// Guarded because a handler is free to do something that shuts us down
	// again - blowing the precache budget does exactly that - and the second
	// pass would run every handler a second time against a half-dead state.
	if (m_L && !m_unloading) {
		m_unloading = true;
		g_events.fire_plugin_unload();
	}

	// Detach from the game DLL before the lua_State dies, or a spawn/kill hook
	// could fire into freed handler refs.
	cslua_regamedll_remove_hooks();

	// Disowns replies still on the wire. Not a join: waiting for the slowest
	// request here would freeze the server for the length of a lua_reload.
	cslua_http_reset();
	cslua_httpserver_reset();

	g_events.clear();
	cslua_errors_clear();
	cslua_timers_clear();
	cslua_command_shutdown();
	cslua_cvar_shutdown();
	cslua_sound_clear();
	cslua_player_shutdown();
	cslua_entity_shutdown();
	// Last of the API modules, and the only one holding an OS resource: the
	// files have to be closed for real, not left for lua_close to forget.
	cslua_db_shutdown();
	m_plugins.clear();
	m_loading = -1;
	m_current = -1;

	if (m_L) {
		lua_close(m_L);
		m_L = nullptr;
	}

	m_unloading = false;
}

void LuaEngine::reload()
{
	shutdown();
	init();
}

bool LuaEngine::request_plugin_reload(const char *id)
{
	for (size_t i = 0; i < m_plugins.size(); i++) {
		if (m_plugins[i].id == id) {
			m_pending_plugin_reload = id;
			m_pending_full_reload = false;
			return true;
		}
	}
	return false;
}

void LuaEngine::process_pending_reload()
{
	if (m_pending_full_reload) {
		m_pending_full_reload = false;
		m_pending_plugin_reload.clear();
		reload();
		return;
	}

	if (!m_pending_plugin_reload.empty()) {
		std::string id = m_pending_plugin_reload;
		m_pending_plugin_reload.clear();
		reload_plugin(id.c_str());
	}
}

void LuaEngine::add_unload_handler(int plugin_index, int ref)
{
	if (plugin_index < 0 || plugin_index >= (int)m_plugins.size()) {
		luaL_unref(m_L, LUA_REGISTRYINDEX, ref);
		return;
	}

	m_plugins[plugin_index].unload_refs.push_back(ref);
}

// Everything a plugin leaves behind, in the order that keeps it safe: its own
// cleanup code runs first, while its handlers, timers and databases still
// work, and only then does any of that go away.
void LuaEngine::unload_plugin(int index, bool run_handlers)
{
	LuaPlugin &plugin = m_plugins[index];

	if (run_handlers && m_L) {
		// Last in, first out: a plugin undoes its setup in reverse, the same
		// way it would if this were one function.
		for (size_t i = plugin.unload_refs.size(); i-- > 0; ) {
			PluginScope scope(index);
			int errfunc = push_errfunc();

			lua_rawgeti(m_L, LUA_REGISTRYINDEX, plugin.unload_refs[i]);
			if (lua_pcall(m_L, 0, 0, errfunc) != 0)
				report_error(plugin.id.c_str());

			lua_remove(m_L, errfunc);
		}
	}

	if (m_L) {
		for (size_t i = 0; i < plugin.unload_refs.size(); i++)
			luaL_unref(m_L, LUA_REGISTRYINDEX, plugin.unload_refs[i]);
	}
	plugin.unload_refs.clear();

	// Now tell everyone else, while this plugin's registrations are still in
	// place: the core export registry drops its entries here, and a plugin
	// with a soft dependency on it gets to fall back.
	if (run_handlers)
		g_events.fire_plugin_unload(plugin.id.c_str());

	g_events.remove_plugin(index);
	cslua_timers_remove_plugin(index);
	cslua_command_remove_plugin(index);
	cslua_db_remove_plugin(index);
	cslua_http_remove_plugin(index);
	cslua_httpserver_remove_plugin(index);
}

// One plugin, torn down and started again, with everyone else left alone.
//
// The precached resources are deliberately not released: the engine has no way
// to take a model back out of the map's table mid-round, and re-running the
// plugin's precache calls simply resolves to the slots it already owns.
bool LuaEngine::reload_plugin(const char *id)
{
	if (!m_L)
		return false;

	int index = -1;
	for (size_t i = 0; i < m_plugins.size(); i++) {
		if (m_plugins[i].id == id) {
			index = (int)i;
			break;
		}
	}

	if (index < 0)
		return false;

	unload_plugin(index, true);

	LuaPlugin &plugin = m_plugins[index];

	// A fresh environment and a fresh module cache: reloading has to pick up
	// edits to the plugin's lib/ files too, and the old cache would hand back
	// the versions from last time.
	luaL_unref(m_L, LUA_REGISTRYINDEX, plugin.env_ref);
	luaL_unref(m_L, LUA_REGISTRYINDEX, plugin.modules_ref);
	plugin.env_ref = LUA_NOREF;
	plugin.modules_ref = LUA_NOREF;
	plugin.declared = false;
	plugin.failed = false;
	plugin.required_modules.clear();

	load_plugin(index);

	// Gameplay hookchains are installed once for the whole state based on what
	// is listened for. A reloaded plugin may listen for something new, so ask
	// again - installing an already-installed hook is a no-op.
	cslua_regamedll_install_hooks();

	return true;
}

bool LuaEngine::check_plugin(const char *id, std::string &out)
{
	if (!m_L) {
		out = "Lua state is not running";
		return false;
	}

	std::string plugin_dir = cslua_base_dir() + "/plugins/" + id;
	std::string manifest = plugin_dir + "/manifest.lua";
	std::string init = plugin_dir + "/init.lua";

	if (!cslua_file_exists(manifest)) {
		out = "no manifest.lua in plugins/" + std::string(id);
		return false;
	}
	if (!cslua_file_exists(init)) {
		out = "no init.lua in plugins/" + std::string(id) +
			" - both files are required, manifest.lua was not checked";
		return false;
	}

	// Appended past every real plugin, so its index cannot collide with one
	// require() closures elsewhere already captured by value.
	int index = (int)m_plugins.size();

	LuaPlugin plugin;
	plugin.id = id;
	plugin.dir = plugin_dir;
	plugin.manifest = manifest;
	plugin.entry = init;
	plugin.name = plugin.id;
	m_plugins.push_back(plugin);

	// Same environment setup as load_plugin(): its own globals table and its
	// own require() cache, so a check sees exactly what a real load would.
	lua_newtable(m_L);
	lua_newtable(m_L);
	lua_pushvalue(m_L, LUA_GLOBALSINDEX);
	lua_setfield(m_L, -2, "__index");
	lua_setmetatable(m_L, -2);
	lua_pushinteger(m_L, index);
	lua_pushcclosure(m_L, LuaEngine::l_require, 1);
	lua_setfield(m_L, -2, "require");
	m_plugins[index].env_ref = luaL_ref(m_L, LUA_REGISTRYINDEX);
	lua_newtable(m_L);
	m_plugins[index].modules_ref = luaL_ref(m_L, LUA_REGISTRYINDEX);

	m_loading = index;
	bool ok;
	{
		PluginScope scope(index);
		ok = run_file(m_plugins[index].manifest, m_plugins[index].env_ref, id);

		if (ok && !m_plugins[index].declared) {
			cslua_error("plugin '%s': manifest.lua must call plugin{}", id);
			ok = false;
		}
	}
	m_loading = -1;

	if (ok) {
		const LuaPlugin &checked = m_plugins[index];
		out = "manifest.lua OK: '" + (checked.name.empty() ? checked.id : checked.name) +
			"' v" + (checked.version.empty() ? "?" : checked.version) +
			" by " + (checked.author.empty() ? "?" : checked.author);
		if (!checked.required_modules.empty()) {
			out += ", requires:";
			for (size_t i = 0; i < checked.required_modules.size(); i++)
				out += " " + checked.required_modules[i];
		}
	} else {
		out = "manifest.lua failed - see console";
	}

	// Nothing it registered is meant to outlive the check: undo it the same
	// way a failed real load would, whether or not this one succeeded.
	unload_plugin(index, false);
	luaL_unref(m_L, LUA_REGISTRYINDEX, m_plugins[index].env_ref);
	luaL_unref(m_L, LUA_REGISTRYINDEX, m_plugins[index].modules_ref);
	m_plugins.pop_back();

	return ok;
}

void LuaEngine::load_core()
{
	std::string dir = cslua_base_dir() + "/core";

	std::vector<CsLuaDirEntry> entries;
	if (!cslua_list_dir(dir, entries))
		return;			// no core layer installed; base API just won't exist

	std::vector<std::string> files;
	for (size_t i = 0; i < entries.size(); i++) {
		const std::string &name = entries[i].name;
		if (entries[i].is_dir || name.size() < 5)
			continue;
		if (name.compare(name.size() - 4, 4, ".lua") == 0)
			files.push_back(name);
	}

	std::sort(files.begin(), files.end());

	// Core runs in the global environment: unlike plugins it is meant to
	// define globals (chat_command, ...) that everyone sees. Its handlers are
	// tagged with plugin index -1, so they survive plugin unloads but die on a
	// full reload like everything else.
	for (size_t i = 0; i < files.size(); i++) {
		std::string path = dir + "/" + files[i];
		int errfunc = push_errfunc();
		if (luaL_loadfile(m_L, path.c_str()) != 0) {
			report_error(files[i].c_str());
			lua_remove(m_L, errfunc);
			continue;
		}
		// No setfenv: the chunk runs against _G.
		if (lua_pcall(m_L, 0, 0, errfunc) != 0)
			report_error(files[i].c_str());
		lua_remove(m_L, errfunc);
	}
}

void LuaEngine::discover_plugins()
{
	std::string dir = cslua_base_dir() + "/plugins";

	std::vector<CsLuaDirEntry> entries;
	if (!cslua_list_dir(dir, entries)) {
		cslua_print("no plugins found in %s", dir.c_str());
		return;
	}

	for (size_t i = 0; i < entries.size(); i++) {
		const std::string &entry = entries[i].name;

		// A leading underscore disables a plugin without deleting it.
		if (entry.empty() || entry[0] == '_' || entry[0] == '.')
			continue;

		if (!entries[i].is_dir) {
			cslua_error("plugins/%s is not a folder, skipped - a plugin needs "
				"its own folder with manifest.lua and init.lua", entry.c_str());
			continue;
		}

		std::string plugin_dir = dir + "/" + entry;
		std::string manifest = plugin_dir + "/manifest.lua";
		std::string init = plugin_dir + "/init.lua";

		if (!cslua_file_exists(manifest)) {
			cslua_error("plugin folder '%s' has no manifest.lua, skipped", entry.c_str());
			continue;
		}
		if (!cslua_file_exists(init)) {
			cslua_error("plugin folder '%s' has no init.lua, skipped", entry.c_str());
			continue;
		}

		LuaPlugin plugin;
		plugin.id = entry;
		plugin.dir = plugin_dir;
		plugin.manifest = manifest;
		plugin.entry = init;
		plugin.name = plugin.id;
		m_plugins.push_back(plugin);
	}

	apply_load_order();
}

// Reads addons/lua/load_order.txt: one plugin id per line, blanks and lines
// starting with # ignored. Returns them in file order.
static std::vector<std::string> read_load_order()
{
	std::vector<std::string> order;

	FILE *fh = fopen((cslua_base_dir() + "/load_order.txt").c_str(), "r");
	if (!fh)
		return order;

	char line[256];
	while (fgets(line, sizeof line, fh)) {
		std::string name = line;

		size_t comment = name.find('#');
		if (comment != std::string::npos)
			name.erase(comment);

		while (!name.empty() && isspace((unsigned char)name.back()))
			name.pop_back();

		size_t first = 0;
		while (first < name.size() && isspace((unsigned char)name[first]))
			first++;
		name.erase(0, first);

		// A .lua on the end is what people type for a single-file plugin; the
		// id it loads under has no extension.
		if (name.size() > 4 && name.compare(name.size() - 4, 4, ".lua") == 0)
			name.erase(name.size() - 4);

		if (!name.empty())
			order.push_back(name);
	}

	fclose(fh);
	return order;
}

// Handlers run in the order they were registered, which is the order plugins
// load in. Alphabetical is fine until two plugins care who goes first - and on
// a cancellable event that decides who wins, since e:cancel() ends the chain.
//
// The lever for that belongs to whoever runs the server, not to plugin
// authors: a priority number in hook.add() only works until every plugin sets
// it, and then it means nothing again. load_order.txt is one file, in one
// place, that says exactly what happens.
void LuaEngine::apply_load_order()
{
	std::vector<std::string> order = read_load_order();

	// Position in the file, or the end for anything not listed.
	std::map<std::string, int> rank;
	for (size_t i = 0; i < order.size(); i++)
		rank[order[i]] = (int)i;

	const int unlisted = (int)order.size();

	std::sort(m_plugins.begin(), m_plugins.end(),
		[&rank, unlisted](const LuaPlugin &a, const LuaPlugin &b) {
			std::map<std::string, int>::const_iterator ra = rank.find(a.id);
			std::map<std::string, int>::const_iterator rb = rank.find(b.id);

			int pa = ra == rank.end() ? unlisted : ra->second;
			int pb = rb == rank.end() ? unlisted : rb->second;

			// Everything the file did not mention keeps sorting by name, so a
			// half-filled file still gives a predictable order.
			if (pa != pb)
				return pa < pb;
			return a.id < b.id;
		});

	// A name in the file that matches no plugin is almost always a typo or a
	// plugin that was removed; either way the file no longer says what the
	// server does.
	for (size_t i = 0; i < order.size(); i++) {
		bool found = false;
		for (size_t j = 0; j < m_plugins.size() && !found; j++)
			found = m_plugins[j].id == order[i];

		if (!found)
			cslua_print("load_order.txt names '%s', which is not installed",
				order[i].c_str());
	}

	if (!order.empty())
		m_ordered = (int)order.size();
}

void LuaEngine::load_plugin(int index)
{
	LuaPlugin &plugin = m_plugins[index];

	// The plugin's globals table: writes stay private, reads fall back to _G.
	lua_newtable(m_L);
	lua_newtable(m_L);
	lua_pushvalue(m_L, LUA_GLOBALSINDEX);
	lua_setfield(m_L, -2, "__index");
	lua_setmetatable(m_L, -2);

	lua_pushinteger(m_L, index);
	lua_pushcclosure(m_L, LuaEngine::l_require, 1);
	lua_setfield(m_L, -2, "require");

	plugin.env_ref = luaL_ref(m_L, LUA_REGISTRYINDEX);

	lua_newtable(m_L);
	plugin.modules_ref = luaL_ref(m_L, LUA_REGISTRYINDEX);

	m_loading = index;
	bool ok;
	{
		PluginScope scope(index);

		// manifest.lua runs first and has one job: call plugin{}. Its rules
		// (requires, api_version, ...) are enforced right there, before a
		// single line of init.lua runs.
		ok = run_file(plugin.manifest, plugin.env_ref, plugin.id.c_str());

		if (ok && !plugin.declared) {
			// A very easy typo: `plugin = { ... }` assigns a table instead of
			// calling plugin{ ... }, silently shadowing the manifest function.
			cslua_error("plugin '%s': manifest.lua must call plugin{} - "
				"did you write 'plugin = {' instead of 'plugin {'?", plugin.id.c_str());
			ok = false;
		}

		if (ok)
			ok = run_file(plugin.entry, plugin.env_ref, plugin.id.c_str());
	}
	m_loading = -1;

	if (!ok) {
		// Drop whatever it registered before blowing up, so we never run
		// half-initialized code. Its own on_unload handlers do not run: the
		// plugin never finished starting, so there is nothing it undid.
		plugin.failed = true;
		unload_plugin(index, false);
		return;
	}
}

bool LuaEngine::run_file(const std::string &path, int env_ref, const char *where)
{
	int errfunc = push_errfunc();

	if (luaL_loadfile(m_L, path.c_str()) != 0) {
		report_error(where);
		lua_remove(m_L, errfunc);
		return false;
	}

	lua_rawgeti(m_L, LUA_REGISTRYINDEX, env_ref);
	lua_setfenv(m_L, -2);

	bool ok = lua_pcall(m_L, 0, 0, errfunc) == 0;
	if (!ok)
		report_error(where);

	lua_remove(m_L, errfunc);
	return ok;
}
