-- Core: the console side of the hook system.
--
-- hook.add / hook.remove / hook.run / hook.list come from the module itself.
-- What is missing from C is a way to look at the result, which is the first
-- thing you want when a handler is not firing.
--
--   lua_hooks                 everything, grouped by event
--   lua_hooks player_hurt     one event
--
-- The order shown is the order handlers run in, which is registration order,
-- which is plugin load order (alphabetical). That matters for player_hurt:
-- the first handler to cancel ends the chain.

local function group(rows)
	local by_event, order = {}, {}

	for _, row in ipairs(rows) do
		if not by_event[row.event] then
			by_event[row.event] = {}
			order[#order + 1] = row.event
		end
		local list = by_event[row.event]
		list[#list + 1] = row
	end

	table.sort(order)
	return by_event, order
end

cmd.add("lua_hooks", function(ctx)
	local want = ctx.args[1]
	local rows = hook.list(want)

	if #rows == 0 then
		return ctx.reply(want
			and ("nothing listens for '" .. want .. "'")
			or "no handlers registered")
	end

	local by_event, order = group(rows)

	for _, event in ipairs(order) do
		ctx.reply(("  %s"):format(event))
		for _, row in ipairs(by_event[event]) do
			ctx.reply(("      %-24s %s"):format(row.plugin, row.id))
		end
	end
end, { source = "server" })
