#include "cslua.h"
#include <regamedll_api.h>
#include <in_buttons.h>

#include "lua_player.h"
#include "lua_message.h"
#include "lua_natives.h"
#include "lua_sound.h"
#include "regamedll.h"
#include "players.h"
#include "lua_pev.h"

// Registry refs to the cached objects: one per slot plus the broadcast target.
static int s_player_ref[CSLUA_MAXPLAYERS];
static int s_all_ref = LUA_NOREF;

// pfnGetPlayerStats reads client_t->latency, which the engine keeps live from
// its own frame_latency[] ring. The only gap is the couple of frames right
// after connect, when it can hand back 0; this cache holds the last non-zero
// reading to paper over just that window.
static int s_last_ping[CSLUA_MAXPLAYERS];
static int s_last_loss[CSLUA_MAXPLAYERS];

// The shared __index table behind every player object. Kept so players.method()
// can extend it.
static int s_methods_ref = LUA_NOREF;

// Reads self.id. Works for both a player object and `all` (id 0).
static int self_id(lua_State *L)
{
	luaL_checktype(L, 1, LUA_TTABLE);

	lua_getfield(L, 1, "id");
	if (!lua_isnumber(L, -1))
		return luaL_error(L, "p: expected a player object, use p:method() and not p.method()");

	int id = (int)lua_tointeger(L, -1);
	lua_pop(L, 1);

	if (!cslua_valid_player_or_broadcast_id(id))
		return luaL_error(L, "p: player index %d out of range", id);

	// "id" is a plain table field, so __newindex (l_readonly) never fires for
	// it - the key already exists. p.id = N would silently repoint this shared,
	// cached object, so check here that it still matches the slot it claims.
	if (id == 0)
		cslua_push_all(L);
	else
		cslua_push_player(L, id);
	bool genuine = lua_rawequal(L, 1, -1) != 0;
	lua_pop(L, 1);
	if (!genuine)
		return luaL_error(L, "p: stale player object (its id field was overwritten)");

	return id;
}

// Same, but rejects `all` for things that only make sense per player.
static int self_player_id(lua_State *L)
{
	int id = self_id(L);
	if (id == 0)
		return luaL_error(L, "p: this method is not available on 'all'");
	return id;
}

static int l_name(lua_State *L)
{
	lua_pushstring(L, g_players.name(self_player_id(L)));
	return 1;
}

static int l_ip(lua_State *L)
{
	lua_pushstring(L, g_players.ip(self_player_id(L)));
	return 1;
}

static int l_steamid(lua_State *L)
{
	lua_pushstring(L, g_players.authid(self_player_id(L)));
	return 1;
}

// p:info("_pw") - a key from the client's own infobuffer (setinfo). Anything
// the client sets is client-controlled; never trust it for more than matching
// against a stored secret.
static int l_info(lua_State *L)
{
	int id = self_player_id(L);
	const char *key = luaL_checkstring(L, 2);

	if (!g_players.is_connected(id)) {
		lua_pushstring(L, "");
		return 1;
	}

	edict_t *e = g_engfuncs.pfnPEntityOfEntIndex(id);
	if (!e || e->free) {
		lua_pushstring(L, "");
		return 1;
	}

	char *buffer = g_engfuncs.pfnGetInfoKeyBuffer(e);
	const char *value = buffer ? g_engfuncs.pfnInfoKeyValue(buffer, key) : NULL;
	lua_pushstring(L, value ? value : "");
	return 1;
}

static int l_userid(lua_State *L)
{
	int id = self_player_id(L);
	if (!g_players.is_connected(id)) {
		lua_pushinteger(L, -1);
		return 1;
	}

	edict_t *e = g_engfuncs.pfnPEntityOfEntIndex(id);
	lua_pushinteger(L, e ? g_engfuncs.pfnGetPlayerUserId(e) : -1);
	return 1;
}

static int l_connected(lua_State *L)
{
	lua_pushboolean(L, g_players.is_connected(self_player_id(L)));
	return 1;
}

// p:ping() -> ms [, loss_pct]. Read fresh every call; a zero (no samples yet)
// falls back to the last non-zero reading.
static int l_ping(lua_State *L)
{
	int id = self_player_id(L);

	if (!g_players.is_connected(id)) {
		lua_pushinteger(L, -1);
		return 1;
	}

	edict_t *e = g_engfuncs.pfnPEntityOfEntIndex(id);
	if (e && !e->free) {
		int ping = 0, loss = 0;
		g_engfuncs.pfnGetPlayerStats(e, &ping, &loss);
		if (ping > 0) {
			s_last_ping[id] = ping;
			s_last_loss[id] = loss;
		}
	}

	lua_pushinteger(L, s_last_ping[id]);
	lua_pushinteger(L, s_last_loss[id]);
	return 2;
}

void cslua_player_reset_ping(int id)
{
	if (cslua_valid_player_or_broadcast_id(id)) {
		s_last_ping[id] = 0;
		s_last_loss[id] = 0;
	}
}

// Everything below reads and writes entvars_t directly through the edict, so it
// works on any game DLL. A missing player is an error, not a silent zero.
static entvars_t *self_pev(lua_State *L)
{
	int id = self_player_id(L);

	if (!g_players.is_connected(id))
		luaL_error(L, "p: player #%d is not connected", id);

	edict_t *e = g_engfuncs.pfnPEntityOfEntIndex(id);
	if (!e || e->free)
		luaL_error(L, "p: player #%d has no valid entity", id);

	return &e->v;
}

// One method does both jobs: p:health() reads, p:health(100) writes.
static int scalar_field(lua_State *L, float &field)
{
	if (lua_isnoneornil(L, 2)) {
		lua_pushnumber(L, field);
		return 1;
	}

	field = (float)luaL_checknumber(L, 2);
	return 0;
}

// Same idea for vectors: returns x, y, z or takes all three.
static int vector_field(lua_State *L, Vector &field)
{
	if (lua_isnoneornil(L, 2)) {
		lua_pushnumber(L, field.x);
		lua_pushnumber(L, field.y);
		lua_pushnumber(L, field.z);
		return 3;
	}

	field.x = (float)luaL_checknumber(L, 2);
	field.y = (float)luaL_checknumber(L, 3);
	field.z = (float)luaL_checknumber(L, 4);
	return 0;
}

