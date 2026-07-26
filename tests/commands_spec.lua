-- Роутер команд: одна регистрация обслуживает чат, командный чат и консоль,
-- и на нём же висят проверка прав и иммунитет. Разъедься эти пути — и право
-- будет проверяться в чате, но не в консоли.

local GROUPS = [[{
	default   = { weight = 0 },
	moderator = { weight = 30, allow = { "admin.kick" } },
	admin     = { weight = 50, allow = { "admin.*" } },
}]]

return function(t, support)
	local function setup(users)
		support.reset()
		support.load_access(GROUPS, users or "{}")
		support.load_core("commands")
	end

	-- Имитирует «игрок написал в чат» так же, как это делает движок.
	local function say(p, text, team)
		return support.fire("player_chat", { player = p, text = text, team = team or false })
	end

	--------------------------------------------------------------------
	-- players.find
	--------------------------------------------------------------------

	t.it("находит по номеру слота", function()
		setup()
		local p = support.player(3)
		t.eq(players.find("3"), p)
	end)

	t.it("находит по #userid", function()
		setup()
		local p = support.player(1, { userid = 42 })
		t.eq(players.find("#42"), p)
	end)

	t.it("находит по части ника, регистр не важен", function()
		setup()
		local p = support.player(1, { name = "KotyaRakryt" })
		t.eq(players.find("kotya"), p)
	end)

	-- Иначе !slay ko выбрал бы случайного из двоих.
	t.it("неоднозначный ник — отказ с причиной", function()
		setup()
		support.player(1, { name = "kotya_one" })
		support.player(2, { name = "kotya_two" })

		local found, why = players.find("kotya")
		t.eq(found, nil)
		t.ok(why:find("more than one", 1, true) or why:find("больше", 1, true), why)
	end)

	t.it("пустой слот — отказ с причиной", function()
		setup()
		local found, why = players.find("7")
		t.eq(found, nil)
		t.ok(why)
	end)

	t.it("никого с таким userid — отказ", function()
		setup()
		support.player(1, { userid = 42 })
		t.eq(players.find("#99"), nil)
	end)

	t.it("пустая строка — отказ, а не первый попавшийся", function()
		setup()
		support.player(1)
		t.eq(players.find(""), nil)
	end)

	--------------------------------------------------------------------
	-- Разбор чата
	--------------------------------------------------------------------

	t.it("оба префикса работают", function()
		setup()
		local hits = 0
		cmd.add("go", function() hits = hits + 1 end)

		local p = support.player(1)
		say(p, "!go")
		say(p, "/go")
		t.eq(hits, 2)
	end)

	t.it("распознанная команда не уходит в общий чат", function()
		setup()
		cmd.add("go", function() end)
		t.ok(say(support.player(1), "!go").cancelled)
	end)

	t.it("обычное сообщение проходит насквозь", function()
		setup()
		cmd.add("go", function() end)
		t.no(say(support.player(1), "привет всем").cancelled)
	end)

	t.it("незнакомая команда не проглатывается", function()
		setup()
		t.no(say(support.player(1), "!nosuchcommand").cancelled,
			"иначе опечатка молча исчезала бы из чата")
	end)

	t.it("регистр имени команды не важен", function()
		setup()
		local hits = 0
		cmd.add("Go", function() hits = hits + 1 end)
		say(support.player(1), "!GO")
		t.eq(hits, 1)
	end)

	t.it("аргументы разбиваются по пробелам", function()
		setup()
		local got
		cmd.add("give", function(ctx) got = ctx.args end)

		say(support.player(1), "!give kotya 500")
		t.eq(#got, 2)
		t.eq(got[1], "kotya")
		t.eq(got[2], "500")
	end)

	t.it("лишние пробелы не создают пустых аргументов", function()
		setup()
		local got
		cmd.add("give", function(ctx) got = ctx.args end)

		say(support.player(1), "!give   kotya    500  ")
		t.eq(#got, 2)
	end)

	t.it("команда без аргументов даёт пустой список", function()
		setup()
		local got
		cmd.add("hp", function(ctx) got = ctx.args end)
		say(support.player(1), "!hp")
		t.eq(#got, 0)
	end)

	--------------------------------------------------------------------
	-- Источники
	--------------------------------------------------------------------

	t.it("ctx.source различает чат и командный чат", function()
		setup()
		local seen = {}
		cmd.add("where", function(ctx) seen[#seen + 1] = ctx.source end)

		local p = support.player(1)
		say(p, "!where", false)
		say(p, "!where", true)
		t.eq(seen[1], "chat")
		t.eq(seen[2], "chat_team")
	end)

	t.it("chat_team не срабатывает из обычного чата", function()
		setup()
		local hits = 0
		cmd.add("plant", function() hits = hits + 1 end, { source = "chat_team" })

		local p = support.player(1)
		say(p, "!plant", false)
		t.eq(hits, 0)
		say(p, "!plant", true)
		t.eq(hits, 1)
	end)

	t.it("консольная команда не срабатывает из чата", function()
		setup()
		local hits = 0
		cmd.add("lua_thing", function() hits = hits + 1 end, { source = "console" })

		say(support.player(1), "!lua_thing")
		t.eq(hits, 0)
	end)

	t.it("консольная команда регистрируется в движке", function()
		setup()
		cmd.add("lua_thing", function() end, { source = "console" })
		t.ok(support.console_commands["lua_thing"])
	end)

	t.it("чат-команда движок не занимает", function()
		setup()
		cmd.add("hp", function() end, { source = "chat" })
		t.eq(support.console_commands["hp"], nil)
	end)

	t.it("неизвестный источник — ошибка при регистрации", function()
		setup()
		t.raises(function()
			cmd.add("bad", function() end, { source = "telepathy" })
		end, "source")
	end)

	--------------------------------------------------------------------
	-- Консоль
	--------------------------------------------------------------------

	t.it("из консоли ctx.player равен nil", function()
		setup()
		local seen = "не вызывалось"
		cmd.add("lua_who", function(ctx) seen = ctx.player end, { source = "console" })

		support.console_commands["lua_who"]({})
		t.eq(seen, nil)
	end)

	-- Консоль — высшая власть на машине, ей права не проверяют.
	t.it("из консоли право не проверяется", function()
		setup()
		local hits = 0
		cmd.add("lua_slay", function() hits = hits + 1 end,
			{ source = "console", perm = "admin.slay" })

		support.console_commands["lua_slay"]({})
		t.eq(hits, 1)
	end)

	--------------------------------------------------------------------
	-- Права
	--------------------------------------------------------------------

	t.it("без права обработчик не вызывается", function()
		setup()
		local hits = 0
		cmd.add("slay", function() hits = hits + 1 end, { perm = "admin.slay" })

		say(support.player(1), "!slay")
		t.eq(hits, 0)
	end)

	t.it("с правом обработчик вызывается", function()
		setup([[{ ["STEAM_0:0:1"] = { groups = { "admin" } } }]])
		local hits = 0
		cmd.add("slay", function() hits = hits + 1 end, { perm = "admin.slay" })

		say(support.player(1), "!slay")
		t.eq(hits, 1)
	end)

	t.it("ctx.can проверяет право вызвавшего", function()
		setup([[{ ["STEAM_0:0:1"] = { groups = { "moderator" } } }]])
		local yes, no
		cmd.add("check", function(ctx)
			yes = ctx.can("admin.kick")
			no  = ctx.can("admin.rcon")
		end)

		say(support.player(1), "!check")
		t.ok(yes)
		t.no(no)
	end)

	--------------------------------------------------------------------
	-- target и иммунитет
	--------------------------------------------------------------------

	t.it("target находит игрока в ctx.target", function()
		setup([[{ ["STEAM_0:0:1"] = { groups = { "admin" } } }]])
		local target
		cmd.add("slay", function(ctx) target = ctx.target end,
			{ perm = "admin.slay", target = 1 })

		local victim = support.player(2, { name = "victim" })
		say(support.player(1), "!slay victim")
		t.eq(target, victim)
	end)

	t.it("ненайденный target не вызывает обработчик", function()
		setup([[{ ["STEAM_0:0:1"] = { groups = { "admin" } } }]])
		local hits = 0
		cmd.add("slay", function() hits = hits + 1 end,
			{ perm = "admin.slay", target = 1 })

		say(support.player(1), "!slay nobody")
		t.eq(hits, 0)
	end)

	-- Главное, ради чего target декларативный: забыть проверку нельзя.
	t.it("цель с не меньшим весом не трогается", function()
		setup([[{
			["STEAM_0:0:1"] = { groups = { "admin" } },
			["STEAM_0:0:2"] = { groups = { "admin" } },
		}]])
		local hits = 0
		cmd.add("slay", function() hits = hits + 1 end,
			{ perm = "admin.slay", target = 1 })

		support.player(2, { name = "peer" })
		say(support.player(1), "!slay peer")
		t.eq(hits, 0, "равный по весу должен быть защищён")
	end)

	t.it("младшего по весу тронуть можно", function()
		setup([[{
			["STEAM_0:0:1"] = { groups = { "admin" } },
			["STEAM_0:0:2"] = { groups = { "moderator" } },
		}]])
		local hits = 0
		cmd.add("slay", function() hits = hits + 1 end,
			{ perm = "admin.slay", target = 1 })

		support.player(2, { name = "junior" })
		say(support.player(1), "!slay junior")
		t.eq(hits, 1)
	end)

	t.it("immunity = false снимает проверку", function()
		setup([[{
			["STEAM_0:0:1"] = { groups = { "admin" } },
			["STEAM_0:0:2"] = { groups = { "admin" } },
		}]])
		local hits = 0
		cmd.add("swap", function() hits = hits + 1 end,
			{ perm = "admin.swap", target = 1, immunity = false })

		support.player(2, { name = "peer" })
		say(support.player(1), "!swap peer")
		t.eq(hits, 1)
	end)

	--------------------------------------------------------------------
	-- Реестр
	--------------------------------------------------------------------

	t.it("повторная регистрация заменяет обработчик", function()
		setup()
		local which
		cmd.add("go", function() which = "first" end)
		cmd.add("go", function() which = "second" end)

		say(support.player(1), "!go")
		t.eq(which, "second")
	end)

	t.it("remove снимает команду", function()
		setup()
		local hits = 0
		cmd.add("go", function() hits = hits + 1 end)
		t.ok(cmd.remove("go"))

		say(support.player(1), "!go")
		t.eq(hits, 0)
	end)

	t.it("remove несуществующей возвращает false", function()
		setup()
		t.no(cmd.remove("nope"))
	end)

	t.it("list отдаёт отсортированные имена", function()
		setup()
		cmd.add("zebra", function() end)
		cmd.add("alpha", function() end)

		local names = cmd.list()
		t.eq(names[1], "alpha")
		t.eq(names[#names], "zebra")
	end)

	t.it("имя не строка — ошибка при регистрации", function()
		setup()
		t.raises(function() cmd.add(42, function() end) end, "name")
	end)

	t.it("обработчик не функция — ошибка при регистрации", function()
		setup()
		t.raises(function() cmd.add("bad", "не функция") end, "handler")
	end)

	--------------------------------------------------------------------
	-- reply
	--------------------------------------------------------------------

	t.it("из чата reply уходит в чат тому же игроку", function()
		setup()
		cmd.add("ping", function(ctx) ctx.reply("pong") end)

		local p = support.player(1)
		say(p, "!ping")
		t.eq(p.said[1], "pong")
	end)

	t.it("из консоли reply уходит в консоль", function()
		setup()
		cmd.add("lua_ping", function(ctx) ctx.reply("pong") end, { source = "console" })

		support.console_commands["lua_ping"]({})
		t.eq(support.printed[#support.printed], "pong")
	end)
end
