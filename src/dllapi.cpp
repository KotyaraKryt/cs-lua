#include "cslua.h"
#include "lua_engine.h"
#include "lua_events.h"
#include "lua_timers.h"
#include "lua_http.h"
#include "lua_mysql.h"
#include "lua_httpserver.h"
#include "lua_sound.h"
#include "lua_menu.h"
#include "lua_entity.h"
#include "players.h"
#include "regamedll.h"
#include "cslua_netwatch.h"
#include "cslua_visibility.h"

#include <entity_state.h>

#include <in_buttons.h>
#include <string.h>

// Defined further down, next to the polls they reset.
void cslua_reset_auth_poll();
void cslua_reset_team_cache();
void cslua_forget_team(int id);

// worldspawn is the map's first entity and the window where the engine still
// accepts precache calls. This runs *post*, after the game DLL has precached
// its own resources, so the slot numbers we get back reflect the real usage of
// the server rather than an empty table.
// Set from worldspawn until the map ends. See cslua_world_ready() in cslua.h
// for why anything touching edicts has to consult it.
static bool s_world_ready = false;

bool cslua_world_ready()
{
	return s_world_ready;
}

static int Spawn_Post(edict_t *pent)
{
	if (pent && !strcmp(STRING(pent->v.classname), "worldspawn")) {
		s_world_ready = true;
		cslua_sound_set_window(true);
		cslua_precache_all();
	}

	RETURN_META_VALUE(MRES_IGNORED, 0);
}

// Runs once per map. Two things happen here: the precache window shuts, and
// everything keyed on gpGlobals->time is moved onto the new clock - it restarts
// from zero on a map change while the Lua state carries on.
static void ServerActivate(edict_t *pEdictList, int edictCount, int clientMax)
{
	cslua_sound_set_window(false);
	cslua_timers_rebase();
	cslua_reset_auth_poll();
	cslua_reset_team_cache();

	for (int id = 1; id < CSLUA_MAXPLAYERS; id++)
		cslua_menu_reset(id);

	RETURN_META(MRES_IGNORED);
}

// The map is ending - the next one, or the server stopping. Last chance for a
// plugin to undo what it did to the world and write its data out; everything
// keyed on the map is about to be wrong.
//
// The Lua state itself lives on across a map change, so this is not a shutdown:
// that is plugin_unload.
static void ServerDeactivate()
{
	g_events.fire_map_change(gpGlobals && gpGlobals->mapname ? STRING(gpGlobals->mapname) : "");

	// After this the edicts go away. Cleared last so a map_change handler can
	// still remove its own entities.
	s_world_ready = false;
	cslua_touch_detonate_clear();
	cslua_visibility_reset();
	RETURN_META(MRES_IGNORED);
}

static qboolean ClientConnect(edict_t *pEntity, const char *pszName, const char *pszAddress, char szRejectReason[128])
{
	int id = g_engfuncs.pfnIndexOfEdict(pEntity);
	g_players.on_connect(id, pszName, pszAddress, g_engfuncs.pfnGetPlayerAuthId(pEntity));

	// Slots get reused; the new occupant starts with a clean screen and no
	// memory of the previous player's team, which would otherwise show up as
	// a team change if both happen inside one frame.
	cslua_menu_reset(id);
	cslua_forget_team(id);

	RejectInfo reject;
	reject.reason[0] = '\0';

	if (g_events.fire_client_connect(id, pszName, pszAddress, reject)) {
		strncpy(szRejectReason, reject.reason, 127);
		szRejectReason[127] = '\0';

		// The engine won't call ClientDisconnect for a refused client.
		g_players.on_disconnect(id);
		RETURN_META_VALUE(MRES_SUPERCEDE, FALSE);
	}

	// Bots, LAN players and clients Steam already knows are authorized right
	// here; everyone else gets the event from the poll in StartFrame.
	if (g_players.is_authorized(id))
		g_events.fire_player_authorized(id, g_players.authid(id));

	RETURN_META_VALUE(MRES_IGNORED, TRUE);
}

static void ClientPutInServer(edict_t *pEntity)
{
	// Only now does the client have the user message table, so this is the
	// first point where chat/HUD output actually reaches them.
	g_events.fire_player_ready(g_engfuncs.pfnIndexOfEdict(pEntity));
	RETURN_META(MRES_IGNORED);
}

