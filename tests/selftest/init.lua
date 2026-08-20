-- Самопроверка C++-поверхности на живом сервере.
--
-- Юниты в tests/ гоняются без игры и покрывают core/ и include/. Всё, что
-- ниже — то, что без движка не проверить: регистрация в пространствах имён,
-- цепочки хуков, объект события, таймеры, база, прекеш.
--
-- Плагин выключен: папка начинается с подчёркивания. Чтобы включить, убери
-- его и сделай lua_reload. Дальше `lua_test` в консоли прогоняет всё заново
-- без перезагрузки.

plugin {
	name        = "SelfTest",
	version     = "1.0",
	author      = "cs-lua",
	api_version = 1,
}

local passed, failed, notes = 0, 0, {}

local function ok(what, cond, extra)
	if cond then
		passed = passed + 1
	else
		failed = failed + 1
		notes[#notes + 1] = what .. (extra and ("  <- " .. tostring(extra)) or "")
	end
end

-- Возвращает текст ошибки, если вызов упал. Сообщение — часть API: оно должно
-- объяснять, а не просто быть.
local function errmsg(fn, ...)
	local good, err = pcall(fn, ...)
	return (not good) and tostring(err) or nil
end

local function run()
	passed, failed, notes = 0, 0, {}

	----------------------------------------------------------------
	-- Пространства имён
	----------------------------------------------------------------

	for _, ns in ipairs({ "hook", "cmd", "timer", "ents", "res", "sv", "db",
	                      "menu", "players", "access", "ui", "plugin" }) do
		ok("есть пространство " .. ns, type(_G[ns]) == "table")
	end

	-- v1 не должен воскреснуть ни через core, ни через плагин.
	for _, gone in ipairs({ "on", "command", "after", "every", "cancel", "all",
	                        "find_player", "create_entity", "precache_sound",
	                        "sqlite", "emit", "on_export", "permission",
	                        "player_method", "_CSLUA_DIR", "_server_command" }) do
		ok("глобал v1 убран: " .. gone, _G[gone] == nil)
	end

	ok("menu._show спрятан", menu._show == nil)
	ok("cmd._register спрятан", cmd._register == nil)

	ok("sv.api == 2", sv.api == 2, sv.api)
	ok("sv.version", type(sv.version) == "string")
	ok("sv.dir", type(sv.dir) == "string" and #sv.dir > 0)
	ok("sv.map()", type(sv.map()) == "string")
	ok("sv.time()", type(sv.time()) == "number")
	-- Не сравниваем с литералом: папку переименовывают, чтобы включить плагин.
	ok("plugin.id()", type(plugin.id()) == "string" and #plugin.id() > 0, plugin.id())
	ok("plugin.data_dir()", type(plugin.data_dir()) == "string")

	----------------------------------------------------------------
	-- Хуки и объект события
	----------------------------------------------------------------

	hook.add("selftest.evt", "a", function(e) e.seen = (e.seen or 0) + 1 end)
	hook.add("selftest.evt", "b", function(e)
		e.seen = (e.seen or 0) + 1
		if e.stop then e:cancel() end
	end)
	hook.add("selftest.evt", "c", function(e) e.seen = (e.seen or 0) + 1 end)

	local e = hook.run("selftest.evt", { stop = false })
	ok("цепочка доходит до всех", e.seen == 3, e.seen)
	ok("hook.run отдаёт ту же таблицу", e.name == "selftest.evt")
	ok("не отменено", e.cancelled == false)

	local e2 = hook.run("selftest.evt", { stop = true })
	ok("отмена обрывает цепочку", e2.seen == 2, e2.seen)
	ok("флаг отмены выставлен", e2.cancelled == true)

	-- То, ради чего у обработчика есть id.
	hook.add("selftest.evt", "a", function(ev) ev.seen = (ev.seen or 0) + 10 end)
	ok("повторная регистрация заменяет", hook.run("selftest.evt", {}).seen == 12)

	ok("hook.remove снимает", hook.remove("selftest.evt", "a") == true)
	ok("hook.remove второй раз", hook.remove("selftest.evt", "a") == false)

	ok("опечатка в событии ловится",
		errmsg(hook.add, "playr_spawn", "x", function() end) ~= nil)
	ok("движковое событие нельзя запустить",
		errmsg(hook.run, "player_spawn") ~= nil)
	ok("своё событие требует точки", errmsg(hook.run, "bare") ~= nil)

	local me, mine = plugin.id(), 0
	for _, row in ipairs(hook.list("selftest.evt")) do
		if row.plugin == me then mine = mine + 1 end
	end
	ok("hook.list видит наши подписки", mine == 2, mine)

	----------------------------------------------------------------
	-- Игроки
	----------------------------------------------------------------

	ok("players.get на пустой слот", players.get(31) == nil)
	ok("players.list — таблица", type(players.list()) == "table")
	ok("players.find не находит выдумку",
		select(2, players.find("nobody_here_at_all")) ~= nil)

	ok("broadcast умеет слать", type(players.broadcast.chat) == "function")
	ok("broadcast умеет motd", type(players.broadcast.motd) == "function")

	-- Ловушка v1: раньше это молча возвращало nil.
	local trap = errmsg(function() return players.broadcast.alive end)
	ok("broadcast отказывает по состоянию", trap and trap:find("broadcast"), trap)

	----------------------------------------------------------------
	-- Цвет
	----------------------------------------------------------------

	local color = require("color")
	ok("цвет по имени", color.parse("green")[2] == 255)
	ok("цвет hex", color.parse("#ff8000")[1] == 255)
	ok("цвет rgb насквозь", color.parse({ 1, 2, 3 })[3] == 3)
	ok("неизвестный цвет — ошибка", select(2, color.parse("mauve")) ~= nil)
	ok("код меню из имени", color.menu("red") == "\\r")
	ok("ближайший код меню", color.menu("#ffffff") == "\\w")
	ok("тег чата", color.chat("green") == "{green}")
	ok("ui.color", ui.color("blue")[3] == 255)
	ok("ui.palette общая", ui.palette.green ~= nil)

	-- Модуль обязан принимать имя там же, где раньше принимал только rgb.
	ok("hud принимает имя цвета",
		errmsg(function() players.broadcast:hud("x", { color = "green" }) end) == nil)
	ok("hud отвергает выдуманный цвет",
		errmsg(function() players.broadcast:hud("x", { color = "mauve" }) end) ~= nil)

	----------------------------------------------------------------
	-- Хранилище
	----------------------------------------------------------------

	local store = require("store")
	local s = store.open("selftest")

	s:set("alpha", { n = 1, nested = { "a", "b" } })
	s:set("beta", { n = 2 })
	ok("очередь считается", s:pending() == 2, s:pending())
	ok("читаем своё до записи", s:get("alpha").n == 1)
	ok("flush", s:flush() == true)
	ok("очередь пуста", s:pending() == 0)
	ok("читаем после записи", s:get("beta").n == 2)
	ok("вложенное пережило", s:get("alpha").nested[2] == "b")
	ok("ключи отсортированы", table.concat(s:keys(), ",") == "alpha,beta",
		table.concat(s:keys(), ","))

	s:delete("beta")
	ok("удаление видно сразу", s:get("beta") == nil)
	s:flush()
	ok("удаление записалось", s:get("beta") == nil)
	ok("тот же объект на то же имя", store.open("selftest") == s)

	s:delete("alpha")
	s:flush()

	local datafile = require("datafile")
	ok("datafile.serialize", datafile.serialize({ b = 2, a = 1 }, ""):find("a = 1") ~= nil)

	----------------------------------------------------------------
	-- База
	----------------------------------------------------------------

	local handle = db.open(":memory:")
	ok("db.open", handle ~= nil)

	if handle then
		handle:exec("CREATE TABLE t (k TEXT PRIMARY KEY, n INTEGER)")
		handle:exec("INSERT INTO t VALUES (?, ?)", "a", 1)
		handle:exec("INSERT INTO t VALUES (?, ?)", "b", 2)

		ok("db:first", handle:first("SELECT n FROM t WHERE k = ?", "a").n == 1)
		ok("db:query", #handle:query("SELECT * FROM t") == 2)
		ok("несовпадение параметров — ошибка",
			errmsg(function() handle:exec("INSERT INTO t VALUES (?, ?)", "c") end) ~= nil)

		local st = handle:prepare("INSERT INTO t VALUES (?, ?)")
		handle:transaction(function()
			st:run("c", 3)
			st:run("d", 4)
		end)
		ok("транзакция применилась", #handle:query("SELECT * FROM t") == 4)
		st:close()

		handle:close()
		ok("закрытая база отвечает ошибкой",
			errmsg(function() handle:query("SELECT 1") end) ~= nil)
		ok("повторный close не ошибка", errmsg(function() handle:close() end) == nil)
	end

	----------------------------------------------------------------
	-- Таймеры
	----------------------------------------------------------------

	local ran = 0
	timer.create("selftest.tick", 1, function() ran = ran + 1 end)
	timer.create("selftest.tick", 1, function() ran = ran + 1 end)
	ok("именованный снимается", timer.destroy("selftest.tick") == true)
	ok("снять его же второй раз", timer.destroy("selftest.tick") == false)

	local tid = timer.after(600, function() end)
	ok("timer.after отдаёт id", type(tid) == "number")
	ok("timer.cancel", timer.cancel(tid) == true)
	ok("timer.cancel повторно", timer.cancel(tid) == false)
	ok("нулевой интервал отвергается",
		errmsg(function() timer.every(0, function() end) end) ~= nil)

	----------------------------------------------------------------
	-- Права
	----------------------------------------------------------------

	access.declare("selftest.node", { desc = "проверка", default = true })
	ok("access.declare", access.permissions()["selftest.node"] ~= nil)
	ok("консоли разрешено всё", access.can(nil, "selftest.whatever"))

	----------------------------------------------------------------
	-- Сущности
	----------------------------------------------------------------

	local ent = ents.create("info_target")
	ok("ents.create", ent ~= nil)

	if ent then
		ent:origin(100, 200, 300)
		local x, y, z = ent:origin()
		ok("origin читается обратно", x == 100 and y == 200 and z == 300)

		ent:solid(1)
		ok("solid", ent:solid() == 1)

		ent:movetype(5)
		ok("movetype", ent:movetype() == 5)

		ent:render({ mode = 2, amount = 128, color = { 255, 0, 0 } })
		local r = ent:render()
		ok("render.mode", r.mode == 2)
		ok("render.amount", r.amount == 128)
		ok("render.color", r.color[1] == 255)

		ok("keyvalue не падает",
			errmsg(function() ent:keyvalue("targetname", "selftest_probe") end) == nil)

		ok("ent:valid до удаления", ent:valid() == true)
		ent:remove()
		ok("ent:valid после удаления", ent:valid() == false)
		-- Serial number — то, что не даёт писать в чужую сущность.
		ok("протухший объект бросает ошибку",
			errmsg(function() ent:origin() end) ~= nil)
	end

	ok("неизвестный classname", ents.create("no_such_entity_at_all") == nil)

	----------------------------------------------------------------
	-- Объект события у движковых
	----------------------------------------------------------------

	hook.add("player:spawn", "selftest.nocancel", function(ev)
		local why = errmsg(function() ev:cancel() end)
		ok("уведомление нельзя отменить", why and why:find("cannot be cancelled"), why)
	end)

	return passed, failed, notes
end

local function report(reply)
	local p, f, why = run()

	if f == 0 then
		reply(("SELFTEST: %d проверок, все прошли"):format(p))
		return
	end

	reply(("SELFTEST: %d прошло, %d УПАЛО"):format(p, f))
	for _, note in ipairs(why) do
		reply("  - " .. note)
	end
end

cmd.add("lua_test", function(ctx)
	report(ctx.reply)
end, { source = "console" })

-- Не при загрузке: сущностей тогда ещё нет, и проверка ents.* упала бы на том
-- самом правиле, которое проверяет. Первый кадр наступает уже с картой.
timer.after(1, function()
	report(print)
end)
