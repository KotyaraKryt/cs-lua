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
-- Not a list of choices - a game board, a free-form panel? menu.custom() lets
-- you draw it yourself: hand back the text and the live keys, get the pressed
-- key. A plain yes/no is menu.confirm(). menu.raw_show() is the bare
-- AMXModX-style call under all of it.
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
--
-- opts.layout restyles the generated lines - the "1. " prefix, the title, the
-- Exit/Back/Next labels:
--
--   menu.new("Меню", { layout = { prefix = "[%d] ", exit_label = "Выйти" } })

local color = require("color")

-- Raw panel drawing, taken into locals and cleared: menu.new() is what a
-- plugin sees by default...
local show_panel  = menu._show
local close_panel = menu._close
local panel_open  = menu._is_open
menu._show, menu._close, menu._is_open = nil, nil, nil

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

-- A prefix built by hand (opts.layout.prefix, or a per-item one) can carry its
-- own raw codes, same as any other menu text. If it ends on one, that is the
-- colour the client is actually sitting in - even though it never went
-- through paint() and the "only send it if it changed" tracker never saw it.
-- Reads that back out so the next paint() call knows where it really stands,
-- instead of repainting nothing because its own copy of `cur` is stale.
local RAW_CODES = { w = true, r = true, y = true, d = true }

local function trailing_raw_code(s)
	local found
	for i = 1, #s - 1 do
		if s:sub(i, i) == "\\" and RAW_CODES[s:sub(i + 1, i + 1)] then
			found = s:sub(i, i + 1)
		end
	end
	return found
end

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

-- opts.layout: relabels and reshapes the lines menu.new draws for you. Every
-- field is optional - leave it out and the panel looks exactly as it did.
--   prefix  : the "1. " in front of an item. A string with %d, or a
--             function(slot) -> string. Used for the 9/0 nav keys too.
--   title   : a string with %s, or function(text) -> string.
--   counter : page marker glued after the title once there is more than one
--             page. A string with two %d (page, total), or true for " [%d/%d]".
--   exit_label / back_label / next_label : the nav labels, "Exit" / "Back" /
--             "Next" by default.
--
--   menu.new("Меню", { layout = {
--       prefix = "[%d] ", exit_label = "Выйти", counter = true,
--   }})   -->   Меню [1/2]
--             [1] Первый
--             [0] Выйти
local function as_fmt(v, default)
	v = v or default
	if type(v) == "function" then
		return v
	end
	return function(...) return (v):format(...) end
end

local function menu_layout(v)
	v = v or {}
	local counter = v.counter
	if counter == true then
		counter = " [%d/%d]"
	end
	return {
		prefix  = as_fmt(v.prefix or v.number, "%d. "),   -- `number` still accepted
		title   = as_fmt(v.title, "%s"),
		counter = counter and as_fmt(counter),
		exit    = v.exit_label or v.exit or "Exit",       -- `exit`/`back`/`next` too
		back    = v.back_label or v.back or "Back",
		next    = v.next_label or v.next or "Next",
	}
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

local Custom = {}
Custom.__index = Custom

-- Who is looking at what, so a reply lands on the right menu and page.
local open = {}

-- Drops whatever panel a player has on screen, tracked or not (menu.new,
-- custom, or raw_show all go through the same show_panel/close_panel pair).
local function close_any(p)
	open[p.id] = nil
	close_panel(p.id)
end

-- players.broadcast has no :connected() or .id in the per-player sense - a
-- caller that wants "everyone" hands it in instead of walking players.list()
-- itself. Rejects it where a single player is required (is_open queries):
-- "is it open" only means something for one specific screen.
local function each_connected(fn)
	for _, pl in ipairs(players.list()) do
		fn(pl)
	end
end

local function reject_broadcast(p, who)
	if p == players.broadcast then
		error(who .. ": needs a single player, not players.broadcast", 3)
	end
end

-- menu.is_open(p) -> is anything at all on their screen right now (menu.new,
-- menu.custom, menu.confirm, or a raw_show panel). Backed by the engine-side
-- state, not the open[] table below, so a raw_show panel counts too.
function menu.is_open(p)
	reject_broadcast(p, "menu.is_open")
	return panel_open(p.id)
end

-- menu.close_all([p]) -> close whatever is on screen, no matter whose menu it
-- is. No argument (or players.broadcast) hits every connected player; a
-- single player closes just that one screen.
function menu.close_all(p)
	if p == nil or p == players.broadcast then
		return each_connected(close_any)
	end
	close_any(p)
end

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

