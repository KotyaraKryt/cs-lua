-- One colour type for every channel.
--
-- A colour is written the same way everywhere:
--
--   p:hud(text, { color = "green" })
--   p:hud(text, { color = { 0, 255, 0 } })
--   p:hud(text, { color = "#00ff00" })
--   menu.new("Weapons", { color = { title = "green" } })
--
-- What a channel can actually draw differs wildly - the HUD takes any RGB, a
-- menu panel has four codes, chat has three - so each one degrades to its
-- nearest available colour rather than making you write it three ways.
--
-- The names come from ui.palette, which the module owns, so this file and the
-- C side can never disagree on what "green" is.

local color = {}

local palette = ui and ui.palette or {}

local function hex_to_rgb(s)
	local r, g, b = s:match("^#(%x%x)(%x%x)(%x%x)$")
	if not r then
		return nil
	end
	return { tonumber(r, 16), tonumber(g, 16), tonumber(b, 16) }
end

-- color.parse(v) -> { r, g, b }, or nil plus why not.
-- Accepts a palette name, "#rrggbb", or a {r, g, b} table.
function color.parse(v)
	if type(v) == "table" then
		local r, g, b = v[1], v[2], v[3]
		if type(r) ~= "number" or type(g) ~= "number" or type(b) ~= "number" then
			return nil, "a colour table needs three numbers: { r, g, b }"
		end
		return { r, g, b }
	end

	if type(v) ~= "string" then
		return nil, "a colour is a name, \"#rrggbb\" or { r, g, b }, got " .. type(v)
	end

	local name = v:lower()
	if name == "gray" then
		name = "grey"
	end

	local named = palette[name]
	if named then
		return { named[1], named[2], named[3] }
	end

	local hex = hex_to_rgb(v)
	if hex then
		return hex
	end

	local known = {}
	for key in pairs(palette) do
		known[#known + 1] = key
	end
	table.sort(known)

	return nil, ("unknown colour '%s' (known: %s, or \"#rrggbb\", or { r, g, b })")
		:format(v, table.concat(known, ", "))
end

-- Squared distance in plain RGB. Not perceptually correct, and it does not
-- need to be: it is picking between four fixed codes, and every reasonable
-- input lands on the obvious one.
local function distance(a, b)
	local dr, dg, db = a[1] - b[1], a[2] - b[2], a[3] - b[3]
	return dr * dr + dg * dg + db * db
end

-- Builds a "nearest of these" function over a list of { code, rgb } pairs.
local function nearest_of(choices)
	return function(v)
		local rgb, err = color.parse(v)
		if not rgb then
			return nil, err
		end

		local best, best_d = choices[1], distance(rgb, choices[1].rgb)
		for i = 2, #choices do
			local d = distance(rgb, choices[i].rgb)
			if d < best_d then
				best, best_d = choices[i], d
			end
		end

		return best.code
	end
end

--------------------------------------------------------------------------
-- Menu panels: four codes, and yellow is what an unpainted panel already is.
--------------------------------------------------------------------------

local MENU_CODES = {
	{ code = "\\w", rgb = { 255, 255, 255 } },
	{ code = "\\r", rgb = { 255, 64,  64  } },
	{ code = "\\y", rgb = { 255, 208, 0   } },
	{ code = "\\d", rgb = { 160, 160, 160 } },
}

local menu_nearest = nearest_of(MENU_CODES)

-- color.menu(v) -> "\r" and friends. A raw code passes straight through, so
-- anything copied out of an AMX Mod X menu keeps working.
function color.menu(v)
	if type(v) == "string" and #v == 2 and v:sub(1, 1) == "\\" then
		for _, choice in ipairs(MENU_CODES) do
			if choice.code == v then
				return v
			end
		end
		return nil, ("unknown menu colour code '%s' (\\w, \\r, \\y, \\d)"):format(v)
	end

	return menu_nearest(v)
end

--------------------------------------------------------------------------
-- Chat: two real colours plus {team}, which is not a colour at all - it is
-- "whatever side the sender is on" and cannot be chosen.
--------------------------------------------------------------------------

local CHAT_CODES = {
	{ code = "{default}", rgb = { 255, 208, 0 } },
	{ code = "{green}",   rgb = { 0,   255, 0 } },
}

local chat_nearest = nearest_of(CHAT_CODES)

-- color.chat(v) -> "{green}" or "{default}". "team" is passed through as the
-- one contextual tag.
function color.chat(v)
	if v == "team" then
		return "{team}"
	end
	return chat_nearest(v)
end

-- The exact tags SayText understands - see s_chat_tags in lua_message.cpp.
-- Nothing else (console, rcon, a player's own client console) renders them,
-- so a reply that started life as a chat message needs these gone, not just
-- left as literal "{green}" noise.
local CHAT_TAGS = { "{default}", "{yellow}", "{team}", "{green}" }

-- color.strip_chat(text) -> text with every {tag} above removed.
function color.strip_chat(text)
	for _, tag in ipairs(CHAT_TAGS) do
		text = text:gsub(tag, "")
	end
	return text
end

--------------------------------------------------------------------------
-- HUD: whatever it is given.
--------------------------------------------------------------------------

color.rgb = color.parse

return color
