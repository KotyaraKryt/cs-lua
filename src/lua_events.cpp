#include "cslua.h"
#include "lua_events.h"
#include "lua_engine.h"
#include "lua_natives.h"
#include "lua_player.h"
#include "lua_entity.h"

#include <algorithm>
#include <string.h>

LuaEvents g_events;

// Order must match the CsLuaEvent enum exactly.
// subject:verb - colon, not dot, since a dot already means "plugin's own event".
static const char *const s_event_names[CSLUA_EVENT_COUNT] =
{
	"client:connect",
	"client:disconnect",
	"client:authorized",
	"client:connected",
	"player:chat",
	"menu:select",
	"server:map_change",
	"module:plugin_unload",
	"player:spawn",
	"player:hurt",
	"player:hurt_post",
	"player:death",
	"player:team_change",
	"weapon:fire",
	"weapon:deploy",
	"weapon:reload",
	"round:start",
	"round:end",
	"round:freeze_end",
	"bomb:planted",
	"bomb:defused",
	"bomb:exploded",
	"grenade:throw",
	"grenade:thrown",
	"grenade:explode",
	"weapon:throw",
	"weapon:secondary_attack",
	"weapon:buy",
	"ammo:buy",
	"item:buy",
	"player:money_change",
	"ammo:pickup",
	"weapon:drop",
	"player:jump",
	"player:duck",
	"player:spectate",
	"player:radio",
	"player:respawn_check",
	"bomb:defusing",
};

static int find_event(const char *name)
{
	for (int i = 0; i < CSLUA_EVENT_COUNT; i++)
		if (!strcmp(name, s_event_names[i]))
			return i;
	return -1;
}

// The events whose outcome a handler can change. Everything else is a
// notification: e:cancel() on one of those says so instead of quietly doing
// nothing, which is the kind of bug that takes an evening to find.
static bool is_cancellable(int ev)
{
	return ev == CSLUA_EVENT_CLIENT_CONNECT
		|| ev == CSLUA_EVENT_PLAYER_CHAT
		|| ev == CSLUA_EVENT_PLAYER_HURT
		|| ev == CSLUA_EVENT_GRENADE_EXPLODE
		|| ev == CSLUA_EVENT_WEAPON_THROW
		// Bug fix: these three were added (commit cae02d9) without adding
		// them here, so e:cancel() on any of them raised "event cannot be
		// cancelled" despite the docs promising it blocks the purchase.
		|| ev == CSLUA_EVENT_WEAPON_BUY
		|| ev == CSLUA_EVENT_AMMO_BUY
		|| ev == CSLUA_EVENT_ITEM_BUY
		|| ev == CSLUA_EVENT_MONEY_CHANGE
		|| ev == CSLUA_EVENT_AMMO_PICKUP
		|| ev == CSLUA_EVENT_WEAPON_DROP
		|| ev == CSLUA_EVENT_PLAYER_RADIO
		|| ev == CSLUA_EVENT_PLAYER_CAN_RESPAWN;
}

static std::string known_events()
{
	std::string list;
	for (int i = 0; i < CSLUA_EVENT_COUNT; i++) {
		if (i) list += ", ";
		list += s_event_names[i];
	}
	return list;
}

// Holds the dispatch depth for the length of one walk, and sweeps on the way
// out of the outermost one.
namespace {
struct DispatchScope
{
	explicit DispatchScope(int &depth) : m_depth(depth) { m_depth++; }
	~DispatchScope() { m_depth--; }

private:
	int &m_depth;
};
}

// ---------------------------------------------------------------------------
// The event object

static int l_event_cancel(lua_State *L)
{
	luaL_checktype(L, 1, LUA_TTABLE);
	lua_pushboolean(L, 1);
	lua_setfield(L, 1, "cancelled");
	return 0;
}

// Same method on a notify event. Raising beats returning quietly: a script
// that calls cancel() believes it changed something.
static int l_event_cancel_refused(lua_State *L)
{
	luaL_checktype(L, 1, LUA_TTABLE);

	lua_getfield(L, 1, "name");
	const char *name = lua_tostring(L, -1);

	return luaL_error(L, "e:cancel: event '%s' cannot be cancelled - it reports something "
		"that already happened", name ? name : "?");
}

void LuaEvents::init(lua_State *L)
{
	// Cancellable events.
	lua_newtable(L);					// metatable
	lua_newtable(L);					// __index
	lua_pushcfunction(L, l_event_cancel);
	lua_setfield(L, -2, "cancel");
	lua_setfield(L, -2, "__index");
	m_mt_cancellable = luaL_ref(L, LUA_REGISTRYINDEX);

	// Notify events: same shape, cancel() explains itself.
	lua_newtable(L);
	lua_newtable(L);
	lua_pushcfunction(L, l_event_cancel_refused);
	lua_setfield(L, -2, "cancel");
	lua_setfield(L, -2, "__index");
	m_mt_notify = luaL_ref(L, LUA_REGISTRYINDEX);
}

