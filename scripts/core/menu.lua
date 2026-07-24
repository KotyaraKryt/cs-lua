-- Core: menus.
--
--   local m = menu("Choose a weapon")
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
-- Colours: the panel understands four codes, and every part of a menu takes
-- one, by name ("red") or as the raw code ("\\r"):
--
--   menu("Title", { color = { title = "yellow", number = "red" } })
--   m:add("Boss room", fn, { color = "red" })
--   m:add("Locked",    nil, { disabled = true })   -- uses colors.disabled
--
-- Nothing is coloured by default, which looks exactly like an uncoloured
-- panel did before - so old menus keep their look.

local ITEMS_PER_PAGE = 8

-- Slots the engine gives us: bit 0 is key "1" ... bit 9 is key "0".
local KEY_9    = 9
local KEY_0    = 10

-- The four colours the client can draw, and the friendly names for them.
-- Yellow is what the panel paints with when no code was sent at all, so it
-- doubles as the "back to normal" colour.
local DEFAULT_COLOR = "\\y"

local COLORS = {
	white  = "\\w", w = "\\w",
	red    = "\\r", r = "\\r",
	yellow = "\\y", y = "\\y",
	grey   = "\\d", gray = "\\d", d = "\\d", dim = "\\d",
}

-- Accepts a name, a raw code, or nil. Anything else is a typo worth shouting
-- about: a bad colour silently prints as literal text otherwise.
-- `level` is where to point the blame: 4 is the script that called menu() or
-- add(), which is the line worth showing.
local function color_code(v, what, level)
	if v == nil then
		return nil
	end

	level = level or 4

	if type(v) ~= "string" then
		error(("menu: %s color must be a string, got %s"):format(what, type(v)), level)
	end

	if #v == 2 and v:sub(1, 1) == "\\" then
		if v == "\\w" or v == "\\r" or v == "\\y" or v == "\\d" then
			return v
		end
	else
		local code = COLORS[v:lower()]
		if code then
			return code
		end
	end

	error(("menu: unknown %s color '%s' (white, red, yellow, grey)"):format(what, v), level)
end

-- color = "red"                  -> paints the item text
-- color = { number = , text = }  -> the two halves separately
local function item_colors(v)
	if v == nil then
		return nil
	end

	if type(v) == "table" then
		return {
			number = color_code(v.number, "number"),
			text   = color_code(v.text, "text"),
		}
	end

	return { text = color_code(v, "item") }
end

local function menu_colors(v)
	v = v or {}

	if type(v) == "string" then
		v = { text = v }
	end

	return {
		title    = color_code(v.title, "title"),
		number   = color_code(v.number, "number"),
		text     = color_code(v.text, "text"),
		nav      = color_code(v.nav, "nav"),
		-- Disabled items were always greyed out; that stays the default.
		disabled = color_code(v.disabled, "disabled") or "\\d",
	}
end

local Menu = {}
Menu.__index = Menu

-- Who is looking at what, so a reply lands on the right menu and page.
local open = {}

function menu(title, opts)
	opts = opts or {}
	return setmetatable({
		title    = title or "",
		items    = {},
		exit     = opts.exit ~= false,      -- key 0 closes, unless exit = false
		time     = opts.time or -1,         -- seconds on screen, -1 = until answered
		on_exit  = opts.on_exit,            -- called when the player closes it
		colors   = menu_colors(opts.color), -- see color_code() above
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
	if type(spec) == "string" then
		spec = { text = spec }
	end

	for _, k in ipairs({ "title", "number", "text", "nav", "disabled" }) do
		if spec[k] ~= nil then
			self.colors[k] = color_code(spec[k], k, 3)
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

	_show_menu(p.id, keys, self.time, text)
end

function Menu:close(p)
	open[p.id] = nil
	_close_menu(p.id)
end

on("menu_select", function(p, key)
	local state = open[p.id]
	if not state then
		return
	end

	local m = state.menu

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
on("client_disconnect", function(p)
	open[p.id] = nil
end)
