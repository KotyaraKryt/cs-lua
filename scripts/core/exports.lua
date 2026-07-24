-- Core: plugins talking to each other.
--
-- Two independent mechanisms, answering different questions.
--
--   export/import  "call something that plugin owns"
--
--     -- in admin_manager
--     export("open", function(p) ui.root(p) end)
--
--     -- anywhere else
--     local admin = import("admin_manager")    -- errors if it is not loaded
--     local admin = optional("admin_manager")  -- nil instead, for a soft dep
--     admin.open(p)
--
--   emit/on_export  "tell whoever cares that something happened"
--
--     emit("admin.rights_changed", target, "give", actor)
--     on_export("admin.rights_changed", "Bans.Sync", function(target, what) end)
--
-- Ownership is not guessed from the call stack: plugin_id() comes from the
-- engine and names the plugin whose code is running right now, so a bare
-- single-file plugin is identified exactly like a folder one.

local exports_registry = {}		-- [plugin] = { [name] = { fn, version } }
local event_listeners  = {}		-- [event]  = list of listeners

-- The core layer has no plugin id of its own. Nothing stops it from exporting,
-- so give it a name rather than let it collide with a plugin called nil.
local function owner()
	return plugin_id() or "core"
end

-- 5.1 has no table.pack, and an export may legitimately return nils in the
-- middle of its results.
local function pack(...)
	return { n = select("#", ...), ... }
end

--------------------------------------------------------------------------
-- export / import
--------------------------------------------------------------------------

-- export("open", fn) or export("open", fn, { version = 2, override = true })
function export(name, fn, opts)
	opts = opts or {}
	assert(type(name) == "string", "export: name must be a string")
	assert(type(fn) == "function", "export: value must be a function")

	local plugin = owner()
	local list = exports_registry[plugin] or {}
	exports_registry[plugin] = list

	if list[name] and not opts.override then
		error(("export '%s.%s' is already registered"):format(plugin, name), 2)
	end

	list[name] = { fn = fn, version = opts.version or 1 }
end

-- The table import() hands back. Every lookup goes through the registry, so a
-- caller can never end up holding a function from a plugin that has since been
-- unloaded - it gets a clear error instead.
local function make_proxy(plugin)
	if not exports_registry[plugin] then
		return nil
	end

	return setmetatable({}, {
		__index = function(_, key)
			local list = exports_registry[plugin]
			if not list or not list[key] then
				return nil
			end

			return function(...)
				local current = exports_registry[plugin]
				local entry = current and current[key]
				if not entry then
					error(("export '%s.%s' is no longer available"):format(plugin, key), 2)
				end

				-- pcall so a broken export cannot take the caller down with it,
				-- but the message names whose fault it was.
				local r = pack(pcall(entry.fn, ...))
				if not r[1] then
					error(("[export %s.%s] %s"):format(plugin, key, tostring(r[2])), 2)
				end
				return unpack(r, 2, r.n)
			end
		end,

		__newindex = function()
			error("exports proxy is read-only", 2)
		end,

		__tostring = function()
			return "exports<" .. plugin .. ">"
		end,
	})
end

-- A hard dependency: the plugin must be there. Failing at load time beats a nil
-- call somewhere deep in a handler an hour later.
function import(plugin)
	local proxy = make_proxy(plugin)
	if not proxy then
		error(("plugin '%s' has no exports (or is not loaded)"):format(plugin), 2)
	end
	return proxy
end

-- A soft dependency: nil when the plugin is absent, so the caller can degrade.
function optional(plugin)
	return make_proxy(plugin)
end

--------------------------------------------------------------------------
-- events
--------------------------------------------------------------------------

function emit(event, ...)
	local list = event_listeners[event]
	if not list then
		return
	end

	-- Copied first: a listener is allowed to add or drop listeners for the very
	-- event it is handling.
	local snapshot = {}
	for i = 1, #list do
		snapshot[i] = list[i]
	end

	for _, listener in ipairs(snapshot) do
		local ok, err = pcall(listener.fn, ...)
		if not ok then
			print(("[event %s] listener '%s' error: %s")
				:format(event, listener.id, tostring(err)))
		end
	end
end

-- on_export(event, id, fn). The id makes registration idempotent: running a
-- plugin's load code again replaces its listener instead of stacking a second.
function on_export(event, id, fn)
	assert(type(event) == "string", "on_export: event must be a string")
	assert(type(id) == "string", "on_export: listener id must be a string")
	assert(type(fn) == "function", "on_export: listener must be a function")

	local list = event_listeners[event] or {}
	event_listeners[event] = list

	for i = #list, 1, -1 do
		if list[i].id == id then
			table.remove(list, i)
		end
	end

	list[#list + 1] = { id = id, fn = fn, plugin = owner() }
end

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

-- `exports_debug` in the server console: who publishes what, who listens to
-- what. The first thing to run when import() says a plugin has no exports.
command("exports_debug", function(ctx)
	ctx.reply("exports:")
	for _, plugin in ipairs(sorted_keys(exports_registry)) do
		local names = {}
		for _, name in ipairs(sorted_keys(exports_registry[plugin])) do
			names[#names + 1] = ("%s (v%d)")
				:format(name, exports_registry[plugin][name].version)
		end
		ctx.reply(("  %-20s %s"):format(plugin, table.concat(names, ", ")))
	end

	ctx.reply("listeners:")
	for _, event in ipairs(sorted_keys(event_listeners)) do
		local ids = {}
		for _, listener in ipairs(event_listeners[event]) do
			ids[#ids + 1] = listener.id
		end
		ctx.reply(("  %-28s %s"):format(event, table.concat(ids, ", ")))
	end
end, { source = "console" })