void LuaEvents::push_event(lua_State *L, const char *name, bool cancellable,
	const FillFields &fill)
{
	lua_newtable(L);

	lua_pushstring(L, name);
	lua_setfield(L, -2, "name");

	// Present from the start rather than left nil, so `if e.cancelled` reads
	// the same before and after a handler touches it.
	lua_pushboolean(L, 0);
	lua_setfield(L, -2, "cancelled");

	if (fill)
		fill(L);

	lua_rawgeti(L, LUA_REGISTRYINDEX, cancellable ? m_mt_cancellable : m_mt_notify);
	lua_setmetatable(L, -2);
}

// ---------------------------------------------------------------------------
// Registration

int LuaEvents::l_add(lua_State *L)
{
	const char *name = luaL_checkstring(L, 1);
	const char *id = luaL_checkstring(L, 2);
	luaL_checktype(L, 3, LUA_TFUNCTION);

	HandlerList *list;
	int ev = find_event(name);

	if (ev >= 0) {
		list = &g_events.m_handlers[ev];
	} else if (!strchr(name, '.') && strncmp(name, "msg:", 4) != 0) {
		// No dot, and not a msg:Name receive hook either: this is a
		// misspelled engine event, not somebody's own. Registering it would
		// hand back a handler that never fires and never complains.
		return luaL_error(L, "hook.add: unknown event '%s'. Engine events: %s. "
			"A plugin's own event needs a dot in the name, like 'shop.bought'. "
			"To receive a network message, use 'msg:Name', like 'msg:TextMsg'",
			name, known_events().c_str());
	} else {
		list = &g_events.m_custom[name];
	}

	int plugin = g_lua.current_index();

	// Same plugin, same id: replace. That is what makes running a plugin's
	// load code twice leave one handler instead of two. The id only has to be
	// unique within the plugin - two plugins may both call theirs "init".
	for (size_t i = 0; i < list->size(); i++) {
		Handler &h = (*list)[i];
		if (h.dead || h.plugin != plugin || h.id != id)
			continue;

		luaL_unref(L, LUA_REGISTRYINDEX, h.ref);
		lua_pushvalue(L, 3);
		h.ref = luaL_ref(L, LUA_REGISTRYINDEX);
		return 0;
	}

	Handler h;
	lua_pushvalue(L, 3);
	h.ref = luaL_ref(L, LUA_REGISTRYINDEX);
	h.plugin = plugin;
	h.id = id;
	h.spent = 0.0;
	h.calls = 0;
	h.dead = false;
	list->push_back(h);
	return 0;
}

int LuaEvents::l_remove(lua_State *L)
{
	const char *name = luaL_checkstring(L, 1);
	const char *id = luaL_checkstring(L, 2);

	HandlerList *list = NULL;
	int ev = find_event(name);

	if (ev >= 0) {
		list = &g_events.m_handlers[ev];
	} else {
		std::map<std::string, HandlerList>::iterator it = g_events.m_custom.find(name);
		if (it != g_events.m_custom.end())
			list = &it->second;
	}

	int plugin = g_lua.current_index();
	bool removed = false;

	if (list) {
		for (size_t i = 0; i < list->size(); i++) {
			Handler &h = (*list)[i];
			// Only your own: one plugin silently unhooking another's handler
			// is a debugging session nobody wins.
			if (h.dead || h.plugin != plugin || h.id != id)
				continue;

			luaL_unref(L, LUA_REGISTRYINDEX, h.ref);
			h.ref = LUA_NOREF;
			h.dead = true;
			removed = true;
			break;
		}
	}

	g_events.sweep();
	lua_pushboolean(L, removed);
	return 1;
}

int LuaEvents::l_run(lua_State *L)
{
	const char *name = luaL_checkstring(L, 1);

	if (find_event(name) >= 0)
		return luaL_error(L, "hook.run: '%s' is an engine event - the module "
			"fires it, scripts only listen", name);

	if (!strchr(name, '.'))
		return luaL_error(L, "hook.run: a plugin event needs a dot in the name, "
			"like 'shop.bought' (got '%s')", name);

	if (lua_isnoneornil(L, 2))
		lua_newtable(L);
	else
		luaL_checktype(L, 2, LUA_TTABLE);
	lua_settop(L, 2);

	// The caller's table becomes the event object: same fields, plus the two
	// the shape guarantees. Handing it back lets hook.run read results out.
	lua_pushstring(L, name);
	lua_setfield(L, 2, "name");
	lua_pushboolean(L, 0);
	lua_setfield(L, 2, "cancelled");
	lua_rawgeti(L, LUA_REGISTRYINDEX, g_events.m_mt_cancellable);
	lua_setmetatable(L, 2);

	std::map<std::string, HandlerList>::iterator it = g_events.m_custom.find(name);
	if (it != g_events.m_custom.end()) {
		DispatchScope walking(g_events.m_dispatching);
		g_events.dispatch_list(L, it->second, 2);
	}

	g_events.sweep();

	lua_settop(L, 2);
	return 1;
}

