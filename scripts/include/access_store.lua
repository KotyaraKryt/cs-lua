-- Reading and writing the access data files. Nothing here knows what a
-- permission means: it loads a Lua table from addons/lua/data/, and writes one
-- back. Kept separate from core/access.lua so the format can change without
-- touching the rules, and so it can be tested on its own.
--
-- Data files are plain Lua that returns a table - same idea as the prefix
-- table in chat_manager - so there is no parser and no second config syntax.

local store = {}

local DIR = (_CSLUA_DIR or ".") .. "/data"

local function path(name)
	return DIR .. "/" .. name .. ".lua"
end

local function exists(file)
	local fh = io.open(file, "r")
	if not fh then
		return false
	end
	fh:close()
	return true
end

-- A data file is data. It runs in an empty environment, so a file that got
-- clever (or was tampered with) cannot reach the server: no io, no os, not
-- even print. Anything but a returned table is an error.
local function load_file(file)
	local chunk, err = loadfile(file)
	if not chunk then
		return nil, err
	end

	setfenv(chunk, {})

	local ok, result = pcall(chunk)
	if not ok then
		return nil, result
	end
	if type(result) ~= "table" then
		return nil, file .. ": must return a table"
	end
	return result
end

-- Loads data/<name>.lua. A missing file is not an error - a fresh server has
-- no users yet - and gives back `fallback`. A broken one is: the caller has to
-- decide loudly, never silently run with half the rights.
function store.load(name, fallback)
	local file = path(name)
	if not exists(file) then
		return fallback, nil
	end
	return load_file(file)
end

local function is_array(t)
	local n = 0
	for k in pairs(t) do
		if type(k) ~= "number" then
			return false
		end
		n = n + 1
	end
	return n == #t
end

-- Sorted output, always: two saves of the same data must produce byte-identical
-- files, otherwise every write shows up as a diff.
local function sorted_keys(t)
	local keys = {}
	for k in pairs(t) do
		keys[#keys + 1] = k
	end
	table.sort(keys, function(a, b)
		if type(a) == type(b) then
			return tostring(a) < tostring(b)
		end
		return type(a) < type(b)
	end)
	return keys
end

local serialize

local function serialize_value(value, indent)
	local t = type(value)
	if t == "string" then
		return string.format("%q", value)
	elseif t == "number" or t == "boolean" then
		return tostring(value)
	elseif t == "table" then
		return serialize(value, indent)
	end
	-- Functions and userdata have no textual form; dropping them silently
	-- would lose rights without a word, so refuse.
	error("access: cannot save a " .. t .. " to a data file")
end

serialize = function(t, indent)
	indent = indent or ""
	local inner = indent .. "\t"

	if next(t) == nil then
		return "{}"
	end

	local out = { "{" }

	if is_array(t) then
		local parts = {}
		for i = 1, #t do
			parts[#parts + 1] = serialize_value(t[i], inner)
		end
		return "{ " .. table.concat(parts, ", ") .. " }"
	end

	for _, k in ipairs(sorted_keys(t)) do
		local key = type(k) == "string" and k:match("^[%a_][%w_]*$")
			and k or ("[" .. serialize_value(k, inner) .. "]")
		out[#out + 1] = inner .. key .. " = " .. serialize_value(t[k], inner) .. ","
	end

	out[#out + 1] = indent .. "}"
	return table.concat(out, "\n")
end

store.serialize = serialize

-- Writes data/<name>.lua. Never writes over the live file directly: a crash
-- mid-write would take the whole admin list with it. New content goes to .tmp,
-- the previous file becomes .bak, and only then does .tmp take its place.
function store.save(name, t, header)
	local file = path(name)
	local tmp, bak = file .. ".tmp", file .. ".bak"

	local ok, body = pcall(serialize, t, "")
	if not ok then
		return false, body
	end

	local fh, err = io.open(tmp, "w")
	if not fh then
		return false, err
	end

	if header then
		fh:write("-- ", header, "\n")
	end
	fh:write("-- Written by cs-lua; hand edits are kept until the next save.\n\n")
	fh:write("return ", body, "\n")
	fh:close()

	os.remove(bak)
	if exists(file) then
		os.rename(file, bak)		-- rename onto an existing name fails on Windows
	end

	local moved, rename_err = os.rename(tmp, file)
	if not moved then
		os.rename(bak, file)		-- put the old one back rather than leave none
		return false, rename_err
	end

	return true
end

function store.dir()
	return DIR
end

return store
