-- Groups. Hand-edited: cs-lua reads this file and never rewrites it, so
-- comments and layout stay yours. `lua_perms reload` picks up changes without
-- restarting the server.
--
--   weight   immunity. Acting on another player needs strictly more weight,
--            so two admins of the same rank cannot slay each other.
--   inherit  one group name or a list; everything they allow, this group
--            allows too, at a lower priority than its own nodes.
--   allow    permission nodes. "shop.vip.*" covers everything below it,
--            "*" covers everything at all.
--   deny     nodes taken back. A deny beats an allow of the same precision.
--   where    optional context: { map = "de_dust2" } or { maps = { ... } }.
--
-- Everyone is in `default`, including players with no entry in users.lua.

return {
	default = {
		weight = 0,
		allow  = { "chat.say" },
	},

	vip = {
		weight  = 10,
		inherit = "default",
		allow   = { "shop.vip.*", "chat.prefix" },
	},

	-- Может открыть админ-меню и смотреть чужие права, но не раздавать их:
	-- выдача групп начинается с admin.
	moderator = {
		weight  = 30,
		inherit = "vip",
		allow   = { "admin.menu", "admin.rights.view",
		            "admin.kick", "admin.slay", "admin.mute" },
	},

	admin = {
		weight  = 50,
		inherit = "moderator",
		allow   = { "admin.*" },
		deny    = { "admin.rcon" },
	},

	owner = {
		weight  = 100,
		inherit = "admin",
		allow   = { "*" },
	},
}