static int l_model(lua_State *L)
{
	int id = self_player_id(L);
	entvars_t *pev = self_pev(L);

	if (lua_isnoneornil(L, 2)) {
		lua_pushstring(L, STRING(pev->model));
		return 1;
	}

	edict_t *e = g_engfuncs.pfnPEntityOfEntIndex(id);
	SET_MODEL(e, luaL_checkstring(L, 2));
	return 0;
}

static int l_health(lua_State *L)
{
	return scalar_field(L, self_pev(L)->health);
}

static int l_armor(lua_State *L)
{
	return scalar_field(L, self_pev(L)->armorvalue);
}

static int l_frags(lua_State *L)
{
	return scalar_field(L, self_pev(L)->frags);
}

static int l_gravity(lua_State *L)
{
	return scalar_field(L, self_pev(L)->gravity);
}

static int l_maxspeed(lua_State *L)
{
	return scalar_field(L, self_pev(L)->maxspeed);
}

// Writing origin goes through the engine so the entity gets relinked.
static int l_origin(lua_State *L)
{
	int id = self_player_id(L);
	entvars_t *pev = self_pev(L);

	if (lua_isnoneornil(L, 2)) {
		lua_pushnumber(L, pev->origin.x);
		lua_pushnumber(L, pev->origin.y);
		lua_pushnumber(L, pev->origin.z);
		return 3;
	}

	Vector where(
		(float)luaL_checknumber(L, 2),
		(float)luaL_checknumber(L, 3),
		(float)luaL_checknumber(L, 4));

	SET_ORIGIN(g_engfuncs.pfnPEntityOfEntIndex(id), where);
	return 0;
}

static int l_angles(lua_State *L)
{
	return vector_field(L, self_pev(L)->angles);
}

static int l_velocity(lua_State *L)
{
	return vector_field(L, self_pev(L)->velocity);
}

static int l_punchangle(lua_State *L)
{
	return vector_field(L, self_pev(L)->punchangle);
}

// Where the player is actually looking; read-only, the client owns it.
static int l_aim(lua_State *L)
{
	entvars_t *pev = self_pev(L);
	lua_pushnumber(L, pev->v_angle.x);
	lua_pushnumber(L, pev->v_angle.y);
	lua_pushnumber(L, pev->v_angle.z);
	return 3;
}

// p:pev(name[, v...]) - generic read/write for any entvars_t field by name,
// sharing the field table and dispatch in lua_pev.cpp with e:pev().
static int l_pev(lua_State *L)
{
	return cslua_pev_call(L, self_pev(L), 2);
}

// Shared by l_trace and l_trace_to: turns a TraceResult into the
// { kind, player, classname, x, y, z, distance, hitgroup } table both return.
void cslua_push_trace_result(lua_State *L, const TraceResult &tr, const Vector &start)
{
	lua_newtable(L);

	// A trace that ran the whole way hit nothing; the coordinates are still the
	// ray's end.
	edict_t *hit = tr.pHit;
	int hit_index = (hit && !hit->free) ? ENTINDEX(hit) : -1;

	const char *kind;
	if (tr.flFraction >= 1.0f && hit_index <= 0) {
		kind = "none";
	} else if (cslua_valid_player_id(hit_index) && g_players.is_connected(hit_index)) {
		kind = "player";
		cslua_push_player(L, hit_index);
		lua_setfield(L, -2, "player");
	} else if (hit_index == 0) {
		kind = "world";
	} else {
		kind = "entity";
	}

	lua_pushstring(L, kind);
	lua_setfield(L, -2, "kind");

	if (hit_index >= 0) {
		lua_pushstring(L, STRING(hit->v.classname));
		lua_setfield(L, -2, "classname");
	}

	lua_pushnumber(L, tr.vecEndPos.x);
	lua_setfield(L, -2, "x");
	lua_pushnumber(L, tr.vecEndPos.y);
	lua_setfield(L, -2, "y");
	lua_pushnumber(L, tr.vecEndPos.z);
	lua_setfield(L, -2, "z");

	lua_pushnumber(L, (tr.vecEndPos - start).Length());
	lua_setfield(L, -2, "distance");

	lua_pushinteger(L, tr.iHitgroup);
	lua_setfield(L, -2, "hitgroup");
}

// p:trace([distance]) - fire a ray from the player's eyes along their aim.
static int l_trace(lua_State *L)
{
	int id = self_player_id(L);
	entvars_t *pev = self_pev(L);
	float distance = (float)luaL_optnumber(L, 2, 8192.0);

	MAKE_VECTORS(pev->v_angle);

	Vector start = pev->origin + pev->view_ofs;
	Vector end = start + gpGlobals->v_forward * distance;

	TraceResult tr;
	TRACE_LINE(start, end, dont_ignore_monsters, g_engfuncs.pfnPEntityOfEntIndex(id), &tr);

	cslua_push_trace_result(L, tr, start);
	return 1;
}

// p:trace_to(other) - a line from p's eyes to other's eyes, for "was there a
// clear shot" (weapon spread means the crosshair isn't where the bullet went).
static int l_trace_to(lua_State *L)
{
	int id = self_player_id(L);
	entvars_t *pev = self_pev(L);

	luaL_checktype(L, 2, LUA_TTABLE);
	lua_getfield(L, 2, "id");
	if (!lua_isnumber(L, -1))
		return luaL_error(L, "p:trace_to: expected a player object");
	int other_id = (int)lua_tointeger(L, -1);
	lua_pop(L, 1);

	if (!cslua_valid_player_id(other_id) || !g_players.is_connected(other_id))
		return luaL_error(L, "p:trace_to: target player is not connected");

	edict_t *other_edict = g_engfuncs.pfnPEntityOfEntIndex(other_id);
	if (!other_edict || other_edict->free)
		return luaL_error(L, "p:trace_to: target player has no valid entity");

	entvars_t *other_pev = &other_edict->v;

	Vector start = pev->origin + pev->view_ofs;
	Vector end = other_pev->origin + other_pev->view_ofs;

	TraceResult tr;
	TRACE_LINE(start, end, dont_ignore_monsters, g_engfuncs.pfnPEntityOfEntIndex(id), &tr);

	cslua_push_trace_result(L, tr, start);
	return 1;
}