-- A menu you draw yourself. No items - you hand back the panel text and which
-- keys are live every time it is shown, and get the pressed key back. For a
-- game board, a confirm box, anything that is not a list of choices.
--
--   local board = menu.custom({
--       render = function(p)
--           return draw(game), free_cells(game)   -- text, list of keys 1..9
--       end,
--       on_key = function(p, key)
--           play(game, key)
--           board:show(p)                          -- redraw for the next move
--       end,
--   })
--   board:show(p)
--
-- render(player) -> text, keys
--   text : the whole panel, "\n" between lines, AMX colour codes if you want
--   keys : which keys the player may press - a list like { 1, 2, 3, 0 } or a
--          raw bitmask. Nil means all of 1-9 and 0. The number in the list is
--          the key on the keyboard; "0" is the zero key.
-- on_key(player, key) is called with that same number (0-9). The panel is
-- gone by then; call :show() again to keep it up.
local KEYMASK_ALL = 0x3FF

local function keymask(keys)
	if type(keys) == "number" then
		return keys
	end

	local mask = 0
	for _, k in ipairs(keys) do
		k = tonumber(k)
		if k == 0 then
			mask = mask + 2 ^ (KEY_0 - 1)
		elseif k and k >= 1 and k <= 9 then
			mask = mask + 2 ^ (k - 1)
		end
	end
	return mask
end

function menu.custom(opts)
	opts = opts or {}
	if type(opts.render) ~= "function" then
		error("menu.custom: opts.render must be a function", 2)
	end

	return setmetatable({
		render = opts.render,
		on_key = opts.on_key,
		time   = opts.time or -1,
	}, Custom)
end

function Custom:show(p)
	if p == players.broadcast then
		return each_connected(function(pl) self:show(pl) end)
	end

	if not p or not p:connected() then
		return
	end

	local text, keys = self.render(p)
	if type(text) ~= "string" then
		error("menu.custom: render must return the panel text as a string", 2)
	end

	open[p.id] = { custom = self }
	show_panel(p.id, keys == nil and KEYMASK_ALL or keymask(keys), self.time, text)
end

-- Only touches a player whose screen is actually showing this panel right
-- now - a broadcast close must not swat some other menu they happen to have
-- open instead.
function Custom:close(p)
	if p == players.broadcast then
		return each_connected(function(pl)
			if self:is_open(pl) then close_any(pl) end
		end)
	end
	close_any(p)
end

-- Is this exact panel (not just some panel) on screen for them right now.
function Custom:is_open(p)
	reject_broadcast(p, "menu:is_open")
	local state = open[p.id]
	return state ~= nil and state.custom == self
end

-- menu.confirm(p, text, on_yes[, on_no][, opts]) - a yes/no box, the most
-- common two-line menu there is. opts.yes / opts.no relabel the choices
-- ("Купить" / "Отмена"), opts.timeout sets the time on screen.
--
--   menu.confirm(p, "Выдать VIP игроку " .. t:name() .. "?", function()
--       grant_vip(t)
--   end)
function menu.confirm(p, text, on_yes, on_no, opts)
	if type(on_no) == "table" then
		opts, on_no = on_no, nil
	end
	opts = opts or {}

	local box = menu.custom({
		time = opts.timeout or -1,
		render = function()
			return ("%s\n\n\\r1. \\w%s\n\\r2. \\w%s"):format(
				text, opts.yes or "Да", opts.no or "Нет"), { 1, 2 }
		end,
		on_key = function(pl, key)
			if key == 1 then
				if on_yes then on_yes(pl) end
			elseif on_no then
				on_no(pl)
			end
		end,
	})
	box:show(p)
	return box
end

-- opts, all optional:
--   closable : key 0 closes the menu. Default true. (was `exit`)
--   timeout  : seconds on screen, -1 = until answered. Default -1. (was `time`)
--   per_page : selectable items before it splits into pages. Default 8, max 8.
--              Set 5 and a 6-item menu already paginates.
--   on_close : called with the player when they close it. (was `on_exit`)
--   on_select: called with (player, item) for any pick, before the item's own
--              handler - a single switch instead of a function per item.
--   color    : see the colours block above.
--   layout   : labels and prefixes, see menu_layout above.
--
-- items can also be handed in up front instead of :add()'ing them one by one:
--   menu.new("Оружие", { items = {
--       { "AK-47", give_ak },
--       { "AWP", give_awp, color = "red" },
--       "---",                       -- a bare string is a separator
--   }})
function menu.new(title, opts)
	opts = opts or {}
	local function pick(new, old) local v = opts[new]; if v == nil then v = opts[old] end; return v end

	local closable = pick("closable", "exit")
	local per_page = tonumber(pick("per_page", "page_size")) or ITEMS_PER_PAGE

	local m = setmetatable({
		title     = title or "",
		items     = {},
		closable  = closable ~= false,
		time      = pick("timeout", "time") or -1,
		per_page  = math.max(1, math.min(ITEMS_PER_PAGE, math.floor(per_page))),
		on_close  = pick("on_close", "on_exit"),
		on_select = opts.on_select,
		colors    = menu_colors(opts.color),
		layout    = menu_layout(opts.layout),
	}, Menu)

	for _, row in ipairs(opts.items or {}) do
		if type(row) == "string" then
			m:separator(row == "---" and "" or row)
		elseif row.sep ~= nil then
			m:separator(row.sep == true and "" or row.sep)
		else
			m:add(row[1] or row.text, row[2] or row.handler, row)
		end
	end

	return m