// hook.list() -> { { event = , id = , plugin = }, ... }, sorted by nothing in
// particular; core/hook.lua does the formatting for `lua_hooks`.
int LuaEvents::l_list(lua_State *L)
{
	const char *want = lua_isnoneornil(L, 1) ? NULL : luaL_checkstring(L, 1);

	lua_newtable(L);
	int n = 0;

	struct Emit
	{
		static void one(lua_State *L, const char *event, const Handler &h,
			const char *plugin, int &n)
		{
			lua_newtable(L);
			lua_pushstring(L, event);
			lua_setfield(L, -2, "event");
			lua_pushstring(L, h.id.c_str());
			lua_setfield(L, -2, "id");
			lua_pushstring(L, plugin);
			lua_setfield(L, -2, "plugin");
			lua_rawseti(L, -2, ++n);
		}
	};

	for (int i = 0; i < CSLUA_EVENT_COUNT; i++) {
		if (want && strcmp(want, s_event_names[i]))
			continue;
		const HandlerList &list = g_events.m_handlers[i];
		for (size_t j = 0; j < list.size(); j++)
			if (!list[j].dead)
				Emit::one(L, s_event_names[i], list[j],
					g_events.plugin_id(list[j].plugin), n);
	}

	std::map<std::string, HandlerList>::const_iterator it;
	for (it = g_events.m_custom.begin(); it != g_events.m_custom.end(); ++it) {
		if (want && it->first != want)
			continue;
		for (size_t j = 0; j < it->second.size(); j++)
			if (!it->second[j].dead)
				Emit::one(L, it->first.c_str(), it->second[j],
					g_events.plugin_id(it->second[j].plugin), n);
	}

	return 1;
}

static const luaL_Reg s_hook[] =
{
	{ "add",    LuaEvents::l_add },
	{ "remove", LuaEvents::l_remove },
	{ "run",    LuaEvents::l_run },
	{ "list",   LuaEvents::l_list },
	{ NULL, NULL }
};

void cslua_register_hooks(lua_State *L)
{
	cslua_register_namespace(L, "hook", s_hook);
	g_events.init(L);
}

// ---------------------------------------------------------------------------
// Bookkeeping

void LuaEvents::clear()
{
	// Called right before lua_close(), so the refs die with the state.
	for (int i = 0; i < CSLUA_EVENT_COUNT; i++)
		m_handlers[i].clear();
	m_custom.clear();
	m_dispatching = 0;
	m_mt_cancellable = LUA_NOREF;
	m_mt_notify = LUA_NOREF;
}

int LuaEvents::count(CsLuaEvent ev) const
{
	int n = 0;
	for (size_t i = 0; i < m_handlers[ev].size(); i++)
		n += m_handlers[ev][i].dead ? 0 : 1;
	return n;
}

bool LuaEvents::any(CsLuaEvent ev) const
{
	return count(ev) > 0;
}

int LuaEvents::count_for_plugin(int plugin_index) const
{
	int n = 0;

	for (int i = 0; i < CSLUA_EVENT_COUNT; i++)
		for (size_t j = 0; j < m_handlers[i].size(); j++)
			n += (!m_handlers[i][j].dead && m_handlers[i][j].plugin == plugin_index) ? 1 : 0;

	std::map<std::string, HandlerList>::const_iterator it;
	for (it = m_custom.begin(); it != m_custom.end(); ++it)
		for (size_t j = 0; j < it->second.size(); j++)
			n += (!it->second[j].dead && it->second[j].plugin == plugin_index) ? 1 : 0;

	return n;
}

