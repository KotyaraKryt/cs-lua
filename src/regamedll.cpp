#include "cslua.h"
#include <regamedll_api.h>

#include "regamedll.h"
#include "lua_events.h"
#include "players.h"
#include "platform.h"

static IReGameApi *s_api = nullptr;
static IReGameHookchains *s_hooks = nullptr;
static char s_version[64] = "unavailable";

bool cslua_regamedll_ready()
{
	return s_api != nullptr;
}

const char *cslua_regamedll_version()
{
	return s_version;
}

IReGameHookchains *cslua_regamedll_hooks()
{
	return s_hooks;
}

bool cslua_regamedll_init()
{
	if (s_api)
		return true;

	// metamod knows exactly which file it loaded as the game DLL, so ask it
	// instead of guessing "mp.dll" or "cs.so".
	const char *path = GET_GAME_INFO(PLID, GINFO_REALDLL_FULLPATH);
	void *gamedll = path ? cslua_module_open(path) : NULL;
	if (!gamedll) {
		// Naming the path makes this one debuggable instead of mysterious.
		cslua_print("game DLL handle not found (%s), CS-specific features are off",
			path ? path : "no path from metamod");
		return false;
	}

	CreateInterfaceFn factory =
		(CreateInterfaceFn)cslua_module_symbol(gamedll, "CreateInterface");
	cslua_module_close(gamedll);

	if (!factory) {
		cslua_print("game DLL exports no CreateInterface: not ReGameDLL, CS-specific features are off");
		return false;
	}

	int result = 0;
	IReGameApi *api = (IReGameApi *)factory(VRE_GAMEDLL_API_VERSION, &result);
	if (!api) {
		cslua_print("ReGameDLL API not found, CS-specific features are off");
		return false;
	}

	int major = api->GetMajorVersion();
	int minor = api->GetMinorVersion();

	// A major bump means the interface changed shape; refusing beats crashing.
	if (major != REGAMEDLL_API_VERSION_MAJOR) {
		cslua_error("ReGameDLL API major version mismatch: server has %d, we expect %d",
			major, REGAMEDLL_API_VERSION_MAJOR);
		return false;
	}

	if (minor < REGAMEDLL_API_VERSION_MINOR) {
		cslua_error("ReGameDLL API minor version too old: server has %d.%d, we need at least %d.%d",
			major, minor, REGAMEDLL_API_VERSION_MAJOR, REGAMEDLL_API_VERSION_MINOR);
		return false;
	}

	s_api = api;
	s_hooks = api->GetHookchains();
	cslua_snprintf(s_version, sizeof s_version, "%d.%d", major, minor);
	s_version[sizeof s_version - 1] = '\0';

	cslua_print("ReGameDLL API %s connected", s_version);
	return true;
}

CBasePlayer *cslua_player_entity(int id)
{
	if (!s_api || !g_players.is_connected(id))
		return nullptr;

	edict_t *e = g_engfuncs.pfnPEntityOfEntIndex(id);
	if (!e || e->free || !e->pvPrivateData)
		return nullptr;

	return static_cast<CBasePlayer *>(CBaseEntity::Instance(e));
}

bool cslua_player_alive(int id)
{
	CBasePlayer *p = cslua_player_entity(id);
	return p && p->IsAlive();
}

const char *cslua_player_team_name(int id)
{
	CBasePlayer *p = cslua_player_entity(id);
	if (!p)
		return "";

	switch (p->m_iTeam) {
	case TERRORIST: return "T";
	case CT:        return "CT";
	case SPECTATOR: return "SPEC";
	default:        return "NONE";
	}
}

// The hookchains below are pre-hooks: they call the chain (so the game still
// does its thing), then hand the event to Lua. We never supercede here; cs-lua
// only observes for now.

static void hook_spawn(IReGameHook_CBasePlayer_Spawn *chain, CBasePlayer *player)
{
	chain->callNext(player);

	if (player && player->IsPlayer())
		g_events.fire_player_spawn(player->entindex());
}

// Resolves an attacker's entvars to a player slot, or 0 for world / non-player.
static int attacker_slot(entvars_t *pevAttacker)
{
	if (!pevAttacker)
		return 0;

	edict_t *e = ENT(pevAttacker);
	if (!e)
		return 0;

	int idx = ENTINDEX(e);
	return (idx >= 1 && idx < CSLUA_MAXPLAYERS) ? idx : 0;
}

static BOOL hook_takedamage(IReGameHook_CBasePlayer_TakeDamage *chain, CBasePlayer *victim,
	entvars_t *pevInflictor, entvars_t *pevAttacker, float &flDamage, int bitsDamageType)
{
	if (!victim || !victim->IsPlayer())
		return chain->callNext(victim, pevInflictor, pevAttacker, flDamage, bitsDamageType);

	int vslot = victim->entindex();
	int aslot = attacker_slot(pevAttacker);

	// Pre: let Lua change or cancel the damage before the game applies it.
	flDamage = g_events.fire_player_hurt(vslot, aslot, flDamage, bitsDamageType);
	if (flDamage <= 0.0f)
		return FALSE;			// fully blocked: no pain sound, no armor loss

	BOOL result = chain->callNext(victim, pevInflictor, pevAttacker, flDamage, bitsDamageType);

	// Post: damage is applied, health/armor are current.
	g_events.fire_player_hurt_post(vslot, aslot, flDamage, bitsDamageType);
	return result;
}

