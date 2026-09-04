return function(t, support)
	local color = require("color")

	t.it("имя из палитры разбирается в rgb", function()
		local rgb = color.parse("green")
		t.eq(rgb[1], 0)
		t.eq(rgb[2], 255)
		t.eq(rgb[3], 0)
	end)

	t.it("hex разбирается в rgb", function()
		local rgb = color.parse("#ffa000")
		t.eq(rgb[1], 255)
		t.eq(rgb[2], 160)
		t.eq(rgb[3], 0)
	end)

	t.it("готовая таблица проходит насквозь", function()
		local rgb = color.parse({ 1, 2, 3 })
		t.eq(rgb[1], 1)
		t.eq(rgb[3], 3)
	end)

	t.it("gray — синоним grey", function()
		local a, b = color.parse("gray"), color.parse("grey")
		t.eq(a[1], b[1])
	end)

	t.it("регистр имени не важен", function()
		t.ok(color.parse("GREEN"))
	end)

	t.it("неизвестное имя возвращает ошибку со списком известных", function()
		local rgb, err = color.parse("mauve")
		t.eq(rgb, nil)
		t.ok(err:find("green", 1, true), "в подсказке нет известных имён")
	end)

	t.it("таблица не из трёх чисел — ошибка", function()
		local rgb, err = color.parse({ 1, 2 })
		t.eq(rgb, nil)
		t.ok(err)
	end)

	t.it("число вместо цвета — ошибка с указанием типа", function()
		local rgb, err = color.parse(42)
		t.eq(rgb, nil)
		t.ok(err:find("number", 1, true))
	end)

	--------------------------------------------------------------------
	-- Деградация под канал
	--------------------------------------------------------------------

	t.it("имя превращается в код меню", function()
		t.eq(color.menu("red"), "\\r")
		t.eq(color.menu("white"), "\\w")
		t.eq(color.menu("yellow"), "\\y")
		t.eq(color.menu("grey"), "\\d")
	end)

	t.it("сырой код AMX Mod X проходит насквозь", function()
		t.eq(color.menu("\\y"), "\\y")
	end)

	t.it("неизвестный сырой код — ошибка", function()
		local code, err = color.menu("\\q")
		t.eq(code, nil)
		t.ok(err)
	end)

	-- Ради этого вся затея: цвет пишется один раз, а канал берёт ближайшее.
	t.it("произвольный rgb садится на ближайший код меню", function()
		t.eq(color.menu("#ffffff"), "\\w")
		t.eq(color.menu({ 250, 10, 10 }), "\\r")
		t.eq(color.menu({ 150, 150, 150 }), "\\d")
	end)

	t.it("чат знает только свои два цвета", function()
		t.eq(color.chat("green"), "{green}")
		t.eq(color.chat("yellow"), "{default}")
	end)

	t.it("team в чате — не цвет, а контекст", function()
		t.eq(color.chat("team"), "{team}")
	end)

	t.it("цвет без пары в чате садится на ближайший", function()
		-- Синего в чате нет вообще; важно, что вернётся код, а не nil.
		local code = color.chat("blue")
		t.ok(code == "{green}" or code == "{default}")
	end)

	t.it("color.rgb — это parse", function()
		t.eq(color.rgb, color.parse)
	end)

	--------------------------------------------------------------------
	-- strip_chat
	--------------------------------------------------------------------

	t.it("strip_chat убирает все теги чата", function()
		t.eq(color.strip_chat("{green}~ {default}До конца карты: {team}15:00"),
			"~ До конца карты: 15:00")
	end)

	t.it("strip_chat не трогает текст без тегов", function()
		t.eq(color.strip_chat("обычный текст"), "обычный текст")
	end)

	t.it("strip_chat не трогает фигурные скобки, которые не тег", function()
		t.eq(color.strip_chat("{unknown} {notacolor}"), "{unknown} {notacolor}")
	end)
end
