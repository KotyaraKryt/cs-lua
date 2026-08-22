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

// The shared __index table behind every player object. Kept so Lua can add
// methods to it - see players.method() below.
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

	if (id < 0 || id >= CSLUA_MAXPLAYERS)
		return luaL_error(L, "p: player index %d out of range", id);

	// "id" lives as a plain table field - every method here reads it that way
	// without a metamethod round trip - but that means __newindex (l_readonly,
	// below) never fires for it: the key already exists by the time a script
	// gets its hands on the object, and __newindex only triggers on an absent
	// key. p.id = N would otherwise silently repoint this shared, cached
	// object at a different slot instead of erroring. These objects are one
	// singleton per slot for the whole process (s_player_ref), so checking
	// here protects every other plugin, not just the one that wrote it.
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

// p:info("_pw") - a key from the client's own infobuffer, set client-side with
// `setinfo`. The access layer uses it for the nick+password login, the way
// AmxModX does; anything the client sets is client-controlled, so never trust
// it for more than matching against a stored secret.
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

// Everything below reads and writes entvars_t directly through the edict, so
// it needs no offsets and works on any game DLL. A missing player is an error
// rather than a silent zero: use p:connected() if you are unsure.
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

// Writing origin goes through the engine so the entity gets relinked; setting
// pev->origin by hand leaves the player stuck in the old spot for collisions.
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

// p:pev(name[, v...]) - generic read/write for any entvars_t field by its
// real name, sharing the field table and dispatch in lua_pev.cpp with
// e:pev() in lua_entity.cpp so both objects agree on it byte for byte.
static int l_pev(lua_State *L)
{
	return cslua_pev_call(L, self_pev(L), 2);
}

