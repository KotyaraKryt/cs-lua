#include "cslua.h"
#include "commands.h"
#include "lua_engine.h"
#include "lua_events.h"
#include "lua_message.h"
#include "lua_timers.h"
#include "lua_sound.h"
#include "lua_db.h"
#include "lua_http.h"
#include "regamedll.h"
#include "rehlds.h"
#include "players.h"

#include <algorithm>

// Server commands run between frames, never from inside a hook, so tearing
// the Lua state down here is safe.
//
//   lua_reload            everything: the whole Lua state comes back fresh
//   lua_reload <plugin>   just that one, leaving the others running
//
// The single-plugin form is the edit-test loop. The full one is still what you
// want after touching core/ or include/, which every plugin shares.
static void cmd_lua_reload()
{
	const char *who = CMD_ARGC() > 1 ? CMD_ARGV(1) : NULL;

	if (!who) {
		cslua_print("reloading plugins...");
		g_lua.reload();
		return;
	}

	if (!g_lua.ready()) {
		cslua_print("Lua state is not running");
		return;
	}

	if (!g_lua.reload_plugin(who)) {
		cslua_print("no plugin named '%s' - run lua_list to see them", who);
		return;
	}

	cslua_print("reloaded '%s'", who);
}

// lua_check <plugin> - runs plugins/<plugin>/manifest.lua on its own, without
// touching init.lua or the running plugin list, so a bad plugin{} call or a
// missing require shows up before `lua_reload` actually starts the thing.
static void cmd_lua_check()
{
	if (CMD_ARGC() < 2) {
		cslua_print("usage: lua_check <plugin>");
		return;
	}

	if (!g_lua.ready()) {
		cslua_print("Lua state is not running");
		return;
	}

	std::string result;
	g_lua.check_plugin(CMD_ARGV(1), result);
	cslua_print("%s", result.c_str());
}

// lua_load <plugin> - starts a plugin that is not currently running: a
// folder that appeared under plugins/ after the server came up (never
// discovered at startup, so `lua_reload <plugin>` has nothing to find), or
// one a previous `lua_unload` stopped. Everything else already running
// keeps running untouched - unlike a bare `lua_reload`, which tears down
// and restarts the whole state.
static void cmd_lua_load()
{
	if (CMD_ARGC() < 2) {
		cslua_print("usage: lua_load <plugin>");
		return;
	}

	std::string result;
	g_lua.load_plugin_by_name(CMD_ARGV(1), result);
	cslua_print("%s", result.c_str());
}

// lua_unload <plugin> - stops one plugin (its own on_unload handlers run,
// then every handler/timer/command/db it registered goes away) without
// touching any other plugin or restarting the Lua state. `lua_load` brings
// it back later.
static void cmd_lua_unload()
{
	if (CMD_ARGC() < 2) {
		cslua_print("usage: lua_unload <plugin>");
		return;
	}

	std::string result;
	g_lua.unload_plugin_by_name(CMD_ARGV(1), result);
	cslua_print("%s", result.c_str());
}

static void cmd_lua_list()
{
	if (!g_lua.ready()) {
		cslua_print("Lua state is not running");
		return;
	}

	const std::vector<LuaPlugin> &plugins = g_lua.plugins();

	int ordered = g_lua.ordered_count();
	if (ordered > 0)
		cslua_print("%d plugin(s), first %d placed by load_order.txt:",
			(int)plugins.size(), ordered);
	else
		cslua_print("%d plugin(s):", (int)plugins.size());

	for (size_t i = 0; i < plugins.size(); i++) {
		const LuaPlugin &p = plugins[i];

		// Per plugin, not just server-wide: the totals below say something is
		// leaking handlers, this column says which plugin.
		cslua_print("  [%d] %-16s %-22s v%-6s h:%-3d t:%-2d db:%-2d %s%s",
			(int)i + 1,
			p.id.c_str(),
			p.name.c_str(),
			p.version.empty() ? "?" : p.version.c_str(),
			g_events.count_for_plugin((int)i),
			cslua_timers_count_for_plugin((int)i),
			cslua_db_count_for_plugin((int)i),
			p.author.empty() ? "?" : p.author.c_str(),
			!p.loaded ? "   [UNLOADED]" : p.failed ? "   [FAILED]" : "");
	}

	cslua_print("  (h = handlers, t = timers, db = open databases)");

	cslua_print("client: connect=%d disconnect=%d ready=%d chat=%d menu=%d, timers=%d",
		g_events.count(CSLUA_EVENT_CLIENT_CONNECT),
		g_events.count(CSLUA_EVENT_CLIENT_DISCONNECT),
		g_events.count(CSLUA_EVENT_PLAYER_READY),
		g_events.count(CSLUA_EVENT_PLAYER_CHAT),
		g_events.count(CSLUA_EVENT_MENU_SELECT),
		cslua_timers_count());

	cslua_print("lifetime: map_change=%d plugin_unload=%d, databases open=%d, http pending=%d",
		g_events.count(CSLUA_EVENT_MAP_CHANGE),
		g_events.count(CSLUA_EVENT_PLUGIN_UNLOAD),
		cslua_db_open_count(),
		cslua_http_pending());

	cslua_print("gameplay: spawn=%d hurt=%d/%d death=%d team=%d fire=%d round=%d/%d/%d (ReGameDLL %s)",
		g_events.count(CSLUA_EVENT_PLAYER_SPAWN),
		g_events.count(CSLUA_EVENT_PLAYER_HURT),
		g_events.count(CSLUA_EVENT_PLAYER_HURT_POST),
		g_events.count(CSLUA_EVENT_PLAYER_DEATH),
		g_events.count(CSLUA_EVENT_PLAYER_TEAM_CHANGE),
		g_events.count(CSLUA_EVENT_WEAPON_FIRE),
		g_events.count(CSLUA_EVENT_ROUND_START),
		g_events.count(CSLUA_EVENT_ROUND_END),
		g_events.count(CSLUA_EVENT_ROUND_FREEZE_END),
		cslua_regamedll_ready() ? cslua_regamedll_version() : "off");

	cslua_print("bomb: planted=%d defused=%d exploded=%d",
		g_events.count(CSLUA_EVENT_BOMB_PLANTED),
		g_events.count(CSLUA_EVENT_BOMB_DEFUSED),
		g_events.count(CSLUA_EVENT_BOMB_EXPLODED));
}