// sv.hull_free(x, y, z[, ducking]) - would a player fit here without spawning
// stuck? A zero-length TRACE_HULL at the point; fStartSolid/fAllSolid come back
// true when the hull already overlaps something solid.
int cslua_sv_hull_free(lua_State *L)
{
	Vector point;
	point.x = (float)luaL_checknumber(L, 1);
	point.y = (float)luaL_checknumber(L, 2);
	point.z = (float)luaL_checknumber(L, 3);
	bool ducking = lua_toboolean(L, 4) != 0;

	TraceResult tr;
	TRACE_HULL(point, point, ignore_monsters, ducking ? head_hull : human_hull, NULL, &tr);

	lua_pushboolean(L, !tr.fStartSolid && !tr.fAllSolid);
	return 1;
}

static int l_alive(lua_State *L)
{
	entvars_t *pev = self_pev(L);
	lua_pushboolean(L, pev->deadflag == DEAD_NO && pev->health > 0);
	return 1;
}

static int l_on_ground(lua_State *L)
{
	lua_pushboolean(L, (self_pev(L)->flags & FL_ONGROUND) != 0);
	return 1;
}

static int l_ducking(lua_State *L)
{
	lua_pushboolean(L, (self_pev(L)->flags & FL_DUCKING) != 0);
	return 1;
}

// Name -> IN_* bit, for p:button().
static int button_bit(const char *name)
{
	if (!strcmp(name, "attack"))   return IN_ATTACK;
	if (!strcmp(name, "attack2"))  return IN_ATTACK2;
	if (!strcmp(name, "jump"))     return IN_JUMP;
	if (!strcmp(name, "duck"))     return IN_DUCK;
	if (!strcmp(name, "use"))      return IN_USE;
	if (!strcmp(name, "reload"))   return IN_RELOAD;
	if (!strcmp(name, "forward"))  return IN_FORWARD;
	if (!strcmp(name, "back"))     return IN_BACK;
	if (!strcmp(name, "moveleft")) return IN_MOVELEFT;
	if (!strcmp(name, "moveright"))return IN_MOVERIGHT;
	return 0;
}

// p:button("use") - true while the player holds that key, read fresh off
// pev->button. Read-only: the client owns this field.
static int l_button(lua_State *L)
{
	const char *name = luaL_checkstring(L, 2);
	int bit = button_bit(name);
	if (!bit)
		return luaL_error(L, "p:button: unknown button '%s'", name);

	lua_pushboolean(L, (self_pev(L)->button & bit) != 0);
	return 1;
}

// FL_FAKECLIENT marks a server-side bot; FL_PROXY marks an HLTV proxy.
static int l_is_bot(lua_State *L)
{
	lua_pushboolean(L, (self_pev(L)->flags & FL_FAKECLIENT) != 0);
	return 1;
}

static int l_is_hltv(lua_State *L)
{
	lua_pushboolean(L, (self_pev(L)->flags & FL_PROXY) != 0);
	return 1;
}

// Read/write in one method for a single bit of pev->flags.
static int flag_field(lua_State *L, entvars_t *pev, int bit)
{
	if (lua_isnoneornil(L, 2)) {
		lua_pushboolean(L, (pev->flags & bit) != 0);
		return 1;
	}

	if (lua_toboolean(L, 2))
		pev->flags |= bit;
	else
		pev->flags &= ~bit;
	return 0;
}

// p:freeze([bool]) - FL_FROZEN, the engine's own freeze. maxspeed(1) leaves the
// client predicting movement the server refuses, which is the stutter.
static int l_freeze(lua_State *L)
{
	return flag_field(L, self_pev(L), FL_FROZEN);
}

// p:godmode([bool]) - takedamage is the source of truth; FL_GODMODE is set
// alongside because that is what other mods and `god` look at.
static int l_godmode(lua_State *L)
{
	entvars_t *pev = self_pev(L);

	if (lua_isnoneornil(L, 2)) {
		lua_pushboolean(L, pev->takedamage == DAMAGE_NO);
		return 1;
	}

	if (lua_toboolean(L, 2)) {
		pev->takedamage = DAMAGE_NO;
		pev->flags |= FL_GODMODE;
	} else {
		pev->takedamage = DAMAGE_AIM;
		pev->flags &= ~FL_GODMODE;
	}
	return 0;
}

// p:noclip([bool]) - fly through walls. MOVETYPE_WALK is the live-player value.
static int l_noclip(lua_State *L)
{
	entvars_t *pev = self_pev(L);

	if (lua_isnoneornil(L, 2)) {
		lua_pushboolean(L, pev->movetype == MOVETYPE_NOCLIP);
		return 1;
	}

	pev->movetype = lua_toboolean(L, 2) ? MOVETYPE_NOCLIP : MOVETYPE_WALK;
	return 0;
}

// p:noblock([bool]) - true lets other players walk through this one. SOLID_NOT
// only affects how other entities treat this one, not its own movement against
// the world, so unlike noclip this cannot drop someone through the floor.
static int l_noblock(lua_State *L)
{
	entvars_t *pev = self_pev(L);

	if (lua_isnoneornil(L, 2)) {
		lua_pushboolean(L, pev->solid == SOLID_NOT);
		return 1;
	}

	pev->solid = lua_toboolean(L, 2) ? SOLID_NOT : SOLID_SLIDEBOX;
	return 0;
}

// p:suppress_attack([bool]) - true drops IN_ATTACK/IN_ATTACK2 from the button
// state every frame (dllapi.cpp's PlayerPreThink), before the weapon code sees
// it.
static int l_suppress_attack(lua_State *L)
{
	int id = self_player_id(L);

	if (lua_isnoneornil(L, 2)) {
		lua_pushboolean(L, g_players.suppress_attack(id));
		return 1;
	}

	g_players.set_suppress_attack(id, lua_toboolean(L, 2) != 0);
	return 0;
}

// p:suppress_move([bool]) - same for IN_FORWARD/IN_BACK/IN_MOVELEFT/IN_MOVERIGHT.
// Pins them in place without FL_FROZEN, which also zeroes pev->button.
static int l_suppress_move(lua_State *L)
{
	int id = self_player_id(L);

	if (lua_isnoneornil(L, 2)) {
		lua_pushboolean(L, g_players.suppress_move(id));
		return 1;
	}

	g_players.set_suppress_move(id, lua_toboolean(L, 2) != 0);
	return 0;
}

// p:suppress_drop([bool]) - true blocks the "drop" console command outright
// (dllapi.cpp's ClientCommand).
static int l_suppress_drop(lua_State *L)
{
	int id = self_player_id(L);

	if (lua_isnoneornil(L, 2)) {
		lua_pushboolean(L, g_players.suppress_drop(id));
		return 1;
	}

	g_players.set_suppress_drop(id, lua_toboolean(L, 2) != 0);
	return 0;
}

