#include "cslua.h"
#include "lua_command.h"
#include "lua_engine.h"
#include "lua_natives.h"

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

// Names already handed to the engine. AddServerCommand keeps the pointer and
// never copies it, so the string must outlive the server.
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

// The single entry point the engine calls for every Lua command; dispatches
// by argv[0].
static void command_trampoline()
{
	const char *name = CMD_ARGV(0);
	if (!name)
		return;

	Command *cmd = find_command(name);
	if (!cmd || !cmd->live)
		return;			// plugin gone after a reload

	lua_State *L = g_lua.state();
	if (!L)
		return;

	PluginScope scope(cmd->plugin);
	int errfunc = g_lua.push_errfunc();

	lua_rawgeti(L, LUA_REGISTRYINDEX, cmd->ref);

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
		// Re-registered (typically after a reload): swap the handler.
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
		char *stable = cslua_strdup(name);
		s_engine_registered.push_back(stable);
		REG_SVR_COMMAND(stable, command_trampoline);
	}
	return 0;
}

void cslua_register_command_api(lua_State *L)
{
	// Low-level primitive; core/commands.lua builds the friendly cmd.add() on
	// top of this and clears this entry.
	static const luaL_Reg s_internal[] =
	{
		{ "_register", l_command },
		{ NULL, NULL }
	};

	cslua_register_namespace(L, "cmd", s_internal);
}

void cslua_command_remove_plugin(int plugin_index)
{
	lua_State *L = g_lua.state();

	for (size_t i = 0; i < s_commands.size(); i++) {
		if (s_commands[i].plugin != plugin_index || !s_commands[i].live)
			continue;

		if (L)
			luaL_unref(L, LUA_REGISTRYINDEX, s_commands[i].ref);
		s_commands[i].ref = LUA_NOREF;
		s_commands[i].live = false;
	}
}

void cslua_command_shutdown()
{
	lua_State *L = g_lua.state();

	// Refs die with the state; mark every command dormant. Names re-registered
	// on the next load flip back to live.
	for (size_t i = 0; i < s_commands.size(); i++) {
		if (L)
			luaL_unref(L, LUA_REGISTRYINDEX, s_commands[i].ref);
		s_commands[i].ref = LUA_NOREF;
		s_commands[i].live = false;
	}
}
