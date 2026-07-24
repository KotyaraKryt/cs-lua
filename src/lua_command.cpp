#include "cslua.h"
#include "lua_command.h"
#include "lua_engine.h"

#include <string.h>
#include <string>
#include <vector>

struct Command
{
	std::string name;
	int ref;			// registry ref to the Lua handler
	int plugin;			// index into LuaEngine::plugins()
	bool live;			// false after reload until re-registered this load
};

static std::vector<Command> s_commands;

// Names already handed to the engine. AddServerCommand keeps the pointer we
// pass and never copies it, so the string must outlive the server - a plain
// strdup that we never free is exactly right, since commands are permanent.
static std::vector<char *> s_engine_registered;

static Command *find_command(const char *name)
{
	for (size_t i = 0; i < s_commands.size(); i++)
		if (s_commands[i].name == name)
			return &s_commands[i];
	return nullptr;
}

static char *engine_name(const char *name)
{
	for (size_t i = 0; i < s_engine_registered.size(); i++)
		if (!strcmp(s_engine_registered[i], name))
			return s_engine_registered[i];
	return nullptr;
}

// The single entry point the engine calls for every Lua command. It figures
// out which command ran from argv[0] and dispatches to the matching handler.
static void command_trampoline()
{
	const char *name = CMD_ARGV(0);
	if (!name)
		return;

	Command *cmd = find_command(name);
	if (!cmd || !cmd->live)
		return;			// plugin gone after a reload: silently ignore

	lua_State *L = g_lua.state();
	if (!L)
		return;

	PluginScope scope(cmd->plugin);
	int errfunc = g_lua.push_errfunc();

	lua_rawgeti(L, LUA_REGISTRYINDEX, cmd->ref);

	// Hand the handler an array of the arguments after the command name.
	lua_newtable(L);
	int argc = CMD_ARGC();
	for (int i = 1; i < argc; i++) {
		lua_pushstring(L, CMD_ARGV(i));
		lua_rawseti(L, -2, i);
	}

	if (lua_pcall(L, 1, 0, errfunc) != 0) {
		const std::vector<LuaPlugin> &plugins = g_lua.plugins();
		const char *who = (cmd->plugin >= 0 && cmd->plugin < (int)plugins.size())
			? plugins[cmd->plugin].id.c_str() : "?";
		g_lua.report_error(who);
	}

	lua_remove(L, errfunc);
}

static int l_command(lua_State *L)
{
	const char *name = luaL_checkstring(L, 1);
	luaL_checktype(L, 2, LUA_TFUNCTION);

	lua_pushvalue(L, 2);
	int ref = luaL_ref(L, LUA_REGISTRYINDEX);

	Command *existing = find_command(name);
	if (existing) {
		// Same command re-registered (typically after a reload): swap the
		// handler, drop the old ref, keep the engine registration.
		luaL_unref(L, LUA_REGISTRYINDEX, existing->ref);
		existing->ref = ref;
		existing->plugin = g_lua.current_index();
		existing->live = true;
		return 0;
	}

	Command cmd;
	cmd.name = name;
	cmd.ref = ref;
	cmd.plugin = g_lua.current_index();
	cmd.live = true;
	s_commands.push_back(cmd);

	if (!engine_name(name)) {
		char *stable = _strdup(name);
		s_engine_registered.push_back(stable);
		REG_SVR_COMMAND(stable, command_trampoline);
	}
	return 0;
}

void cslua_register_command_api(lua_State *L)
{
	// Low-level primitive: registers a raw server console command whose handler
	// gets the argument array. The friendly, unified command() that also covers
	// chat and builds a ctx lives in core/commands.lua on top of this.
	lua_pushcfunction(L, l_command);
	lua_setglobal(L, "_server_command");
}

void cslua_command_shutdown()
{
	lua_State *L = g_lua.state();

	// Refs die with the state; mark every command dormant so a stray engine
	// call before the next load does nothing. Names re-registered on the next
	// load flip back to live.
	for (size_t i = 0; i < s_commands.size(); i++) {
		if (L)
			luaL_unref(L, LUA_REGISTRYINDEX, s_commands[i].ref);
		s_commands[i].ref = LUA_NOREF;
		s_commands[i].live = false;
	}
}
