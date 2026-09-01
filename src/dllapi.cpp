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
#include "lua_player.h"
#include "players.h"
#include "regamedll.h"
#include "cslua_netwatch.h"
#include "cslua_visibility.h"

#include <entity_state.h>

#include <in_buttons.h>
#include <string.h>
#include <string>

void cslua_reset_auth_poll();
void cslua_reset_team_cache();
void cslua_forget_team(int id);

// Set from worldspawn until the map ends. See cslua_world_ready() in cslua.h.
static bool s_world_ready = false;

bool cslua_world_ready()
{
	return s_world_ready;
}

// Set by cslua_set_game_desc(). Empty means "no override".
static std::string s_game_desc;

void cslua_set_game_desc(const char *text)
{
	s_game_desc = text ? text : "";
}

static const char *GetGameDescription()
{
	if (s_game_desc.empty())
		RETURN_META_VALUE(MRES_IGNORED, NULL);

	RETURN_META_VALUE(MRES_SUPERCEDE, s_game_desc.c_str());
}

// worldspawn is the map's first entity and the window where the engine still
// accepts precache calls. Post, after the game DLL has precached its own.
static int Spawn_Post(edict_t *pent)
{
	if (pent && !strcmp(STRING(pent->v.classname), "worldspawn")) {
		s_world_ready = true;
		cslua_sound_set_window(true);
		cslua_precache_all();
	}

	RETURN_META_VALUE(MRES_IGNORED, 0);
}

// Runs once per map: the precache window shuts, and everything keyed on
// gpGlobals->time (which restarts from zero) is rebased.
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

// The map is ending. Last chance for a plugin to undo world changes and write
// data out. Not a shutdown - the Lua state lives on (that is plugin_unload).
static void ServerDeactivate()
{
	g_events.fire_map_change(gpGlobals && gpGlobals->mapname ? STRING(gpGlobals->mapname) : "");

	// After this the edicts go away. Cleared last so a handler can still remove
	// its own entities.
	s_world_ready = false;
	cslua_touch_detonate_clear();
	cslua_visibility_reset();
	RETURN_META(MRES_IGNORED);
}

static qboolean ClientConnect(edict_t *pEntity, const char *pszName, const char *pszAddress, char szRejectReason[128])
{
	int id = g_engfuncs.pfnIndexOfEdict(pEntity);
	g_players.on_connect(id, pszName, pszAddress, g_engfuncs.pfnGetPlayerAuthId(pEntity));

	// Slots get reused; the new occupant starts clean.
	cslua_menu_reset(id);
	cslua_forget_team(id);

	RejectInfo reject;
	reject.reason[0] = '\0';

	if (g_events.fire_client_connect(id, pszName, pszAddress, reject)) {
		strncpy(szRejectReason, reject.reason, 127);
		szRejectReason[127] = '\0';

		// The engine won't call ClientDisconnect for a refused client, so fire
		// client:disconnect here instead of leaving it silent.
		g_events.fire_client_disconnect(id, pszName, reject.reason, true);
		g_players.on_disconnect(id);
		RETURN_META_VALUE(MRES_SUPERCEDE, FALSE);
	}

	// Bots, LAN players and already-known clients are authorized right here;
	// everyone else gets the event from the poll in StartFrame.
	if (g_players.is_authorized(id))
		g_events.fire_player_authorized(id, g_players.authid(id));

	RETURN_META_VALUE(MRES_IGNORED, TRUE);
}

static void ClientPutInServer(edict_t *pEntity)
{
	// Only now does the client have the user message table.
	g_events.fire_player_ready(g_engfuncs.pfnIndexOfEdict(pEntity));
	RETURN_META(MRES_IGNORED);
}

static void ClientCommand(edict_t *pEntity)
{
	const char *cmd = CMD_ARGV(0);
	if (!cmd)
		RETURN_META(MRES_IGNORED);

	// A menu answer is consumed here so CS never treats the key as a reply to
	// its own team/buy menu.
	if (cslua_menu_handle_select(g_engfuncs.pfnIndexOfEdict(pEntity), cmd))
		RETURN_META(MRES_SUPERCEDE);

	int id = g_engfuncs.pfnIndexOfEdict(pEntity);

	// Superceded outright: the game DLL's "drop" handler never runs, so there
	// is no weapon entity in the world - see p:suppress_drop.
	if (!strcmp(cmd, "drop") && g_players.suppress_drop(id))
		RETURN_META(MRES_SUPERCEDE);

	bool team = !strcmp(cmd, "say_team");
	if (!team && strcmp(cmd, "say"))
		RETURN_META(MRES_IGNORED);

	const char *text = CMD_ARGC() > 1 ? CMD_ARGV(1) : "";
	if (!text || !*text)
		RETURN_META(MRES_IGNORED);

	if (g_events.fire_player_chat(id, text, team))
		RETURN_META(MRES_SUPERCEDE);

	RETURN_META(MRES_IGNORED);
}

// Every entity-to-entity touch. Cheap: cslua_touch_detonate_check's early-out
// is one size check against a list that is empty unless a script used
// e:detonate_on_touch().
static void Touch(edict_t *pentTouched, edict_t *pentOther)
{
	cslua_touch_detonate_check(pentTouched);
	RETURN_META(MRES_IGNORED);
}

// Runs before the engine reads this frame's button state, so clearing
// IN_ATTACK here means the shot never happens - no muzzle flash, no ammo spent.
static void PlayerPreThink(edict_t *pEntity)
{
	int id = g_engfuncs.pfnIndexOfEdict(pEntity);

	// Synthesized SecondaryAttack: no hookchain for it, so edge-detect here.
	// Read before suppress_attack strips the button below.
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
		g_events.fire_client_disconnect(id, g_players.name(id), g_players.drop_reason(id), false);

	g_players.on_disconnect(id);
	cslua_menu_reset(id);
	cslua_netwatch_forget(id);
	RETURN_META(MRES_IGNORED);
}