// Shared by l_trace and l_trace_to: turns a finished TraceResult into the
// { kind, player, classname, x, y, z, distance, hitgroup } table both hand
// back. `start` is only needed to compute distance.
void cslua_push_trace_result(lua_State *L, const TraceResult &tr, const Vector &start)
{
	lua_newtable(L);

	// A trace that ran the whole way hit nothing. The coordinates are still
	// filled in with the ray's end, so a plugin drawing an effect there does
	// not have to special-case it.
	edict_t *hit = tr.pHit;
	int hit_index = (hit && !hit->free) ? ENTINDEX(hit) : -1;

	const char *kind;
	if (tr.flFraction >= 1.0f && hit_index <= 0) {
		kind = "none";
	} else if (hit_index >= 1 && hit_index < CSLUA_MAXPLAYERS && g_players.is_connected(hit_index)) {
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

// p:trace([distance]) - fire a ray from the player's eyes along their aim and
// report the first thing it meets. p:aim() gives the direction but nothing to
// aim at; everything of the "what am I looking at" kind (HP over the crosshair,
// a laser, zone triggers) needs the ray.
//
// The result is a table rather than a row of returns: the fields are of mixed
// types, and a positional kind, player, classname, x, y, z, distance reads
// badly and cannot grow later.
static int l_trace(lua_State *L)
{
	int id = self_player_id(L);
	entvars_t *pev = self_pev(L);
	float distance = (float)luaL_optnumber(L, 2, 8192.0);

	// v_angle is the client's view; MAKE_VECTORS turns it into v_forward.
	MAKE_VECTORS(pev->v_angle);

	Vector start = pev->origin + pev->view_ofs;
	Vector end = start + gpGlobals->v_forward * distance;

	TraceResult tr;
	TRACE_LINE(start, end, dont_ignore_monsters, g_engfuncs.pfnPEntityOfEntIndex(id), &tr);

	cslua_push_trace_result(L, tr, start);
	return 1;
}

// p:trace_to(other) - a line from p's eyes straight to other's eyes, not
// along p's current crosshair. p:trace() answers "what is p looking at
// right now"; this answers "is there anything solid between p and other" -
// the question that actually matters for "did p have a clear shot at other",
// since weapon spread and recoil can point the crosshair somewhere other
// than where the bullet that hit `other` actually went. Aiming at the real
// target instead of guessing the shot's direction sidesteps that entirely.
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

	if (other_id <= 0 || other_id >= CSLUA_MAXPLAYERS || !g_players.is_connected(other_id))
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

// sv.hull_free(x, y, z[, ducking]) - would a standing (or, with `ducking`,
// crouching) player fit at this point without spawning stuck in world
// geometry? A zero-length TRACE_HULL at the point is the standard GoldSrc
// way to ask that: fStartSolid/fAllSolid come back true when the human hull
// (or head_hull while ducked) is already overlapping something solid there.
// Not a player method - nobody has to already exist at the point in question,
// unlike trace()/trace_to() which fire from a player's own eyes.
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

// Name -> IN_* bit, for p:button(). Only the ones a script can plausibly want
// to poll; the rest of usercmd (mouse deltas, impulse) has no bit to check.
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

// p:button("use") - true while the player holds that key down, read fresh off
// pev->button every call. This is the same field AMXX's var_button reads: the
// engine writes it from the client's usercmd before every PlayerPreThink, live
// entvars, no hook needed on our side.
//
// Read-only: the client owns this field, and writing it would just get
// overwritten by the next usercmd a frame later.
static int l_button(lua_State *L)
{
	const char *name = luaL_checkstring(L, 2);
	int bit = button_bit(name);
	if (!bit)
		return luaL_error(L, "p:button: unknown button '%s'", name);

	lua_pushboolean(L, (self_pev(L)->button & bit) != 0);
	return 1;
}

// FL_FAKECLIENT marks a server-side bot; FL_PROXY marks an HLTV spectator
// proxy. Both live in entvars, so they work without ReGameDLL.
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

// Read/write in one method for a single bit of pev->flags, the way
// scalar_field does it for a whole field.
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

// p:freeze([bool]) - the engine's own freeze: PM_Move sees FL_FROZEN and stops
// the player where they stand. The usual workaround, maxspeed(1), leaves the
// client predicting movement the server refuses, which is the stutter you see.
static int l_freeze(lua_State *L)
{
	return flag_field(L, self_pev(L), FL_FROZEN);
}

// p:godmode([bool]) - takedamage is what the damage path actually checks, so
// that is the source of truth here; FL_GODMODE is set alongside it because
// that is the bit other mods and the engine's own `god` command look at.
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

// p:noclip([bool]) - fly through walls. MOVETYPE_WALK is what a live player
// normally has, so that is what turning it off restores.
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

// p:noblock([bool]) - true lets other players walk through this one instead
// of shoving them apart. SOLID_NOT only affects how OTHER entities treat this
// one (their hull traces skip it); it does not touch this player's own
// movement against the level, which PM_Move clips against the world
// directly - so unlike noclip this cannot drop someone through the floor.
// SOLID_SLIDEBOX is the normal value a live player has.
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

// p:suppress_attack([bool]) - true drops IN_ATTACK/IN_ATTACK2 from this
// player's button state every frame (dllapi.cpp's pfnPlayerPreThink),
// before the weapon code sees it: held-down fire never happens at all,
// rather than firing and then having its damage or effects undone.
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

// p:suppress_move([bool]) - true drops IN_FORWARD/IN_BACK/IN_MOVELEFT/
// IN_MOVERIGHT from this player's button state every frame, the same way
// p:suppress_attack drops the fire buttons. Pins them in place without
// FL_FROZEN (p:freeze) - that flag turned out to zero pev->button along
// with movement, which is exactly the field a "hold USE to keep going"
// mechanic reads every tick to see if the key is still down.
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
// (dllapi.cpp's ClientCommand, before the game DLL sees it), so the current
// weapon can never become a separate on-the-ground entity with its own ammo
// in the first place.
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
// On a vanilla mp.dll they raise a clear error instead of reading garbage.
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

// Reads a flag out of an options table. Every method that used to take a bare
// positional boolean takes one of these instead: p:money(500, true) never said
// what the true meant, and the same slot meant something different on the next
// method along.
//
// A bare boolean is refused rather than quietly accepted - taking it would
// leave half a codebase written the old way and half the new.
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

// p:progress_bar(seconds) - the same under-crosshair progress bar the game
// shows for planting/defusing the bomb, driven by CSPlayer's own
// SetProgressBarTime (ICSPlayer, CSPlayer.h). The client counts it down on
// its own once sent - there is nothing to update per frame - so this is a
// one-shot trigger, not a readable value. 0 clears the bar early, the way
// letting go of the plant/defuse key does.
static int l_progress_bar(lua_State *L)
{
	CBasePlayer *player = self_cbase(L);
	int seconds = (int)luaL_checknumber(L, 2);
	if (seconds < 0)
		seconds = 0;

	player->CSPlayer()->SetProgressBarTime(seconds);
	return 0;
}

// p:team() reads "CT"/"T"/"SPEC"/"NONE".
// p:team("CT")                    soft move: change side, keep the player alive.
// p:team("CT", { force = true })  the player dies and joins fresh, the way the
//                                 game does it (JoinTeam kills a living player
//                                 on switch).
// The soft path can't put a spectator into a round on its own, so it upgrades
// to a force join for that case.
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
		// JoinTeam does the full transition (and the kill, if alive).
		player->CSPlayer()->JoinTeam(team);
	} else {
		// Just move sides: swap the team, refresh model and scoreboard.
		player->m_iTeam = team;
		player->m_bTeamChanged = true;
		player->CSPlayer()->TeamChangeUpdate();
	}
	return 0;
}

