-- Small helpers vanilla Lua leaves out: shuffling, searching a table either
-- way (by key or by value), a random string. Nothing here is specific to any
-- one plugin - pull in what you need instead of writing it again.
--
--   local util = require("util")
--   local pick = util.shuffle(maps, 7)          -- 7 random maps, in order
--   if util.contains(maps, name) then ... end
--   local token = util.random_string(12)

local util = {}

-- util.shuffle(list[, count]) -> a new array, the first `count` entries
-- shuffled in from `list` (all of them if `count` is nil or >= #list).
-- Partial Fisher-Yates: stops as soon as it has picked enough, so asking for
-- 7 out of 500 does 7 swaps, not 500. `list` itself is never touched.
function util.shuffle(list, count)
	local copy = {}
	for i, v in ipairs(list) do
		copy[i] = v
	end

	local n = #copy
	local k = math.min(count or n, n)

	for i = 1, k do
		local j = math.random(i, n)
		copy[i], copy[j] = copy[j], copy[i]
	end

	if k == n then
		return copy
	end

	local out = {}
	for i = 1, k do
		out[i] = copy[i]
	end
	return out
end

-- util.contains(list, value) -> true if value is anywhere in the array part
-- of list (linear scan - fine for the tens-of-items lists this is for, not
-- for a hot path over thousands).
function util.contains(list, value)
	for _, v in ipairs(list) do
		if v == value then
			return true
		end
	end
	return false
end

-- util.index_of(list, value) -> its position in the array, or nil.
function util.index_of(list, value)
	for i, v in ipairs(list) do
		if v == value then
			return i
		end
	end
	return nil
end

-- util.has_key(t, key) -> true if t[key] is set to anything but nil.
function util.has_key(t, key)
	return t[key] ~= nil
end

-- util.key_of(t, value) -> the first key whose value equals `value`, or nil.
-- The reverse of indexing: you have the value, you want to know where it
-- lives. Order is whatever pairs() gives you - do not rely on "first" when
-- more than one key could match.
function util.key_of(t, value)
	for k, v in pairs(t) do
		if v == value then
			return k
		end
	end
	return nil
end

local DEFAULT_ALPHABET = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789"

-- util.random_string(length[, alphabet]) -> a random string of `length`
-- characters drawn from `alphabet` (default: letters and digits). For
-- tokens, temp codes, that kind of thing - not for anything that needs to
-- resist a determined attacker, math.random is not a crypto RNG.
function util.random_string(length, alphabet)
	alphabet = alphabet or DEFAULT_ALPHABET
	local n = #alphabet

	local chars = {}
	for i = 1, length do
		local idx = math.random(1, n)
		chars[i] = alphabet:sub(idx, idx)
	end
	return table.concat(chars)
end

return util