// The "kill" console command. Cancelling here skips ClientKill's chain
// entirely, so a blocked suicide leaves no trace - not even the cooldown timer.
static void ClientKill(edict_t *pEntity)
{
	int id = g_engfuncs.pfnIndexOfEdict(pEntity);
	if (g_events.fire_player_suicide(id))
		RETURN_META(MRES_SUPERCEDE);
	RETURN_META(MRES_IGNORED);
}

DLL_FUNCTIONS g_DllFunctionTable =
{
	NULL,					// pfnGameInit
	NULL,					// pfnSpawn
	NULL,					// pfnThink
	NULL,					// pfnUse (hooked post, see below)
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
	ClientKill,				// pfnClientKill
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
	GetGameDescription,		// pfnGetGameDescription
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

// Steam answers the authid a moment after connect and there is no callback.
// Four times a second over 32 slots; only slots still waiting are touched.
static float s_next_poll = 0.0f;

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

// Team changes have no useful hookchain. Polling m_iTeam catches every path -
// the team menu, autobalance, p:team(), another mod - at 32 string compares
// a frame.
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

		// Always one of a handful of literals; compare is by content.
		const char *now = cslua_player_team_name(id);
		const char *was = s_team[id];

		if (was && !strcmp(was, now))
			continue;

		s_team[id] = now;

		// The first reading for a slot is not a change.
		if (was)
			g_events.fire_player_team_change(id, was, now);
	}
}

static void StartFrame()
{
	// Before anything else touches Lua: a handler earlier this frame may have
	// asked for a reload, and this is the first point the C stack is clean.
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

// Once per (recipient, potentially-visible entity) pair on every network
// update - the only point one client's copy of an entity can be made to look
// different. Post: the engine has already built `state`.
static int AddToFullPack_Post(entity_state_s *state, int e, edict_t *ent, edict_t *host,
	int hostflags, int player, unsigned char *pSet)
{
	cslua_visibility_apply(state, e, ent, host);
	RETURN_META_VALUE(MRES_IGNORED, 1);
}

// pfnUse just ran. Post - only observe. entity is whatever was used, player is
// whoever pressed +use (0 if the activator was not a connected player).
static void Use_Post(edict_t *pentUsed, edict_t *pentOther)
{
	int entity = pentUsed ? g_engfuncs.pfnIndexOfEdict(pentUsed) : 0;

	int player = pentOther ? g_engfuncs.pfnIndexOfEdict(pentOther) : 0;
	if (player < 1 || player >= CSLUA_MAXPLAYERS || !g_players.is_connected(player))
		player = 0;

	g_events.fire_player_use(player, entity);
	RETURN_META(MRES_IGNORED);
}

static DLL_FUNCTIONS make_post_table()
{
	DLL_FUNCTIONS table = {};
	table.pfnSpawn = Spawn_Post;
	table.pfnClientPutInServer = ClientPutInServer;
	table.pfnStartFrame = StartFrame;
	table.pfnAddToFullPack = AddToFullPack_Post;
	table.pfnUse = Use_Post;
	return table;
}

DLL_FUNCTIONS g_DllFunctionTable_Post = make_post_table();

// The four hooks below are NEW_DLL_FUNCTIONS fields ReGameDLL leaves null, so
// this is the only implementation there is - registered pre, not post.

// pfnShouldCollide: defaults to normal collision; only an explicit
// e.collide = false turns it off for one pair.
static int ShouldCollide(edict_t *pentTouched, edict_t *pentOther)
{
	int a = pentTouched ? g_engfuncs.pfnIndexOfEdict(pentTouched) : 0;
	int b = pentOther ? g_engfuncs.pfnIndexOfEdict(pentOther) : 0;

	bool collide = g_events.fire_ents_should_collide(a, b, true);
	RETURN_META_VALUE(MRES_SUPERCEDE, collide ? 1 : 0);
}

// Fires before ReGameDLL's own cleanup - the last point the entity can be read.
static void OnFreeEntPrivateData(edict_t *pEnt)
{
	int index = pEnt ? g_engfuncs.pfnIndexOfEdict(pEnt) : 0;
	g_events.fire_ents_free(index);
	RETURN_META(MRES_IGNORED);
}

// A client's answer to a QueryClientCvarValue request - the context-free
// version (no request id, no cvar name).
static void CvarValue(const edict_t *pEnt, const char *value)
{
	int id = pEnt ? g_engfuncs.pfnIndexOfEdict((edict_t *)pEnt) : 0;
	g_events.fire_player_cvar_value(id, value);
	RETURN_META(MRES_IGNORED);
}

// The QueryClientCvarValue2 version: request_id + cvar name.
static void CvarValue2(const edict_t *pEnt, int requestID, const char *cvarName, const char *value)
{
	int id = pEnt ? g_engfuncs.pfnIndexOfEdict((edict_t *)pEnt) : 0;
	g_events.fire_player_cvar_value2(id, requestID, cvarName, value);
	RETURN_META(MRES_IGNORED);
}

static NEW_DLL_FUNCTIONS make_new_table()
{
	NEW_DLL_FUNCTIONS table = {};
	table.pfnShouldCollide = ShouldCollide;
	table.pfnOnFreeEntPrivateData = OnFreeEntPrivateData;
	table.pfnCvarValue = CvarValue;
	table.pfnCvarValue2 = CvarValue2;
	return table;
}

NEW_DLL_FUNCTIONS g_NewDllFunctionTable = make_new_table();
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
