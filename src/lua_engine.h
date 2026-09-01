#pragma once

#include <lua.hpp>

#include <string>
#include <vector>

// One loaded plugin: a folder under plugins/ with manifest.lua (declares the
// plugin - name, api_version, requires) and init.lua (the code). The split is
// enforced by load_plugin().
struct LuaPlugin
{
	std::string id;			// folder name
	std::string dir;		// plugin folder, full path
	std::string manifest;	// full path of manifest.lua
	std::string entry;		// full path of init.lua

	std::string name;		// from plugin{}, defaults to id
	std::string version;
	std::string author;
	// From plugin{ requires = {...} }. Not `requires` - a keyword from C++20 on.
	std::vector<std::string> required_modules;
	bool declared = false;				// manifest.lua called plugin{}

	int env_ref = LUA_NOREF;		// the plugin's globals table
	int modules_ref = LUA_NOREF;	// require() cache for this plugin

	// plugin.on_unload(fn) handlers, in registration order. Per plugin: a
	// single-plugin reload runs only its own.
	std::vector<int> unload_refs;

	// Kept in the list even when it blows up during load, so lua_list can show
	// it as broken.
	bool failed = false;

	// False after `lua_unload <plugin>`: torn down, slot kept (erasing would
	// shift every other index) so `lua_load` can start it back up in place.
	bool loaded = true;
};

// Owns the single lua_State shared by all plugins. Every plugin gets its own
// environment table; every file in one plugin folder shares that table.
class LuaEngine
{
public:
	void init();
	void shutdown();
	void reload();

	// Tears one plugin down and runs its entry file again, leaving the others
	// untouched. Returns false when nothing is named `id`. Behind
	// `lua_reload <plugin>`.
	bool reload_plugin(const char *id);

	// Runs only manifest.lua for plugins/<id> in a disposable slot, to catch a
	// bad plugin{} call or a missing require before `lua_reload` starts it.
	// Behind `lua_check <plugin>`. Reason goes in `out`.
	bool check_plugin(const char *id, std::string &out);

	// Starts a plugin not currently running - brand new, or previously
	// lua_unload'ed. Every other plugin's index stays put. `out` explains what
	// happened either way. Behind `lua_load <plugin>`.
	bool load_plugin_by_name(const char *id, std::string &out);

	// The other half: tears one plugin down like a reload would, but does not
	// start it back up. Behind `lua_unload <plugin>`.
	bool unload_plugin_by_name(const char *id, std::string &out);

	// A reload requested from inside a route handler cannot call reload()
	// directly - that would tear down the m_L the handler is running on. The
	// request is recorded here and carried out by process_pending_reload() from
	// the top of the next frame. request_plugin_reload validates the name up
	// front.
	void request_full_reload() { m_pending_full_reload = true; m_pending_plugin_reload.clear(); }
	bool request_plugin_reload(const char *id);
	void process_pending_reload();

	// Registered by plugin.on_unload(). The engine owns the ref from here on.
	void add_unload_handler(int plugin_index, int ref);

	lua_State *state() const { return m_L; }
	bool ready() const { return m_L != nullptr; }

	const std::vector<LuaPlugin> &plugins() const { return m_plugins; }

	// How many got their position from load_order.txt; the rest are alphabetical.
	int ordered_count() const { return m_ordered; }

	// Index of the plugin currently being loaded, or -1.
	int loading_index() const { return m_loading; }
	LuaPlugin *loading_plugin() { return m_loading >= 0 ? &m_plugins[m_loading] : nullptr; }

	// Index of the plugin whose code is running right now - during load, and
	// inside an event handler or timer callback. Handlers/timers are tagged
	// with it so they can be dropped per plugin.
	int current_index() const { return m_current; }
	void set_current(int index) { m_current = index; }

	// Marks a plugin dead after it was shut down at runtime (e.g. for blowing
	// the precache budget).
	void mark_failed(int index);

	// Pushes the traceback message handler; pass its index as pcall's errfunc.
	int push_errfunc();

	// Pops the error message left on the stack by a failed load/pcall and
	// reports it to the console.
	void report_error(const char *where);

	// Lua: require("lib.util") - resolved inside the calling plugin's folder,
	// then in the shared include/ directory.
	static int l_require(lua_State *L);

	// The file a module name resolves to for the given plugin (plugin folder
	// first, then include/), or empty if nothing matches.
	std::string resolve_module(int plugin_index, const char *modname) const;

	// The same paths formatted for an error message.
	std::string module_candidates(int plugin_index, const char *modname) const;

private:
	void load_core();
	void discover_plugins();
	void load_plugin(int index);

	// Puts m_plugins in load_order.txt order, everything unmentioned after that
	// by name.
	void apply_load_order();
	bool run_file(const std::string &path, int env_ref, const char *where);
	int module_paths(int plugin_index, const char *modname, std::string out[4]) const;

	// Runs the plugin's own on_unload handlers, then drops everything it
	// registered. Shared by a single-plugin reload and the failed-load path.
	void unload_plugin(int index, bool run_handlers);

	lua_State *m_L = nullptr;
	std::vector<LuaPlugin> m_plugins;
	int m_loading = -1;
	int m_current = -1;

	int m_ordered = 0;

	// True from the moment plugin_unload starts firing until the state is gone,
	// so a handler that triggers another shutdown cannot make it fire twice.
	bool m_unloading = false;

	bool m_pending_full_reload = false;
	std::string m_pending_plugin_reload;
};

extern LuaEngine g_lua;

// Forgets which errors have already been printed. Called when the state is
// rebuilt.
void cslua_errors_clear();

// Marks whose code is running while a handler or timer callback executes.
class PluginScope
{
public:
	explicit PluginScope(int index) : m_prev(g_lua.current_index()) { g_lua.set_current(index); }
	~PluginScope() { g_lua.set_current(m_prev); }

private:
	int m_prev;
};