// The CS-specific methods below need ReGameDLL: they reach into CBasePlayer.
static CBasePlayer *self_cbase(lua_State *L)
{
	int id = self_player_id(L);

	if (!cslua_regamedll_ready())
		luaL_error(L, "p: %s requires ReGameDLL, which is not loaded", "this method");

	CBasePlayer *player = cslua_player_entity(id);
	if (!player)
		luaL_error(L, "p: player #%d is not in the game yet", id);

	return player;
}

// Reads a flag out of an options table. A bare boolean is refused.
static bool opt_flag(lua_State *L, int index, const char *key, const char *method)
{
	if (lua_isnoneornil(L, index))
		return false;

	if (!lua_istable(L, index)) {
		luaL_error(L, "p:%s: options must be a table, like { %s = true }", method, key);
		return false;			// luaL_error does not return
	}

	lua_getfield(L, index, key);
	bool value = lua_toboolean(L, -1) != 0;
	lua_pop(L, 1);
	return value;
}

static const char *team_name(int team)
{
	switch (team) {
	case TERRORIST: return "T";
	case CT:        return "CT";
	case SPECTATOR: return "SPEC";
	default:        return "NONE";
	}
}

// p:progress_bar(seconds) - the same under-crosshair bar the game shows for
// planting/defusing (CSPlayer::SetProgressBarTime). One-shot; 0 clears it.
static int l_progress_bar(lua_State *L)
{
	CBasePlayer *player = self_cbase(L);
	int seconds = (int)luaL_checknumber(L, 2);
	if (seconds < 0)
		seconds = 0;

	player->CSPlayer()->SetProgressBarTime(seconds);
	return 0;
}

// p:team() reads.
// p:team("CT")                    soft move: change side, keep the player alive.
// p:team("CT", { force = true })  the player dies and rejoins (JoinTeam).
// The soft path can't put a spectator into a round, so it upgrades to force for
// that case.
static int l_team(lua_State *L)
{
	CBasePlayer *player = self_cbase(L);

	if (lua_isnoneornil(L, 2)) {
		lua_pushstring(L, team_name(player->m_iTeam));
		return 1;
	}

	const char *want = luaL_checkstring(L, 2);
	TeamName team;
	if (!strcmp(want, "CT"))        team = CT;
	else if (!strcmp(want, "T"))    team = TERRORIST;
	else if (!strcmp(want, "SPEC")) team = SPECTATOR;
	else return luaL_error(L, "p:team: expected 'CT', 'T' or 'SPEC', got '%s'", want);

	bool force = opt_flag(L, 3, "force", "team");
	bool from_limbo = player->m_iTeam == SPECTATOR || player->m_iTeam == UNASSIGNED;

	if (force || from_limbo || team == SPECTATOR) {
		player->CSPlayer()->JoinTeam(team);
	} else {
		player->m_iTeam = team;
		player->m_bTeamChanged = true;
		player->CSPlayer()->TeamChangeUpdate();
	}
	return 0;
}

// Admin actions.
//
// kick and ban go through the engine's own console commands, addressed by
// userid (a nickname can contain spaces or quotes).

// p:kick([reason]) - "kick #<userid> [reason]" folds the reason straight into
// the SV_DropClient message the client sees.
static int l_kick(lua_State *L)
{
	int id = self_player_id(L);
	const char *reason = luaL_optstring(L, 2, NULL);

	if (!g_players.is_connected(id))
		return 0;

	edict_t *e = g_engfuncs.pfnPEntityOfEntIndex(id);
	if (!e || e->free)
		return 0;

	char cmd[256];
	if (reason && *reason) {
		// The reason rides the console command line as a quoted argument, so it
		// can't carry a '"' of its own.
		char escaped[192];
		size_t w = 0;
		for (const char *p = reason; *p && w < sizeof escaped - 1; p++)
			escaped[w++] = (*p == '"') ? '\'' : *p;
		escaped[w] = '\0';

		cslua_snprintf(cmd, sizeof cmd, "kick #%d \"%s\"\n", g_engfuncs.pfnGetPlayerUserId(e), escaped);
	} else {
		cslua_snprintf(cmd, sizeof cmd, "kick #%d\n", g_engfuncs.pfnGetPlayerUserId(e));
	}
	cmd[sizeof cmd - 1] = '\0';
	SERVER_COMMAND(cmd);
	return 0;
}

// p:ban(minutes [, reason]) - 0 minutes = permanent. Written to the ban list so
// it survives a restart.
static int l_ban(lua_State *L)
{
	int id = self_player_id(L);
	int minutes = (int)luaL_optinteger(L, 2, 0);
	const char *reason = luaL_optstring(L, 3, NULL);

	if (minutes < 0)
		minutes = 0;

	if (!g_players.is_connected(id))
		return 0;

	edict_t *e = g_engfuncs.pfnPEntityOfEntIndex(id);
	if (!e || e->free)
		return 0;

	if (reason && *reason)
		cslua_send_console(id, reason);

	char cmd[96];
	cslua_snprintf(cmd, sizeof cmd, "banid %d #%d kick\n", minutes, g_engfuncs.pfnGetPlayerUserId(e));
	cmd[sizeof cmd - 1] = '\0';
	SERVER_COMMAND(cmd);
	SERVER_COMMAND("writeid\n");
	return 0;
}

// p:exec("spec_mode 1") - make the client run a console command. It runs on the
// player's machine and nothing reports back.
static int l_exec(lua_State *L)
{
	int id = self_player_id(L);
	const char *cmd = luaL_checkstring(L, 2);

	if (!g_players.is_connected(id))
		return 0;

	edict_t *e = g_engfuncs.pfnPEntityOfEntIndex(id);
	if (!e || e->free)
		return 0;

	// The engine's buffer for this is 128 bytes and does not truncate politely.
	if (strlen(cmd) > 120)
		return luaL_error(L, "p:exec: command is too long (%d chars, 120 max)", (int)strlen(cmd));

	CLIENT_COMMAND(e, "%s\n", cmd);
	return 0;
}

// Damage dealt "by the world", so the kill is not credited to anybody.
static entvars_t *world_pev()
{
	edict_t *world = g_engfuncs.pfnPEntityOfEntIndex(0);
	return world ? &world->v : NULL;
}

