#include "cslua.h"
#include <regamedll_api.h>

#include "regamedll.h"
#include "lua_events.h"
#include "players.h"
#include "platform.h"

#include <string>

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

IReGameApi *cslua_regamedll_api()
{
	return s_api;
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

const char *cslua_player_active_weapon(int id)
{
	CBasePlayer *p = cslua_player_entity(id);
	if (!p || !p->m_pActiveItem || !p->m_pActiveItem->pev)
		return "";

	return STRING(p->m_pActiveItem->pev->classname);
}

int cslua_player_active_weapon_ammo_type(int id)
{
	CBasePlayer *p = cslua_player_entity(id);
	if (!p || !p->m_pActiveItem || !p->m_pActiveItem->IsWeapon())
		return -1;

	return static_cast<CBasePlayerWeapon *>(p->m_pActiveItem)->m_iPrimaryAmmoType;
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

// TraceAttack fires once per hit - once per shotgun pellet, say - and only
// accumulates into the multi-damage buffer TakeDamage later reads; it never
// applies anything itself. flDamage is the raw per-hit amount here, before
// armor and any multiplier TakeDamage would apply. Neither this parameter nor
// the SDK's own TraceAttack(entvars_t *, float, Vector, TraceResult *, int)
// take it by reference, so a changed value has to be forwarded into callNext
// explicitly rather than mutated in place, same idea as hook_takedamage below.
static void hook_trace_attack(IReGameHook_CBasePlayer_TraceAttack *chain, CBasePlayer *victim,
	entvars_t *pevAttacker, float flDamage, Vector &vecDir, TraceResult *ptr, int bitsDamageType)
{
	if (!victim || !victim->IsPlayer()) {
		chain->callNext(victim, pevAttacker, flDamage, vecDir, ptr, bitsDamageType);
		return;
	}

	int vslot = victim->entindex();
	int aslot = attacker_slot(pevAttacker);
	int hitgroup = ptr ? ptr->iHitgroup : -1;
	float x = ptr ? ptr->vecEndPos.x : 0.0f;
	float y = ptr ? ptr->vecEndPos.y : 0.0f;
	float z = ptr ? ptr->vecEndPos.z : 0.0f;

	flDamage = g_events.fire_player_trace_attack(vslot, aslot, flDamage, bitsDamageType, hitgroup, x, y, z);
	if (flDamage <= 0.0f)
		return;				// fully blocked: this hit contributes nothing - no blood, no multidamage

	chain->callNext(victim, pevAttacker, flDamage, vecDir, ptr, bitsDamageType);
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

// TakeHealth is what a first aid kit or an admin heal command goes through -
// CS has no natural regen, so this is the only source. flHealth is by value
// in both the SDK's own TakeHealth(float, int) and this chain, same reason as
// flDamage in hook_trace_attack above: forward the changed value explicitly.
static BOOL hook_take_health(IReGameHook_CBasePlayer_TakeHealth *chain, CBasePlayer *player,
	float flHealth, int bitsDamageType)
{
	if (!player || !player->IsPlayer())
		return chain->callNext(player, flHealth, bitsDamageType);

	flHealth = g_events.fire_player_heal(player->entindex(), flHealth, bitsDamageType);
	if (flHealth <= 0.0f)
		return FALSE;			// fully blocked: no health given

	return chain->callNext(player, flHealth, bitsDamageType);
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

// szViewModel/szWeaponModel are string literals the caller passed in, not
// buffers to write through - the standard HL SDK deploy call is always
// DefaultDeploy("models/v_x.mdl", "models/p_x.mdl", ...). Lua overrides by
// value, so the fix is to hand the chain a different pointer, not to mutate
// this one.
static BOOL hook_deploy(IReGameHook_CBasePlayerWeapon_DefaultDeploy *chain, CBasePlayerWeapon *weapon,
	char *szViewModel, char *szWeaponModel, int iAnim, char *szAnimExt, int skiplocal)
{
	if (!weapon || !weapon->pev || !weapon->m_pPlayer)
		return chain->callNext(weapon, szViewModel, szWeaponModel, iAnim, szAnimExt, skiplocal);

	std::string view_model = szViewModel ? szViewModel : "";
	std::string world_model = szWeaponModel ? szWeaponModel : "";

	g_events.fire_weapon_deploy(weapon->m_pPlayer->entindex(), STRING(weapon->pev->classname),
		view_model, world_model);

	return chain->callNext(weapon, (char *)view_model.c_str(), (char *)world_model.c_str(),
		iAnim, szAnimExt, skiplocal);
}

// DefaultReload/DefaultShotgunReload are the two helpers real weapon Reload()
// implementations call - but calling them is not the same as reloading.
// Both no-op and return 0/false when there is nothing to do (clip already
// full, no reserve ammo): that check lives inside them, not before the
// call, so a held/macroed reload key still reaches this hook every time.
// The only reliable signal is the return value - true means the engine is
// about to actually run the reload animation and refill the clip. Everything
// but the shotguns (m3/xm1014, which reload one shell at a time through the
// special path) goes through DefaultReload; both need a hook or shotgun
// reloads go unseen.
static int hook_reload(IReGameHook_CBasePlayerWeapon_DefaultReload *chain, CBasePlayerWeapon *weapon,
	int iClipSize, int iAnim, float fDelay)
{
	int clip_before = (weapon && weapon->pev) ? weapon->m_iClip : 0;

	int result = chain->callNext(weapon, iClipSize, iAnim, fDelay);

	if (result && weapon && weapon->pev && weapon->m_pPlayer)
		g_events.fire_weapon_reload(weapon->m_pPlayer->entindex(), STRING(weapon->pev->classname), clip_before, fDelay, iClipSize);

	return result;
}

// Shotguns load one shell at a time, so DefaultShotgunReload runs once per
// shell and carries two delays: fStartDelay times the animation that opens
// the reload (the first call, m_fInSpecialReload still false going in),
// fDelay times every shell-insert loop after that. Reading the flag before
// callNext picks the one that actually applies to this call.
static bool hook_shotgun_reload(IReGameHook_CBasePlayerWeapon_DefaultShotgunReload *chain, CBasePlayerWeapon *weapon,
	int iAnim, int iStartAnim, float fDelay, float fStartDelay, const char *pszReloadSound1, const char *pszReloadSound2)
{
	int clip_before = (weapon && weapon->pev) ? weapon->m_iClip : 0;
	float delay = (weapon && weapon->m_fInSpecialReload) ? fDelay : fStartDelay;

	// DefaultShotgunReload never gets iClipSize handed in - it loads one
	// shell at a time, not to a fixed target in one call - so the only way
	// to know the magazine capacity here is to ask the weapon directly.
	ItemInfo info;
	memset(&info, 0, sizeof info);
	int max_clip = (weapon && weapon->GetItemInfo(&info)) ? info.iMaxClip : -1;

	bool result = chain->callNext(weapon, iAnim, iStartAnim, fDelay, fStartDelay, pszReloadSound1, pszReloadSound2);

	if (result && weapon && weapon->pev && weapon->m_pPlayer)
		g_events.fire_weapon_reload(weapon->m_pPlayer->entindex(), STRING(weapon->pev->classname), clip_before, delay, max_clip);

	return result;
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

static void hook_balance_teams(IReGameHook_CSGameRules_BalanceTeams *chain)
{
	chain->callNext();
	g_events.fire_round_balance_teams();
}

static void hook_go_to_intermission(IReGameHook_CSGameRules_GoToIntermission *chain)
{
	chain->callNext();
	g_events.fire_round_intermission();
}

// fuse is read from grenade_throw before the engine creates the projectile -
// a script can shorten it there, which is the only way to make a grenade
// pop on (near enough) contact rather than ride out a multi-second timer,
// since neither HE nor smoke exposes a touch hook. grenade_thrown fires
// after, once the entity is real and spawned, so a script can reskin it with
// the same ents API it would use on anything from ents.create.
static CGrenade *hook_throw_he_grenade(IReGameHook_ThrowHeGrenade *chain, entvars_t *pevOwner,
	Vector &vecStart, Vector &vecVelocity, float time, int iTeam, unsigned short usEvent)
{
	int owner = attacker_slot(pevOwner);

	float fuse = time;
	g_events.fire_grenade_throw(owner, "weapon_hegrenade", fuse);

	CGrenade *grenade = chain->callNext(pevOwner, vecStart, vecVelocity, fuse, iTeam, usEvent);

	int index = (grenade && grenade->pev) ? ENTINDEX(ENT(grenade->pev)) : 0;
	g_events.fire_grenade_thrown(owner, "weapon_hegrenade", index);

	return grenade;
}

// The one dispatcher every grenade-slot throw goes through before the
// engine picks a specific type and creates its CGrenade - unlike
// ThrowHeGrenade/ThrowSmokeGrenade above, this fires for ANY weapon in that
// slot, including one a plugin has repurposed for something that is not a
// grenade at all (see weapon_throw's docs). A handler that cancels is
// expected to have already built its own projectile inside its own handler
// (typically ents.create + e:movetype(8)); returning nullptr here skips the
// engine's own CGrenade entirely, so nothing throws twice.
static CGrenade *hook_throw_grenade(IReGameHook_CBasePlayer_ThrowGrenade *chain,
	CBasePlayer *player, CBasePlayerWeapon *item, Vector &vecSrc, Vector &vecThrow,
	float time, unsigned short usEvent)
{
	int owner = (player && player->IsPlayer()) ? player->entindex() : 0;
	const char *weapon = (item && item->pev) ? STRING(item->pev->classname) : "";
	int ammo_type = item ? item->m_iPrimaryAmmoType : -1;

	bool cancelled = g_events.fire_weapon_throw(owner, weapon, ammo_type,
		vecSrc.x, vecSrc.y, vecSrc.z, vecThrow.x, vecThrow.y, vecThrow.z, time);

	if (cancelled)
		return nullptr;

	return chain->callNext(player, item, vecSrc, vecThrow, time, usEvent);
}

// The buy menu goes through here for every weapon purchase - rebuy, autobuy,
// a manual pick, all the same chain. The entity does not exist yet at this
// point, so the classname comes from GetItemInfo(id) rather than reading it
// off the result the way grenade_thrown does.
static CBaseEntity *hook_buy_weapon(IReGameHook_BuyWeaponByWeaponID *chain, CBasePlayer *player, WeaponIdType id)
{
	int slot = (player && player->IsPlayer()) ? player->entindex() : 0;

	ItemInfo *info = s_api ? s_api->GetItemInfo(id) : nullptr;
	const char *weapon = (info && info->pszName) ? info->pszName : "";

	bool cancelled = g_events.fire_weapon_buy(slot, weapon);
	if (cancelled)
		return nullptr;			// no charge, no weapon - same as buying nothing

	return chain->callNext(player, id);
}

// Ammo for whatever is currently in the player's hands, not a specific
// weapon slot - weapon is that held item's own classname.
static bool hook_buy_gun_ammo(IReGameHook_BuyGunAmmo *chain, CBasePlayer *player,
	CBasePlayerItem *weapon, bool bBlink)
{
	int slot = (player && player->IsPlayer()) ? player->entindex() : 0;
	const char *wname = (weapon && weapon->pev) ? STRING(weapon->pev->classname) : "";

	bool cancelled = g_events.fire_ammo_buy(slot, wname);
	if (cancelled)
		return false;

	return chain->callNext(player, weapon, bBlink);
}

// Everything the buy menu sells that isn't a weapon: armor, NVGs, defuse
// kit, shield, and (through the same menu slot) HE/flash/smoke. item is the
// BuyItemMenuSlot the client sent - named here rather than left as a raw
// number, same reason weapon/grenade events give a classname instead of a
// WeaponIdType.
static const char *buy_item_name(int item)
{
	switch (item) {
	case MENU_SLOT_ITEM_VEST:      return "vest";
	case MENU_SLOT_ITEM_VESTHELM:  return "vesthelm";
	case MENU_SLOT_ITEM_FLASHGREN: return "flashbang";
	case MENU_SLOT_ITEM_HEGREN:    return "hegrenade";
	case MENU_SLOT_ITEM_SMOKEGREN: return "smokegrenade";
	case MENU_SLOT_ITEM_NVG:       return "nvg";
	case MENU_SLOT_ITEM_DEFUSEKIT: return "defusekit";
	case MENU_SLOT_ITEM_SHIELD:    return "shield";
	default:                       return "";
	}
}

static void hook_buy_item(IReGameHook_BuyItem *chain, CBasePlayer *player, int item)
{
	int slot = (player && player->IsPlayer()) ? player->entindex() : 0;

	bool cancelled = g_events.fire_item_buy(slot, buy_item_name(item));
	if (cancelled)
		return;					// no charge, nothing given

	chain->callNext(player, item);
}

// player.h's RewardType, named for Lua the same way buy_item_name() names
// BuyItemMenuSlot - a plugin sees why the money moved, not a raw enum.
static const char *reward_type_name(RewardType type)
{
	switch (type) {
	case RT_ROUND_BONUS:             return "round_bonus";
	case RT_PLAYER_RESET:            return "player_reset";
	case RT_PLAYER_JOIN:             return "player_join";
	case RT_PLAYER_SPEC_JOIN:        return "player_spec_join";
	case RT_PLAYER_BOUGHT_SOMETHING: return "bought_something";
	case RT_HOSTAGE_TOOK:            return "hostage_took";
	case RT_HOSTAGE_RESCUED:         return "hostage_rescued";
	case RT_HOSTAGE_DAMAGED:         return "hostage_damaged";
	case RT_HOSTAGE_KILLED:          return "hostage_killed";
	case RT_TEAMMATES_KILLED:        return "teammates_killed";
	case RT_ENEMY_KILLED:            return "enemy_killed";
	case RT_INTO_GAME:               return "into_game";
	case RT_VIP_KILLED:              return "vip_killed";
	case RT_VIP_RESCUED_MYSELF:      return "vip_rescued_myself";
	default:                         return "none";
	}
}

static void hook_add_account(IReGameHook_CBasePlayer_AddAccount *chain, CBasePlayer *player,
	int amount, RewardType type, bool bTrackChange)
{
	int slot = (player && player->IsPlayer()) ? player->entindex() : 0;

	bool cancelled = g_events.fire_money_change(slot, amount, reward_type_name(type));
	if (cancelled)
		return;					// balance untouched

	chain->callNext(player, amount, type, bTrackChange);
}

static int hook_give_ammo(IReGameHook_CBasePlayer_GiveAmmo *chain, CBasePlayer *player,
	int iCount, const char *pszName, int iMax)
{
	int slot = (player && player->IsPlayer()) ? player->entindex() : 0;

	bool cancelled = g_events.fire_ammo_pickup(slot, pszName ? pszName : "", iCount, iMax);
	if (cancelled)
		return -1;				// GiveAmmo's own "nothing given" return

	return chain->callNext(player, iCount, pszName, iMax);
}

// Not the same path as p:give() (GiveNamedItemEx, a separate ReGameDLL API
// call the SDK does not document as routing through this virtual) - whatever
// else hands a player an item by classname: default round gear, an rcon
// give, another mod's CBasePlayer::GiveNamedItem call.
static CBaseEntity *hook_give_item(IReGameHook_CBasePlayer_GiveNamedItem *chain, CBasePlayer *player,
	const char *pszName)
{
	if (!player || !player->IsPlayer())
		return chain->callNext(player, pszName);

	if (g_events.fire_item_give(player->entindex(), pszName ? pszName : ""))
		return NULL;			// blocked: nothing created, nothing given

	return chain->callNext(player, pszName);
}

static void hook_strip_items(IReGameHook_CBasePlayer_RemoveAllItems *chain, CBasePlayer *player, BOOL bRemoveSuit)
{
	chain->callNext(player, bRemoveSuit);

	if (player && player->IsPlayer())
		g_events.fire_player_strip(player->entindex(), bRemoveSuit != 0);
}

static CBaseEntity *hook_drop_item(IReGameHook_CBasePlayer_DropPlayerItem *chain, CBasePlayer *player,
	const char *pszItemName)
{
	int slot = (player && player->IsPlayer()) ? player->entindex() : 0;

	bool cancelled = g_events.fire_weapon_drop(slot, pszItemName ? pszItemName : "");
	if (cancelled)
		return NULL;			// weapon stays in the player's hands

	return chain->callNext(player, pszItemName);
}

// CanHavePlayerItem's own comment in the SDK: "the player is touching an
// CBasePlayerItem, do I give it to him?" - broader than a ground pickup,
// whatever else asks this question before handing over an item goes through
// here too. Pre-check only: cancelling forces "no", there is no way to force
// a "yes" the game's own rules would not have given.
static BOOL hook_can_have_item(IReGameHook_CSGameRules_CanHavePlayerItem *chain,
	CBasePlayer *player, CBasePlayerItem *item)
{
	int slot = (player && player->IsPlayer()) ? player->entindex() : 0;
	const char *weapon = (item && item->pev) ? STRING(item->pev->classname) : "";

	if (g_events.fire_player_can_have_item(slot, weapon))
		return FALSE;

	return chain->callNext(player, item);
}

// PlayerGotWeapon's own comment: "called each time a player picks up a
// weapon from the ground" - unlike hook_give_item above, this is
// specifically the ground-touch path.
static void hook_got_weapon(IReGameHook_CSGameRules_PlayerGotWeapon *chain,
	CBasePlayer *player, CBasePlayerItem *item)
{
	chain->callNext(player, item);

	if (player && player->IsPlayer()) {
		const char *weapon = (item && item->pev) ? STRING(item->pev->classname) : "";
		g_events.fire_weapon_pickup(player->entindex(), weapon);
	}
}

// Jump/Duck can't be blocked here - by the time this fires the engine has
// already committed to the frame's movement, and skipping the chain would
// leave player physics half-applied. Notify only.
static void hook_jump(IReGameHook_CBasePlayer_Jump *chain, CBasePlayer *player)
{
	chain->callNext(player);

	if (player && player->IsPlayer())
		g_events.fire_player_jump(player->entindex());
}

static void hook_duck(IReGameHook_CBasePlayer_Duck *chain, CBasePlayer *player)
{
	chain->callNext(player);

	if (player && player->IsPlayer())
		g_events.fire_player_duck(player->entindex());
}

static void hook_start_observer(IReGameHook_CBasePlayer_StartObserver *chain, CBasePlayer *player,
	Vector &vecPosition, Vector &vecViewAngle)
{
	chain->callNext(player, vecPosition, vecViewAngle);

	if (player && player->IsPlayer())
		g_events.fire_player_spectate(player->entindex());
}

static void hook_radio(IReGameHook_CBasePlayer_Radio *chain, CBasePlayer *player,
	const char *radio_sentence, const char *sample, short pitch, bool bSpecific)
{
	int slot = (player && player->IsPlayer()) ? player->entindex() : 0;

	bool cancelled = g_events.fire_player_radio(slot, radio_sentence ? radio_sentence : "",
		sample ? sample : "");
	if (cancelled)
		return;					// no sound, no "Radio:" console line

	chain->callNext(player, radio_sentence, sample, pitch, bSpecific);
}

static BOOL hook_can_respawn(IReGameHook_CSGameRules_FPlayerCanRespawn *chain, CBasePlayer *player)
{
	int slot = (player && player->IsPlayer()) ? player->entindex() : 0;

	// Pre-check only: cancelling forces "no" without ever asking the game's
	// own rules. There is no way to force a "yes" the game would not have
	// given on its own - this can only take away permission, not grant it.
	if (g_events.fire_player_can_respawn(slot))
		return FALSE;

	return chain->callNext(player);
}

static void hook_defuse_start(IReGameHook_CGrenade_DefuseBombStart *chain, CGrenade *grenade,
	CBasePlayer *player)
{
	chain->callNext(grenade, player);

	if (player && player->IsPlayer())
		g_events.fire_bomb_defuse_start(player->entindex(), player->m_bHasDefuser);
}

static CGrenade *hook_throw_smoke_grenade(IReGameHook_ThrowSmokeGrenade *chain, entvars_t *pevOwner,
	Vector &vecStart, Vector &vecVelocity, float time, unsigned short usEvent)
{
	int owner = attacker_slot(pevOwner);

	float fuse = time;
	g_events.fire_grenade_throw(owner, "weapon_smokegrenade", fuse);

	CGrenade *grenade = chain->callNext(pevOwner, vecStart, vecVelocity, fuse, usEvent);

	int index = (grenade && grenade->pev) ? ENTINDEX(ENT(grenade->pev)) : 0;
	g_events.fire_grenade_thrown(owner, "weapon_smokegrenade", index);

	return grenade;
}

// ThrowFlashbang has no usEvent parameter - the engine picks the flashbang's
// own event internally, unlike ThrowHeGrenade/ThrowSmokeGrenade above.
static CGrenade *hook_throw_flashbang(IReGameHook_ThrowFlashbang *chain, entvars_t *pevOwner,
	Vector &vecStart, Vector &vecVelocity, float time)
{
	int owner = attacker_slot(pevOwner);

	float fuse = time;
	g_events.fire_grenade_throw(owner, "weapon_flashbang", fuse);

	CGrenade *grenade = chain->callNext(pevOwner, vecStart, vecVelocity, fuse);

	int index = (grenade && grenade->pev) ? ENTINDEX(ENT(grenade->pev)) : 0;
	g_events.fire_grenade_thrown(owner, "weapon_flashbang", index);

	return grenade;
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

// Standard HL SDK convention: a thrown grenade's pev->owner is the thrower.
static int grenade_owner_slot(CGrenade *grenade)
{
	if (!grenade || !grenade->pev || !grenade->pev->owner)
		return 0;

	int idx = ENTINDEX(grenade->pev->owner);
	return (idx >= 1 && idx < CSLUA_MAXPLAYERS) ? idx : 0;
}

// This is ExplodeHeGrenade specifically, not FireBullets or TakeDamage: an HE
// grenade's blast never goes through either, so there was no way for a script
// to tell "this player's HE just popped" from "the bomb just popped" short of
// guessing off DMG_BLAST - which C4 sets too. Hooking the grenade's own
// explode call sidesteps that entirely.
static void hook_explode_he_grenade(IReGameHook_CGrenade_ExplodeHeGrenade *chain, CGrenade *grenade,
	TraceResult *ptr, int bitsDamageType)
{
	Vector where = grenade && grenade->pev ? grenade->pev->origin : Vector(0, 0, 0);
	int owner = grenade_owner_slot(grenade);
	int index = (grenade && grenade->pev) ? ENTINDEX(ENT(grenade->pev)) : 0;

	bool cancelled = g_events.fire_grenade_explode(owner, "weapon_hegrenade", index, where.x, where.y, where.z);
	if (cancelled)
		return;			// a script took the explosion over: no damage, no effects, no cleanup

	chain->callNext(grenade, ptr, bitsDamageType);
}

// Same idea as above, for the smoke grenade's own explode call. Cancelling
// here does not free anyone from damage - smoke never dealt any - it only
// stops the stock smoke cloud/sound so a script's own effect isn't fighting
// the game's.
static void hook_explode_smoke_grenade(IReGameHook_CGrenade_ExplodeSmokeGrenade *chain, CGrenade *grenade)
{
	Vector where = grenade && grenade->pev ? grenade->pev->origin : Vector(0, 0, 0);
	int owner = grenade_owner_slot(grenade);
	int index = (grenade && grenade->pev) ? ENTINDEX(ENT(grenade->pev)) : 0;

	bool cancelled = g_events.fire_grenade_explode(owner, "weapon_smokegrenade", index, where.x, where.y, where.z);
	if (cancelled)
		return;

	chain->callNext(grenade);
}

// Same idea as hook_explode_smoke_grenade: flashbang deals no direct damage,
// so cancelling only skips the stock blind/deafen effect and pop sound, not
// any TakeDamage call.
static void hook_explode_flashbang(IReGameHook_CGrenade_ExplodeFlashbang *chain, CGrenade *grenade,
	TraceResult *ptr, int bitsDamageType)
{
	Vector where = grenade && grenade->pev ? grenade->pev->origin : Vector(0, 0, 0);
	int owner = grenade_owner_slot(grenade);
	int index = (grenade && grenade->pev) ? ENTINDEX(ENT(grenade->pev)) : 0;

	bool cancelled = g_events.fire_grenade_explode(owner, "weapon_flashbang", index, where.x, where.y, where.z);
	if (cancelled)
		return;

	chain->callNext(grenade, ptr, bitsDamageType);
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
	HOOK_DEPLOY,
	HOOK_RELOAD,
	HOOK_SHOTGUN_RELOAD,
	HOOK_ROUND_START,
	HOOK_ROUND_END,
	HOOK_FREEZE_END,
	HOOK_PLANT,
	HOOK_DEFUSE,
	HOOK_EXPLODE,
	HOOK_EXPLODE_HE,
	HOOK_EXPLODE_SMOKE,
	HOOK_EXPLODE_FLASH,
	HOOK_THROW_HE,
	HOOK_THROW_SMOKE,
	HOOK_THROW_FLASH,
	HOOK_THROW_GENERIC,
	HOOK_BUY_WEAPON,
	HOOK_BUY_AMMO,
	HOOK_BUY_ITEM,
	HOOK_ADD_ACCOUNT,
	HOOK_GIVE_AMMO,
	HOOK_DROP_ITEM,
	HOOK_JUMP,
	HOOK_DUCK,
	HOOK_START_OBSERVER,
	HOOK_RADIO,
	HOOK_CAN_RESPAWN,
	HOOK_DEFUSE_START,
	HOOK_TRACE_ATTACK,
	HOOK_TAKE_HEALTH,
	HOOK_BALANCE_TEAMS,
	HOOK_GO_TO_INTERMISSION,
	HOOK_GIVE_ITEM,
	HOOK_STRIP_ITEMS,
	HOOK_CAN_HAVE_ITEM,
	HOOK_GOT_WEAPON,
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

	sync_hook(HOOK_DEPLOY, s_hooks->CBasePlayerWeapon_DefaultDeploy(), &hook_deploy,
		g_events.any(CSLUA_EVENT_WEAPON_DEPLOY));

	bool reloading = g_events.any(CSLUA_EVENT_WEAPON_RELOAD);
	sync_hook(HOOK_RELOAD, s_hooks->CBasePlayerWeapon_DefaultReload(), &hook_reload, reloading);
	sync_hook(HOOK_SHOTGUN_RELOAD, s_hooks->CBasePlayerWeapon_DefaultShotgunReload(), &hook_shotgun_reload, reloading);

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

	bool exploding = g_events.any(CSLUA_EVENT_GRENADE_EXPLODE);
	sync_hook(HOOK_EXPLODE_HE, s_hooks->CGrenade_ExplodeHeGrenade(), &hook_explode_he_grenade, exploding);
	sync_hook(HOOK_EXPLODE_SMOKE, s_hooks->CGrenade_ExplodeSmokeGrenade(), &hook_explode_smoke_grenade, exploding);
	sync_hook(HOOK_EXPLODE_FLASH, s_hooks->CGrenade_ExplodeFlashbang(), &hook_explode_flashbang, exploding);

	bool throwing = g_events.any(CSLUA_EVENT_GRENADE_THROW) || g_events.any(CSLUA_EVENT_GRENADE_THROWN);
	sync_hook(HOOK_THROW_HE, s_hooks->ThrowHeGrenade(), &hook_throw_he_grenade, throwing);
	sync_hook(HOOK_THROW_SMOKE, s_hooks->ThrowSmokeGrenade(), &hook_throw_smoke_grenade, throwing);
	sync_hook(HOOK_THROW_FLASH, s_hooks->ThrowFlashbang(), &hook_throw_flashbang, throwing);

	sync_hook(HOOK_THROW_GENERIC, s_hooks->CBasePlayer_ThrowGrenade(), &hook_throw_grenade,
		g_events.any(CSLUA_EVENT_WEAPON_THROW));

	sync_hook(HOOK_BUY_WEAPON, s_hooks->BuyWeaponByWeaponID(), &hook_buy_weapon,
		g_events.any(CSLUA_EVENT_WEAPON_BUY));

	sync_hook(HOOK_BUY_AMMO, s_hooks->BuyGunAmmo(), &hook_buy_gun_ammo,
		g_events.any(CSLUA_EVENT_AMMO_BUY));

	sync_hook(HOOK_BUY_ITEM, s_hooks->BuyItem(), &hook_buy_item,
		g_events.any(CSLUA_EVENT_ITEM_BUY));

	sync_hook(HOOK_ADD_ACCOUNT, s_hooks->CBasePlayer_AddAccount(), &hook_add_account,
		g_events.any(CSLUA_EVENT_MONEY_CHANGE));

	sync_hook(HOOK_GIVE_AMMO, s_hooks->CBasePlayer_GiveAmmo(), &hook_give_ammo,
		g_events.any(CSLUA_EVENT_AMMO_PICKUP));

	sync_hook(HOOK_DROP_ITEM, s_hooks->CBasePlayer_DropPlayerItem(), &hook_drop_item,
		g_events.any(CSLUA_EVENT_WEAPON_DROP));

	sync_hook(HOOK_JUMP, s_hooks->CBasePlayer_Jump(), &hook_jump,
		g_events.any(CSLUA_EVENT_PLAYER_JUMP));

	sync_hook(HOOK_DUCK, s_hooks->CBasePlayer_Duck(), &hook_duck,
		g_events.any(CSLUA_EVENT_PLAYER_DUCK));

	sync_hook(HOOK_START_OBSERVER, s_hooks->CBasePlayer_StartObserver(), &hook_start_observer,
		g_events.any(CSLUA_EVENT_PLAYER_SPECTATE));

	sync_hook(HOOK_RADIO, s_hooks->CBasePlayer_Radio(), &hook_radio,
		g_events.any(CSLUA_EVENT_PLAYER_RADIO));

	sync_hook(HOOK_CAN_RESPAWN, s_hooks->CSGameRules_FPlayerCanRespawn(), &hook_can_respawn,
		g_events.any(CSLUA_EVENT_PLAYER_CAN_RESPAWN));

	sync_hook(HOOK_DEFUSE_START, s_hooks->CGrenade_DefuseBombStart(), &hook_defuse_start,
		g_events.any(CSLUA_EVENT_BOMB_DEFUSE_START));

	sync_hook(HOOK_TRACE_ATTACK, s_hooks->CBasePlayer_TraceAttack(), &hook_trace_attack,
		g_events.any(CSLUA_EVENT_PLAYER_TRACE_ATTACK));

	sync_hook(HOOK_TAKE_HEALTH, s_hooks->CBasePlayer_TakeHealth(), &hook_take_health,
		g_events.any(CSLUA_EVENT_PLAYER_HEAL));

	sync_hook(HOOK_BALANCE_TEAMS, s_hooks->CSGameRules_BalanceTeams(), &hook_balance_teams,
		g_events.any(CSLUA_EVENT_ROUND_BALANCE_TEAMS));

	sync_hook(HOOK_GO_TO_INTERMISSION, s_hooks->CSGameRules_GoToIntermission(), &hook_go_to_intermission,
		g_events.any(CSLUA_EVENT_ROUND_INTERMISSION));

	sync_hook(HOOK_GIVE_ITEM, s_hooks->CBasePlayer_GiveNamedItem(), &hook_give_item,
		g_events.any(CSLUA_EVENT_ITEM_GIVE));

	sync_hook(HOOK_STRIP_ITEMS, s_hooks->CBasePlayer_RemoveAllItems(), &hook_strip_items,
		g_events.any(CSLUA_EVENT_PLAYER_STRIP));

	sync_hook(HOOK_CAN_HAVE_ITEM, s_hooks->CSGameRules_CanHavePlayerItem(), &hook_can_have_item,
		g_events.any(CSLUA_EVENT_PLAYER_CAN_HAVE_ITEM));

	sync_hook(HOOK_GOT_WEAPON, s_hooks->CSGameRules_PlayerGotWeapon(), &hook_got_weapon,
		g_events.any(CSLUA_EVENT_WEAPON_PICKUP));
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
	s_hooks->CBasePlayerWeapon_DefaultDeploy()->unregisterHook(&hook_deploy);
	s_hooks->CBasePlayerWeapon_DefaultReload()->unregisterHook(&hook_reload);
	s_hooks->CBasePlayerWeapon_DefaultShotgunReload()->unregisterHook(&hook_shotgun_reload);
	s_hooks->CSGameRules_RestartRound()->unregisterHook(&hook_round_start);
	s_hooks->RoundEnd()->unregisterHook(&hook_round_end);
	s_hooks->CSGameRules_OnRoundFreezeEnd()->unregisterHook(&hook_freeze_end);
	s_hooks->PlantBomb()->unregisterHook(&hook_plant_bomb);
	s_hooks->CGrenade_DefuseBombEnd()->unregisterHook(&hook_defuse_end);
	s_hooks->CGrenade_ExplodeBomb()->unregisterHook(&hook_explode_bomb);
	s_hooks->CGrenade_ExplodeHeGrenade()->unregisterHook(&hook_explode_he_grenade);
	s_hooks->CGrenade_ExplodeSmokeGrenade()->unregisterHook(&hook_explode_smoke_grenade);
	s_hooks->CGrenade_ExplodeFlashbang()->unregisterHook(&hook_explode_flashbang);
	s_hooks->ThrowHeGrenade()->unregisterHook(&hook_throw_he_grenade);
	s_hooks->ThrowSmokeGrenade()->unregisterHook(&hook_throw_smoke_grenade);
	s_hooks->ThrowFlashbang()->unregisterHook(&hook_throw_flashbang);
	s_hooks->CBasePlayer_ThrowGrenade()->unregisterHook(&hook_throw_grenade);
	s_hooks->BuyWeaponByWeaponID()->unregisterHook(&hook_buy_weapon);
	s_hooks->BuyGunAmmo()->unregisterHook(&hook_buy_gun_ammo);
	s_hooks->BuyItem()->unregisterHook(&hook_buy_item);
	s_hooks->CBasePlayer_AddAccount()->unregisterHook(&hook_add_account);
	s_hooks->CBasePlayer_GiveAmmo()->unregisterHook(&hook_give_ammo);
	s_hooks->CBasePlayer_DropPlayerItem()->unregisterHook(&hook_drop_item);
	s_hooks->CBasePlayer_Jump()->unregisterHook(&hook_jump);
	s_hooks->CBasePlayer_Duck()->unregisterHook(&hook_duck);
	s_hooks->CBasePlayer_StartObserver()->unregisterHook(&hook_start_observer);
	s_hooks->CBasePlayer_Radio()->unregisterHook(&hook_radio);
	s_hooks->CSGameRules_FPlayerCanRespawn()->unregisterHook(&hook_can_respawn);
	s_hooks->CGrenade_DefuseBombStart()->unregisterHook(&hook_defuse_start);
	s_hooks->CBasePlayer_TraceAttack()->unregisterHook(&hook_trace_attack);
	s_hooks->CBasePlayer_TakeHealth()->unregisterHook(&hook_take_health);
	s_hooks->CSGameRules_BalanceTeams()->unregisterHook(&hook_balance_teams);
	s_hooks->CSGameRules_GoToIntermission()->unregisterHook(&hook_go_to_intermission);
	s_hooks->CBasePlayer_GiveNamedItem()->unregisterHook(&hook_give_item);
	s_hooks->CBasePlayer_RemoveAllItems()->unregisterHook(&hook_strip_items);
	s_hooks->CSGameRules_CanHavePlayerItem()->unregisterHook(&hook_can_have_item);
	s_hooks->CSGameRules_PlayerGotWeapon()->unregisterHook(&hook_got_weapon);

	for (int i = 0; i < HOOK_COUNT; i++)
		s_installed[i] = false;
}