void LuaEvents::profile_snapshot(std::vector<ProfileRow> &out) const
{
	for (int i = 0; i < CSLUA_EVENT_COUNT; i++) {
		for (size_t j = 0; j < m_handlers[i].size(); j++) {
			const Handler &h = m_handlers[i][j];
			if (h.dead || h.calls == 0)
				continue;

			ProfileRow row;
			row.plugin = plugin_id(h.plugin);
			row.event = s_event_names[i];
			row.id = h.id;
			row.seconds = h.spent;
			row.calls = h.calls;
			out.push_back(row);
		}
	}

	std::map<std::string, HandlerList>::const_iterator it;
	for (it = m_custom.begin(); it != m_custom.end(); ++it) {
		for (size_t j = 0; j < it->second.size(); j++) {
			const Handler &h = it->second[j];
			if (h.dead || h.calls == 0)
				continue;

			ProfileRow row;
			row.plugin = plugin_id(h.plugin);
			row.event = it->first;
			row.id = h.id;
			row.seconds = h.spent;
			row.calls = h.calls;
			out.push_back(row);
		}
	}
}

void LuaEvents::profile_reset()
{
	for (int i = 0; i < CSLUA_EVENT_COUNT; i++)
		for (size_t j = 0; j < m_handlers[i].size(); j++) {
			m_handlers[i][j].spent = 0.0;
			m_handlers[i][j].calls = 0;
		}

	std::map<std::string, HandlerList>::iterator it;
	for (it = m_custom.begin(); it != m_custom.end(); ++it)
		for (size_t j = 0; j < it->second.size(); j++) {
			it->second[j].spent = 0.0;
			it->second[j].calls = 0;
		}
}

// Marks a plugin's handlers dead and drops their refs. Safe to call from
// inside a handler: nothing is erased until the walk unwinds.
void LuaEvents::remove_plugin(int plugin_index)
{
	lua_State *L = g_lua.state();

	for (int i = 0; i < CSLUA_EVENT_COUNT; i++) {
		HandlerList &list = m_handlers[i];
		for (size_t j = 0; j < list.size(); j++) {
			if (list[j].plugin != plugin_index || list[j].dead)
				continue;
			if (L)
				luaL_unref(L, LUA_REGISTRYINDEX, list[j].ref);
			list[j].ref = LUA_NOREF;
			list[j].dead = true;
		}
	}

	std::map<std::string, HandlerList>::iterator it;
	for (it = m_custom.begin(); it != m_custom.end(); ++it) {
		HandlerList &list = it->second;
		for (size_t j = 0; j < list.size(); j++) {
			if (list[j].plugin != plugin_index || list[j].dead)
				continue;
			if (L)
				luaL_unref(L, LUA_REGISTRYINDEX, list[j].ref);
			list[j].ref = LUA_NOREF;
			list[j].dead = true;
		}
	}

	sweep();
}

void LuaEvents::sweep()
{
	if (m_dispatching > 0)
		return;

	for (int i = 0; i < CSLUA_EVENT_COUNT; i++) {
		HandlerList &list = m_handlers[i];
		for (size_t j = list.size(); j-- > 0; )
			if (list[j].dead)
				list.erase(list.begin() + j);
	}

	// Plugin events go away entirely once nobody listens, so hook.list() shows
	// what is live rather than everything that ever existed.
	std::map<std::string, HandlerList>::iterator it = m_custom.begin();
	while (it != m_custom.end()) {
		HandlerList &list = it->second;
		for (size_t j = list.size(); j-- > 0; )
			if (list[j].dead)
				list.erase(list.begin() + j);

		if (list.empty())
			m_custom.erase(it++);
		else
			++it;
	}
}

const char *LuaEvents::plugin_id(int plugin_index) const
{
	// -1 is the core layer, which registers handlers outside any plugin.
	if (plugin_index == -1)
		return "core";

	const std::vector<LuaPlugin> &plugins = g_lua.plugins();
	if (plugin_index < 0 || plugin_index >= (int)plugins.size())
		return "?";
	return plugins[plugin_index].id.c_str();
}

// ---------------------------------------------------------------------------
// Dispatch

void LuaEvents::dispatch_list(lua_State *L, HandlerList &list, int event_index)
{
	bool profiling = cslua_profiling();

	for (size_t i = 0; i < list.size(); i++) {
		if (list[i].dead)
			continue;

		PluginScope scope(list[i].plugin);
		int errfunc = g_lua.push_errfunc();

		lua_rawgeti(L, LUA_REGISTRYINDEX, list[i].ref);
		lua_pushvalue(L, event_index);

		// Reading the clock costs more than a trivial handler does, so it only
		// happens when somebody asked to measure.
		double started = profiling ? cslua_now_seconds() : 0.0;

		int rc = lua_pcall(L, 1, 0, errfunc);

		if (profiling) {
			list[i].spent += cslua_now_seconds() - started;
			list[i].calls++;
		}

		if (rc != 0) {
			// Naming the handler, not just the plugin: with ids there is
			// finally something more precise to point at.
			std::string where = plugin_id(list[i].plugin);
			where += ":";
			where += list[i].id;
			g_lua.report_error(where.c_str());
		}

		lua_remove(L, errfunc);

		lua_getfield(L, event_index, "cancelled");
		bool stop = lua_toboolean(L, -1) != 0;
		lua_pop(L, 1);

		if (stop)
			break;
	}
}