// Which handler is eating the frame. The question every server owner asks when
// it starts stuttering, and the one AMXX cannot answer at all.
//
// Off by default (cslua_profile 0): the clock read per handler is cheap but not
// free. Turn it on, play a round, read this, turn it back off.
static void cmd_lua_profile()
{
	if (!g_lua.ready()) {
		cslua_print("Lua state is not running");
		return;
	}

	if (CMD_ARGC() > 1 && !strcmp(CMD_ARGV(1), "reset")) {
		g_events.profile_reset();
		cslua_print("profile counters cleared");
		return;
	}

	if (!cslua_profiling()) {
		cslua_print("profiling is off - set cslua_profile 1, play a bit, then run this again");
		return;
	}

	std::vector<LuaEvents::ProfileRow> rows;
	g_events.profile_snapshot(rows);

	if (rows.empty()) {
		cslua_print("nothing measured yet: no handler has run since profiling was turned on");
		return;
	}

	// Heaviest first: the answer is almost always the top line.
	std::sort(rows.begin(), rows.end(),
		[](const LuaEvents::ProfileRow &a, const LuaEvents::ProfileRow &b) {
			return a.seconds > b.seconds;
		});

	double total = 0.0;
	for (size_t i = 0; i < rows.size(); i++)
		total += rows[i].seconds;

	cslua_print("%d handler(s) measured, %.1f ms total:", (int)rows.size(), total * 1000.0);

	for (size_t i = 0; i < rows.size() && i < 20; i++) {
		const LuaEvents::ProfileRow &r = rows[i];

		// Per call is what tells a heavy handler apart from a busy one: a
		// weapon_fire hook runs thousands of times and should still be cheap.
		cslua_print("  %8.2f ms  %6d calls  %7.3f ms/call  %s:%s (%s)",
			r.seconds * 1000.0, r.calls,
			r.calls ? (r.seconds * 1000.0 / r.calls) : 0.0,
			r.plugin.c_str(), r.id.c_str(), r.event.c_str());
	}

	if (rows.size() > 20)
		cslua_print("  ... and %d more", (int)rows.size() - 20);
}

// Joins the command's arguments back into one string.
static std::string command_text()
{
	std::string text;
	for (int i = 1; i < CMD_ARGC(); i++) {
		if (i > 1)
			text += ' ';
		text += CMD_ARGV(i);
	}
	return text;
}

// The three below exist to test output without writing a plugin: everyone
// currently on the server gets the message right now.
static void cmd_lua_chat()
{
	std::string text = command_text();
	if (text.empty()) {
		cslua_print("usage: lua_chat <text>   (colour tags: {default} {team} {green})");
		return;
	}
	cslua_send_chat(0, text.c_str(), 0);
}

// lua_chat_as <id> <text> - send a chat line as if player <id> said it.
// Handy to see what {team} does: the colour follows the quoted speaker, so a
// message "from" a T shows red even to a CT reading it.
static void cmd_lua_chat_as()
{
	if (CMD_ARGC() < 3) {
		cslua_print("usage: lua_chat_as <playerid> <text>");
		return;
	}

	int from = atoi(CMD_ARGV(1));
	if (!g_players.is_connected(from)) {
		cslua_print("lua_chat_as: nobody in slot %d", from);
		return;
	}

	std::string text;
	for (int i = 2; i < CMD_ARGC(); i++) {
		if (i > 2)
			text += ' ';
		text += CMD_ARGV(i);
	}

	cslua_send_chat(0, text.c_str(), from);
}

