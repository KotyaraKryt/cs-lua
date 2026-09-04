-- Sugar on top of the native res.sound/res.model: declare a whole resource
-- tree in one table instead of a "list it, then loop calling res.*" pair.
-- Any shape works - array entries, named keys, nested sub-tables, a lone
-- string under a key - because a leaf is just "a string, not a table",
-- regardless of how it got there.
--
--   local resource = res.declare({
--       sound = {
--           "i18/mm/1.wav", "i18/mm/2.wav",
--           start = "i18/mm/start.wav",
--           voice = { "i18/vo/a.wav", "i18/vo/b.wav" },
--       },
--   })
--   players.broadcast:play_sound(resource.sound.start)
--
-- Returns the same table back: the paths you declared are already the
-- handles you use later, nothing to unwrap.

local function walk(t, precache)
	for _, v in pairs(t) do
		if type(v) == "string" then
			precache(v)
		elseif type(v) == "table" then
			walk(v, precache)
		end
	end
end

function res.declare(t)
	if t.sound then
		walk(t.sound, res.sound)
	end
	if t.model then
		walk(t.model, res.model)
	end
	return t
end