bool LuaEvents::run(CsLuaEvent ev, const FillFields &fill, const ReadFields &read)
{
	lua_State *L = g_lua.state();
	if (!L)
		return false;

	// Nothing listening: skip building the table. weapon_fire arrives on every
	// shot fired on the server, and an unused event should cost nothing.
	if (m_handlers[ev].empty())
		return false;

	bool cancelled = false;

	{
		DispatchScope walking(m_dispatching);

		push_event(L, s_event_names[ev], is_cancellable(ev), fill);
		int event_index = lua_gettop(L);

		dispatch_list(L, m_handlers[ev], event_index);

		if (read)
			read(L);

		lua_getfield(L, event_index, "cancelled");
		cancelled = lua_toboolean(L, -1) != 0;

		lua_settop(L, event_index - 1);
	}

	sweep();
	return cancelled;
}

void LuaEvents::notify(CsLuaEvent ev, const FillFields &fill)
{
	run(ev, fill);
}

bool LuaEvents::any_custom(const std::string &name) const
{
	std::map<std::string, HandlerList>::const_iterator it = m_custom.find(name);
	return it != m_custom.end() && !it->second.empty();
}

bool LuaEvents::run_custom(const std::string &name, const FillFields &fill)
{
	lua_State *L = g_lua.state();
	if (!L)
		return false;

	std::map<std::string, HandlerList>::iterator it = m_custom.find(name);
	if (it == m_custom.end() || it->second.empty())
		return false;

	bool cancelled = false;

	{
		DispatchScope walking(m_dispatching);

		push_event(L, name.c_str(), /*cancellable=*/true, fill);
		int event_index = lua_gettop(L);

		dispatch_list(L, it->second, event_index);

		lua_getfield(L, event_index, "cancelled");
		cancelled = lua_toboolean(L, -1) != 0;

		lua_settop(L, event_index - 1);
	}

	sweep();
	return cancelled;
}

// ---------------------------------------------------------------------------
// The events themselves
//
// Every one of these is the same three lines: name the fields, hand them to
// the dispatcher, read back whatever the game needs. Nothing inspects a
// return value - a handler changes the outcome by writing to the event.

// killer/attacker 0 means "no player" (world, fall damage): the field is nil,
// not a bogus object, so scripts test it plainly.
static void set_player_or_nil(lua_State *L, int id, const char *field)
{
	if (id > 0)
		cslua_push_player(L, id);
	else
		lua_pushnil(L);
	lua_setfield(L, -2, field);
}

static void set_player(lua_State *L, int id, const char *field)
{
	cslua_push_player(L, id);
	lua_setfield(L, -2, field);
}

// entity 0 - the throw failed, or whatever made the entity is gone - is nil,
// same convention as set_player_or_nil.
static void set_entity_or_nil(lua_State *L, int index, const char *field)
{
	cslua_push_entity_index(L, index);
	lua_setfield(L, -2, field);
}

static void set_string(lua_State *L, const std::string &s, const char *field)
{
	lua_pushstring(L, s.c_str());
	lua_setfield(L, -2, field);
}

static void set_number(lua_State *L, double v, const char *field)
{
	lua_pushnumber(L, v);
	lua_setfield(L, -2, field);
}

// Damage that never went through TraceAttack - a fall, the world, gas - has no
// body region. nil rather than 0, because 0 is a real hitgroup (the body).
static void set_hitgroup(lua_State *L, int hitgroup)
{
	if (hitgroup < 0)
		return;

	lua_pushinteger(L, hitgroup);
	lua_setfield(L, -2, "hitgroup");
}

bool LuaEvents::fire_client_connect(int id, const char *name, const char *ip, RejectInfo &reject)
{
	std::string nm = name ? name : "";
	std::string addr = ip ? ip : "";

	return run(CSLUA_EVENT_CLIENT_CONNECT,
		[=](lua_State *L) {
			set_player(L, id, "player");
			set_string(L, nm, "name");
			set_string(L, addr, "ip");
		},
		[&reject](lua_State *L) {
			lua_getfield(L, -1, "reason");
			const char *why = lua_tostring(L, -1);
			cslua_snprintf(reject.reason, sizeof reject.reason, "%s",
				why && *why ? why : "Rejected by a Lua plugin");
			reject.reason[sizeof reject.reason - 1] = '\0';
			lua_pop(L, 1);
		});
}

