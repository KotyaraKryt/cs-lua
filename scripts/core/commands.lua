-- Core: the command system. One command() covers three sources - the server
-- console/rcon, chat (!name), and team chat (say_team) - and a command may
-- listen on any combination of them.
--
--   command("heal", function(ctx)
--       local p = ctx.player          -- player object, or nil from the console
--       local n = tonumber(ctx.args[1]) or 25
--       if p then p:health(p:health() + n) end
--       ctx.reply("healed +" .. n)    -- answers back where the command came from
--   end)
--
--   command("plant", fn, { source = "chat_team" })          -- team chat only
--   command("kick",  fn, { source = { "console", "chat" } }) -- both of these
--
-- Sources: "console", "chat", "chat_team". Omitting source means all three.
--
-- Rights come from core/access.lua and are declared right here, so a handler
-- never starts running for someone who may not use it:
--
--   command("slay", fn, { perm = "admin.slay", target = 1 })
--
-- `perm` is a permission node checked before the handler runs. `target = N`
-- says the Nth argument names a player: the router looks them up into
-- ctx.target and refuses when they outrank the caller. The console has no
-- player object and passes both checks - it is already the highest authority
-- on the box.

local prefixes = { "!", "/" }
local VALID = { console = true, chat = true, chat_team = true }

-- name -> { fn = fn, console = bool, chat = bool, chat_team = bool }
local registry = {}

local function parse_sources(source)
	local set = {}
	if source == nil then
		set.console, set.chat, set.chat_team = true, true, true
	elseif type(source) == "string" then
		assert(VALID[source], "command: unknown source '" .. source .. "'")
		set[source] = true
	elseif type(source) == "table" then
		for _, s in ipairs(source) do
			assert(VALID[s], "command: unknown source '" .. tostring(s) .. "'")
			set[s] = true
		end
	else
		error("command: source must be a string or a list of strings")
	end
	return set
end

-- find_player("#12" | "kotyarakryt" | "3") -> the player, or nil plus why not.
-- #N is a userid, a bare number is a slot, anything else is matched against
-- names case-insensitively and has to be unambiguous.
function find_player(token)
	if type(token) ~= "string" or token == "" then
		return nil, "no player given"
	end

	local userid = token:match("^#(%d+)$")
	if userid then
		userid = tonumber(userid)
		for _, p in ipairs(players()) do
			if p:userid() == userid then
				return p
			end
		end
		return nil, "nobody with userid " .. userid
	end

	local slot = tonumber(token)
	if slot then
		local p = player(slot)
		return p, p and nil or ("nobody in slot " .. slot)
	end

	local needle, found = token:lower(), nil
	for _, p in ipairs(players()) do
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
		local target, err = find_player(ctx.args[entry.target])
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

function command(name, fn, opts)
	assert(type(name) == "string", "command: name must be a string")
	assert(type(fn) == "function", "command: handler must be a function")
	name = name:lower()

	local sources = parse_sources(opts and opts.source)
	local entry = {
		fn        = fn,
		perm      = opts and opts.perm,
		target    = opts and opts.target,
		immunity  = not (opts and opts.immunity == false),
		console   = sources.console or false,
		chat      = sources.chat or false,
		chat_team = sources.chat_team or false,
	}
	assert(entry.perm == nil or type(entry.perm) == "string",
		"command: perm must be a permission node")
	registry[name] = entry

	-- The console side needs an engine registration; do it once, only if this
	-- command actually listens there.
	if sources.console then
		_server_command(name, function(args)
			-- No player: rights are skipped, but a `target` still gets looked
			-- up so console and chat handlers can share one body.
			run(registry[name] or entry, {
				name   = name,
				source = "console",
				player = nil,
				args   = args,
				reply  = function(text) print(text) end,
			})
		end)
	end
end

local function strip_prefix(text)
	for _, prefix in ipairs(prefixes) do
		if text:sub(1, #prefix) == prefix then
			return text:sub(#prefix + 1)
		end
	end
	return nil
end

on("player_chat", function(p, text, team)
	local body = strip_prefix(text)
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
	local source = team and "chat_team" or "chat"
	if not entry[source] then
		return
	end

	run(entry, {
		name   = name:lower(),
		source = source,
		player = p,
		args   = split_args(rest),
		reply  = function(text) p:chat(text) end,
	})
	return false        -- swallow: a recognised command never shows in chat
end)
