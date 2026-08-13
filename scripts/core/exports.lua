-- Core: plugins calling into each other.
--
--   -- in admin_manager
--   export("open", function(p) ui.root(p) end)
--
--   -- anywhere else
--   local admin = import("admin_manager")    -- errors if it is not loaded
--   local admin = optional("admin_manager")  -- nil instead, for a soft dep
--   admin.open(p)
--
-- This is the "call something that plugin owns" half. The other half - "tell
-- whoever cares that something happened" - is hook.run/hook.add, the same pair
-- engine events use. There is no separate plugin-event system:
--
--   hook.run("admin.rights_changed", { target = p, what = "give" })
--   hook.add("admin.rights_changed", "bans.sync", function(e) ... end)
--
-- Ownership is not guessed from the call stack: plugin.id() comes from the
-- engine and names the plugin whose code is running right now.

local exports_registry = {}		-- [plugin] = { [name] = { fn, version } }

-- The core layer has no plugin id of its own. Nothing stops it from exporting,
-- so give it a name rather than let it collide with a plugin called nil.
local function owner()
	return plugin.id() or "core"
end

-- 5.1 has no table.pack, and an export may legitimately return nils in the
-- middle of its results.
local function pack(...)
	return { n = select("#", ...), ... }
end

-- export("open", fn) or export("open", fn, { version = 2, override = true })
function export(name, fn, opts)
	opts = opts or {}
	assert(type(name) == "string", "export: name must be a string")
	assert(type(fn) == "function", "export: value must be a function")

	local id = owner()
	local list = exports_registry[id] or {}
	exports_registry[id] = list

	if list[name] and not opts.override then
		error(("export '%s.%s' is already registered"):format(id, name), 2)
	end

	list[name] = { fn = fn, version = opts.version or 1 }
end

-- The table import() hands back. Every lookup goes through the registry, so a
-- caller can never end up holding a function from a plugin that has since been
-- unloaded - it gets a clear error instead.
local function make_proxy(id)
	if not exports_registry[id] then
		return nil
	end

	return setmetatable({}, {
		__index = function(_, key)
			local list = exports_registry[id]
			if not list or not list[key] then
				return nil
			end

			return function(...)
				local current = exports_registry[id]
				local entry = current and current[key]
				if not entry then
					error(("export '%s.%s' is no longer available"):format(id, key), 2)
				end

				-- pcall so a broken export cannot take the caller down with it,
				-- but the message names whose fault it was.
				local r = pack(pcall(entry.fn, ...))
				if not r[1] then
					error(("[export %s.%s] %s"):format(id, key, tostring(r[2])), 2)
				end
				return unpack(r, 2, r.n)
			end
		end,

		__newindex = function()
			error("exports proxy is read-only", 2)
		end,

		__tostring = function()
			return "exports<" .. id .. ">"
		end,
	})
end

-- A hard dependency: the plugin must be there. Failing at load time beats a nil
-- call somewhere deep in a handler an hour later.
function import(id)
	local proxy = make_proxy(id)
	if not proxy then
		error(("plugin '%s' has no exports (or is not loaded)"):format(id), 2)
	end
	return proxy
end

-- A soft dependency: nil when the plugin is absent, so the caller can degrade.
function optional(id)
	return make_proxy(id)
end

-- A reloaded plugin re-runs its export() calls, and the second one would hit
-- the "already registered" guard. Dropping its entries as it goes is what
-- makes `lua_reload <plugin>` work without every plugin passing override.
--
-- e.plugin is nil when the whole state is going away; there is nothing to
-- clean up in that case, the registry dies with it.
hook.add("plugin_unload", "core.exports_cleanup", function(e)
	if e.plugin then
		exports_registry[e.plugin] = nil
	end
end)

--------------------------------------------------------------------------
-- debugging
--------------------------------------------------------------------------

local function sorted_keys(t)
	local keys = {}
	for key in pairs(t) do
		keys[#keys + 1] = key
	end
	table.sort(keys)
	return keys
end

-- `lua_exports` in the server console: who publishes what. The first thing to
-- run when import() says a plugin has no exports.
cmd.add("lua_exports", function(ctx)
	local plugins = sorted_keys(exports_registry)
	if #plugins == 0 then
		return ctx.reply("nothing exported")
	end

	for _, id in ipairs(plugins) do
		local names = {}
		for _, name in ipairs(sorted_keys(exports_registry[id])) do
			names[#names + 1] = ("%s (v%d)")
				:format(name, exports_registry[id][name].version)
		end
		ctx.reply(("  %-20s %s"):format(id, table.concat(names, ", ")))
	end
end, { source = "console" })