// p:slay() - kill without blaming a player. Goes through TakeDamage so the game
// does the whole death (dropped weapons, death message, round win check).
static int l_slay(lua_State *L)
{
	CBasePlayer *player = self_cbase(L);

	if (player->pev->deadflag != DEAD_NO)
		return 0;

	entvars_t *world = world_pev();
	if (!world)
		return 0;

	player->TakeDamage(world, world, player->pev->health + 100.0f, DMG_GENERIC);
	return 0;
}

// p:spawn() - the between-rounds respawn: money, score and team survive it.
// Unlike p:team(t, true), which blinks the screen and resets the scoreboard.
static int l_spawn(lua_State *L)
{
	CBasePlayer *player = self_cbase(L);

	// RoundRespawn on a spectator would drop them into the map with no team.
	if (player->m_iTeam != TERRORIST && player->m_iTeam != CT)
		return 0;

	player->RoundRespawn();
	return 0;
}

// p:slap([damage]) - a little damage and a shove. Damage defaults to 1, 0 for a
// pure shove.
static int l_slap(lua_State *L)
{
	CBasePlayer *player = self_cbase(L);
	int damage = (int)luaL_optinteger(L, 2, 1);

	if (player->pev->deadflag != DEAD_NO)
		return 0;

	// Shove first: if the damage kills them, the body still flies.
	player->pev->velocity.x += (float)(RANDOM_LONG(-180, 180));
	player->pev->velocity.y += (float)(RANDOM_LONG(-180, 180));
	player->pev->velocity.z += (float)(RANDOM_LONG(100, 200));

	if (damage > 0) {
		entvars_t *world = world_pev();
		if (world)
			player->TakeDamage(world, world, (float)damage, DMG_GENERIC);
	}
	return 0;
}

// Pushes the "Money" HUD update so the on-screen figure matches the field.
static void sync_money(int id, int amount, bool flash)
{
	int msg = GET_USER_MSG_ID(PLID, "Money", NULL);
	if (!msg)
		return;

	edict_t *e = g_engfuncs.pfnPEntityOfEntIndex(id);
	if (!e || e->free)
		return;

	MESSAGE_BEGIN(MSG_ONE, msg, NULL, e);
	WRITE_LONG(amount);
	WRITE_BYTE(flash ? 1 : 0);
	MESSAGE_END();
}

// p:money() reads, p:money(1000) sets. p:money(1000, { hud = true }) flashes.
static int l_money(lua_State *L)
{
	CBasePlayer *player = self_cbase(L);

	if (lua_isnoneornil(L, 2)) {
		lua_pushinteger(L, player->m_iAccount);
		return 1;
	}

	int amount = (int)luaL_checkinteger(L, 2);
	if (amount < 0)
		amount = 0;

	bool flash = opt_flag(L, 3, "hud", "money");
	player->m_iAccount = amount;
	sync_money(self_player_id(L), amount, flash);
	return 0;
}

// Refreshes the scoreboard row so a changed death/frag count shows.
static void sync_score(CBasePlayer *player, int id)
{
	int msg = GET_USER_MSG_ID(PLID, "ScoreInfo", NULL);
	if (!msg)
		return;

	MESSAGE_BEGIN(MSG_ALL, msg);
	WRITE_BYTE(id);
	WRITE_SHORT((int)player->pev->frags);
	WRITE_SHORT(player->m_iDeaths);
	WRITE_SHORT(0);
	WRITE_SHORT(player->m_iTeam);
	MESSAGE_END();
}

static int l_deaths(lua_State *L)
{
	CBasePlayer *player = self_cbase(L);

	if (lua_isnoneornil(L, 2)) {
		lua_pushinteger(L, player->m_iDeaths);
		return 1;
	}

	player->m_iDeaths = (int)luaL_checkinteger(L, 2);
	sync_score(player, self_player_id(L));
	return 0;
}

// p:defuser([bool]) - whether the player carries a defuse kit. m_bHasDefuser is
// the same field GiveDefuser/CS_DropWeapon check.
static int l_defuser(lua_State *L)
{
	CBasePlayer *player = self_cbase(L);

	if (lua_isnoneornil(L, 2)) {
		lua_pushboolean(L, player->m_bHasDefuser);
		return 1;
	}

	player->m_bHasDefuser = lua_toboolean(L, 2) != 0;
	return 0;
}

// p:give("weapon_ak47"[, opts]) - GiveNamedItemEx does the full pickup.
//
// opts disguises the result for inventory-bookkeeping only:
//   id        - classname of a real weapon whose m_iId (and HUD icon) to borrow,
//               resolved through ReGameDLL's weapon table (GetWeaponInfo).
//   ammo_type - a spare index (0..31) into m_iPrimaryAmmoType so this item's
//               reserve is tracked separately.
static int l_give(lua_State *L)
{
	CBasePlayer *player = self_cbase(L);
	const char *item_name = luaL_checkstring(L, 2);

	CBaseEntity *given = player->CSPlayer()->GiveNamedItemEx(item_name);
	if (!given || !lua_istable(L, 3))
		return 0;

	CBasePlayerItem *item = static_cast<CBasePlayerItem *>(given);

	lua_getfield(L, 3, "id");
	if (lua_isstring(L, -1) && cslua_regamedll_api()) {
		WeaponInfoStruct *info = cslua_regamedll_api()->GetWeaponInfo(lua_tostring(L, -1));
		if (info)
			item->m_iId = info->id;
	}
	lua_pop(L, 1);

	lua_getfield(L, 3, "ammo_type");
	if (lua_isnumber(L, -1) && item->IsWeapon()) {
		int ammo_type = (int)lua_tointeger(L, -1);
		if (ammo_type >= 0 && ammo_type < MAX_AMMO_SLOTS)
			static_cast<CBasePlayerWeapon *>(item)->m_iPrimaryAmmoType = ammo_type;
	}
	lua_pop(L, 1);

	return 0;
}

// p:strip() removes all weapons; p:strip(true) also takes the suit.
static int l_strip(lua_State *L)
{
	CBasePlayer *player = self_cbase(L);
	bool remove_suit = lua_toboolean(L, 2) != 0;
	player->CSPlayer()->RemoveAllItems(remove_suit);
	return 0;
}

// p:weapon() -> classname of the active weapon, or nil.
static int l_weapon(lua_State *L)
{
	CBasePlayer *player = self_cbase(L);

	CBasePlayerItem *active = player->m_pActiveItem;
	if (!active || !active->pev) {
		lua_pushnil(L);
		return 1;
	}

	lua_pushstring(L, STRING(active->pev->classname));
	return 1;
}