static void ClientCommand(edict_t *pEntity)
{
	const char *cmd = CMD_ARGV(0);
	if (!cmd)
		RETURN_META(MRES_IGNORED);

	// A menu answer is consumed here so the game never sees it - otherwise CS
	// would treat the key as a reply to its own team/buy menu.
	if (cslua_menu_handle_select(g_engfuncs.pfnIndexOfEdict(pEntity), cmd))
		RETURN_META(MRES_SUPERCEDE);

	int id = g_engfuncs.pfnIndexOfEdict(pEntity);

	// Superceded outright: the game DLL's own "drop" handler never runs, so
	// there is no weapon entity in the world to have lost its ammo in the
	// first place - see p:suppress_drop in lua_player.cpp.
	if (!strcmp(cmd, "drop") && g_players.suppress_drop(id))
		RETURN_META(MRES_SUPERCEDE);

	bool team = !strcmp(cmd, "say_team");
	if (!team && strcmp(cmd, "say"))
		RETURN_META(MRES_IGNORED);

	// The engine hands us `say "some text"`, already unquoted in argv 1.
	const char *text = CMD_ARGC() > 1 ? CMD_ARGV(1) : "";
	if (!text || !*text)
		RETURN_META(MRES_IGNORED);

	if (g_events.fire_player_chat(id, text, team))
		RETURN_META(MRES_SUPERCEDE);

	RETURN_META(MRES_IGNORED);
}

// Every entity-to-entity touch in the game goes through here - triggers,
// weapons landing on the floor, a grenade bouncing off a wall. Cheap to run
// unconditionally: cslua_touch_detonate_check's early-out is one size check
// against a list that is empty unless a script called e:detonate_on_touch().
static void Touch(edict_t *pentTouched, edict_t *pentOther)
{
	cslua_touch_detonate_check(pentTouched);
	RETURN_META(MRES_IGNORED);
}

// Runs before the engine (and the weapon code inside it) reads this frame's
// button state at all, so clearing IN_ATTACK/IN_ATTACK2 here means the shot
// never happens - no muzzle flash, no sound, no ammo spent - unlike blocking
// after the fact (FireBullets, weapon_fire), which runs once the gun has
// already gone off.
static void PlayerPreThink(edict_t *pEntity)
{
	int id = g_engfuncs.pfnIndexOfEdict(pEntity);

	// Synthesized SecondaryAttack: ReGameDLL has no hookchain for it
	// (unlike PrimaryAttack, which weapon_fire piggybacks on), so a button
	// edge-detect here is the only way to catch "player just pressed RMB"
	// without HamSandwich-style vtable hooking. Read before suppress_attack
	// strips the button below - a script using suppress_attack to swallow a
	// repurposed weapon's real fire (see docs/api/hook/weapon_secondary_attack.md)
	// still needs its own RMB to register.
	bool attack2_down = (pEntity->v.button & IN_ATTACK2) != 0;
	if (g_players.secondary_attack_pressed(id, attack2_down))
		g_events.fire_weapon_secondary_attack(id, cslua_player_active_weapon(id),
			cslua_player_active_weapon_ammo_type(id));

	if (g_players.suppress_attack(id))
		pEntity->v.button &= ~(IN_ATTACK | IN_ATTACK2);
	if (g_players.suppress_move(id))
		pEntity->v.button &= ~(IN_FORWARD | IN_BACK | IN_MOVELEFT | IN_MOVERIGHT);

	RETURN_META(MRES_IGNORED);
}

static void ClientDisconnect(edict_t *pEntity)
{
	int id = g_engfuncs.pfnIndexOfEdict(pEntity);

	if (g_players.is_connected(id))
		g_events.fire_client_disconnect(id, g_players.name(id));

	g_players.on_disconnect(id);
	cslua_menu_reset(id);
	cslua_netwatch_forget(id);
	RETURN_META(MRES_IGNORED);
}