void LuaEvents::fire_client_disconnect(int id, const char *name)
{
	std::string nm = name ? name : "";
	notify(CSLUA_EVENT_CLIENT_DISCONNECT, [=](lua_State *L) {
		set_player(L, id, "player");
		set_string(L, nm, "name");
	});
}

void LuaEvents::fire_player_authorized(int id, const char *authid)
{
	std::string sid = authid ? authid : "";
	notify(CSLUA_EVENT_PLAYER_AUTHORIZED, [=](lua_State *L) {
		set_player(L, id, "player");
		set_string(L, sid, "steamid");
	});
}

void LuaEvents::fire_player_ready(int id)
{
	notify(CSLUA_EVENT_PLAYER_READY, [=](lua_State *L) {
		set_player(L, id, "player");
	});
}

bool LuaEvents::fire_player_chat(int id, const char *text, bool team)
{
	std::string body = text ? text : "";

	return run(CSLUA_EVENT_PLAYER_CHAT, [=](lua_State *L) {
		set_player(L, id, "player");
		set_string(L, body, "text");
		lua_pushboolean(L, team);
		lua_setfield(L, -2, "team");
	});
}

void LuaEvents::fire_menu_select(int id, int key)
{
	notify(CSLUA_EVENT_MENU_SELECT, [=](lua_State *L) {
		set_player(L, id, "player");
		lua_pushinteger(L, key);
		lua_setfield(L, -2, "key");
	});
}

void LuaEvents::fire_player_spawn(int id)
{
	notify(CSLUA_EVENT_PLAYER_SPAWN, [=](lua_State *L) {
		set_player(L, id, "player");
	});
}

float LuaEvents::fire_player_hurt(int victim, int attacker, float damage, int bits,
	int hitgroup)
{
	float result = damage;

	bool cancelled = run(CSLUA_EVENT_PLAYER_HURT,
		[=](lua_State *L) {
			set_player(L, victim, "victim");
			set_player_or_nil(L, attacker, "attacker");
			set_number(L, damage, "damage");
			lua_pushinteger(L, bits);
			lua_setfield(L, -2, "bits");
			set_hitgroup(L, hitgroup);
		},
		[&result](lua_State *L) {
			// Whatever the chain left in e.damage is what the game applies.
			// A handler that scaled it sees the previous one's value, because
			// every handler got the same table.
			lua_getfield(L, -1, "damage");
			if (lua_isnumber(L, -1))
				result = (float)lua_tonumber(L, -1);
			lua_pop(L, 1);
		});

	if (cancelled || result < 0.0f)
		result = 0.0f;

	return result;
}

void LuaEvents::fire_player_hurt_post(int victim, int attacker, float damage, int bits,
	int hitgroup)
{
	notify(CSLUA_EVENT_PLAYER_HURT_POST, [=](lua_State *L) {
		set_player(L, victim, "victim");
		set_player_or_nil(L, attacker, "attacker");
		set_number(L, damage, "damage");
		lua_pushinteger(L, bits);
		lua_setfield(L, -2, "bits");
		set_hitgroup(L, hitgroup);
	});
}

void LuaEvents::fire_player_death(int victim, int killer, int headshot,
	const char *weapon, float distance)
{
	std::string weapon_name = weapon ? weapon : "";

	notify(CSLUA_EVENT_PLAYER_DEATH, [=](lua_State *L) {
		set_player(L, victim, "victim");
		set_player_or_nil(L, killer, "killer");
		lua_pushboolean(L, headshot);
		lua_setfield(L, -2, "headshot");

		if (!weapon_name.empty())
			set_string(L, weapon_name, "weapon");
		if (distance >= 0.0f)
			set_number(L, distance, "distance");
	});
}

void LuaEvents::fire_player_team_change(int id, const char *old_team, const char *new_team)
{
	std::string was = old_team ? old_team : "";
	std::string now = new_team ? new_team : "";

	notify(CSLUA_EVENT_PLAYER_TEAM_CHANGE, [=](lua_State *L) {
		set_player(L, id, "player");
		set_string(L, was, "old_team");
		set_string(L, now, "new_team");
	});
}

void LuaEvents::fire_weapon_fire(int id, const char *weapon, int clip)
{
	std::string name = weapon ? weapon : "";

	notify(CSLUA_EVENT_WEAPON_FIRE, [=](lua_State *L) {
		set_player(L, id, "player");
		set_string(L, name, "weapon");
		lua_pushinteger(L, clip);
		lua_setfield(L, -2, "clip");
	});
}