// p:switch_weapon(classname) -> true if the player had it and the game agreed
// to switch. p:give() alone rarely makes the new weapon active.
static int l_switch_weapon(lua_State *L)
{
	CBasePlayer *player = self_cbase(L);
	const char *classname = luaL_checkstring(L, 2);

	CBasePlayerItem *item = player->CSPlayer()->GetItemByName(classname);
	if (!item) {
		lua_pushboolean(L, false);
		return 1;
	}

	lua_pushboolean(L, player->CSPlayer()->SwitchWeapon(item) != 0);
	return 1;
}

// p:weapons() -> array of classnames. The inventory is six HUD slots, each a
// linked list.
static int l_weapons(lua_State *L)
{
	CBasePlayer *player = self_cbase(L);

	lua_newtable(L);

	int n = 0;
	for (int slot = 0; slot < MAX_ITEM_TYPES; slot++) {
		for (CBasePlayerItem *item = player->m_rgpPlayerItems[slot]; item; item = item->m_pNext) {
			if (!item->pev)
				continue;
			lua_pushstring(L, STRING(item->pev->classname));
			lua_rawseti(L, -2, ++n);
		}
	}

	return 1;
}

// Both weapon accessors take an optional classname first: p:clip() works on the
// weapon in hand, p:clip("weapon_ak47") on a named one. NULL means the player
// has no such weapon.
static CBasePlayerWeapon *weapon_arg(lua_State *L, CBasePlayer *player, int &value_at)
{
	CBasePlayerItem *item;

	if (lua_isstring(L, 2) && !lua_isnumber(L, 2)) {
		item = player->CSPlayer()->GetItemByName(lua_tostring(L, 2));
		value_at = 3;
	} else {
		item = player->m_pActiveItem;
		value_at = 2;
	}

	// Only weapons carry a clip and an ammo type; the shield and the C4 do not.
	if (!item || !item->pev || !item->IsWeapon())
		return NULL;

	return static_cast<CBasePlayerWeapon *>(item);
}

// Pushes the "AmmoX" HUD update for one ammo type.
static void sync_ammo(int id, int type, int count)
{
	int msg = GET_USER_MSG_ID(PLID, "AmmoX", NULL);
	if (!msg)
		return;

	edict_t *e = g_engfuncs.pfnPEntityOfEntIndex(id);
	if (!e || e->free)
		return;

	MESSAGE_BEGIN(MSG_ONE, msg, NULL, e);
	WRITE_BYTE(type);
	WRITE_BYTE(count);
	MESSAGE_END();
}

// p:ammo([weapon]) reads the reserve, p:ammo(weapon, 90) sets it. Keyed by the
// weapon that fires it; weapons sharing a calibre share the pool. nil = no such
// weapon (or a knife).
static int l_ammo(lua_State *L)
{
	CBasePlayer *player = self_cbase(L);

	int value_at;
	CBasePlayerWeapon *weapon = weapon_arg(L, player, value_at);

	int type = weapon ? weapon->PrimaryAmmoIndex() : -1;
	if (type < 0 || type >= MAX_AMMO_SLOTS) {
		if (lua_isnoneornil(L, value_at)) {
			lua_pushnil(L);
			return 1;
		}
		return 0;
	}

	if (lua_isnoneornil(L, value_at)) {
		lua_pushinteger(L, player->m_rgAmmo[type]);
		return 1;
	}

	int count = (int)luaL_checkinteger(L, value_at);
	if (count < 0)
		count = 0;

	// The game's own cap. AmmoX carries one byte.
	ItemInfo info;
	memset(&info, 0, sizeof info);
	int cap = weapon->GetItemInfo(&info) ? info.iMaxAmmo1 : 255;
	if (cap <= 0 || cap > 255)
		cap = 255;
	if (count > cap)
		count = cap;

	player->m_rgAmmo[type] = count;
	sync_ammo(self_player_id(L), type, count);
	return 0;
}

// Pushes "CurWeapon" so the clip figure on screen matches the field.
static void sync_clip(int id, int weapon_id, int clip)
{
	int msg = GET_USER_MSG_ID(PLID, "CurWeapon", NULL);
	if (!msg)
		return;

	edict_t *e = g_engfuncs.pfnPEntityOfEntIndex(id);
	if (!e || e->free)
		return;

	MESSAGE_BEGIN(MSG_ONE, msg, NULL, e);
	WRITE_BYTE(1);				// 1 = this is the weapon in hand
	WRITE_BYTE(weapon_id);
	WRITE_BYTE(clip);
	MESSAGE_END();
}

// p:clip([weapon, ]v) - rounds in the magazine. A knife/grenade keeps m_iClip
// at -1, which reads back as nil. Writing only reaches the HUD for the weapon
// in hand.
static int l_clip(lua_State *L)
{
	CBasePlayer *player = self_cbase(L);

	int value_at;
	CBasePlayerWeapon *weapon = weapon_arg(L, player, value_at);

	if (!weapon || weapon->m_iClip < 0) {
		if (lua_isnoneornil(L, value_at)) {
			lua_pushnil(L);
			return 1;
		}
		return 0;
	}

	if (lua_isnoneornil(L, value_at)) {
		lua_pushinteger(L, weapon->m_iClip);
		return 1;
	}

	int clip = (int)luaL_checkinteger(L, value_at);
	if (clip < 0)
		clip = 0;

	ItemInfo info;
	memset(&info, 0, sizeof info);
	int cap = weapon->GetItemInfo(&info) ? info.iMaxClip : 0;
	if (cap > 0 && clip > cap)
		clip = cap;

	weapon->m_iClip = clip;

	if (weapon == player->m_pActiveItem)
		sync_clip(self_player_id(L), weapon->m_iId, clip);
	return 0;
}

// p:drop([weapon]) - throw a weapon on the floor the way the drop command does.
// Without an argument it drops what is in hand.
static int l_drop(lua_State *L)
{
	CBasePlayer *player = self_cbase(L);
	const char *name = luaL_optstring(L, 2, NULL);

	if (!name) {
		CBasePlayerItem *active = player->m_pActiveItem;
		if (!active || !active->pev)
			return 0;
		name = STRING(active->pev->classname);
	}

	player->CSPlayer()->DropPlayerItem(name);
	return 0;
}

static int l_console(lua_State *L)
{
	cslua_send_console(self_id(L), luaL_checkstring(L, 2));
	return 0;
}