// Admin actions.
//
// kick and ban go through the engine's own console commands - that is the only
// way to drop a client, and it keeps the ban in the same list the server admin
// already manages. They are addressed by userid, never by name: a nickname can
// contain spaces or quotes and would break the command line.

// p:kick([reason]) - the engine's own "kick #<userid> [reason]" takes a
// trailing reason argument (see Host_Kick_f) and folds it straight into the
// SV_DropClient message ("Kicked :<reason>"), which is what actually shows
// up on the client's disconnect screen. A bare "kick #<userid>" with no
// reason argument at all falls through to the engine's generic "Kicked" -
// this used to only print the reason to the player's own console
// (cslua_send_console), invisible unless their console happened to already
// be open, and gone by the time it would matter anyway since the drop
// follows immediately after.
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
		// The reason rides the same console command line as a quoted
		// argument, so it can't carry a '"' of its own - Cmd_TokenizeString
		// would end the argument right there and split the rest into
		// further tokens.
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

// p:ban(minutes [, reason]) - 0 minutes means permanent. The id is written to
// the ban list so it survives a restart, which is what an admin expects.
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

	// banid takes minutes; "kick" makes it drop the player right away.
	char cmd[96];
	cslua_snprintf(cmd, sizeof cmd, "banid %d #%d kick\n", minutes, g_engfuncs.pfnGetPlayerUserId(e));
	cmd[sizeof cmd - 1] = '\0';
	SERVER_COMMAND(cmd);
	SERVER_COMMAND("writeid\n");
	return 0;
}

// p:exec("spec_mode 1") - make the client run a console command, the way
// amx_client_cmd does. The command runs on the player's machine, so it only
// reaches what their engine and their config expose; a client can also have
// unbound or removed it, and nothing reports back when that happens.
static int l_exec(lua_State *L)
{
	int id = self_player_id(L);
	const char *cmd = luaL_checkstring(L, 2);

	if (!g_players.is_connected(id))
		return 0;

	edict_t *e = g_engfuncs.pfnPEntityOfEntIndex(id);
	if (!e || e->free)
		return 0;

	// The engine's buffer for this is 128 bytes and it does not truncate
	// politely, so refuse anything that would not fit.
	if (strlen(cmd) > 120)
		return luaL_error(L, "p:exec: command is too long (%d chars, 120 max)", (int)strlen(cmd));

	CLIENT_COMMAND(e, "%s\n", cmd);
	return 0;
}

