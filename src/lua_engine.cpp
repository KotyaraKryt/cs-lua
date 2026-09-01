#include "cslua.h"
#include "lua_engine.h"
#include "lua_events.h"
#include "lua_natives.h"
#include "lua_player.h"
#include "lua_timers.h"
#include "lua_time.h"
#include "lua_cvar.h"
#include "lua_command.h"
#include "lua_sound.h"
#include "lua_menu.h"
#include "lua_message.h"
#include "lua_msg.h"
#include "lua_entity.h"
#include "lua_vector.h"
#include "lua_regex.h"
#include "lua_fx.h"
#include "lua_db.h"
#include "lua_file.h"
#include "lua_http.h"
#include "lua_mysql.h"
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

// A handler that throws does it every time. So the same error from the same
// place is printed a few times, then muted for a while, keyed by error text +
// place so a second failure is not hidden behind the first.
static const int ERROR_BURST = 3;
static const double ERROR_MUTE_SECONDS = 60.0;

struct ErrorRecord
{
	int seen;			// since last reported
	int muted;			// swallowed while muted
	double until;		// server time the mute expires
};

static std::map<std::string, ErrorRecord> s_errors;

// gpGlobals->time restarts every map; this only needs to be monotonic.
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

	// First line only for the key: the traceback below it repeats per call.
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

	// The console eats everything after the first newline; print line by line.
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