void LuaEvents::fire_weapon_deploy(int id, const char *weapon,
	std::string &view_model, std::string &world_model)
{
	std::string wname = weapon ? weapon : "";
	std::string vm = view_model;
	std::string wm = world_model;

	run(CSLUA_EVENT_WEAPON_DEPLOY,
		[=](lua_State *L) {
			set_player(L, id, "player");
			set_string(L, wname, "weapon");
			set_string(L, vm, "view_model");
			set_string(L, wm, "world_model");
		},
		[&](lua_State *L) {
			lua_getfield(L, -1, "view_model");
			if (lua_isstring(L, -1))
				view_model = lua_tostring(L, -1);
			lua_pop(L, 1);

			lua_getfield(L, -1, "world_model");
			if (lua_isstring(L, -1))
				world_model = lua_tostring(L, -1);
			lua_pop(L, 1);
		});
}

void LuaEvents::fire_weapon_reload(int id, const char *weapon, int clip, float delay, int max_clip)
{
	std::string name = weapon ? weapon : "";

	notify(CSLUA_EVENT_WEAPON_RELOAD, [=](lua_State *L) {
		set_player(L, id, "player");
		set_string(L, name, "weapon");
		lua_pushinteger(L, clip);
		lua_setfield(L, -2, "clip");
		set_number(L, delay, "delay");

		if (max_clip >= 0)
			lua_pushinteger(L, max_clip);
		else
			lua_pushnil(L);
		lua_setfield(L, -2, "max_clip");
	});
}

void LuaEvents::fire_map_change(const char *map)
{
	std::string name = map ? map : "";
	notify(CSLUA_EVENT_MAP_CHANGE, [=](lua_State *L) {
		set_string(L, name, "map");
	});
}

void LuaEvents::fire_plugin_unload(const char *only_id)
{
	std::string who = only_id ? only_id : "";

	notify(CSLUA_EVENT_PLUGIN_UNLOAD, [=](lua_State *L) {
		if (!who.empty())
			set_string(L, who, "plugin");
	});
}

void LuaEvents::fire_round_start()
{
	notify(CSLUA_EVENT_ROUND_START);
}

void LuaEvents::fire_round_end(int winner)
{
	notify(CSLUA_EVENT_ROUND_END, [=](lua_State *L) {
		lua_pushinteger(L, winner);
		lua_setfield(L, -2, "winner");
	});
}

void LuaEvents::fire_round_freeze_end()
{
	notify(CSLUA_EVENT_ROUND_FREEZE_END);
}

void LuaEvents::fire_bomb_planted(int planter)
{
	notify(CSLUA_EVENT_BOMB_PLANTED, [=](lua_State *L) {
		set_player_or_nil(L, planter, "player");
	});
}

void LuaEvents::fire_bomb_defused(int defuser, bool success)
{
	notify(CSLUA_EVENT_BOMB_DEFUSED, [=](lua_State *L) {
		set_player_or_nil(L, defuser, "player");
		lua_pushboolean(L, success);
		lua_setfield(L, -2, "success");
	});
}

void LuaEvents::fire_bomb_exploded(float x, float y, float z)
{
	notify(CSLUA_EVENT_BOMB_EXPLODED, [=](lua_State *L) {
		set_number(L, x, "x");
		set_number(L, y, "y");
		set_number(L, z, "z");
	});
}

void LuaEvents::fire_grenade_throw(int owner, const char *weapon, float &fuse)
{
	std::string wname = weapon ? weapon : "";
	float f = fuse;

	run(CSLUA_EVENT_GRENADE_THROW,
		[=](lua_State *L) {
			set_player_or_nil(L, owner, "player");
			set_string(L, wname, "weapon");
			set_number(L, f, "fuse");
		},
		[&](lua_State *L) {
			lua_getfield(L, -1, "fuse");
			if (lua_isnumber(L, -1))
				fuse = (float)lua_tonumber(L, -1);
			lua_pop(L, 1);
		});
}

void LuaEvents::fire_grenade_thrown(int owner, const char *weapon, int entity_index)
{
	std::string wname = weapon ? weapon : "";

	notify(CSLUA_EVENT_GRENADE_THROWN, [=](lua_State *L) {
		set_player_or_nil(L, owner, "player");
		set_string(L, wname, "weapon");
		set_entity_or_nil(L, entity_index, "entity");
	});
}

bool LuaEvents::fire_grenade_explode(int owner, const char *weapon, int entity_index,
	float x, float y, float z)
{
	std::string wname = weapon ? weapon : "";

	return run(CSLUA_EVENT_GRENADE_EXPLODE, [=](lua_State *L) {
		set_player_or_nil(L, owner, "player");
		set_string(L, wname, "weapon");
		set_entity_or_nil(L, entity_index, "entity");
		set_number(L, x, "x");
		set_number(L, y, "y");
		set_number(L, z, "z");
	});
}

