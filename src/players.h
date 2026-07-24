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

private:
	static bool valid(int id) { return id >= 1 && id < CSLUA_MAXPLAYERS; }

	struct Info
	{
		bool connected = false;
		bool authorized = false;
		std::string name;
		std::string ip;
		std::string authid;
	};

	Info m_players[CSLUA_MAXPLAYERS];
};

extern Players g_players;
