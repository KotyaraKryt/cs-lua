-- Разрешение прав — самый неочевидный код в проекте: четыре признака
-- приоритета, наследование групп, звёздочки, явные запреты и сроки. Ломается
-- он молча: админ просто перестаёт кого-то кикать.

local GROUPS = [[{
	default   = { weight = 0,   allow = { "chat.say" } },
	vip       = { weight = 10,  inherit = "default", allow = { "shop.vip.*" } },
	moderator = { weight = 30,  inherit = "vip", allow = { "admin.kick" } },
	admin     = { weight = 50,  inherit = "moderator", allow = { "admin.*" },
	              deny = { "admin.rcon" } },
	owner     = { weight = 100, inherit = "admin", allow = { "*" } },
}]]

return function(t, support)
	local function setup(users)
		support.reset()
		support.load_access(GROUPS, users)
	end

	--------------------------------------------------------------------
	-- База
	--------------------------------------------------------------------

	t.it("игрок без записи получает права default", function()
		setup("{}")
		local p = support.player(1)
		t.ok(p:can("chat.say"))
		t.no(p:can("admin.kick"))
	end)

	t.it("консоль может всё", function()
		setup("{}")
		t.ok(access.can(nil, "admin.rcon"))
	end)

	t.it("группа выдаёт свои ноды", function()
		setup([[{ ["STEAM_0:0:1"] = { groups = { "moderator" } } }]])
		t.ok(support.player(1):can("admin.kick"))
	end)

	t.it("наследование поднимает ноды снизу", function()
		setup([[{ ["STEAM_0:0:1"] = { groups = { "moderator" } } }]])
		local p = support.player(1)
		t.ok(p:can("shop.vip.buy"), "moderator наследует vip")
		t.ok(p:can("chat.say"), "и default через vip")
	end)

	t.it("звёздочка покрывает поддерево", function()
		setup([[{ ["STEAM_0:0:1"] = { groups = { "vip" } } }]])
		t.ok(support.player(1):can("shop.vip.buy"))
	end)

	t.it("одинокая звёздочка покрывает всё", function()
		setup([[{ ["STEAM_0:0:1"] = { groups = { "owner" } } }]])
		t.ok(support.player(1):can("что.угодно"))
	end)

	--------------------------------------------------------------------
	-- Разрешение конфликтов
	--------------------------------------------------------------------

	-- admin: allow admin.*, deny admin.rcon. Точность бьёт ширину.
	t.it("точная нода перебивает звёздочку в той же группе", function()
		setup([[{ ["STEAM_0:0:1"] = { groups = { "admin" } } }]])
		local p = support.player(1)
		t.ok(p:can("admin.ban"), "admin.* должно дать admin.ban")
		t.no(p:can("admin.rcon"), "deny admin.rcon точнее, чем allow admin.*")
	end)

	t.it("личная запись перебивает группу", function()
		setup([[{ ["STEAM_0:0:1"] = { groups = { "admin" }, allow = { "admin.rcon" } } }]])
		t.ok(support.player(1):can("admin.rcon"), "личный allow сильнее группового deny")
	end)

	t.it("личный запрет забирает то, что дала группа", function()
		setup([[{ ["STEAM_0:0:1"] = { groups = { "owner" }, deny = { "admin.ban" } } }]])
		local p = support.player(1)
		t.no(p:can("admin.ban"))
		t.ok(p:can("admin.kick"), "остальное у owner на месте")
	end)

	t.it("минус впереди ноды означает запрет", function()
		setup([[{ ["STEAM_0:0:1"] = { groups = { "owner" }, allow = { "-admin.ban" } } }]])
		t.no(support.player(1):can("admin.ban"))
	end)

	t.it("при равной точности выигрывает запрет", function()
		setup([[{ ["STEAM_0:0:1"] = { allow = { "shop.buy" }, deny = { "shop.buy" } } }]])
		t.no(support.player(1):can("shop.buy"))
	end)

	-- Документированный порядок: "shop.vip.buy" > "shop.vip.*" > "shop.*" > "*".
	t.it("одинокая звёздочка — самая слабая из выдач", function()
		setup([[{ ["STEAM_0:0:1"] = { allow = { "*" }, deny = { "admin.rcon" } } }]])
		local p = support.player(1)
		t.ok(p:can("admin.kick"), "* должна покрывать всё остальное")
		t.no(p:can("admin.rcon"), "точная нода должна перебить *")
	end)

	-- Голая "*" слабее любой звёздочки с префиксом: precision("*") = 0,
	-- precision("admin.*") = 2.
	t.it("звёздочка с префиксом перебивает голую", function()
		setup([[{ ["STEAM_0:0:1"] = { allow = { "*" }, deny = { "admin.*" } } }]])
		local p = support.player(1)
		t.ok(p:can("chat.say"), "* должна работать вне admin")
		t.no(p:can("admin.kick"), "deny admin.* точнее, чем allow *")
	end)

	t.it("звёздочка глубже перебивает звёздочку выше", function()
		setup([[{ ["STEAM_0:0:1"] = { allow = { "shop.*" }, deny = { "shop.vip.*" } } }]])
		local p = support.player(1)
		t.ok(p:can("shop.buy"))
		t.no(p:can("shop.vip.buy"), "shop.vip.* точнее, чем shop.*")
	end)

	-- Источник сильнее точности: своя группа бьёт унаследованную, даже если
	-- та выдала более точную ноду.
	t.it("своя группа перебивает унаследованную", function()
		setup([[{ ["STEAM_0:0:1"] = { groups = { "owner" } } }]])
		t.ok(support.player(1):can("admin.rcon"),
			"allow * у owner сильнее deny admin.rcon у наследуемого admin")
	end)

	t.it("порядок ключей в файле ни на что не влияет", function()
		setup([[{ ["STEAM_0:0:1"] = { deny = { "admin.ban" }, groups = { "owner" } } }]])
		t.no(support.player(1):can("admin.ban"))
	end)

	--------------------------------------------------------------------
	-- Иммунитет
	--------------------------------------------------------------------

	t.it("вес берётся из самой тяжёлой группы", function()
		setup([[{ ["STEAM_0:0:1"] = { groups = { "vip", "admin" } } }]])
		t.eq(support.player(1):weight(), 50)
	end)

	t.it("старший по весу перебивает младшего", function()
		setup([[{
			["STEAM_0:0:1"] = { groups = { "admin" } },
			["STEAM_0:0:2"] = { groups = { "vip" } },
		}]])
		t.ok(support.player(1):outranks(support.player(2)))
	end)

	-- Два админа одного ранга не должны трогать друг друга.
	t.it("равные по весу друг друга не перебивают", function()
		setup([[{
			["STEAM_0:0:1"] = { groups = { "admin" } },
			["STEAM_0:0:2"] = { groups = { "admin" } },
		}]])
		t.no(support.player(1):outranks(support.player(2)))
	end)

	t.it("консоль перебивает кого угодно", function()
		setup([[{ ["STEAM_0:0:1"] = { groups = { "owner" } } }]])
		t.ok(access.outranks(nil, support.player(1)))
	end)

	--------------------------------------------------------------------
	-- Группы
	--------------------------------------------------------------------

	t.it("p:group видит унаследованную группу", function()
		setup([[{ ["STEAM_0:0:1"] = { groups = { "moderator" } } }]])
		local p = support.player(1)
		t.ok(p:group("moderator"))
		t.ok(p:group("vip"), "moderator наследует vip")
		t.no(p:group("admin"))
	end)

	t.it("несуществующая группа в записи не роняет игрока", function()
		setup([[{ ["STEAM_0:0:1"] = { groups = { "nosuchgroup" } } }]])
		t.ok(support.player(1):can("chat.say"), "default должен остаться")
	end)

	--------------------------------------------------------------------
	-- Ограничение по карте
	--------------------------------------------------------------------

	t.it("where.map держит права только на своей карте", function()
		setup([[{ ["STEAM_0:0:1"] = { groups = { "admin" }, where = { map = "de_dust2" } } }]])
		local p = support.player(1)
		t.ok(p:can("admin.kick"))

		support.map = "de_inferno"
		access.invalidate()
		t.no(p:can("admin.kick"), "на другой карте прав быть не должно")
	end)

	--------------------------------------------------------------------
	-- Объявление нод
	--------------------------------------------------------------------

	t.it("declare кладёт ноду в реестр", function()
		setup("{}")
		access.declare("shop.buy", { desc = "Покупка" })
		t.eq(access.permissions()["shop.buy"].desc, "Покупка")
	end)

	t.it("default в declare раздаёт ноду группе", function()
		setup([[{ ["STEAM_0:0:1"] = { groups = { "vip" } } }]])
		access.declare("shop.discount", { default = "vip" })
		t.ok(support.player(1):can("shop.discount"))
		t.no(support.player(2):can("shop.discount"), "не-vip не должен получить")
	end)

	t.it("default = true раздаёт ноду всем", function()
		setup("{}")
		access.declare("chat.emoji", { default = true })
		t.ok(support.player(1):can("chat.emoji"))
	end)

	-- Иначе default втихую вернул бы право, которое админ явно забрал.
	t.it("явный запрет сильнее default из declare", function()
		setup([[{ ["STEAM_0:0:1"] = { deny = { "chat.emoji" } } }]])
		access.declare("chat.emoji", { default = true })
		t.no(support.player(1):can("chat.emoji"))
	end)

	t.it("declare со звёздочкой отвергается", function()
		setup("{}")
		t.raises(function() access.declare("shop.*") end, "wildcard")
	end)

	--------------------------------------------------------------------
	-- Динамические правила
	--------------------------------------------------------------------

	t.it("правило решает, когда ноду не покрыл никто", function()
		setup("{}")
		local seen
		access.rule("shop.buy", function(p) seen = p; return true end)

		local p = support.player(1)
		t.ok(p:can("shop.buy"))
		t.eq(seen, p, "правилу должны передать игрока")
	end)

	t.it("явный запрет сильнее правила", function()
		setup([[{ ["STEAM_0:0:1"] = { deny = { "shop.buy" } } }]])
		access.rule("shop.buy", function() return true end)
		t.no(support.player(1):can("shop.buy"))
	end)

	--------------------------------------------------------------------
	-- Выдача из кода
	--------------------------------------------------------------------

	t.it("grant действует сразу", function()
		setup("{}")
		local p = support.player(1)
		t.no(p:can("admin.kick"))

		access.grant(p:steamid(), { groups = "moderator" })
		access.invalidate()
		t.ok(p:can("admin.kick"))
	end)

	t.it("revoke забирает группу", function()
		setup([[{ ["STEAM_0:0:1"] = { groups = { "moderator" } } }]])
		local p = support.player(1)

		access.revoke(p:steamid(), "moderator")
		access.invalidate()
		t.no(p:can("admin.kick"))
	end)

	t.it("revoke без аргументов сносит запись целиком", function()
		setup([[{ ["STEAM_0:0:1"] = { groups = { "owner" } } }]])
		local p = support.player(1)

		access.revoke(p:steamid())
		access.invalidate()
		t.eq(access.user(p:steamid()), nil)
		t.no(p:can("admin.kick"))
	end)

	t.it("save записывает users.lua, который читается обратно", function()
		setup("{}")
		local p = support.player(1)
		access.grant(p:steamid(), { groups = "vip" })
		t.ok(access.save())

		access.reload()
		access.invalidate()
		t.ok(p:can("shop.vip.buy"))
	end)

	t.it("save/reload переживают смешанный список групп со сроками", function()
		setup("{}")
		local p = support.player(1)
		access.grant(p:steamid(), { groups = "vip" })						-- бессрочно
		access.grant(p:steamid(), { groups = "moderator", until_ = os.time() + 100 })	-- со сроком
		t.ok(access.save())

		access.reload()
		access.invalidate()
		t.ok(p:can("shop.vip.buy"), "vip должен пережить перезагрузку файла")
		t.ok(p:can("admin.kick"), "moderator ещё не истёк - должен тоже остаться")

		local e = access.user(p:steamid())
		local by_name = {}
		for _, rec in ipairs(e.groups) do
			by_name[rec.name] = rec
		end
		t.eq(by_name.vip.until_, nil)
		t.ok(type(by_name.moderator.until_) == "number")
	end)

	--------------------------------------------------------------------
	-- Срок по группам
	--
	-- Раньше until_ был один на всю запись: несколько access.grant подряд
	-- (GameCMS - по вызову на каждую купленную услугу) означали, что
	-- последний вызов с реальным сроком тихо переписывал срок вообще всех
	-- групп на этом ключе, включая бессрочные.
	--------------------------------------------------------------------

	t.it("срок одной группы не трогает бессрочность другой", function()
		setup("{}")
		local p = support.player(1)

		access.grant(p:steamid(), { groups = "vip" })
		access.grant(p:steamid(), { groups = "moderator", until_ = 30 })
		access.invalidate()

		local e = access.user(p:steamid())
		local by_name = {}
		for _, rec in ipairs(e.groups) do
			by_name[rec.name] = rec
		end
		t.eq(by_name.vip.until_, nil, "vip выдавался без срока")
		t.eq(by_name.moderator.until_, 30, "moderator должен хранить свой срок")
	end)

	t.it("просроченная группа отваливается, вечная остаётся", function()
		setup("{}")
		local p = support.player(1)

		access.grant(p:steamid(), { groups = "vip" })
		access.grant(p:steamid(), { groups = "moderator", until_ = os.time() - 10 })
		access.invalidate()

		t.ok(p:can("shop.vip.buy"), "vip бессрочный, должен остаться")
		t.no(p:can("admin.kick"), "moderator уже истёк")
	end)

	t.it("повторная выдача без until_ сохраняет прежний срок группы", function()
		setup("{}")
		local p = support.player(1)

		access.grant(p:steamid(), { groups = "vip", until_ = 12345 })
		access.grant(p:steamid(), { groups = "vip" })	-- напр. gamecms.reload_services без нового until_
		access.invalidate()

		local e = access.user(p:steamid())
		t.eq(e.groups[1].until_, 12345, "срок не должен слетать без явного until_")
	end)

	t.it("until_ у allow/deny не трогает срок групп", function()
		setup("{}")
		local p = support.player(1)

		access.grant(p:steamid(), { groups = "vip" })
		access.grant(p:steamid(), { allow = "admin.kick", until_ = os.time() - 10 })
		access.invalidate()

		t.ok(p:can("shop.vip.buy"), "vip не должен истечь из-за чужого until_")
		t.no(p:can("admin.kick"), "личный allow истёк")
	end)

	--------------------------------------------------------------------
	-- Кеш
	--------------------------------------------------------------------

	-- Кеш на слот — то место, где право «залипает» после выдачи.
	t.it("invalidate сбрасывает кеш конкретного игрока", function()
		setup("{}")
		local p = support.player(1)
		p:can("admin.kick")

		access.grant(p:steamid(), { allow = "admin.kick" })
		access.invalidate(p)
		t.ok(p:can("admin.kick"))
	end)

	t.it("declare сбрасывает кеш сам", function()
		setup("{}")
		local p = support.player(1)
		t.no(p:can("shop.gift"))

		access.declare("shop.gift", { default = true })
		t.ok(p:can("shop.gift"), "новый default должен подействовать без invalidate")
	end)

	--------------------------------------------------------------------
	-- Устойчивость файлов
	--------------------------------------------------------------------

	t.it("без groups.lua работают встроенные группы", function()
		support.reset()
		support.load_access(nil, [[{ ["STEAM_0:0:1"] = { groups = { "owner" } } }]])
		t.ok(support.player(1):can("admin.kick"))
	end)

	t.it("битый users.lua не выдаёт прав молча", function()
		support.reset()
		local root = support.tmpdir()
		support.write(root .. "/data" , "")
		support.load_access(GROUPS, nil)
		t.no(support.player(1):can("admin.kick"))
	end)
end