bool LuaEvents::fire_weapon_throw(int player, const char *weapon, int ammo_type,
	float x, float y, float z, float vx, float vy, float vz, float time)
{
	std::string wname = weapon ? weapon : "";

	return run(CSLUA_EVENT_WEAPON_THROW, [=](lua_State *L) {
		set_player_or_nil(L, player, "player");
		set_string(L, wname, "weapon");
		set_number(L, ammo_type, "ammo_type");
		set_number(L, x, "x");
		set_number(L, y, "y");
		set_number(L, z, "z");
		set_number(L, vx, "vx");
		set_number(L, vy, "vy");
		set_number(L, vz, "vz");
		set_number(L, time, "time");
	});
}

void LuaEvents::fire_weapon_secondary_attack(int player, const char *weapon, int ammo_type)
{
	std::string wname = weapon ? weapon : "";

	notify(CSLUA_EVENT_WEAPON_SECONDARY_ATTACK, [=](lua_State *L) {
		set_player_or_nil(L, player, "player");
		set_string(L, wname, "weapon");
		set_number(L, ammo_type, "ammo_type");
	});
}

bool LuaEvents::fire_weapon_buy(int player, const char *weapon)
{
	std::string wname = weapon ? weapon : "";

	return run(CSLUA_EVENT_WEAPON_BUY, [=](lua_State *L) {
		set_player_or_nil(L, player, "player");
		set_string(L, wname, "weapon");
	});
}

bool LuaEvents::fire_ammo_buy(int player, const char *weapon)
{
	std::string wname = weapon ? weapon : "";

	return run(CSLUA_EVENT_AMMO_BUY, [=](lua_State *L) {
		set_player_or_nil(L, player, "player");
		set_string(L, wname, "weapon");
	});
}

bool LuaEvents::fire_item_buy(int player, const char *item)
{
	std::string iname = item ? item : "";

	return run(CSLUA_EVENT_ITEM_BUY, [=](lua_State *L) {
		set_player_or_nil(L, player, "player");
		set_string(L, iname, "item");
	});
}

bool LuaEvents::fire_money_change(int player, int amount, const char *reason)
{
	std::string rname = reason ? reason : "";

	return run(CSLUA_EVENT_MONEY_CHANGE, [=](lua_State *L) {
		set_player_or_nil(L, player, "player");
		set_number(L, amount, "amount");
		set_string(L, rname, "reason");
	});
}

bool LuaEvents::fire_ammo_pickup(int player, const char *weapon, int count, int max)
{
	std::string wname = weapon ? weapon : "";

	return run(CSLUA_EVENT_AMMO_PICKUP, [=](lua_State *L) {
		set_player_or_nil(L, player, "player");
		set_string(L, wname, "weapon");
		set_number(L, count, "count");
		set_number(L, max, "max");
	});
}

bool LuaEvents::fire_weapon_drop(int player, const char *weapon)
{
	std::string wname = weapon ? weapon : "";

	return run(CSLUA_EVENT_WEAPON_DROP, [=](lua_State *L) {
		set_player_or_nil(L, player, "player");
		set_string(L, wname, "weapon");
	});
}

void LuaEvents::fire_player_jump(int player)
{
	notify(CSLUA_EVENT_PLAYER_JUMP, [=](lua_State *L) {
		set_player_or_nil(L, player, "player");
	});
}

void LuaEvents::fire_player_duck(int player)
{
	notify(CSLUA_EVENT_PLAYER_DUCK, [=](lua_State *L) {
		set_player_or_nil(L, player, "player");
	});
}

void LuaEvents::fire_player_spectate(int player)
{
	notify(CSLUA_EVENT_PLAYER_SPECTATE, [=](lua_State *L) {
		set_player_or_nil(L, player, "player");
	});
}

bool LuaEvents::fire_player_radio(int player, const char *sentence, const char *sample)
{
	std::string sen = sentence ? sentence : "";
	std::string samp = sample ? sample : "";

	return run(CSLUA_EVENT_PLAYER_RADIO, [=](lua_State *L) {
		set_player_or_nil(L, player, "player");
		set_string(L, sen, "sentence");
		set_string(L, samp, "sample");
	});
}

bool LuaEvents::fire_player_can_respawn(int player)
{
	return run(CSLUA_EVENT_PLAYER_CAN_RESPAWN, [=](lua_State *L) {
		set_player_or_nil(L, player, "player");
	});
}

void LuaEvents::fire_bomb_defuse_start(int player, bool defuser)
{
	notify(CSLUA_EVENT_BOMB_DEFUSE_START, [=](lua_State *L) {
		set_player_or_nil(L, player, "player");
		lua_pushboolean(L, defuser);
		lua_setfield(L, -2, "defuser");
	});
}