end

-- add(text, handler, opts) -> item
--
-- `handler` is function(player, item), OR another menu.new menu - picking the
-- row then opens it, with key 0 walking back to this one (see Menu:submenu).
--
-- text and opts.disabled may each be a function(player) resolved every time
-- the menu is shown - one menu object then serves every player and keeps
-- itself current (a live count in the label, a row that greys out when the
-- player can't afford it), no rebuild needed.
--
-- opts, all optional:
--   disabled : true, or function(player) -> bool. Shown, not pressable.
--   prefix   : its own "1. " - a string, or function(slot). Good for a locked
--              row: { disabled = true, prefix = "[#] " }. Settable later too.
--   value    : anything you want to hang on the item.
--   color    : see the colours block above.
function Menu:add(text, handler, opts)
	opts = opts or {}
	local item = {
		text     = text,
		handler  = handler,
		disabled = opts.disabled or false,
		value    = opts.value,
		colors   = item_colors(opts.color),
		prefix   = opts.prefix or opts.number,   -- `number` still accepted
	}
	self.items[#self.items + 1] = item
	return item
end

-- A line with no key - a heading or a gap between groups. text is optional
-- and may be a function(player).
function Menu:separator(text)
	local item = { sep = true, text = text or "" }
	self.items[#self.items + 1] = item
	return item
end

-- submenu(text, child, opts) -> item. Sugar for add() with a menu as the
-- handler; reads better and hints that key 0 in `child` returns here rather
-- than closing. The chain is unlimited - a submenu can hold its own submenus.
function Menu:submenu(text, child, opts)
	return self:add(text, child, opts)
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

-- Splits items into page windows { first, last } by raw index, counting only
-- selectable rows toward per - separators ride along for free and never push
-- an item to the next page on their own.
local function paginate(items, per)
	local pages, first, n = {}, 1, 0
	for i = 1, #items do
		if not items[i].sep then
			n = n + 1
			if n >= per then
				pages[#pages + 1] = { first, i }
				first, n = i + 1, 0
			end
		end
	end
	if first <= #items then
		pages[#pages + 1] = { first, #items }
	end
	if #pages == 0 then
		pages[1] = { 1, 0 }
	end
	return pages
end

-- Builds the panel text and the key mask for one page. Returns the clamped
-- page as the last value so show() records where the player actually landed.
local function render(self, page, p, has_parent)
	local windows = paginate(self.items, self.per_page)
	local pages = #windows
	page = math.max(0, math.min(page, pages - 1))
	local first, last = windows[page + 1][1], windows[page + 1][2]

	local c = self.colors
	local L = self.layout

	-- A function(player) field is resolved here, once per show.
	local function val(v)
		return type(v) == "function" and v(p) or v
	end

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

	-- A raw code baked into a hand-written prefix (see trailing_raw_code above)
	-- is invisible to paint() unless something tells `cur` it happened - this
	-- is that something, wrapped around every place a prefix string is built.
	local function resync(s)
		local raw = trailing_raw_code(s)
		if raw then
			cur = raw
		end
		return s
	end

	-- The "1. " in front of a row: the item's own prefix if it set one, else
	-- the menu-wide layout.prefix.
	local function prefix(item, slot)
		local n = item.prefix
		if n == nil then
			return resync(L.prefix(slot))
		end
		return resync(type(n) == "function" and n(slot) or n)
	end

	local title = L.title(tostring(val(self.title)))
	if L.counter and pages > 1 then
		title = title .. L.counter(page + 1, pages)
	end

	local lines = { paint(c.title) .. title, "" }
	local keys  = 0
	local slots = {}          -- key number -> item index

	local slot = 0
	for i = first, last do
		local item = self.items[i]
		local ic = item.colors
		local text = tostring(val(item.text))

		if item.sep then
			lines[#lines + 1] = paint(c.disabled) .. text
		elseif val(item.disabled) then
			slot = slot + 1
			-- Shown, but the key is not in the mask, so it cannot be pressed.
			-- The whole line goes grey, number included.
			lines[#lines + 1] = paint(c.disabled) .. prefix(item, slot) .. text
		else
			slot = slot + 1
			lines[#lines + 1] = paint(ic and ic.number or c.number)
				.. prefix(item, slot)
				.. paint(ic and ic.text or c.text)
				.. text
			keys = keys + 2 ^ (slot - 1)
			slots[slot] = i
		end
	end

	local has_next = page + 1 < pages
	local has_back = page > 0

	lines[#lines + 1] = ""

	local nav_num = c.nav or c.number

	if has_next then
		lines[#lines + 1] = paint(nav_num) .. resync(L.prefix(KEY_9)) .. paint(c.text) .. L.next
		keys = keys + 2 ^ (KEY_9 - 1)
	end

	if has_back or has_parent then
		-- Key 0 walks back: a page first, then out to the parent menu.
		lines[#lines + 1] = paint(nav_num) .. resync(L.prefix(0)) .. paint(c.text) .. L.back
		keys = keys + 2 ^ (KEY_0 - 1)
	elseif self.closable then
		lines[#lines + 1] = paint(nav_num) .. resync(L.prefix(0)) .. paint(c.text) .. L.exit
		keys = keys + 2 ^ (KEY_0 - 1)
	end

	return table.concat(lines, "\n"), keys, slots, has_next, has_back, page
end

-- show(player [, page [, back]])
-- `back` is an internal frame { menu, page, back } - the submenu machinery
-- fills it in; callers pass just the player (and maybe a page).
--
-- player can be players.broadcast to open it for everyone connected. Each
-- gets their own render() pass - text/disabled can be function(player), so
-- one player's panel can legitimately read different from another's.
function Menu:show(p, page, back)
	if p == players.broadcast then
		return each_connected(function(pl) self:show(pl, page, back) end)
	end

	page = page or 0
	if not p or not p:connected() then
		return
	end

	local text, keys, slots, has_next, has_back, landed =
		render(self, page, p, back ~= nil)

	open[p.id] = {
		menu     = self,
		page     = landed,
		slots    = slots,
		has_next = has_next,
		has_back = has_back,
		back     = back,
	}

	show_panel(p.id, keys, self.time, text)
end

-- player can be players.broadcast: closes this menu for everyone who
-- currently has it on screen. Someone looking at a different menu (or
-- another plugin's) is left alone.
function Menu:close(p)
	if p == players.broadcast then
		return each_connected(function(pl)
			if self:is_open(pl) then close_any(pl) end
		end)
	end
	close_any(p)
end

-- Is this exact menu (this page or a submenu of it does NOT count) on their
-- screen right now - not just some menu, this one.
function Menu:is_open(p)
	reject_broadcast(p, "menu:is_open")
	local state = open[p.id]
	return state ~= nil and state.menu == self
end

hook.add("menu:select", "core.menu_select", function(e)
	local p = e.player
	local state = open[p.id]
	if not state then
		return
	end

	local key = e.key

	-- A draw-it-yourself menu: hand the pressed key straight back, "0" as 0.
	if state.custom then
		open[p.id] = nil
		local c = state.custom
		if c.on_key then
			c.on_key(p, key == KEY_0 and 0 or key)
		end
		return
	end

	local m = state.menu

	if key == KEY_9 and state.has_next then
		return m:show(p, state.page + 1, state.back)
	end

	if key == KEY_0 then
		if state.has_back then
			return m:show(p, state.page - 1, state.back)
		end

		open[p.id] = nil

		if state.back then                    -- a submenu: walk out to the parent
			return state.back.menu:show(p, state.back.page, state.back.back)
		end

		local on_close = m.on_close or m.on_exit   -- `m.on_exit = fn` still works
		if on_close then
			on_close(p)
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
	if not item then
		return
	end
	if m.on_select then
		m.on_select(p, item)
	end

	if getmetatable(item.handler) == Menu then
		-- The row is a submenu: open it, key 0 comes back here.
		item.handler:show(p, 0, { menu = m, page = state.page, back = state.back })
	elseif type(item.handler) == "function" then
		item.handler(p, item)
	end
end)

-- A player who leaves takes their menu state with them.
hook.add("client:disconnect", "core.menu_cleanup", function(e)
	open[e.player.id] = nil
end)
