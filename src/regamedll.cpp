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

	// TraceAttack records the body region on the victim just before calling
	// TakeDamage, so by now it describes this hit. Damage that never went
	// through a trace - a fall, the world, an explosion - leaves whatever the
	// last real hit put there, which would be a lie, so report nothing for it.
	//
	// The shotgun is DMG_BULLET too; the knife arrives as SLASH or CLUB
	// depending on which attack it was.
	int hitgroup = (bitsDamageType & (DMG_BULLET | DMG_SLASH | DMG_CLUB))
		? victim->m_LastHitGroup : -1;

	// Pre: let Lua change or cancel the damage before the game applies it.
	flDamage = g_events.fire_player_hurt(vslot, aslot, flDamage, bitsDamageType, hitgroup);
	if (flDamage <= 0.0f)
		return FALSE;			// fully blocked: no pain sound, no armor loss

	BOOL result = chain->callNext(victim, pevInflictor, pevAttacker, flDamage, bitsDamageType);

	// Post: damage is applied, health/armor are current.
	g_events.fire_player_hurt_post(vslot, aslot, flDamage, bitsDamageType, hitgroup);
	return result;
}

static void hook_killed(IReGameHook_CBasePlayer_Killed *chain, CBasePlayer *victim,
	entvars_t *pevAttacker, int iGib)
{
	if (!victim || !victim->IsPlayer()) {
		chain->callNext(victim, pevAttacker, iGib);
		return;
	}

	// killer 0 = world / non-player; suicide shows up as attacker == victim.
	int killer = attacker_slot(pevAttacker);

	// Both of these have to be read before the chain runs: the death drops the
	// victim's weapons and the impulse throws the body, so afterwards the
	// weapon is gone and the distance is whatever the corpse flew.
	const char *weapon = NULL;
	float distance = -1.0f;

	if (killer > 0) {
		CBasePlayer *shooter = cslua_player_entity(killer);
		if (shooter) {
			if (shooter->m_pActiveItem && shooter->m_pActiveItem->pev)
				weapon = STRING(shooter->m_pActiveItem->pev->classname);
			distance = (shooter->pev->origin - victim->pev->origin).Length();
		}
	}

	// Copied out: the string points into the game's entity, which the chain
	// below is free to take apart.
	char weapon_name[64];
	if (weapon) {
		cslua_snprintf(weapon_name, sizeof weapon_name, "%s", weapon);
		weapon_name[sizeof weapon_name - 1] = '\0';
		weapon = weapon_name;
	}

	chain->callNext(victim, pevAttacker, iGib);

	bool headshot = victim->m_bHeadshotKilled;
	g_events.fire_player_death(victim->entindex(), killer, headshot ? 1 : 0, weapon, distance);
}

// A shot left the barrel. There is no one hookchain for "fired": the game
// spreads it across three bullet calls, and this covers all of them. `this` is
// whoever pulled the trigger.
//
// Firearms only. The knife goes through TraceAttack and grenades through the
// Throw* chains, neither of which is a bullet - see docs/events.md.
static int firing_slot(CBaseEntity *shooter)
{
	if (!shooter || !shooter->pev)
		return 0;

	edict_t *e = ENT(shooter->pev);
	if (!e)
		return 0;

	int idx = ENTINDEX(e);
	return (idx >= 1 && idx < CSLUA_MAXPLAYERS) ? idx : 0;
}

// The clip is read after the shot, so a plugin counting rounds sees what is
// left rather than what was there.
static void fire_weapon_event(CBaseEntity *shooter)
{
	int slot = firing_slot(shooter);
	if (!slot)
		return;

	CBasePlayer *player = cslua_player_entity(slot);
	if (!player)
		return;

	CBasePlayerItem *active = player->m_pActiveItem;
	if (!active || !active->pev)
		return;

	int clip = -1;
	if (active->IsWeapon())
		clip = static_cast<CBasePlayerWeapon *>(active)->m_iClip;

	g_events.fire_weapon_fire(slot, STRING(active->pev->classname), clip);
}

static void hook_fire_bullets(IReGameHook_CBaseEntity_FireBullets *chain, CBaseEntity *shooter,
	ULONG cShots, Vector &vecSrc, Vector &vecDirShooting, Vector &vecSpread,
	float flDistance, int iBulletType, int iTracerFreq, int iDamage, entvars_t *pevAttacker)
{
	chain->callNext(shooter, cShots, vecSrc, vecDirShooting, vecSpread, flDistance,
		iBulletType, iTracerFreq, iDamage, pevAttacker);

	fire_weapon_event(shooter);
}

static Vector &hook_fire_bullets3(IReGameHook_CBaseEntity_FireBullets3 *chain, CBaseEntity *shooter,
	Vector &vecSrc, Vector &vecDirShooting, float flSpread, float flDistance, int iPenetration,
	int iBulletType, int iDamage, float flRangeModifier, entvars_t *pevAttacker,
	bool bPistol, int shared_rand)
{
	Vector &result = chain->callNext(shooter, vecSrc, vecDirShooting, flSpread, flDistance,
		iPenetration, iBulletType, iDamage, flRangeModifier, pevAttacker, bPistol, shared_rand);

	fire_weapon_event(shooter);
	return result;
}

