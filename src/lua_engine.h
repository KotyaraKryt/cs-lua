#pragma once

#include <lua.hpp>

#include <string>
#include <vector>

// One loaded plugin. A plugin is either a folder with init.lua (the normal
// case) or a bare .lua file dropped into plugins/ (for throwaway scripts).
struct LuaPlugin
{
	std::string id;			// folder name, or file name without .lua
	std::string dir;		// plugin folder; empty for a bare .lua file
	std::string entry;		// full path of the file that gets executed

	std::string name;		// from plugin{}, defaults to id
	std::string version;
	std::string author;
	// Module names declared in plugin{ requires = {...} }. Not called `requires`
	// because that is a keyword from C++20 on.
	std::vector<std::string> required_modules;
	bool declared = false;				// plugin{} was actually called

	int env_ref = LUA_NOREF;		// the plugin's globals table
	int modules_ref = LUA_NOREF;	// require() cache for this plugin

	// Kept in the list even when it blows up during load, so lua_list can
	// show it as broken instead of silently hiding it.
	bool failed = false;
};

// Owns the single lua_State shared by all plugins. Every plugin gets its own
// environment table, and every file inside one plugin folder shares that same
// table - one folder is one namespace.
class LuaEngine
{
public:
	void init();
	void shutdown();
	void reload();

	lua_State *state() const { return m_L; }
	bool ready() const { return m_L != nullptr; }

	const std::vector<LuaPlugin> &plugins() const { return m_plugins; }

	// Index of the plugin currently being loaded, or -1. plugin{} uses it to
	// know which plugin it is describing.
	int loading_index() const { return m_loading; }
	LuaPlugin *loading_plugin() { return m_loading >= 0 ? &m_plugins[m_loading] : nullptr; }

	// Index of the plugin whose code is running right now - during load, but
	// also inside an event handler or a timer callback. Handlers and timers
	// are tagged with it so they can be dropped per plugin.
	int current_index() const { return m_current; }
	void set_current(int index) { m_current = index; }

	// Marks a plugin dead after it was shut down at runtime (for example for
	// blowing the precache budget), so lua_list shows it as failed.
	void mark_failed(int index);

	// Pushes the traceback message handler and returns its stack index.
	// Pass that index as pcall's errfunc.
	int push_errfunc();

	// Pops the error message left on the stack by a failed load/pcall and
	// reports it to the console.
	void report_error(const char *where);

	// Lua: require("lib.util") - resolved inside the calling plugin's folder,
	// then in the shared include/ directory.
	static int l_require(lua_State *L);

	// Finds the file a module name resolves to for the given plugin, searching
	// the plugin folder first, then the shared include/ directory. Returns the
	// path, or empty if nothing matches. Shared by require() and the requires
	// manifest check.
	std::string resolve_module(int plugin_index, const char *modname) const;

	// The same paths, formatted for an error message, so a failed require can
	// show exactly where it looked.
	std::string module_candidates(int plugin_index, const char *modname) const;

private:
	void load_core();
	void discover_plugins();
	void load_plugin(int index);
	bool run_file(const std::string &path, int env_ref, const char *where);
	int module_paths(int plugin_index, const char *modname, std::string out[4]) const;

	lua_State *m_L = nullptr;
	std::vector<LuaPlugin> m_plugins;
	int m_loading = -1;
	int m_current = -1;
};

extern LuaEngine g_lua;

// Marks whose code is running while a handler or timer callback executes.
class PluginScope
{
public:
	explicit PluginScope(int index) : m_prev(g_lua.current_index()) { g_lua.set_current(index); }
	~PluginScope() { g_lua.set_current(m_prev); }

private:
	int m_prev;
};
