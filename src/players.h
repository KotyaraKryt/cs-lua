#pragma once

#include "cslua.h"

// Cached per-player data: by the time ClientDisconnect fires the entity's
// netname can already be gone, and scripts want the steamid from either handler.
class Players
{
public:
	void on_connect(int id, const char *name, const char *ip, const char *authid);
	void on_disconnect(int id);

	bool is_connected(int id) const;
	const char *name(int id) const;
	const char *ip(int id) const;
	const char *authid(int id) const;

	// The engine's drop reason, set from the ReHLDS SV_DropClient hookchain
	// (cslua_netwatch.cpp) before ClientDisconnect fires. Stored here because
	// on_disconnect() clears Info before dllapi.cpp reads it. "" on stock HLDS.
	void set_drop_reason(int id, const char *reason);
	const char *drop_reason(int id) const;

	// The authid at ClientConnect is often still STEAM_ID_PENDING; Steam answers
	// a moment later. See the poll in dllapi.cpp.
	bool authid_pending(int id) const;

	// Re-reads the authid. Returns true the one time it becomes final.
	bool refresh_authid(int id);

	bool is_authorized(int id) const;

	// pfnPlayerPreThink strips these from pev->button before the weapon code
	// sees them - see p:suppress_* in lua_player.cpp.
	void set_suppress_attack(int id, bool suppress);
	bool suppress_attack(int id) const;
	void set_suppress_move(int id, bool suppress);
	bool suppress_move(int id) const;

	// Blocks the "drop" client command in ClientCommand, before the game DLL.
	void set_suppress_drop(int id, bool suppress);
	bool suppress_drop(int id) const;

	// Held-down IN_ATTACK2 -> single "just pressed" edge. No ReGameDLL hookchain
	// for SecondaryAttack, so PlayerPreThink calls this every frame.
	bool secondary_attack_pressed(int id, bool down);

private:
	static bool valid(int id) { return id >= 1 && id < CSLUA_MAXPLAYERS; }

	struct Info
	{
		bool connected = false;
		bool authorized = false;
		std::string name;
		std::string ip;
		std::string authid;
		std::string drop_reason;
		bool suppress_attack = false;
		bool suppress_move = false;
		bool suppress_drop = false;
		bool attack2_down = false;
	};

	Info m_players[CSLUA_MAXPLAYERS];
};

extern Players g_players;
