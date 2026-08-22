-- Core: what the player sees - menus, and the colour helper every channel
-- shares.
--
--   local m = menu.new("Choose a weapon")
--   m:add("AK-47",  function(p) p:give("weapon_ak47") end)
--   m:add("AWP",    function(p) p:give("weapon_awp") end)
--   m:add("Nothing", nil, { disabled = true })   -- greyed out, key does nothing
--   m:show(p)
--
-- Up to 8 items per page; anything past that gets Next/Back automatically, so
-- a long list needs no extra work. Key 0 is Exit unless you turn it off.
--
-- The handler gets (player, item) - `item` is what add() returned, so you can
-- hang your own fields on it.
--
-- Colours are written the same way here as in p:hud() and p:chat(): a palette
-- name, "#rrggbb" or { r, g, b }. A panel can only draw four of them, so it
-- picks the nearest one it has.
--
--   menu.new("Title", { color = { title = "yellow", number = "red" } })
--   m:add("Boss room", fn, { color = "red" })
--   m:add("Locked",    nil, { disabled = true })   -- uses colors.disabled
--
-- Nothing is coloured by default, which looks exactly like an uncoloured
-- panel did before.

local color = require("color")

-- Raw panel drawing, taken into locals and cleared: menu.new() is what a
-- plugin sees by default...
local show_panel  = menu._show
local close_panel = menu._close
menu._show, menu._close = nil, nil

-- ui.color(v) -> { r, g, b }. The one place a colour is turned into numbers,
-- for a plugin that wants to compute one rather than name it.
function ui.color(v)
	local rgb, err = color.parse(v)
	if not rgb then
		error("ui.color: " .. err, 2)
	end
	return rgb
end

local ITEMS_PER_PAGE = 8

-- Slots the engine gives us: bit 0 is key "1" ... bit 9 is key "0".
local KEY_9 = 9
local KEY_0 = 10

-- Yellow is what the panel paints with when no code was sent at all, so it
-- doubles as the "back to normal" colour.
local DEFAULT_COLOR = "\\y"

-- `level` is where to point the blame: the script that called menu.new() or
-- add(), which is the line worth showing.
local function code_for(v, what, level)
	if v == nil then
		return nil
	end

	local code, err = color.menu(v)
	if not code then
		error(("menu: %s colour: %s"):format(what, err), level or 4)
	end
	return code
end

-- color = "red"                  -> paints the item text
-- color = { number = , text = }  -> the two halves separately
local function item_colors(v)
	if v == nil then
		return nil
	end

	if type(v) == "table" and (v.number ~= nil or v.text ~= nil) then
		return {
			number = code_for(v.number, "number"),
			text   = code_for(v.text, "text"),
		}
	end

	return { text = code_for(v, "item") }
end

local function menu_colors(v)
	v = v or {}

	-- A bare colour (a name, or an { r, g, b } triple) paints the item text.
	if type(v) ~= "table" or v[1] ~= nil then
		v = { text = v }
	end

	return {
		title    = code_for(v.title, "title"),
		number   = code_for(v.number, "number"),
		text     = code_for(v.text, "text"),
		nav      = code_for(v.nav, "nav"),
		-- Disabled items were always greyed out; that stays the default.
		disabled = code_for(v.disabled, "disabled") or "\\d",
	}
end

local Menu = {}
Menu.__index = Menu

-- Who is looking at what, so a reply lands on the right menu and page.
local open = {}

-- ...but a plugin that wants to build the panel text and key mask itself -
-- the same shape AMXModX's show_menu()/register_menucmd() offered - gets
-- that here instead. Handle the answer with hook.add("menu:select", ...);
-- e.player and e.key are the same key AMXModX's menu callback got.
--
--   menu.raw_show(p.id, 2^0 + 2^1, -1, "Choose:\n1. Yes\n2. No")
--   hook.add("menu:select", "my_plugin", function(e)
--       if e.key == 1 then ... end
--   end)
--
-- Only one panel can be on screen at a time, so showing a raw menu drops
-- whatever menu.new() menu that player had open, and the other way round.
function menu.raw_show(id, keys, time, text)
	open[id] = nil
	return show_panel(id, keys, time, text)
end

function menu.raw_close(id)
	open[id] = nil
	return close_panel(id)
end

function menu.new(title, opts)
	opts = opts or {}
	return setmetatable({
		title    = title or "",
		items    = {},
		exit     = opts.exit ~= false,      -- key 0 closes, unless exit = false
		time     = opts.time or -1,         -- seconds on screen, -1 = until answered
		on_exit  = opts.on_exit,            -- called when the player closes it
		colors   = menu_colors(opts.color),
	}, Menu)
