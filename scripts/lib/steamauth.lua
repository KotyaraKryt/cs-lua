-- Dproto's own Steam-vs-emulated check, ported from the classic AMXX stock
-- (dp_clientinfo + dp_r_id_provider).
--
--   local steamauth = require("steamauth")
--   if steamauth.is_steam(p) then p:chat("Steam") end

local provider

local function is_steam(p)
	provider = provider or sv.cvar("dp_r_id_provider")
	if not provider then
		return false
	end

	-- dp_clientinfo only lands once the engine flushes its command buffer;
	-- sv.exec() forces that now so the provider reads back in this call.
	sv.cmd(("dp_clientinfo %d"):format(p.id))
	sv.exec()

	return provider:int() == 2
end

return { is_steam = is_steam }