DLL_FUNCTIONS g_DllFunctionTable =
{
	NULL,					// pfnGameInit
	NULL,					// pfnSpawn
	NULL,					// pfnThink
	NULL,					// pfnUse
	Touch,					// pfnTouch
	NULL,					// pfnBlocked
	NULL,					// pfnKeyValue
	NULL,					// pfnSave
	NULL,					// pfnRestore
	NULL,					// pfnSetAbsBox
	NULL,					// pfnSaveWriteFields
	NULL,					// pfnSaveReadFields
	NULL,					// pfnSaveGlobalState
	NULL,					// pfnRestoreGlobalState
	NULL,					// pfnResetGlobalState
	ClientConnect,			// pfnClientConnect
	ClientDisconnect,		// pfnClientDisconnect
	NULL,					// pfnClientKill
	NULL,					// pfnClientPutInServer	(hooked post, see below)
	ClientCommand,			// pfnClientCommand
	NULL,					// pfnClientUserInfoChanged
	ServerActivate,			// pfnServerActivate
	ServerDeactivate,		// pfnServerDeactivate
	PlayerPreThink,			// pfnPlayerPreThink
	NULL,					// pfnPlayerPostThink
	NULL,					// pfnStartFrame
	NULL,					// pfnParmsNewLevel
	NULL,					// pfnParmsChangeLevel
	NULL,					// pfnGetGameDescription
	NULL,					// pfnPlayerCustomization
	NULL,					// pfnSpectatorConnect
	NULL,					// pfnSpectatorDisconnect
	NULL,					// pfnSpectatorThink
	NULL,					// pfnSys_Error
	NULL,					// pfnPM_Move
	NULL,					// pfnPM_Init
	NULL,					// pfnPM_FindTextureType
	NULL,					// pfnSetupVisibility
	NULL,					// pfnUpdateClientData
	NULL,					// pfnAddToFullPack
	NULL,					// pfnCreateBaseline
	NULL,					// pfnRegisterEncoders
	NULL,					// pfnGetWeaponData
	NULL,					// pfnCmdStart
	NULL,					// pfnCmdEnd
	NULL,					// pfnConnectionlessPacket
	NULL,					// pfnGetHullBounds
	NULL,					// pfnCreateInstancedBaselines
	NULL,					// pfnInconsistentFile
	NULL,					// pfnAllowLagCompensation
};

// Steam answers the authid a moment after the client connects, and the engine
// gives us no callback for it. Polling is the only option; four times a second
// over 32 slots is nothing, and only slots still waiting are touched at all.
static float s_next_poll = 0.0f;

// A new map restarts gpGlobals->time. Without this the deadline left over from
// the previous map sits in the future for as long as that map ran, and nobody
// gets authorized - which means nobody gets any rights either.
void cslua_reset_auth_poll()
{
	s_next_poll = 0.0f;
}

static void poll_authorization()
{
	if (gpGlobals->time < s_next_poll)
		return;
	s_next_poll = gpGlobals->time + 0.25f;

	for (int id = 1; id < CSLUA_MAXPLAYERS; id++) {
		if (g_players.refresh_authid(id))
			g_events.fire_player_authorized(id, g_players.authid(id));
	}
}

// Team changes have no hookchain worth using: SwitchTeam is the between-halves
// side swap, and ChooseTeam fires before a join that may not happen. Polling
// m_iTeam catches every path instead - the team menu, an autobalance, p:team()
// from Lua, another mod - at the cost of 32 string compares a frame, which is
// nothing next to a hookchain on the damage path.
static const char *s_team[CSLUA_MAXPLAYERS];

void cslua_reset_team_cache()
{
	for (int id = 0; id < CSLUA_MAXPLAYERS; id++)
		s_team[id] = NULL;
}

void cslua_forget_team(int id)
{
	if (id >= 0 && id < CSLUA_MAXPLAYERS)
		s_team[id] = NULL;
}

static void poll_team_change()
{
	if (!g_events.any(CSLUA_EVENT_PLAYER_TEAM_CHANGE))
		return;

	for (int id = 1; id < CSLUA_MAXPLAYERS; id++) {
		if (!g_players.is_connected(id)) {
			s_team[id] = NULL;
			continue;
		}

		// Always one of a handful of literals, so the pointer is safe to keep;
		// the compare is by content anyway, which is one less thing to be
		// clever about.
		const char *now = cslua_player_team_name(id);
		const char *was = s_team[id];

		if (was && !strcmp(was, now))
			continue;

		s_team[id] = now;

		// The first reading for a slot is not a change: a joining player goes
		// from "we had not looked yet" to NONE, which nobody wants an event for.
		if (was)
			g_events.fire_player_team_change(id, was, now);
	}
}

static void StartFrame()
{
	// Before anything else touches Lua: a route handler or a hook callback
	// earlier this frame may have asked for a reload, and this is the first
	// point since then where the C stack is guaranteed clean - no lua_pcall
	// of any plugin's code is running. See LuaEngine::process_pending_reload.
	g_lua.process_pending_reload();

	poll_authorization();
	poll_team_change();
	cslua_timers_run();

	// Replies land on the game thread, next to the timers.
	cslua_http_run();
	cslua_httpserver_run();
	cslua_mysql_run();
	RETURN_META(MRES_IGNORED);
}

