-- Who gets what. cs-lua rewrites this file on `lua_perms save`, so comments
-- here do not survive a save - keep the commentary in groups.lua.
--
-- The key is a steamid, or "name:<nick>" for a nickname login. A nickname
-- entry with a password only counts when the client has run
-- `setinfo _pw <password>`; without one, the nickname alone is enough, which
-- is fine on a LAN and nowhere else.
--
-- Per entry:
--   groups    group names from groups.lua
--   allow     personal nodes; they outrank every group the player is in
--   deny      personal denials, same priority
--   until_    unix time, or "map" for "until this map ends"
--   where     context, same shape as in groups.lua

return {
	["STEAM_0:1:12345"] = {
		name   = "Owner",
		groups = { "owner" },
	},

	["STEAM_0:0:99999"] = {
		groups = { "vip" },
		until_ = 1785000000,
	},

	["name:Helper"] = {
		password = "change-me",
		groups   = { "moderator" },
		where    = { map = "de_dust2" },
	},
}
