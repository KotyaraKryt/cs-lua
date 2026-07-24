-- Minimal class helper. Optional: it is a library in addons/lua/include, not
-- part of the engine API, so ignore it if you prefer plain metatables.
--
--   local Timer = class("Timer")
--   function Timer:init(sec) self.left = sec end
--   function Timer:tick()    self.left = self.left - 1 end
--
--   local Repeating = class("Repeating", Timer)   -- inheritance
--   function Repeating:tick()
--       Repeating.super.tick(self)
--       if self.left <= 0 then self.left = self.period end
--   end

local function class(name, parent)
	local cls = {}
	cls.__name = name or "Class"
	cls.__index = cls
	cls.super = parent

	if parent then
		setmetatable(cls, { __index = parent })
	end

	cls.__tostring = function(self)
		return "<" .. cls.__name .. ">"
	end

	function cls.new(...)
		local obj = setmetatable({}, cls)
		if obj.init then
			obj:init(...)
		end
		return obj
	end

	-- Instance check that walks the inheritance chain.
	function cls.is(obj)
		local mt = type(obj) == "table" and getmetatable(obj) or nil
		while mt do
			if mt == cls then
				return true
			end
			mt = mt.super
		end
		return false
	end

	return cls
end

return class
