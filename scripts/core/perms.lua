-- Core: the `lua_perms` console command. Server console and rcon only - it is
-- the tool for the person who owns the box, not an in-game admin menu.
--
--   lua_perms groups
--   lua_perms list
--   lua_perms who 3
--   lua_perms check 3 admin.kick
--   lua_perms grant STEAM_0:1:12345 vip 30d
--   lua_perms grant STEAM_0:1:12345 shop.* map
--   lua_perms revoke STEAM_0:1:12345 vip
--   lua_perms reload | save
--
-- grant and revoke change the live rights immediately; they only touch the
-- file when you say `save` (or pass `save` as the last word).

local function sorted_names(t)
	local names = {}
	for name in pairs(t) do
		names[#names + 1] = name
	end
	table.sort(names)
	return names
end

local function describe_until(entry)
	if entry.until_ == nil then
		return "forever"
	end
	if entry.until_ == "map" then
		return "until the map ends (" .. tostring(entry.map) .. ")"
	end
	return "until " .. os.date("%Y-%m-%d %H:%M", entry.until_)
end

local subcommands = {}

function subcommands.groups(ctx)
	local groups = access.all_groups()
	for _, name in ipairs(sorted_names(groups)) do
		local g = groups[name]
		ctx.reply(("  %-12s weight %-5d inherit: %-20s allow: %s")
			:format(name, g.weight,
				#g.inherit > 0 and table.concat(g.inherit, ",") or "-",
				#g.allow > 0 and table.concat(g.allow, " ") or "-"))
		if #g.deny > 0 then
			ctx.reply(("  %-12s deny: %s"):format("", table.concat(g.deny, " ")))
		end
	end
end

function subcommands.list(ctx)
	local perms = access.permissions()
	local names = sorted_names(perms)
	if #names == 0 then
		return ctx.reply("no permissions declared - plugins can use access.declare() to show up here")
	end
	for _, node in ipairs(names) do
		local spec = perms[node]
		ctx.reply(("  %-28s %-16s %s%s"):format(node,
			spec.plugin and ("(" .. spec.plugin .. ")") or "",
			spec.desc or "",
			spec.default and (" [default: " .. tostring(spec.default) .. "]") or ""))
	end
end

function subcommands.users(ctx)
	local users = access.users()
	local keys = sorted_names(users)
	if #keys == 0 then
		return ctx.reply("no entries in data/users.lua")
	end
	for _, key in ipairs(keys) do
		local e = users[key]
		ctx.reply(("  %-24s groups: %-20s %s"):format(key,
			#e.groups > 0 and table.concat(e.groups, ",") or "-",
			describe_until(e)))
	end
end

function subcommands.who(ctx)
	local p, err = players.find(ctx.args[2])
	if not p then
		return ctx.reply(err)
	end

	ctx.reply(("%s (slot %d, %s)"):format(p:name(), p.id, p:steamid()))
	ctx.reply(("  groups: %s"):format(table.concat(p:groups(), ", ")))
	ctx.reply(("  weight: %d"):format(p:weight()))

	local entry = access.user(p:steamid()) or access.user("name:" .. p:name())
	if entry then
		ctx.reply(("  entry:  %s"):format(describe_until(entry)))
		if #entry.allow > 0 then
			ctx.reply("  allow:  " .. table.concat(entry.allow, " "))
		end
		if #entry.deny > 0 then
			ctx.reply("  deny:   " .. table.concat(entry.deny, " "))
		end
	else
		ctx.reply("  entry:  none, running on " .. table.concat(p:groups(), ", "))
	end
end

function subcommands.check(ctx)
	local p, err = players.find(ctx.args[2])
	if not p then
		return ctx.reply(err)
	end
	local node = ctx.args[3]
	if not node then
		return ctx.reply("usage: lua_perms check <player> <node>")
	end
	ctx.reply(("%s %s %s"):format(p:name(), p:can(node) and "CAN" or "cannot", node))
end

function subcommands.grant(ctx)
	local key, what, duration = ctx.args[2], ctx.args[3], ctx.args[4]
	if not key or not what then
		return ctx.reply("usage: lua_perms grant <steamid|name:nick> <group|node> [30d|map]")
	end

	-- A bare word that names a group means the group; anything else is taken
	-- as a permission node, so both live under one verb.
	local spec = access.group(what)
		and { groups = what, until_ = duration }
		or { allow = what, until_ = duration }

	local ok, err = access.grant(key, spec)
	if not ok then
		return ctx.reply("grant failed: " .. tostring(err))
	end

	access.invalidate()
	ctx.reply(("granted %s to %s%s"):format(what, key,
		duration and (" for " .. duration) or ""))
	ctx.reply("run `lua_perms save` to keep it across a restart")
end

function subcommands.revoke(ctx)
	local key, what = ctx.args[2], ctx.args[3]
	if not key then
		return ctx.reply("usage: lua_perms revoke <steamid|name:nick> [group|node]")
	end

	local ok, err
	if what == nil then
		ok, err = access.revoke(key)
	elseif access.group(what) then
		ok, err = access.revoke(key, what)
	else
		ok, err = access.revoke(key, nil, what)
	end

	if not ok then
		return ctx.reply("revoke failed: " .. tostring(err))
	end

	access.invalidate()
	ctx.reply(("revoked %s from %s"):format(what or "everything", key))
	ctx.reply("run `lua_perms save` to keep it across a restart")
end

function subcommands.save(ctx)
	local ok, err = access.save()
	ctx.reply(ok and "saved data/users.lua" or ("save failed: " .. tostring(err)))
end

function subcommands.reload(ctx)
	access.reload()
	access.invalidate()
	ctx.reply("reloaded data/groups.lua and data/users.lua")
end

cmd.add("lua_perms", function(ctx)
	local sub = subcommands[(ctx.args[1] or ""):lower()]
	if not sub then
		return ctx.reply("lua_perms: groups | list | users | who | check | grant | revoke | save | reload")
	end
	sub(ctx)
end, { source = "console" })