// p:chat(text) or p:chat(text, { from = otherPlayer })
static int l_chat(lua_State *L)
{
	int id = self_id(L);
	const char *text = luaL_checkstring(L, 2);

	int from = 0;
	if (!lua_isnoneornil(L, 3)) {
		luaL_checktype(L, 3, LUA_TTABLE);
		lua_getfield(L, 3, "from");
		if (lua_istable(L, -1)) {
			lua_getfield(L, -1, "id");
			from = (int)lua_tointeger(L, -1);
			lua_pop(L, 1);
		} else if (lua_isnumber(L, -1)) {
			from = (int)lua_tointeger(L, -1);
		}
		lua_pop(L, 1);
	}

	cslua_send_chat(id, text, from);
	return 0;
}

static int l_center(lua_State *L)
{
	cslua_send_center(self_id(L), luaL_checkstring(L, 2));
	return 0;
}

static int l_hud(lua_State *L)
{
	int id = self_id(L);
	const char *text = luaL_checkstring(L, 2);

	HudParams params;
	cslua_read_hud_params(L, 3, params);
	cslua_send_hud(id, text, params);
	return 0;
}

static int l_dhud(lua_State *L)
{
	int id = self_id(L);
	const char *text = luaL_checkstring(L, 2);

	HudParams params;
	cslua_read_hud_params(L, 3, params);
	cslua_send_dhud(id, text, params);
	return 0;
}

// p:screen_shake([opts]) - opts = { amplitude, frequency, duration } in
// UTIL_ScreenShake units. No radius/PVS filtering.
static int l_screen_shake(lua_State *L)
{
	int id = self_id(L);

	float amplitude = 16.0f, frequency = 150.0f, duration = 1.0f;

	if (!lua_isnoneornil(L, 2)) {
		luaL_checktype(L, 2, LUA_TTABLE);

		lua_getfield(L, 2, "amplitude");
		if (lua_isnumber(L, -1)) amplitude = (float)lua_tonumber(L, -1);
		lua_pop(L, 1);

		lua_getfield(L, 2, "frequency");
		if (lua_isnumber(L, -1)) frequency = (float)lua_tonumber(L, -1);
		lua_pop(L, 1);

		lua_getfield(L, 2, "duration");
		if (lua_isnumber(L, -1)) duration = (float)lua_tonumber(L, -1);
		lua_pop(L, 1);
	}

	cslua_send_screen_shake(id, amplitude, frequency, duration);
	return 0;
}

// p:screen_fade([opts]) - opts = { color, alpha, duration, hold, out, modulate,
// stay }.
static int l_screen_fade(lua_State *L)
{
	int id = self_id(L);

	ScreenFadeParams params;
	cslua_read_screen_fade_params(L, 2, params);
	cslua_send_screen_fade(id, params);
	return 0;
}

// p:play_sound("misc/foo.wav") or with { volume, channel, pitch, attenuation }.
// attenuation 0 = no falloff (UI-ish sounds).
static int l_play_sound(lua_State *L)
{
	int id = self_id(L);
	const char *sample = luaL_checkstring(L, 2);

	float volume = VOL_NORM;
	float attenuation = ATTN_NONE;
	int channel = CHAN_AUTO;
	int pitch = PITCH_NORM;
	bool priv = false;

	if (!lua_isnoneornil(L, 3)) {
		luaL_checktype(L, 3, LUA_TTABLE);

		lua_getfield(L, 3, "volume");
		if (lua_isnumber(L, -1)) volume = (float)lua_tonumber(L, -1);
		lua_pop(L, 1);

		lua_getfield(L, 3, "attenuation");
		if (lua_isnumber(L, -1)) attenuation = (float)lua_tonumber(L, -1);
		lua_pop(L, 1);

		lua_getfield(L, 3, "channel");
		if (lua_isnumber(L, -1)) channel = (int)lua_tointeger(L, -1);
		lua_pop(L, 1);

		lua_getfield(L, 3, "pitch");
		if (lua_isnumber(L, -1)) pitch = (int)lua_tointeger(L, -1);
		lua_pop(L, 1);

		lua_getfield(L, 3, "private");
		priv = lua_toboolean(L, -1) != 0;
		lua_pop(L, 1);
	}

	if (priv) {
		cslua_play_sound_private(id, sample);
		return 0;
	}

	cslua_play_sound(id, sample, channel, volume, attenuation, pitch);
	return 0;
}

static int l_tostring(lua_State *L)
{
	int id = self_id(L);
	if (id == 0)
		lua_pushstring(L, "all players");
	else
		lua_pushfstring(L, "player #%d %s", id, g_players.name(id));
	return 1;
}

static int l_readonly(lua_State *L)
{
	return luaL_error(L, "player objects are shared and read-only, keep your own state in a table");
}

// p:motd(text[, { raw = true }]) - the client's own MOTD panel, the one
// multi-line surface the game has. Renders HTML; by default the text is wrapped
// so newlines break lines and Cyrillic arrives readable. raw = true sends it
// untouched.
static int l_motd(lua_State *L)
{
	const char *text = luaL_checkstring(L, 2);
	bool raw = opt_flag(L, 3, "raw", "motd");

	cslua_send_motd(self_player_id(L), text, raw);
	return 0;
}

static const luaL_Reg s_messaging[] =
{
	{ "motd",       l_motd },
	{ "console",    l_console },
	{ "chat",       l_chat },
	{ "center",     l_center },
	{ "hud",        l_hud },
	{ "dhud",       l_dhud },
	{ "screen_shake", l_screen_shake },
	{ "screen_fade",  l_screen_fade },
	{ "play_sound", l_play_sound },
	{ NULL, NULL }
};