// Runs once per (recipient, potentially-visible entity) pair on every
// network update - the only point where one client's copy of an entity can
// be made to look different from everyone else's (see cslua_visibility.h).
// *Post*, not pre: the engine and the game DLL have already built `state`
// the normal way by the time this runs, so a script's override just
// overwrites specific fields in it rather than having to reconstruct
// everything itself.
static int AddToFullPack_Post(entity_state_s *state, int e, edict_t *ent, edict_t *host,
	int hostflags, int player, unsigned char *pSet)
{
	cslua_visibility_apply(state, e, ent, host);
	RETURN_META_VALUE(MRES_IGNORED, 1);
}

// Only a couple of hooks here, so build it instead of spelling out 50 NULLs.
static DLL_FUNCTIONS make_post_table()
{
	DLL_FUNCTIONS table = {};
	table.pfnSpawn = Spawn_Post;
	table.pfnClientPutInServer = ClientPutInServer;
	table.pfnStartFrame = StartFrame;
	table.pfnAddToFullPack = AddToFullPack_Post;
	return table;
}

DLL_FUNCTIONS g_DllFunctionTable_Post = make_post_table();
NEW_DLL_FUNCTIONS g_NewDllFunctionTable = {};
NEW_DLL_FUNCTIONS g_NewDllFunctionTable_Post = {};

C_DLLEXPORT int GetEntityAPI2(DLL_FUNCTIONS *pFunctionTable, int *interfaceVersion)
{
	if (!pFunctionTable) {
		ALERT(at_logged, "%s called with null pFunctionTable", __FUNCTION__);
		return FALSE;
	}
	if (*interfaceVersion != INTERFACE_VERSION) {
		ALERT(at_logged, "%s version mismatch; requested=%d ours=%d", __FUNCTION__, *interfaceVersion, INTERFACE_VERSION);
		*interfaceVersion = INTERFACE_VERSION;
		return FALSE;
	}

	memcpy(pFunctionTable, &g_DllFunctionTable, sizeof(DLL_FUNCTIONS));
	return TRUE;
}

C_DLLEXPORT int GetEntityAPI2_Post(DLL_FUNCTIONS *pFunctionTable, int *interfaceVersion)
{
	if (!pFunctionTable) {
		ALERT(at_logged, "%s called with null pFunctionTable", __FUNCTION__);
		return FALSE;
	}
	if (*interfaceVersion != INTERFACE_VERSION) {
		ALERT(at_logged, "%s version mismatch; requested=%d ours=%d", __FUNCTION__, *interfaceVersion, INTERFACE_VERSION);
		*interfaceVersion = INTERFACE_VERSION;
		return FALSE;
	}

	memcpy(pFunctionTable, &g_DllFunctionTable_Post, sizeof(DLL_FUNCTIONS));
	return TRUE;
}

C_DLLEXPORT int GetNewDLLFunctions(NEW_DLL_FUNCTIONS *pNewFunctionTable, int *interfaceVersion)
{
	if (!pNewFunctionTable) {
		ALERT(at_logged, "%s called with null pNewFunctionTable", __FUNCTION__);
		return FALSE;
	}
	if (*interfaceVersion != NEW_DLL_FUNCTIONS_VERSION) {
		ALERT(at_logged, "%s version mismatch; requested=%d ours=%d", __FUNCTION__, *interfaceVersion, NEW_DLL_FUNCTIONS_VERSION);
		*interfaceVersion = NEW_DLL_FUNCTIONS_VERSION;
		return FALSE;
	}

	memcpy(pNewFunctionTable, &g_NewDllFunctionTable, sizeof(NEW_DLL_FUNCTIONS));
	return TRUE;
}

C_DLLEXPORT int GetNewDLLFunctions_Post(NEW_DLL_FUNCTIONS *pNewFunctionTable, int *interfaceVersion)
{
	if (!pNewFunctionTable) {
		ALERT(at_logged, "%s called with null pNewFunctionTable", __FUNCTION__);
		return FALSE;
	}
	if (*interfaceVersion != NEW_DLL_FUNCTIONS_VERSION) {
		ALERT(at_logged, "%s version mismatch; requested=%d ours=%d", __FUNCTION__, *interfaceVersion, NEW_DLL_FUNCTIONS_VERSION);
		*interfaceVersion = NEW_DLL_FUNCTIONS_VERSION;
		return FALSE;
	}

	memcpy(pNewFunctionTable, &g_NewDllFunctionTable_Post, sizeof(NEW_DLL_FUNCTIONS));
	return TRUE;
}
