#pragma once

#include <lua.hpp>
#include <string>

// Shared by players.list{...} and players.broadcast{...}: same fields, same
// matching rules, so a filter reads the same regardless of which one built it.
struct PlayerFilter
{
	int alive = -1;			// -1 = don't care, 0 = dead, 1 = alive
	int bot = -1;
	int hltv = -1;
	std::string team;
	std::string name;

	bool empty() const
	{
		return alive < 0 && bot < 0 && hltv < 0 && team.empty() && name.empty();
	}

	// alive/team need ReGameDLL; bot/hltv/name read off the edict/name cache.
	bool needs_regamedll() const { return alive >= 0 || !team.empty(); }
};

// Reads alive/bot/hltv/team/name out of the plain table at idx, e.g.
// players.list{...}'s argument.
void cslua_read_player_filter(lua_State *L, int idx, PlayerFilter &f);

// Same fields, via raw table access. Safe to call on any players.broadcast
// object (filtered or not) without tripping its __index "unknown key" error.
void cslua_read_broadcast_filter(lua_State *L, int idx, PlayerFilter &f);

bool cslua_player_matches_filter(int id, const PlayerFilter &f);