// Sends the word "Привет" twice with known bytes: once as UTF-8 (what a .lua
// file normally contains) and once as CP1251 (what the client speaks). Whatever
// shows up on screen tells us which path is broken.
static void cmd_lua_test_ru()
{
	static const char utf8[]   = "UTF8: \xD0\x9F\xD1\x80\xD0\xB8\xD0\xB2\xD0\xB5\xD1\x82";
	static const char cp1251[] = "CP1251: \xCF\xF0\xE8\xE2\xE5\xF2";

	cslua_print("sending UTF-8 sample (%d bytes) and CP1251 sample (%d bytes)",
		(int)strlen(utf8), (int)strlen(cp1251));

	cslua_send_chat(0, utf8, 0);
	cslua_send_chat(0, cp1251, 0);
}

static void cmd_lua_hud()
{
	std::string text = command_text();
	if (text.empty()) {
		cslua_print("usage: lua_hud <text>");
		return;
	}

	HudParams params;
	cslua_send_hud(0, text.c_str(), params);
}

static void cmd_lua_dhud()
{
	std::string text = command_text();
	if (text.empty()) {
		cslua_print("usage: lua_dhud <text>");
		return;
	}

	HudParams params;
	cslua_send_dhud(0, text.c_str(), params);
}

// Answers the only question that matters when output does not show up: is the
// message being sent at all, and does the client render it?
static void cmd_lua_debug()
{
	cslua_print("SayText id=%d, TextMsg id=%d, maxClients=%d",
		cslua_msg_saytext_id(), cslua_msg_textmsg_id(),
		gpGlobals ? gpGlobals->maxClients : -1);

	int online = 0;
	for (int id = 1; id < CSLUA_MAXPLAYERS; id++) {
		if (!g_players.is_connected(id))
			continue;

		edict_t *e = g_engfuncs.pfnPEntityOfEntIndex(id);
		cslua_print("  slot %d: name='%s' steamid=%s edict=%s",
			id, g_players.name(id), g_players.authid(id),
			(e && !e->free) ? "ok" : "BAD");
		online++;
	}

	cslua_print("%d player(s) in the cache; sending 4 chat probes", online);
	cslua_chat_probe(0);
}

// The number AMXX never shows you: how close the server is to the 512 precache
// wall, and how much of it came from Lua plugins.
static void cmd_lua_precache()
{
	int limit = cslua_precache_limit();
	int sounds = cslua_precache_used(false);
	int models = cslua_precache_used(true);

	if (sounds < 0 && models < 0) {
		cslua_print("precache: nothing precached yet (counts appear at map start)");
		return;
	}

	cslua_print("precache sounds: %d/%d used, %d free (%d registered by plugins)",
		sounds + 1, limit, limit - (sounds + 1), cslua_precache_registered(false));
	cslua_print("precache models: %d/%d used, %d free (%d registered by plugins)",
		models + 1, limit, limit - (models + 1), cslua_precache_registered(true));

	// Be straight about how good these numbers are.
	if (cslua_rehlds_ready()) {
		cslua_print("counts are exact: every precache on the server is tracked (ReHLDS API %s)",
			cslua_rehlds_version());
	} else {
		cslua_print("counts are a floor, not exact: without the ReHLDS API only our");
		cslua_print("  own precaches are visible, the game DLL may have added more");
	}
}

void cslua_register_commands()
{
	REG_SVR_COMMAND("lua_reload", cmd_lua_reload);
	REG_SVR_COMMAND("lua_load", cmd_lua_load);
	REG_SVR_COMMAND("lua_unload", cmd_lua_unload);
	REG_SVR_COMMAND("lua_check", cmd_lua_check);
	REG_SVR_COMMAND("lua_list", cmd_lua_list);
	REG_SVR_COMMAND("lua_profile", cmd_lua_profile);
	REG_SVR_COMMAND("lua_debug", cmd_lua_debug);
	REG_SVR_COMMAND("lua_precache", cmd_lua_precache);
	REG_SVR_COMMAND("lua_chat", cmd_lua_chat);
	REG_SVR_COMMAND("lua_chat_as", cmd_lua_chat_as);
	REG_SVR_COMMAND("lua_test_ru", cmd_lua_test_ru);
	REG_SVR_COMMAND("lua_hud", cmd_lua_hud);
	REG_SVR_COMMAND("lua_dhud", cmd_lua_dhud);
}
