-- Core: the command system. One cmd.add() covers four sources - the
-- dedicated server console/rcon, a player's own client console, chat
-- (!name), and team chat (say_team) - and a command may listen on any
-- combination of them.
--
--   cmd.add("heal", function(ctx)
--       local p = ctx.player          -- player object, or nil from the server console
--       local n = tonumber(ctx.args[1]) or 25
--       if p then p:health(p:health() + n) end
--       ctx.reply("healed +" .. n)    -- answers back where the command came from
--   end)
--
--   cmd.add("plant", fn, { source = "chat_team" })         -- team chat only
--   cmd.add("kick",  fn, { source = { "server", "chat" } }) -- both of these
--
-- Sources: "server", "console", "chat", "chat_team". Omitting source means
-- all four.
--
-- "server" is the dedicated console and rcon - no player, above all rights,
-- the way it always was. "console" is a connected player typing the command
-- into their own client console instead of chat: a real ctx.player, checked
-- against `perm` and immunity exactly like chat is. Mixing up the two turns
-- a debug-only admin command into something any player can type at home, or
-- the other way round locks a player command out of rcon - so pick the one
-- that actually matches who is meant to run it.
--
-- Rights come from core/access.lua and are declared right here, so a handler
-- never starts running for someone who may not use it:
--
--   cmd.add("slay", fn, { perm = "admin.slay", target = 1 })
--
-- `perm` is a permission node checked before the handler runs. `target = N`
-- says the Nth argument names a player: the router looks them up into
-- ctx.target and refuses when they outrank the caller. The server console
-- has no player object and passes both checks - it is already the highest
-- authority on the box; a player's own console is checked the same as chat.
--
-- The ctx table is the same shape an event handler gets: one table in, fields
-- out. There is no second calling convention to remember.
--
-- name can be a list instead of one string - aliases, all landing on the same
-- handler:
--
--   cmd.add({ "nomination", "nominate" }, fn)
--
-- opts.layout_alias adds one more alias per name: the same word typed in the
-- other keyboard layout ("nomination" -> "тщьштфешщт"), for the player who
-- forgot to switch back before typing. Off by default - most names have
-- nothing sensible to add.
--
--   cmd.add("maps", fn, { layout_alias = true })  -- "ьфзы" works too

-- The raw engine registrations, taken into locals and then cleared: a plugin
-- has no business reaching past cmd.add() to either primitive.
local register_server  = cmd._register_server
local register_console = cmd._register_console
cmd._register_server, cmd._register_console = nil, nil

local color = require("color")

-- QWERTY <-> ЙЦУКЕН (Windows), letters only - enough for a command name.
-- opts.layout_alias on cmd.add uses this to add the name someone gets when
-- they type it without checking which layout they're in. One direction only
-- (Latin -> Cyrillic): command names in this codebase are English, and
-- there is nothing here to translate the other way.
local LAYOUT_EN = "qwertyuiopasdfghjklzxcvbnm"
local LAYOUT_RU = {
	"й", "ц", "у", "к", "е", "н", "г", "ш", "щ", "з",
	"ф", "ы", "в", "а", "п", "р", "о", "л", "д",
	"я", "ч", "с", "м", "и", "т", "ь",
}

local layout_map = {}
for i = 1, #LAYOUT_EN do
	layout_map[LAYOUT_EN:sub(i, i)] = LAYOUT_RU[i]
end

-- "nomination" -> "тщьштфешщт". nil if the name has anything outside a-z -
-- nothing to remap, and a half-transliterated name would just be confusing.
local function to_ru_layout(name)
	local out = {}
	for i = 1, #name do
		local mapped = layout_map[name:sub(i, i)]
		if not mapped then
			return nil
		end
		out[i] = mapped
	end
	return table.concat(out)
end

local prefixes = { "!", "/" }
local VALID = { server = true, console = true, chat = true, chat_team = true }

-- name -> { fn = fn, server = bool, console = bool, chat = bool, chat_team = bool }
local registry = {}

local function parse_sources(source)
	local set = {}
	if source == nil then
		set.server, set.console, set.chat, set.chat_team = true, true, true, true
	elseif type(source) == "string" then
		assert(VALID[source], "cmd.add: unknown source '" .. source .. "'")
		set[source] = true
	elseif type(source) == "table" then
		for _, s in ipairs(source) do
			assert(VALID[s], "cmd.add: unknown source '" .. tostring(s) .. "'")
			set[s] = true
		end
	else
		error("cmd.add: source must be a string or a list of strings")
	end
	return set
end

-- players.find("#12" | "kotyarakryt" | "3") -> the player, or nil plus why not.
-- #N is a userid, a bare number is a slot, anything else is matched against
-- names case-insensitively and has to be unambiguous.
function players.find(token)
	if type(token) ~= "string" or token == "" then
		return nil, "no player given"
	end

	local userid = token:match("^#(%d+)$")
	if userid then
		userid = tonumber(userid)
		for _, p in ipairs(players.list()) do
			if p:userid() == userid then
				return p
			end
		end
		return nil, "nobody with userid " .. userid
	end

	local slot = tonumber(token)
	if slot then
		local p = players.get(slot)
		return p, p and nil or ("nobody in slot " .. slot)
	end

	local needle, found = token:lower(), nil
	for _, p in ipairs(players.list()) do
		if p:name():lower():find(needle, 1, true) then
			if found then
				return nil, ("'%s' matches more than one player"):format(token)
			end
			found = p
		end
	end

	return found, found and nil or ("no player matches '" .. token .. "'")
