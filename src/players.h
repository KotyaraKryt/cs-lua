#pragma once

#include "cslua.h"

// Cached per-player data. Needed because by the time ClientDisconnect fires
// the entity's netname can already be gone, and because scripts want to ask
// for a player's steamid from either handler.
class Players
{
public:
	void on_connect(int id, const char *name, const char *ip, const char *authid);
	void on_disconnect(int id);

	bool is_connected(int id) const;
	const char *name(int id) const;
	const char *ip(int id) const;
	const char *authid(int id) const;

	// The authid taken at ClientConnect is often still STEAM_ID_PENDING: Steam
	// answers a moment later. Anything keyed on the steamid has to wait for
	// that, which is what these two are for - see the poll in dllapi.cpp.
	bool authid_pending(int id) const;

	// Re-reads the authid from the engine. Returns true the one time it turns
	// a pending id into a final one, so the caller can fire the event.
	bool refresh_authid(int id);

	// True once the final authid is known and player_authorized has fired.
	bool is_authorized(int id) const;

	// Whether pfnPlayerPreThink (dllapi.cpp) strips IN_ATTACK/IN_ATTACK2 from
	// this player's button state before the weapon code ever sees it this
	// frame - see p:suppress_attack in lua_player.cpp for why blocking here
	// beats anything after the fact (weapon_fire only fires once the shot,
	// sound and animation already happened).
	void set_suppress_attack(int id, bool suppress);
	bool suppress_attack(int id) const;

	// Same idea, for IN_FORWARD/IN_BACK/IN_MOVELEFT/IN_MOVERIGHT - pinning a
	// player in place without touching FL_FROZEN, which turned out to zero
	// pev->button along with movement (see p:suppress_move in lua_player.cpp).
	void set_suppress_move(int id, bool suppress);
	bool suppress_move(int id) const;

	// Blocks the "drop" client command outright (dllapi.cpp's ClientCommand,
	// which runs before the game DLL ever sees it) rather than trying to
	// undo a drop after the fact - the weapon that hits the ground becomes
	// its own edict with its own ammo, out of reach of anything read or
	// written through the player who no longer holds it.
	void set_suppress_drop(int id, bool suppress);
	bool suppress_drop(int id) const;

private:
	static bool valid(int id) { return id >= 1 && id < CSLUA_MAXPLAYERS; }

	struct Info
	{
		bool connected = false;
		bool authorized = false;
		std::string name;
		std::string ip;
		std::string authid;
		bool suppress_attack = false;
		bool suppress_move = false;
		bool suppress_drop = false;
	};

	Info m_players[CSLUA_MAXPLAYERS];
};

extern Players g_players;