static const luaL_Reg s_queries[] =
{
	{ "name",      l_name },
	{ "ip",        l_ip },
	{ "steamid",   l_steamid },
	{ "info",      l_info },
	{ "userid",    l_userid },
	{ "connected", l_connected },
	{ "ping",      l_ping },

	// entvars: no argument reads, an argument writes
	{ "model",     l_model },
	{ "health",    l_health },
	{ "armor",     l_armor },
	{ "frags",     l_frags },
	{ "gravity",   l_gravity },
	{ "maxspeed",  l_maxspeed },
	{ "origin",    l_origin },
	{ "angles",    l_angles },
	{ "velocity",  l_velocity },
	{ "punchangle", l_punchangle },
	{ "aim",       l_aim },
	{ "pev",       l_pev },
	{ "trace",     l_trace },
	{ "trace_to",  l_trace_to },
	{ "alive",     l_alive },
	{ "button",    l_button },
	{ "on_ground", l_on_ground },
	{ "ducking",   l_ducking },
	{ "is_bot",    l_is_bot },
	{ "is_hltv",   l_is_hltv },
	{ "freeze",    l_freeze },
	{ "godmode",   l_godmode },
	{ "noclip",    l_noclip },
	{ "noblock",   l_noblock },
	{ "suppress_attack", l_suppress_attack },
	{ "suppress_move",   l_suppress_move },
	{ "suppress_drop",   l_suppress_drop },

	// Admin actions: kick, ban and exec are pure engine, no ReGameDLL needed.
	{ "kick",      l_kick },
	{ "ban",       l_ban },
	{ "exec",      l_exec },

	// ReGameDLL-only: raise an error on a vanilla mp.dll
	{ "team",         l_team },
	{ "progress_bar", l_progress_bar },
	{ "spawn",     l_spawn },
	{ "money",     l_money },
	{ "deaths",    l_deaths },
	{ "defuser",   l_defuser },
	{ "give",      l_give },
	{ "strip",     l_strip },
	{ "weapon",    l_weapon },
	{ "switch_weapon", l_switch_weapon },
	{ "weapons",   l_weapons },
	{ "ammo",      l_ammo },
	{ "clip",      l_clip },
	{ "drop",      l_drop },
	{ "slay",      l_slay },
	{ "slap",      l_slap },
	{ NULL, NULL }
};

static void register_all(lua_State *L, const luaL_Reg *list)
{
	for (const luaL_Reg *r = list; r->name; r++) {
		lua_pushcfunction(L, r->func);
		lua_setfield(L, -2, r->name);
	}
}

// Shared tail of both metatables.
static void finish_metatable(lua_State *L)
{
	lua_pushcfunction(L, l_tostring);
	lua_setfield(L, -2, "__tostring");

	// These objects are shared by every plugin.
	lua_pushcfunction(L, l_readonly);
	lua_setfield(L, -2, "__newindex");

	lua_pushboolean(L, 0);
	lua_setfield(L, -2, "__metatable");
}

// The player object: everything, with __index a plain table so players.method()
// can extend it.
static void push_player_metatable(lua_State *L)
{
	lua_newtable(L);				// metatable

	lua_newtable(L);				// methods
	register_all(L, s_messaging);
	register_all(L, s_queries);
	lua_setfield(L, -2, "__index");

	finish_metatable(L);
}

// players.broadcast carries the messaging methods and nothing else. A miss
// raises instead of returning nil.
static int l_broadcast_index(lua_State *L)
{
	lua_pushvalue(L, 2);					// key
	lua_rawget(L, lua_upvalueindex(1));		// methods[key]
	if (!lua_isnil(L, -1))
		return 1;

	const char *key = lua_isstring(L, 2) ? lua_tostring(L, 2) : luaL_typename(L, 2);

	return luaL_error(L, "players.broadcast has no '%s' - it only sends to "
		"everyone at once (chat, console, center, hud, dhud, play_sound). "
		"To read or change player state, walk players.list()", key);
}

static void push_broadcast_metatable(lua_State *L)
{
	lua_newtable(L);				// metatable

	lua_newtable(L);				// methods
	register_all(L, s_messaging);
	lua_pushcclosure(L, l_broadcast_index, 1);
	lua_setfield(L, -2, "__index");

	finish_metatable(L);
}

// players.method("can", function(self, node) ... end)
//
// The player metatable is sealed, so this is the one sanctioned door for the
// core layer to extend the player object. Refuses to replace an existing method.
static int l_player_method(lua_State *L)
{
	const char *name = luaL_checkstring(L, 1);
	luaL_checktype(L, 2, LUA_TFUNCTION);

	if (s_methods_ref == LUA_NOREF)
		return luaL_error(L, "players.method: called before the player API was set up");

	lua_rawgeti(L, LUA_REGISTRYINDEX, s_methods_ref);

	lua_getfield(L, -1, name);
	if (!lua_isnil(L, -1))
		return luaL_error(L, "players.method: player method '%s' already exists", name);
	lua_pop(L, 1);

	lua_pushvalue(L, 2);
	lua_setfield(L, -2, name);
	lua_pop(L, 1);
	return 0;
}

static const luaL_Reg s_players_api[] =
{
	{ "method", l_player_method },
	{ NULL, NULL }
};

void cslua_player_init(lua_State *L)
{
	push_player_metatable(L);
	int player_mt = lua_gettop(L);

	lua_getfield(L, player_mt, "__index");
	s_methods_ref = luaL_ref(L, LUA_REGISTRYINDEX);

	cslua_register_namespace(L, "players", s_players_api);

	for (int id = 1; id < CSLUA_MAXPLAYERS; id++) {
		lua_newtable(L);
		lua_pushinteger(L, id);
		lua_setfield(L, -2, "id");
		lua_pushvalue(L, player_mt);
		lua_setmetatable(L, -2);
		s_player_ref[id] = luaL_ref(L, LUA_REGISTRYINDEX);
	}

	lua_pop(L, 1);					// player metatable

	push_broadcast_metatable(L);
	lua_newtable(L);
	lua_pushinteger(L, 0);
	lua_setfield(L, -2, "id");
	lua_pushvalue(L, -2);			// the broadcast metatable
	lua_setmetatable(L, -2);
	s_all_ref = luaL_ref(L, LUA_REGISTRYINDEX);
	lua_pop(L, 1);					// broadcast metatable

	lua_getglobal(L, "players");
	cslua_push_all(L);
	lua_setfield(L, -2, "broadcast");
	lua_pop(L, 1);
}

void cslua_player_shutdown()
{
	for (int id = 0; id < CSLUA_MAXPLAYERS; id++)
		s_player_ref[id] = LUA_NOREF;
	s_all_ref = LUA_NOREF;
	s_methods_ref = LUA_NOREF;
}

void cslua_push_player(lua_State *L, int id)
{
	if (!cslua_valid_player_id(id) || s_player_ref[id] == LUA_NOREF) {
		lua_pushnil(L);
		return;
	}
	lua_rawgeti(L, LUA_REGISTRYINDEX, s_player_ref[id]);
}

void cslua_push_all(lua_State *L)
{
	if (s_all_ref == LUA_NOREF) {
		lua_pushnil(L);
		return;
	}
	lua_rawgeti(L, LUA_REGISTRYINDEX, s_all_ref);
}
