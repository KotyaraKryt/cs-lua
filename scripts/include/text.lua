-- Text for anything that goes out to a client: templates, and the length the
-- wire actually charges you.
--
-- #s lies. The engine converts an outgoing string from UTF-8 to CP1251, so a
-- Cyrillic letter costs two bytes here and one there, and it collapses every
-- colour tag into a single control byte, so "{green}" costs seven here and one
-- there. Every send path ends in a fixed buffer and truncates without a word.
-- width() answers what the client is really charged; clip() cuts to fit.

local text = {}

-- Colour tags, exactly the ones expand_chat_tags() knows (lua_message.cpp).
local TAGS = { "{default}", "{yellow}", "{green}", "{team}" }

-- Usable length per channel, in bytes as the client counts them. Taken from the
-- buffers in lua_message.cpp, minus the terminator and the newline the protocol
-- appends to chat/center/console.
text.limits = {
	chat    = 188,
	dhud    = 127,
	hud     = 511,
	center  = 254,
	console = 254,
}

-- Chat has a second ceiling, and it bites first. Colour tags are expanded into
-- a 256-byte buffer while the text is still UTF-8, before the CP1251 pass, so a
-- Russian line runs out of room at ~127 letters and never reaches the 188 above.
text.raw_limits = {
	chat = 255,
}

-- Length of the UTF-8 sequence starting at i, or nil if that is not valid
-- UTF-8. Accepts exactly what utf8_decode() in lua_message.cpp accepts: one to
-- three bytes, overlong forms rejected.
local function seq_len(s, i)
	local b = s:byte(i)
	if not b then
		return nil
	end
	if b < 0x80 then
		return 1
	end

	local function cont(j)
		local c = s:byte(j)
		return c and c >= 0x80 and c < 0xC0
	end

	if b >= 0xC0 and b < 0xE0 and cont(i + 1) then
		local cp = (b - 0xC0) * 0x40 + (s:byte(i + 1) - 0x80)
		return cp >= 0x80 and 2 or nil
	end

	if b >= 0xE0 and b < 0xF0 and cont(i + 1) and cont(i + 2) then
		local cp = (b - 0xE0) * 0x1000 + (s:byte(i + 1) - 0x80) * 0x40 + (s:byte(i + 2) - 0x80)
		return cp >= 0x800 and 3 or nil
	end

	return nil
end

-- The same pre-pass the engine does: unless the whole string is valid UTF-8
-- with something multi-byte in it, nothing is converted and every byte stands
-- for itself. That is how a file already saved in CP1251 keeps working.
local function is_utf8(s)
	local i, n = 1, #s
	local multibyte = false

	while i <= n do
		local b = s:byte(i)
		if b < 0x80 then
			i = i + 1
		else
			local len = seq_len(s, i)
			if not len then
				return false
			end
			multibyte = true
			i = i + len
		end
	end

	return multibyte
end

-- Walks the string in the units the wire charges for. Yields the byte range of
-- each unit, what it costs the client (always 1), and what it costs the UTF-8
-- buffer chat passes through on the way. A colour tag is one indivisible unit,
-- which is what keeps clip() from leaving a half-written "{gre" behind.
local function units(s)
	local utf8 = is_utf8(s)
	local i, n = 1, #s

	return function()
		if i > n then
			return nil
		end

		local start = i

		if s:byte(i) == 123 then			-- '{'
			for _, tag in ipairs(TAGS) do
				if s:sub(i, i + #tag - 1) == tag then
					i = i + #tag
					return start, i - 1, 1, 1
				end
			end
		end

		local len = (utf8 and seq_len(s, i)) or 1
		i = i + len
		return start, i - 1, 1, len
	end
end

-- How many bytes reach the client, and how many the UTF-8 buffer sees. The
-- second only matters for chat.
function text.width(s)
	local out, raw = 0, 0
	for _, _, cost, bytes in units(s) do
		out = out + cost
		raw = raw + bytes
	end
	return out, raw
end

local function caps(kind)
	if type(kind) == "number" then
		return kind, nil
	end

	local limit = text.limits[kind]
	if not limit then
		error("text: unknown channel '" .. tostring(kind) .. "'", 3)
	end
	return limit, text.raw_limits[kind]
end

-- Will this survive the trip whole? Check it once when a config is loaded, not
-- after a player complains that the line ends mid-word.
function text.fits(s, kind)
	local limit, raw_limit = caps(kind)
	local out, raw = text.width(s)
	return out <= limit and (not raw_limit or raw <= raw_limit)
end

-- Cuts down to what the channel can carry. kind is a channel name, or a plain
-- number if you have your own budget. Never splits a UTF-8 sequence or a tag.
function text.clip(s, kind)
	local limit, raw_limit = caps(kind)

	local out, raw, cut = 0, 0, 0
	for _, stop, cost, bytes in units(s) do
		if out + cost > limit or (raw_limit and raw + bytes > raw_limit) then
			break
		end
		out, raw, cut = out + cost, raw + bytes, stop
	end

	return cut >= #s and s or s:sub(1, cut)
end

-- text.expand("Hi %name%, %online% online", vars)
-- vars is a table or a function(token). An unknown token is left standing as
-- %token%: a typo should be visible in game, not silently swallowed. A lone %
-- is never touched, so "100% loss" survives.
function text.expand(template, vars)
	local lookup = vars
	if type(vars) ~= "function" then
		lookup = function(token) return vars[token] end
	end

	return (template:gsub("%%([%w_]+)%%", function(token)
		local value = lookup(token)
		if value == nil then
			return nil			-- gsub keeps the original text
		end
		return tostring(value)
	end))
end

-- 65 -> "1:05". Clamps at zero so a countdown that has run out reads "0:00"
-- instead of printing a minus.
function text.mmss(seconds)
	seconds = math.floor(tonumber(seconds) or 0)
	if seconds < 0 then
		seconds = 0
	end
	return ("%d:%02d"):format(math.floor(seconds / 60), seconds % 60)
end

return text
