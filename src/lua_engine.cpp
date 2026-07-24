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
#include "regamedll.h"
#include "platform.h"

#include <algorithm>

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

void LuaEngine::report_error(const char *where)
{
	const char *msg = lua_tostring(m_L, -1);
	cslua_error("error in %s:", where);

	// The traceback is multi-line; the console eats everything after the
	// first newline, so print it line by line.
	std::string text = msg ? msg : "(unknown error)";
	size_t start = 0;
	while (start <= text.size()) {
		size_t end = text.find('\n', start);
		if (end == std::string::npos)
			end = text.size();
		if (end > start)
			cslua_error("  %s", text.substr(start, end - start).c_str());
		start = end + 1;
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
	cslua_player_init(m_L);
	cslua_register_cvar(m_L);
	cslua_register_command_api(m_L);
	cslua_register_sound(m_L);
	cslua_register_menu(m_L);

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
	// Detach from the game DLL before the lua_State dies, or a spawn/kill hook
	// could fire into freed handler refs.
	cslua_regamedll_remove_hooks();

	g_events.clear();
	cslua_timers_clear();
	cslua_command_shutdown();
	cslua_cvar_shutdown();
	cslua_sound_clear();
	cslua_player_shutdown();
	m_plugins.clear();
	m_loading = -1;
	m_current = -1;

	if (m_L) {
		lua_close(m_L);
		m_L = nullptr;
	}
}

void LuaEngine::reload()
{
	shutdown();
	init();
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

		LuaPlugin plugin;

		if (entries[i].is_dir) {
			std::string init = dir + "/" + entry + "/init.lua";
			if (!cslua_file_exists(init)) {
				cslua_error("plugin folder '%s' has no init.lua, skipped", entry.c_str());
				continue;
			}
			plugin.id = entry;
			plugin.dir = dir + "/" + entry;
			plugin.entry = init;
		} else {
			size_t dot = entry.rfind(".lua");
			if (dot == std::string::npos || dot != entry.size() - 4)
				continue;
			// A bare script has no folder of its own, so require() only sees
			// the shared include/ directory.
			plugin.id = entry.substr(0, dot);
			plugin.entry = dir + "/" + entry;
		}

		plugin.name = plugin.id;
		m_plugins.push_back(plugin);
	}

	// Deterministic load order.
	std::sort(m_plugins.begin(), m_plugins.end(),
		[](const LuaPlugin &a, const LuaPlugin &b) { return a.id < b.id; });
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
		ok = run_file(plugin.entry, plugin.env_ref, plugin.id.c_str());
	}
	m_loading = -1;

	if (!ok) {
		// Drop whatever it registered before blowing up, so we never run
		// half-initialized code.
		plugin.failed = true;
		g_events.remove_plugin(index);
		cslua_timers_remove_plugin(index);
		return;
	}

	// A very easy typo: `plugin = { ... }` assigns a table instead of calling
	// plugin{ ... }. The plugin still runs, but its metadata never arrives and
	// the plugin function itself gets shadowed - worth saying out loud.
	if (!plugin.declared)
		cslua_print("note: plugin '%s' never called plugin{} - no metadata. "
			"Did you write 'plugin = {' instead of 'plugin {'?", plugin.id.c_str());
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