end

-- add(text, handler, opts) -> item
function Menu:add(text, handler, opts)
	opts = opts or {}
	local item = {
		text     = text,
		handler  = handler,
		disabled = opts.disabled or false,
		value    = opts.value,
		colors   = item_colors(opts.color),
	}
	self.items[#self.items + 1] = item
	return item
end

function Menu:count()
	return #self.items
end

-- Recolour after the fact, e.g. when a state change should grey an item out.
-- Both take the same spec as the constructor and merge into what is there.
function Menu:color(spec)
	if type(spec) ~= "table" or spec[1] ~= nil then
		spec = { text = spec }
	end

	for _, k in ipairs({ "title", "number", "text", "nav", "disabled" }) do
		if spec[k] ~= nil then
			self.colors[k] = code_for(spec[k], k, 3)
		end
	end
	return self
end

function Menu:item_color(item, spec)
	item.colors = item_colors(spec)
	return item
end

-- Builds the panel text and the key mask for one page.
local function render(self, page)
	local first = page * ITEMS_PER_PAGE + 1
	local last  = math.min(first + ITEMS_PER_PAGE - 1, #self.items)

	local c = self.colors

	-- A colour code stays in effect until the next one, across line breaks, so
	-- the panel is painted as a stream: emit a code only where the colour has
	-- to change. That keeps an uncoloured menu byte-for-byte what it was, and
	-- stops a grey item from bleeding into everything below it.
	local cur = DEFAULT_COLOR      -- a panel starts out yellow on its own
	local function paint(code)
		code = code or DEFAULT_COLOR
		if code == cur then
			return ""
		end
		cur = code
		return code
	end

	local lines = { paint(c.title) .. self.title, "" }
	local keys  = 0
	local slots = {}          -- key number -> item index

	local slot = 0
	for i = first, last do
		slot = slot + 1
		local item = self.items[i]
		local ic = item.colors

		if item.disabled then
			-- Shown, but the key is not in the mask, so it cannot be pressed.
			-- The whole line goes grey, number included.
			lines[#lines + 1] = paint(c.disabled) .. ("%d. %s"):format(slot, item.text)
		else
			lines[#lines + 1] = paint(ic and ic.number or c.number)
				.. slot .. ". "
				.. paint(ic and ic.text or c.text)
				.. item.text
			keys = keys + 2 ^ (slot - 1)
			slots[slot] = i
		end
	end

	local has_next = last < #self.items
	local has_back = page > 0

	lines[#lines + 1] = ""

	local nav_num = c.nav or c.number

	if has_next then
		lines[#lines + 1] = paint(nav_num) .. "9. " .. paint(c.text) .. "Next"
		keys = keys + 2 ^ (KEY_9 - 1)
	end

	if has_back then
		lines[#lines + 1] = paint(nav_num) .. "0. " .. paint(c.text) .. "Back"
		keys = keys + 2 ^ (KEY_0 - 1)
	elseif self.exit then
		lines[#lines + 1] = paint(nav_num) .. "0. " .. paint(c.text) .. "Exit"
		keys = keys + 2 ^ (KEY_0 - 1)
	end

	return table.concat(lines, "\n"), keys, slots, has_next, has_back
end

-- show(player [, page])
function Menu:show(p, page)
	page = page or 0
	if not p or not p:connected() then
		return
	end

	local text, keys, slots, has_next, has_back = render(self, page)

	open[p.id] = {
		menu     = self,
		page     = page,
		slots    = slots,
		has_next = has_next,
		has_back = has_back,
	}

	show_panel(p.id, keys, self.time, text)
end

function Menu:close(p)
	open[p.id] = nil
	close_panel(p.id)
end

hook.add("menu:select", "core.menu_select", function(e)
	local p = e.player
	local state = open[p.id]
	if not state then
		return
	end

	local m = state.menu
	local key = e.key

	if key == KEY_9 and state.has_next then
		return m:show(p, state.page + 1)
	end

	if key == KEY_0 then
		if state.has_back then
			return m:show(p, state.page - 1)
		end

		open[p.id] = nil
		if m.on_exit then
			m.on_exit(p)
		end
		return
	end

	local index = state.slots[key]
	if not index then
		return
	end

	-- The menu is done the moment a real item is picked; a handler that opens
	-- another menu overwrites this entry anyway.
	open[p.id] = nil

	local item = m.items[index]
	if item and item.handler then
		item.handler(p, item)
	end
end)

-- A player who leaves takes their menu state with them.
hook.add("client:disconnect", "core.menu_cleanup", function(e)
	open[e.player.id] = nil
end)
