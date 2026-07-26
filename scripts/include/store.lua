-- The default answer to "where do I put this?".
--
--   local s = store.open("stats")
--
--   local rec = s:get(p:steamid()) or { kills = 0 }
--   rec.kills = rec.kills + 1
--   s:set(p:steamid(), rec)
--
-- A key-value store of Lua tables, kept in a SQLite file in the plugin's own
-- data directory. Values are whatever datafile serialises: tables, nested
-- tables, strings, numbers, booleans.
--
-- Writes are queued and go to disk in one transaction. That is not an
-- optimisation to think about later - an insert outside a transaction is its
-- own commit with a disk sync, and doing that on every frag is the difference
-- between a server that runs and one that stutters. You do not have to
-- remember to flush: it happens on a map change and when the plugin unloads.
--
-- When to use something else:
--   * a handful of values a human edits (the access list, a shop stock list)
--     -> require("datafile"), which writes readable .lua files
--   * queries, sorting, a top-10, anything relational
--     -> db.open() and write the SQL

local datafile = require("datafile")

local store = {}

-- One handle per name per plugin. Opening the same store twice would give two
-- write queues over one file, and the second flush would undo the first.
local opened = {}

local Store = {}
Store.__index = Store

local SCHEMA = [[
	CREATE TABLE IF NOT EXISTS entries (
		key   TEXT PRIMARY KEY,
		value TEXT NOT NULL,
		saved INTEGER NOT NULL DEFAULT 0
	);
]]

-- Values go in as the same Lua-literal text datafile writes, so a store file
-- can be read by eye in any SQLite browser - and there is one serialiser in
-- the project, not two that disagree about nested tables.
local function encode(value)
	if type(value) == "table" then
		return datafile.serialize(value, "")
	end
	if type(value) == "string" then
		return string.format("%q", value)
	end
	if type(value) == "number" or type(value) == "boolean" then
		return tostring(value)
	end
	error("store: cannot save a " .. type(value), 3)
end

-- Runs in an empty environment: a value that got clever, or a file somebody
-- edited by hand, cannot reach the server.
local function decode(text)
	local chunk, err = loadstring("return " .. text)
	if not chunk then
		return nil, err
	end

	setfenv(chunk, {})

	local ok, value = pcall(chunk)
	if not ok then
		return nil, value
	end
	return value
end

-- store.open(name) -> handle. The file lands in the plugin's own data
-- directory, so two plugins can both have a "stats" store.
function store.open(name)
	name = name or "store"
	assert(type(name) == "string", "store.open: name must be a string")

	local owner = plugin.id() or "core"
	local cached = opened[owner .. "/" .. name]
	if cached then
		return cached
	end

	local handle, err = db.open(name)
	if not handle then
		error(("store.open('%s'): %s"):format(name, tostring(err)), 2)
	end

	handle:exec(SCHEMA)

	local self = setmetatable({
		db    = handle,
		name  = name,
		dirty = {},		-- key -> value waiting to be written, or the DELETED marker
		count = 0,
		put   = handle:prepare([[
			INSERT INTO entries (key, value, saved) VALUES (?, ?, ?)
			ON CONFLICT(key) DO UPDATE SET value = excluded.value, saved = excluded.saved
		]]),
		drop  = handle:prepare("DELETE FROM entries WHERE key = ?"),
	}, Store)

	opened[owner .. "/" .. name] = self

	-- Nothing is lost because somebody forgot to flush: a map change and an
	-- unload both write the queue out first.
	hook.add("map_change", "store.flush." .. name, function()
		self:flush()
	end)

	hook.add("plugin_unload", "store.flush_unload." .. name, function()
		self:flush()
	end)

	return self
end

-- A table cannot hold nil, so a pending delete needs a value of its own.
local DELETED = setmetatable({}, { __tostring = function() return "<deleted>" end })

function Store:set(key, value)
	assert(type(key) == "string", "store:set: key must be a string")

	if self.dirty[key] == nil then
		self.count = self.count + 1
	end
	self.dirty[key] = value == nil and DELETED or value
	return self
end

function Store:get(key)
	assert(type(key) == "string", "store:get: key must be a string")

	local pending = self.dirty[key]
	if pending ~= nil then
		return pending ~= DELETED and pending or nil
	end

	local row = self.db:first("SELECT value FROM entries WHERE key = ?", key)
	if not row then
		return nil
	end

	local value, err = decode(row.value)
	if value == nil and err then
		print(("[store %s] key '%s' is corrupt: %s"):format(self.name, key, tostring(err)))
		return nil
	end
	return value
end

function Store:delete(key)
	return self:set(key, nil)
end

-- store:keys() -> every key, queued writes included. Sorted, so two runs over
-- the same data produce the same order.
function Store:keys()
	local seen, keys = {}, {}

	for _, row in ipairs(self.db:query("SELECT key FROM entries")) do
		seen[row.key] = true
		keys[#keys + 1] = row.key
	end

	for key, value in pairs(self.dirty) do
		if value == DELETED then
			seen[key] = nil
		elseif not seen[key] then
			seen[key] = true
			keys[#keys + 1] = key
		end
	end

	local out = {}
	for _, key in ipairs(keys) do
		if seen[key] then
			out[#out + 1] = key
		end
	end

	table.sort(out)
	return out
end

-- store:all() -> { [key] = value }. Reads the whole store into memory, so it
-- is for a report or a migration, not for something on a timer.
function Store:all()
	local out = {}
	for _, key in ipairs(self:keys()) do
		out[key] = self:get(key)
	end
	return out
end

-- Writes the queue out. One transaction for the lot; returns true, or false
-- and the error, in which case the queue is kept so the next flush retries.
function Store:flush()
	if self.count == 0 then
		return true
	end

	local now = os.time()
	local queue = self.dirty

	-- db:transaction raises rather than returning a flag - it has already
	-- rolled back by then. pcall so one unserialisable value cannot take the
	-- calling handler down with it.
	local ok, err = pcall(function()
		self.db:transaction(function()
			for key, value in pairs(queue) do
				if value == DELETED then
					self.drop:run(key)
				else
					self.put:run(key, encode(value), now)
				end
			end
		end)
	end)

	if not ok then
		return false, err
	end

	self.dirty = {}
	self.count = 0
	return true
end

-- How many writes are waiting. Handy in a `lua_` diagnostic command.
function Store:pending()
	return self.count
end

return store
