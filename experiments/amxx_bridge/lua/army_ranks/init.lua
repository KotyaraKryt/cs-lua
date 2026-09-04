-- army_ranks_ultimate from Lua, through cslua_bridge -> cslua_army_ranks.amxx.
--
--   local ar = import("army_ranks")
--   local lvl, name = ar.level(p), ar.level_name(p)

local warned = false

local function warn(msg)
	if warned then return end
	warned = true
	print("[army_ranks] " .. msg)
end

local function call(name, ...)
	if not amxx then
		warn("this lua_mm has no amxx namespace - replace lua_mm_i386.so and restart")
		return nil
	end

	local value, ok, extra = amxx.call(name, ...)
	if ok then
		return value, extra
	end

	warn(name .. ": " .. tostring(extra))
	return nil
end

local function id_of(p)
	if type(p) == "number" then
		return p
	end
	return p and p.id
end

local function num(public, p)
	local id = id_of(p)
	return id and call(public, id)
end

local function str(public, p)
	local id = id_of(p)
	if not id or not amxx then return nil end
	local _, s = call(public, id, amxx.out())
	return s
end

local function level(p)    return num("ARB_Level", p) end
local function all_xp(p)   return num("ARB_AllXP", p) end
local function real_xp(p)  return num("ARB_RealXP", p) end
local function add_xp(p)   return num("ARB_AddXP", p) end
local function anew(p)     return num("ARB_Anew", p) end
local function bonus_hp(p) return num("ARB_BonusHP", p) end
local function update(p)   return num("ARB_UpdatePlayer", p) end

local function level_name(p) return str("ARB_LevelName", p) end
local function style(p)      return str("ARB_Style", p) end
local function write(p)      return str("ARB_Write", p) end

local function max_levels()        return call("ARB_MaxLevels") end
local function level_xp(n)         return call("ARB_LevelXP", n) end
local function csdm()              return call("ARB_Csdm") end
local function map_locked(mapname) return call("ARB_LockMap", mapname) end

local function name_of_level(n)
	if not amxx then return nil end
	local _, name = call("ARB_NameOfLevel", n, amxx.out())
	return name
end

local function give_real_xp(p, xp)
	local id = id_of(p)
	return id and call("ARB_SetRealXP", id, xp)
end

local function give_add_xp(p, xp)
	local id = id_of(p)
	return id and call("ARB_SetAddXP", id, xp)
end

-- admin = -1 skips the chat message army_ranks prints otherwise.
local function give_anew(admin, p, points)
	local id = id_of(p)
	return id and call("ARB_AddAnew", id_of(admin) or -1, id, points)
end

export("level",         level)
export("level_name",    level_name)
export("all_xp",        all_xp)
export("real_xp",       real_xp)
export("add_xp",        add_xp)
export("anew",          anew)
export("bonus_hp",      bonus_hp)
export("style",         style)
export("write",         write)
export("max_levels",    max_levels)
export("level_xp",      level_xp)
export("name_of_level", name_of_level)
export("csdm",          csdm)
export("map_locked",    map_locked)
export("give_real_xp",  give_real_xp)
export("give_add_xp",   give_add_xp)
export("give_anew",     give_anew)
export("update",        update)

cmd.add("ar_check", function(ctx)
	local id = tonumber(ctx.args[1])
	if not id then
		return ctx.reply("ar_check <player index>")
	end

	if not amxx then
		return ctx.reply("this lua_mm has no amxx namespace - replace lua_mm_i386.so and restart")
	end

	local lvl = level(id)
	if not lvl then
		return ctx.reply("bridge or army_ranks unavailable - see the log above")
	end

	ctx.reply(("id %d: level %s (%s), xp %s, /aNew %s")
		:format(id, tostring(lvl), tostring(level_name(id)),
			tostring(all_xp(id)), tostring(anew(id))))
end, { source = "server" })