// Damage dealt "by the world", so the kill is not credited to anybody and the
// scoreboard stays honest.
static entvars_t *world_pev()
{
	edict_t *world = g_engfuncs.pfnPEntityOfEntIndex(0);
	return world ? &world->v : NULL;
}

// p:slay() - kill without blaming a player. Goes through TakeDamage so the
// game does the whole death properly: dropped weapons, death message, round
// win check. Setting health to 0 by hand does none of that.
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

// p:spawn() - put the player into the round alive, from either state. This is
// the respawn the game itself runs between rounds, so money, score and team
// survive it untouched.
//
// The alternative people reach for, p:team(t, true), goes through JoinTeam:
// that kills a living player and re-enters them, which blinks the screen and
// resets the scoreboard. This does neither.
static int l_spawn(lua_State *L)
{
	CBasePlayer *player = self_cbase(L);

	// Nothing to respawn into without a side. RoundRespawn on a spectator
	// would drop them into the map with no team, so refuse quietly - the same
	// way kick does nothing for an empty slot.
	if (player->m_iTeam != TERRORIST && player->m_iTeam != CT)
		return 0;

	player->RoundRespawn();
	return 0;
}

// p:slap([damage]) - the classic: a little damage and a shove in a random
// direction. Damage defaults to 1 and can be 0 for a pure shove.
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

// p:money() reads, p:money(1000) sets.
// p:money(1000, { hud = true }) also flashes the on-screen figure, the way
// picking up cash does.
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

// Refreshes the scoreboard row so a changed death/frag count actually shows.
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

// p:defuser([bool]) - whether the player is carrying a defuse kit. A plain
// public bool on CBasePlayer (player.h), the same GiveDefuser/CS_DropWeapon
// logic already checks - setting it here is exactly what buying one does to
// this field, just without paying for it.
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

// p:give("weapon_ak47"[, opts]) - GiveNamedItemEx does the full pickup (ammo
// included), same as walking over the weapon.
//
// opts lets a script disguise the result as a different weapon for
// inventory-bookkeeping purposes only - what "already have this weapon"
// checks see and which ammo pool it draws from, not what the item actually
// does:
//
//   id        - classname of a real weapon whose WeaponIdType to borrow
//               (m_iId), resolved through ReGameDLL's own weapon table
//               (GetWeaponInfo), not a hardcoded number. Also decides the
//               HUD icon: the client already has that weapon's WeaponList
//               entry from map load, so borrowing its id borrows its icon
//               for free, no custom art or WeaponList hook needed.
//   ammo_type - a spare index (0..31) into the player's ammo array
//               (m_iPrimaryAmmoType), so this item's reserve is tracked
//               separately from whatever it shares a real ammo type with.
//               Pick one no real weapon in cfg uses.
//
// Same trick the reference AmxModX plugins reach for ReAPI's
// rg_set_iteminfo for, minus the fragile part: m_iId and m_iPrimaryAmmoType
// are plain public members on CBasePlayerItem/CBasePlayerWeapon
// (weapons.h), not something that needs signature-scanning to reach - as
// safe to write as m_pActiveItem already is elsewhere in this file.
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

// p:weapon() -> classname of the active weapon ("weapon_ak47"), or nil.
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

// p:switch_weapon(classname) -> true if the player was carrying it and the
// game agreed to switch (same call the weapon-select key makes), false if
// they don't have it. p:give() alone never makes the new weapon active on
// its own unless the slot it lands in was empty - a script that wants a
// just-given weapon in hand right away needs this too.
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

// p:weapons() -> array of classnames the player is carrying. The inventory is
// six HUD slots, each a linked list, so a plain loop over the array misses
// everything but the first grenade.
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