static void hook_killed(IReGameHook_CBasePlayer_Killed *chain, CBasePlayer *victim,
	entvars_t *pevAttacker, int iGib)
{
	chain->callNext(victim, pevAttacker, iGib);

	if (!victim || !victim->IsPlayer())
		return;

	// killer 0 = world / non-player; suicide shows up as attacker == victim.
	int killer = attacker_slot(pevAttacker);
	bool headshot = victim->m_bHeadshotKilled;
	g_events.fire_player_death(victim->entindex(), killer, headshot ? 1 : 0);
}

static void hook_round_start(IReGameHook_CSGameRules_RestartRound *chain)
{
	chain->callNext();
	g_events.fire_round_start();
}

static bool hook_round_end(IReGameHook_RoundEnd *chain, int winStatus,
	ScenarioEventEndRound event, float delay)
{
	bool result = chain->callNext(winStatus, event, delay);

	// winStatus: 1 = CT, 2 = T, 3 = draw (see WinStatus in gamerules.h).
	g_events.fire_round_end(winStatus);
	return result;
}

static void hook_freeze_end(IReGameHook_CSGameRules_OnRoundFreezeEnd *chain)
{
	chain->callNext();
	g_events.fire_round_freeze_end();
}

static CGrenade *hook_plant_bomb(IReGameHook_PlantBomb *chain, entvars_t *pevOwner,
	Vector &vecStart, Vector &vecVelocity)
{
	CGrenade *bomb = chain->callNext(pevOwner, vecStart, vecVelocity);
	g_events.fire_bomb_planted(attacker_slot(pevOwner));
	return bomb;
}

// Fires whether the defuse finished or was interrupted; `success` tells them
// apart, which is the whole point of hooking the end rather than the start.
static void hook_defuse_end(IReGameHook_CGrenade_DefuseBombEnd *chain, CGrenade *bomb,
	CBasePlayer *defuser, bool success)
{
	chain->callNext(bomb, defuser, success);

	int slot = (defuser && defuser->IsPlayer()) ? defuser->entindex() : 0;
	g_events.fire_bomb_defused(slot, success);
}

static void hook_explode_bomb(IReGameHook_CGrenade_ExplodeBomb *chain, CGrenade *bomb,
	TraceResult *ptr, int bitsDamageType)
{
	// Read the position before the chain runs - the entity is on its way out.
	Vector where = bomb && bomb->pev ? bomb->pev->origin : Vector(0, 0, 0);

	chain->callNext(bomb, ptr, bitsDamageType);

	g_events.fire_bomb_exploded(where.x, where.y, where.z);
}

void cslua_regamedll_install_hooks()
{
	if (!s_hooks)
		return;

	// Only hook what someone is listening for: a hookchain has a per-frame
	// cost even when the callback does nothing.
	if (g_events.any(CSLUA_EVENT_PLAYER_SPAWN))
		s_hooks->CBasePlayer_Spawn()->registerHook(&hook_spawn);

	// One TakeDamage hook feeds both the pre and post events.
	if (g_events.any(CSLUA_EVENT_PLAYER_HURT) || g_events.any(CSLUA_EVENT_PLAYER_HURT_POST))
		s_hooks->CBasePlayer_TakeDamage()->registerHook(&hook_takedamage);

	if (g_events.any(CSLUA_EVENT_PLAYER_DEATH))
		s_hooks->CBasePlayer_Killed()->registerHook(&hook_killed);

	if (g_events.any(CSLUA_EVENT_ROUND_START))
		s_hooks->CSGameRules_RestartRound()->registerHook(&hook_round_start);

	if (g_events.any(CSLUA_EVENT_ROUND_END))
		s_hooks->RoundEnd()->registerHook(&hook_round_end);

	if (g_events.any(CSLUA_EVENT_ROUND_FREEZE_END))
		s_hooks->CSGameRules_OnRoundFreezeEnd()->registerHook(&hook_freeze_end);

	if (g_events.any(CSLUA_EVENT_BOMB_PLANTED))
		s_hooks->PlantBomb()->registerHook(&hook_plant_bomb);

	if (g_events.any(CSLUA_EVENT_BOMB_DEFUSED))
		s_hooks->CGrenade_DefuseBombEnd()->registerHook(&hook_defuse_end);

	if (g_events.any(CSLUA_EVENT_BOMB_EXPLODED))
		s_hooks->CGrenade_ExplodeBomb()->registerHook(&hook_explode_bomb);
}

void cslua_regamedll_remove_hooks()
{
	if (!s_hooks)
		return;

	// unregisterHook is a no-op for a hook that was never registered, so it is
	// safe to blanket-remove on reload.
	s_hooks->CBasePlayer_Spawn()->unregisterHook(&hook_spawn);
	s_hooks->CBasePlayer_TakeDamage()->unregisterHook(&hook_takedamage);
	s_hooks->CBasePlayer_Killed()->unregisterHook(&hook_killed);
	s_hooks->CSGameRules_RestartRound()->unregisterHook(&hook_round_start);
	s_hooks->RoundEnd()->unregisterHook(&hook_round_end);
	s_hooks->CSGameRules_OnRoundFreezeEnd()->unregisterHook(&hook_freeze_end);
	s_hooks->PlantBomb()->unregisterHook(&hook_plant_bomb);
	s_hooks->CGrenade_DefuseBombEnd()->unregisterHook(&hook_defuse_end);
	s_hooks->CGrenade_ExplodeBomb()->unregisterHook(&hook_explode_bomb);
}
