-- Доступ к army_ranks_ultimate из Lua.
--
-- Цепочка: этот плагин -> amxx.call -> форвард AMXX -> cslua_army_ranks.amxx
-- -> натив army_ranks_ultimate. Все три звена должны быть на месте, иначе
-- функции ниже вернут nil (а не упадут).
--
--   local ar = import("army_ranks")
--   local lvl  = ar.level(p)
--   local name = ar.level_name(p)
--   if lvl and name then
--       p:chat(("Твоё звание: %s (уровень %d)"):format(name, lvl))
--   end

-- Одно место, где решается "мост вообще жив?". Первый неудачный вызов
-- пишет причину в лог и дальше молчит, чтобы не засорять консоль.
local warned = false

local function call(name, ...)
	local value, ok, extra = amxx.call(name, ...)
	if ok then
		return value, extra
	end

	if not warned then
		warned = true
		print(("[army_ranks] мост недоступен (%s): %s")
			:format(name, tostring(extra)))
	end
	return nil
end

-- p может быть объектом игрока или уже готовым индексом.
local function id_of(p)
	if type(p) == "number" then
		return p
	end
	return p and p.id
end

--------------------------------------------------------------------------
-- чтение
--------------------------------------------------------------------------

-- Уровень игрока числом.
local function level(p)
	local id = id_of(p)
	return id and call("ARB_Level", id)
end

-- Название звания строкой ("Рядовой" и т.д.).
local function level_name(p)
	local id = id_of(p)
	if not id then return nil end
	local _, name = call("ARB_LevelName", id, amxx.out())
	return name
end

-- Весь опыт, реальный опыт, добавочный опыт, очки /aNew.
local function all_xp(p)   local id = id_of(p) return id and call("ARB_AllXP", id) end
local function real_xp(p)  local id = id_of(p) return id and call("ARB_RealXP", id) end
local function add_xp(p)   local id = id_of(p) return id and call("ARB_AddXP", id) end
local function anew(p)     local id = id_of(p) return id and call("ARB_Anew", id) end
local function bonus_hp(p) local id = id_of(p) return id and call("ARB_BonusHP", id) end

-- Стиль MOTD-окон и запись игрока в статистике.
local function style(p)
	local id = id_of(p)
	if not id then return nil end
	local _, s = call("ARB_Style", id, amxx.out())
	return s
end

local function write(p)
	local id = id_of(p)
	if not id then return nil end
	local _, w = call("ARB_Write", id, amxx.out())
	return w
end

--------------------------------------------------------------------------
-- справочное (не про конкретного игрока)
--------------------------------------------------------------------------

local function max_levels()        return call("ARB_MaxLevels") end
local function level_xp(n)         return call("ARB_LevelXP", n) end
local function csdm()              return call("ARB_Csdm") end
local function map_locked(mapname) return call("ARB_LockMap", mapname) end

local function name_of_level(n)
	local _, name = call("ARB_NameOfLevel", n, amxx.out())
	return name
end

--------------------------------------------------------------------------
-- запись
--------------------------------------------------------------------------

-- Прибавить/отнять реальный или дополнительный опыт.
local function give_real_xp(p, xp)
	local id = id_of(p)
	return id and call("ARB_SetRealXP", id, xp)
end

local function give_add_xp(p, xp)
	local id = id_of(p)
	return id and call("ARB_SetAddXP", id, xp)
end

-- admin = -1, если не нужно писать сообщение в чат.
local function give_anew(admin, p, points)
	local id = id_of(p)
	return id and call("ARB_AddAnew", id_of(admin) or -1, id, points)
end

-- Перечитать опыт игрока.
local function update(p)
	local id = id_of(p)
	return id and call("ARB_UpdatePlayer", id)
end

--------------------------------------------------------------------------

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

-- Проверка связки прямо с сервера: ar_check <индекс игрока>
cmd.add("ar_check", function(ctx)
	local id = tonumber(ctx.args[1])
	if not id then
		return ctx.reply("ar_check <player index>")
	end

	local lvl = level(id)
	if not lvl then
		return ctx.reply("мост или army_ranks недоступны - смотри лог выше")
	end

	ctx.reply(("id %d: уровень %s (%s), опыт %s, /aNew %s")
		:format(id, tostring(lvl), tostring(level_name(id)),
			tostring(all_xp(id)), tostring(anew(id))))
end, { source = "server" })