// Both weapon accessors take an optional classname first: p:clip() works on
// the weapon in hand, p:clip("weapon_ak47") on a named one. This resolves the
// weapon and reports where the value argument landed, so the caller does not
// have to count arguments twice.
//
// NULL means the player has no such weapon - an answer, not an error, so the
// caller decides what to push.
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

// Pushes the "AmmoX" HUD update for one ammo type. Without it the reserve
// changes in memory while the client keeps drawing the old figure - the same
// trap sync_money() exists for.
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

// p:ammo([weapon]) reads the reserve, p:ammo(weapon, 90) sets it. Ammo is keyed
// by the weapon that fires it, not by the game's internal ammo name: that name
// is not reachable from the SDK we vendor, and "ammo for the AK" is what a shop
// wants to sell anyway. Weapons sharing a calibre share the pool, so buying for
// the AK also fills a Galil.
//
// nil means the player has no such weapon; a knife has no ammo either.
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

	// The game's own cap, so a plugin cannot hand out a reserve the HUD cannot
	// draw and the weapon would never reload from. AmmoX carries one byte.
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

// p:clip([weapon, ]v) - rounds in the magazine. A knife and the grenades keep
// m_iClip at -1; that reads back as nil rather than -1, so a plugin can tell
// "empty" from "has no magazine at all".
//
// Writing only reaches the HUD for the weapon in hand - the client is told
// about one clip at a time - but the field is right either way, and switching
// to the weapon shows it.
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

// p:drop([weapon]) - throw a weapon on the floor, the way the player's own
// drop command does: the game makes the box, keeps the clip in it and picks
// the next weapon to hold. Without an argument it drops what is in hand.
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

// p:play_sound("misc/foo.wav") or with options:
//   { volume = 1.0, channel = 0, pitch = 100, attenuation = 0.8 }
// attenuation 0 means "no falloff" - the same loudness anywhere on the map,
// which is what you want for UI-ish sounds.
static int l_play_sound(lua_State *L)
{
	int id = self_id(L);
	const char *sample = luaL_checkstring(L, 2);

	float volume = VOL_NORM;
	float attenuation = ATTN_NORM;
	int channel = CHAN_AUTO;
	int pitch = PITCH_NORM;

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

// p:motd(text[, { raw = true }]) - the client's own MOTD panel. The one
// multi-line surface the game has: a menu is nine keys and the HUD is a single
// string, so a shop catalogue, a top-10 or the server rules go here.
//
// The panel renders HTML, which the caller should not have to care about: by
// default the text is wrapped so newlines break lines and Cyrillic arrives
// readable. raw = true sends it untouched, for markup written on purpose.
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

	// entvars: no argument reads, an argument writes
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

	// These objects are shared by every plugin, so one plugin bolting a field
	// onto a player would leak it into all the others.
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

// players.broadcast carries the messaging methods and nothing else - there is
// no single health to read from "everyone".
//
// A miss raises instead of returning nil. Handing back nil is what made the v1
// `all` object a trap: `all:alive()` died three frames later with "attempt to
// call a nil value" and no hint that the object simply never had the method.
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
// The player metatable is sealed (no __metatable, __newindex refuses), so Lua
// cannot bolt methods on by itself - which is what keeps one plugin from
// redefining p:health() for everybody. This is the one sanctioned door: it
// refuses to replace an existing method, so the core layer can extend the
// player object (p:can(), p:groups()) without opening it up to hijacking.
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
	// The refs die with the state; just forget them.
	for (int id = 0; id < CSLUA_MAXPLAYERS; id++)
		s_player_ref[id] = LUA_NOREF;
	s_all_ref = LUA_NOREF;
	s_methods_ref = LUA_NOREF;
}

void cslua_push_player(lua_State *L, int id)
{
	if (id < 1 || id >= CSLUA_MAXPLAYERS || s_player_ref[id] == LUA_NOREF) {
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