end

local function split_args(rest)
	local args = {}
	for word in rest:gmatch("%S+") do
		args[#args + 1] = word
	end
	return args
end

-- Everything that has to happen between "the command was typed" and "the
-- handler runs": rights, target lookup, immunity. One place, so console and
-- chat can never drift apart on who is allowed what.
local function run(entry, ctx)
	ctx.can = function(node) return access.can(ctx.player, node) end

	if entry.perm and not access.can(ctx.player, entry.perm) then
		if access.config.denied then
			ctx.reply(access.config.denied)
		end
		return
	end

	if entry.target then
		local target, err = players.find(ctx.args[entry.target])
		if not target then
			return ctx.reply(err)
		end
		if entry.immunity and not access.outranks(ctx.player, target) then
			if access.config.immune then
				ctx.reply(access.config.immune:format(target:name()))
			end
			return
		end
		ctx.target = target
	end

	entry.fn(ctx)
end

-- name is a string, or a list of names/aliases - "nomination" and "nominate"
-- both landing on the same handler. opts.layout_alias (default false) adds
-- one more alias per name automatically: what it reads like typed in the
-- other keyboard layout, for the player who forgot to switch back.
function cmd.add(name, fn, opts)
	assert(type(name) == "string" or type(name) == "table",
		"cmd.add: name must be a string or a list of strings")
	assert(type(fn) == "function", "cmd.add: handler must be a function")

	local names = type(name) == "table" and name or { name }
	assert(#names > 0, "cmd.add: name list is empty")

	local all_names = {}
	for _, n in ipairs(names) do
		assert(type(n) == "string", "cmd.add: name must be a string or a list of strings")
		all_names[#all_names + 1] = n:lower()
	end

	if opts and opts.layout_alias then
		-- Appended after the loop above, not inside it: a layout alias of a
		-- layout alias would just be the original name again.
		for i = 1, #all_names do
			local swapped = to_ru_layout(all_names[i])
			if swapped and swapped ~= all_names[i] then
				all_names[#all_names + 1] = swapped
			end
		end
	end

	local sources = parse_sources(opts and opts.source)
	local entry = {
		fn        = fn,
		perm      = opts and opts.perm,
		target    = opts and opts.target,
		immunity  = not (opts and opts.immunity == false),
		server    = sources.server or false,
		console   = sources.console or false,
		chat      = sources.chat or false,
		chat_team = sources.chat_team or false,
	}
	assert(entry.perm == nil or type(entry.perm) == "string",
		"cmd.add: perm must be a permission node")

	-- Every name/alias is its own registry entry, its own engine registration
	-- where one applies, and its own ctx.name - a handler that echoes the name
	-- back shows whichever one was actually typed.
	for _, cname in ipairs(all_names) do
		registry[cname] = entry

		if sources.server then
			register_server(cname, function(args)
				-- No player: rights are skipped, but a `target` still gets looked
				-- up so server and chat handlers can share one body.
				run(registry[cname] or entry, {
					name   = cname,
					source = "server",
					player = nil,
					args   = args,
					-- Neither the dedicated console nor rcon render chat markup -
					-- a reply written with {green}/{team}/... for chat would show
					-- the raw tags otherwise.
					reply  = function(text) print(color.strip_chat(text)) end,
				})
			end)
		end

		if sources.console then
			register_console(cname, function(id, args)
				-- A real player: perm and immunity apply exactly like chat.
				local p = players.get(id)
				run(registry[cname] or entry, {
					name   = cname,
					source = "console",
					player = p,
					args   = args,
					-- A player's own console is just as colour-blind as the
					-- server one - same strip.
					reply  = function(text) if p then p:console(color.strip_chat(text)) end end,
				})
			end)
		end
	end
end

-- cmd.remove(name) - drop a command. The engine-side registration (server
-- console keeps our name pointer forever either way) stays, but stops
-- resolving to anything.
function cmd.remove(name)
	assert(type(name) == "string", "cmd.remove: name must be a string")
	local existed = registry[name:lower()] ~= nil
	registry[name:lower()] = nil
	return existed
end

-- cmd.list() -> sorted array of registered names, for `lua_cmds` and for a
-- plugin that wants to print a help screen.
function cmd.list()
	local names = {}
	for name in pairs(registry) do
		names[#names + 1] = name
	end
	table.sort(names)
	return names
end

local function strip_prefix(text)
	for _, prefix in ipairs(prefixes) do
		if text:sub(1, #prefix) == prefix then
			return text:sub(#prefix + 1)
		end
	end
	return nil
end

hook.add("player:chat", "core.command_router", function(e)
	local body = strip_prefix(e.text)
	if not body then
		return
	end

	local name, rest = body:match("^(%S+)%s*(.*)$")
	if not name then
		return
	end

	local entry = registry[name:lower()]
	if not entry then
		return
	end

	-- Same prefix, two channels: honour which ones this command opted into.
	local source = e.team and "chat_team" or "chat"
	if not entry[source] then
		return
	end

	local p = e.player

	run(entry, {
		name   = name:lower(),
		source = source,
		player = p,
		args   = split_args(rest),
		reply  = function(text) p:chat(text) end,
	})

	-- A recognised command never shows up in chat.
	e:cancel()
end)
