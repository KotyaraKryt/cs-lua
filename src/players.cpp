#include "players.h"

#include <string.h>

Players g_players;

// What the engine hands back while Steam has not answered yet. "BOT", "HLTV"
// and "STEAM_ID_LAN" are final answers, not placeholders.
static bool pending_authid(const std::string &authid)
{
	return authid.empty()
		|| authid == "STEAM_ID_PENDING"
		|| authid == "ID_PENDING"
		|| authid == "unknown";
}

void Players::on_connect(int id, const char *name, const char *ip, const char *authid)
{
	if (!valid(id))
		return;

	Info &p = m_players[id];
	p.connected = true;
	p.name = name ? name : "";
	p.ip = ip ? ip : "";
	p.authid = authid ? authid : "";
	p.authorized = !pending_authid(p.authid);
}

void Players::on_disconnect(int id)
{
	if (!valid(id))
		return;

	m_players[id] = Info();
}

bool Players::is_connected(int id) const
{
	return valid(id) && m_players[id].connected;
}

const char *Players::name(int id) const
{
	return valid(id) ? m_players[id].name.c_str() : "";
}

const char *Players::ip(int id) const
{
	return valid(id) ? m_players[id].ip.c_str() : "";
}

const char *Players::authid(int id) const
{
	return valid(id) ? m_players[id].authid.c_str() : "";
}

bool Players::authid_pending(int id) const
{
	return valid(id) && m_players[id].connected && !m_players[id].authorized;
}

bool Players::is_authorized(int id) const
{
	return valid(id) && m_players[id].connected && m_players[id].authorized;
}

bool Players::refresh_authid(int id)
{
	if (!authid_pending(id))
		return false;

	edict_t *e = g_engfuncs.pfnPEntityOfEntIndex(id);
	if (!e || e->free)
		return false;

	const char *now = g_engfuncs.pfnGetPlayerAuthId(e);
	std::string authid = now ? now : "";
	if (pending_authid(authid))
		return false;

	m_players[id].authid = authid;
	m_players[id].authorized = true;
	return true;
}