// Fills `out` with the paths a module name maps to, in search order. Plugin-
// local files win over the shared include/ directory.
int LuaEngine::module_paths(int plugin_index, const char *modname, std::string out[4]) const
{
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
		return luaL_error(L, "require: require() called outside of a plugin");

	LuaPlugin &plugin = g_lua.m_plugins[index];

	lua_rawgeti(L, LUA_REGISTRYINDEX, plugin.modules_ref);
	lua_getfield(L, -1, modname);
	if (!lua_isnil(L, -1)) {
		if (lua_isboolean(L, -1) && !lua_toboolean(L, -1))
			return luaL_error(L, "require: circular require of '%s' in plugin '%s'", modname, plugin.id.c_str());
		return 1;
	}
	lua_pop(L, 1);

	std::string path = g_lua.resolve_module(index, modname);
	if (path.empty()) {
		std::string tried = g_lua.module_candidates(index, modname);
		return luaL_error(L, "require: module '%s' not found for plugin '%s'. Looked in:%s",
			modname, plugin.id.c_str(), tried.c_str());
	}

	// Mark as in-flight so a cycle reports itself instead of hanging.
	lua_pushboolean(L, 0);
	lua_setfield(L, -2, modname);

	if (luaL_loadfile(L, path.c_str()) != 0)
		return luaL_error(L, "require: %s", lua_tostring(L, -1));

	// Every file of a plugin shares the plugin's environment.
	lua_rawgeti(L, LUA_REGISTRYINDEX, plugin.env_ref);
	lua_setfenv(L, -2);

	// Not pcall: let the error travel up so the traceback covers the chain.
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
	cslua_register_time(m_L);
	cslua_player_init(m_L);
	cslua_register_cvar(m_L);
	cslua_register_command_api(m_L);
	cslua_register_sound(m_L);
	cslua_register_ui(m_L);
	cslua_register_msg(m_L);
	cslua_register_menu(m_L);
	cslua_register_entity(m_L);
	cslua_register_vector(m_L);
	cslua_register_regex(m_L);
	cslua_register_fx(m_L);
	cslua_register_db(m_L);
	cslua_register_file(m_L);
	cslua_register_log(m_L);
	cslua_register_http(m_L);
	cslua_register_httpserver(m_L);
	cslua_register_mysql(m_L);

	// Only for code that reaches for the stock require; plugins get their own.
	const std::string &base = cslua_base_dir();
	lua_getglobal(m_L, "package");
	if (lua_istable(m_L, -1)) {
		std::string path = base + "/include/?.lua;" + base + "/include/?/init.lua";
		lua_pushstring(m_L, path.c_str());
		lua_setfield(m_L, -2, "path");
	}
	lua_pop(m_L, 1);

	// The core layer runs before any plugin and defines the shared base API.
	load_core();

	discover_plugins();
	for (size_t i = 0; i < m_plugins.size(); i++)
		load_plugin((int)i);

	// Now that every plugin has declared what it listens for, wire up hooks.
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
	// Plugins get told first, while everything they need still works. Guarded
	// because a handler is free to shut us down again (blowing the precache
	// budget does exactly that).
	if (m_L && !m_unloading) {
		m_unloading = true;
		g_events.fire_plugin_unload();
	}

	// Detach from the game DLL before the lua_State dies.
	cslua_regamedll_remove_hooks();

	// Disowns replies still on the wire; not a join.
	cslua_http_reset();
	cslua_httpserver_reset();
	cslua_mysql_reset();

	g_events.clear();
	cslua_errors_clear();
	cslua_timers_clear();
	cslua_command_shutdown();
	cslua_cvar_shutdown();
	cslua_sound_clear();
	cslua_player_shutdown();
	cslua_entity_shutdown();
	// The only one holding an OS resource: files closed for real, not left for
	// lua_close to forget.
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
// cleanup runs first, then everything else goes away.
void LuaEngine::unload_plugin(int index, bool run_handlers)
{
	LuaPlugin &plugin = m_plugins[index];

	if (run_handlers && m_L) {
		// Last in, first out.
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

	// Tell everyone else while this plugin's registrations are still in place:
	// the core export registry drops its entries here.
	if (run_handlers)
		g_events.fire_plugin_unload(plugin.id.c_str());

	g_events.remove_plugin(index);
	cslua_timers_remove_plugin(index);
	cslua_command_remove_plugin(index);
	cslua_db_remove_plugin(index);
	cslua_http_remove_plugin(index);
	cslua_httpserver_remove_plugin(index);
	cslua_mysql_remove_plugin(index);
}

// One plugin, torn down and started again. Precached resources are not released:
// the engine cannot take a model back out mid-round, and re-running precache
// calls just resolves to the slots it already owns.
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

	// A fresh environment and module cache: reloading has to pick up edits to
	// the plugin's lib/ files too.
	luaL_unref(m_L, LUA_REGISTRYINDEX, plugin.env_ref);
	luaL_unref(m_L, LUA_REGISTRYINDEX, plugin.modules_ref);
	plugin.env_ref = LUA_NOREF;
	plugin.modules_ref = LUA_NOREF;
	plugin.declared = false;
	plugin.failed = false;
	plugin.required_modules.clear();

	load_plugin(index);

	// A reloaded plugin may listen for something new; installing an already-
	// installed hook is a no-op.
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

	// Appended past every real plugin so its index cannot collide with one a
	// require() closure already captured by value.
	int index = (int)m_plugins.size();

	LuaPlugin plugin;
	plugin.id = id;
	plugin.dir = plugin_dir;
	plugin.manifest = manifest;
	plugin.entry = init;
	plugin.name = plugin.id;
	m_plugins.push_back(plugin);

	// Same environment setup as load_plugin(): its own globals table and
	// require() cache, so a check sees exactly what a real load would.
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

	// Nothing it registered is meant to outlive the check.
	unload_plugin(index, false);
	luaL_unref(m_L, LUA_REGISTRYINDEX, m_plugins[index].env_ref);
	luaL_unref(m_L, LUA_REGISTRYINDEX, m_plugins[index].modules_ref);
	m_plugins.pop_back();

	return ok;
}

// Starts plugins/<id> that is not currently running: either an existing
// lua_unload'ed slot (reuse it, with a fresh env/module cache) or a folder
// nobody has loaded this run (append a slot). Every other plugin's index is
// untouched.
bool LuaEngine::load_plugin_by_name(const char *id, std::string &out)
{
	if (!m_L) {
		out = "Lua state is not running";
		return false;
	}

	for (size_t i = 0; i < m_plugins.size(); i++) {
		if (m_plugins[i].id != id)
			continue;

		if (m_plugins[i].loaded) {
			out = "'" + std::string(id) + "' is already loaded - use lua_reload to restart it";
			return false;
		}

		LuaPlugin &plugin = m_plugins[i];
		luaL_unref(m_L, LUA_REGISTRYINDEX, plugin.env_ref);
		luaL_unref(m_L, LUA_REGISTRYINDEX, plugin.modules_ref);
		plugin.env_ref = LUA_NOREF;
		plugin.modules_ref = LUA_NOREF;
		plugin.declared = false;
		plugin.failed = false;
		plugin.required_modules.clear();

		load_plugin((int)i);
		plugin.loaded = true;
		cslua_regamedll_install_hooks();

		out = plugin.failed
			? "'" + std::string(id) + "' loaded with errors - see console"
			: "loaded '" + std::string(id) + "'";
		return !plugin.failed;
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
			" - both files are required";
		return false;
	}

	int index = (int)m_plugins.size();
	LuaPlugin plugin;
	plugin.id = id;
	plugin.dir = plugin_dir;
	plugin.manifest = manifest;
	plugin.entry = init;
	plugin.name = plugin.id;
	m_plugins.push_back(plugin);

	load_plugin(index);
	cslua_regamedll_install_hooks();

	LuaPlugin &loaded = m_plugins[index];
	out = loaded.failed
		? "'" + std::string(id) + "' loaded with errors - see console"
		: "loaded '" + std::string(id) + "'";
	return !loaded.failed;
}

// Tears the plugin down like a reload would, but does not run init.lua again -
// the slot sits marked unloaded until lua_load brings it back.
bool LuaEngine::unload_plugin_by_name(const char *id, std::string &out)
{
	if (!m_L) {
		out = "Lua state is not running";
		return false;
	}

	for (size_t i = 0; i < m_plugins.size(); i++) {
		if (m_plugins[i].id != id)
			continue;

		if (!m_plugins[i].loaded) {
			out = "'" + std::string(id) + "' is already unloaded";
			return false;
		}

		unload_plugin((int)i, true);
		m_plugins[i].loaded = false;

		// This plugin may have been the last listener for some hookchain; ask
		// again so it can be unregistered.
		cslua_regamedll_install_hooks();

		out = "unloaded '" + std::string(id) + "'";
		return true;
	}

	out = "no plugin named '" + std::string(id) + "' - run lua_list to see them";
	return false;
}

void LuaEngine::load_core()
{
	std::string dir = cslua_base_dir() + "/core";

	std::vector<CsLuaDirEntry> entries;
	if (!cslua_list_dir(dir, entries))
		return;			// no core layer installed

	std::vector<std::string> files;
	for (size_t i = 0; i < entries.size(); i++) {
		const std::string &name = entries[i].name;
		if (entries[i].is_dir || name.size() < 5)
			continue;
		if (name.compare(name.size() - 4, 4, ".lua") == 0)
			files.push_back(name);
	}

	std::sort(files.begin(), files.end());

	// Core runs in the global environment: it is meant to define globals that
	// everyone sees. Its handlers are tagged plugin index -1.
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

// Reads addons/lua/load_order.txt: one plugin id per line, blanks and # lines
// ignored. Returns them in file order.
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

		// A .lua on the end is what people type for a single-file plugin.
		if (name.size() > 4 && name.compare(name.size() - 4, 4, ".lua") == 0)
			name.erase(name.size() - 4);

		if (!name.empty())
			order.push_back(name);
	}

	fclose(fh);
	return order;
}

// Handlers run in plugin load order, which matters on a cancellable event. The
// lever belongs to whoever runs the server: load_order.txt, one file in one
// place.
void LuaEngine::apply_load_order()
{
	std::vector<std::string> order = read_load_order();

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

			// Anything the file did not mention keeps sorting by name.
			if (pa != pb)
				return pa < pb;
			return a.id < b.id;
		});

	// A name in the file that matches no plugin is almost always a typo.
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

		// manifest.lua runs first and has one job: call plugin{}.
		ok = run_file(plugin.manifest, plugin.env_ref, plugin.id.c_str());

		if (ok && !plugin.declared) {
			cslua_error("plugin '%s': manifest.lua must call plugin{} - "
				"did you write 'plugin = {' instead of 'plugin {'?", plugin.id.c_str());
			ok = false;
		}

		if (ok)
			ok = run_file(plugin.entry, plugin.env_ref, plugin.id.c_str());
	}
	m_loading = -1;

	if (!ok) {
		// Drop whatever it registered before blowing up. Its on_unload handlers
		// do not run: it never finished starting.
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