static void hook_fire_buckshots(IReGameHook_CBaseEntity_FireBuckshots *chain, CBaseEntity *shooter,
	ULONG cShots, Vector &vecSrc, Vector &vecDirShooting, Vector &vecSpread,
	float flDistance, int iTracerFreq, int iDamage, entvars_t *pevAttacker)
{
	chain->callNext(shooter, cShots, vecSrc, vecDirShooting, vecSpread, flDistance,
		iTracerFreq, iDamage, pevAttacker);

	fire_weapon_event(shooter);
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

// Which chains we currently hold a hook on.
//
// registerHook is fatal when the same handler is added twice - ReGameDLL kills
// the server outright - and `lua_reload <plugin>` calls this again with the
// rest of the plugins still loaded. unregisterHook is the forgiving one, so
// only the install side needs remembering.
enum HookSlot
{
	HOOK_SPAWN,
	HOOK_TAKEDAMAGE,
	HOOK_KILLED,
	HOOK_FIRE_BULLETS,
	HOOK_FIRE_BULLETS3,
	HOOK_FIRE_BUCKSHOTS,
	HOOK_ROUND_START,
	HOOK_ROUND_END,
	HOOK_FREEZE_END,
	HOOK_PLANT,
	HOOK_DEFUSE,
	HOOK_EXPLODE,
	HOOK_COUNT
};

static bool s_installed[HOOK_COUNT];

// Brings one chain in line with whether anything listens for it. Templated
// because every chain is its own type; the body is the same for all of them.
template <typename Chain, typename Handler>
static void sync_hook(HookSlot slot, Chain *chain, Handler handler, bool want)
{
	if (want == s_installed[slot])
		return;

	if (want)
		chain->registerHook(handler);
	else
		chain->unregisterHook(handler);

	s_installed[slot] = want;
}

// Called after every load and after every single-plugin reload. Only hooks
// what someone is listening for - a hookchain has a per-frame cost even when
// the callback does nothing - and drops one whose last listener just went
// away with the plugin that owned it.
void cslua_regamedll_install_hooks()
{
	if (!s_hooks)
		return;

	sync_hook(HOOK_SPAWN, s_hooks->CBasePlayer_Spawn(), &hook_spawn,
		g_events.any(CSLUA_EVENT_PLAYER_SPAWN));

	// One TakeDamage hook feeds both the pre and post events.
	sync_hook(HOOK_TAKEDAMAGE, s_hooks->CBasePlayer_TakeDamage(), &hook_takedamage,
		g_events.any(CSLUA_EVENT_PLAYER_HURT) || g_events.any(CSLUA_EVENT_PLAYER_HURT_POST));

	sync_hook(HOOK_KILLED, s_hooks->CBasePlayer_Killed(), &hook_killed,
		g_events.any(CSLUA_EVENT_PLAYER_DEATH));

	// Three chains for one event: the game splits shooting between them.
	bool shooting = g_events.any(CSLUA_EVENT_WEAPON_FIRE);
	sync_hook(HOOK_FIRE_BULLETS, s_hooks->CBaseEntity_FireBullets(), &hook_fire_bullets, shooting);
	sync_hook(HOOK_FIRE_BULLETS3, s_hooks->CBaseEntity_FireBullets3(), &hook_fire_bullets3, shooting);
	sync_hook(HOOK_FIRE_BUCKSHOTS, s_hooks->CBaseEntity_FireBuckshots(), &hook_fire_buckshots, shooting);

	sync_hook(HOOK_ROUND_START, s_hooks->CSGameRules_RestartRound(), &hook_round_start,
		g_events.any(CSLUA_EVENT_ROUND_START));

	sync_hook(HOOK_ROUND_END, s_hooks->RoundEnd(), &hook_round_end,
		g_events.any(CSLUA_EVENT_ROUND_END));

	sync_hook(HOOK_FREEZE_END, s_hooks->CSGameRules_OnRoundFreezeEnd(), &hook_freeze_end,
		g_events.any(CSLUA_EVENT_ROUND_FREEZE_END));

	sync_hook(HOOK_PLANT, s_hooks->PlantBomb(), &hook_plant_bomb,
		g_events.any(CSLUA_EVENT_BOMB_PLANTED));

	sync_hook(HOOK_DEFUSE, s_hooks->CGrenade_DefuseBombEnd(), &hook_defuse_end,
		g_events.any(CSLUA_EVENT_BOMB_DEFUSED));

	sync_hook(HOOK_EXPLODE, s_hooks->CGrenade_ExplodeBomb(), &hook_explode_bomb,
		g_events.any(CSLUA_EVENT_BOMB_EXPLODED));
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
	s_hooks->CBaseEntity_FireBullets()->unregisterHook(&hook_fire_bullets);
	s_hooks->CBaseEntity_FireBullets3()->unregisterHook(&hook_fire_bullets3);
	s_hooks->CBaseEntity_FireBuckshots()->unregisterHook(&hook_fire_buckshots);
	s_hooks->CSGameRules_RestartRound()->unregisterHook(&hook_round_start);
	s_hooks->RoundEnd()->unregisterHook(&hook_round_end);
	s_hooks->CSGameRules_OnRoundFreezeEnd()->unregisterHook(&hook_freeze_end);
	s_hooks->PlantBomb()->unregisterHook(&hook_plant_bomb);
	s_hooks->CGrenade_DefuseBombEnd()->unregisterHook(&hook_defuse_end);
	s_hooks->CGrenade_ExplodeBomb()->unregisterHook(&hook_explode_bomb);

	for (int i = 0; i < HOOK_COUNT; i++)
		s_installed[i] = false;
}
