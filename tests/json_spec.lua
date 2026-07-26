-- JSON разбирает то, что прислал чужой сервер. Половина тестов ниже — про
-- битый ввод: он обязан возвращать причину, а не падать и не отдавать
-- полутаблицу.

return function(t, support)
	local json = require("json")

	--------------------------------------------------------------------
	-- Сборка
	--------------------------------------------------------------------

	t.it("скаляры", function()
		t.eq(json.encode(42), "42")
		t.eq(json.encode(-1.5), "-1.5")
		t.eq(json.encode(true), "true")
		t.eq(json.encode(false), "false")
		t.eq(json.encode("hi"), '"hi"')
	end)

	t.it("целое без хвоста .0", function()
		t.eq(json.encode(3), "3")
		t.eq(json.encode(1e6), "1000000")
	end)

	t.it("массив", function()
		t.eq(json.encode({ 1, 2, 3 }), "[1,2,3]")
	end)

	t.it("объект с сортированными ключами", function()
		t.eq(json.encode({ b = 1, a = 2 }), '{"a":2,"b":1}')
	end)

	t.it("пустая таблица уходит объектом", function()
		t.eq(json.encode({}), "{}")
	end)

	t.it("вложенность", function()
		t.eq(json.encode({ a = { 1, 2 } }), '{"a":[1,2]}')
	end)

	t.it("кавычки и слэши экранируются", function()
		t.eq(json.encode('он "сказал" \\'), '"он \\"сказал\\" \\\\"')
	end)

	t.it("перевод строки экранируется", function()
		t.eq(json.encode("a\nb"), '"a\\nb"')
	end)

	-- Иначе получился бы текст, который не читает никто на той стороне.
	t.it("управляющий символ экранируется как \\u", function()
		t.eq(json.encode("a\1b"), '"a\\u0001b"')
	end)

	t.it("кириллица уходит как есть", function()
		t.eq(json.encode("привет"), '"привет"')
	end)

	t.it("две сборки одних данных дают один текст", function()
		local data = { zebra = 1, alpha = 2, nested = { b = 1, a = 2 } }
		t.eq(json.encode(data), json.encode(data))
	end)

	t.it("функция не сериализуется", function()
		local text, err = json.encode({ fn = function() end })
		t.eq(text, nil)
		t.ok(tostring(err):find("function", 1, true))
	end)

	t.it("цикл ловится, а не вешает", function()
		local a = {}
		a.self = a
		local text, err = json.encode(a)
		t.eq(text, nil)
		t.ok(err)
	end)

	t.it("inf и nan отвергаются", function()
		t.eq(json.encode(math.huge), nil)
		t.eq(json.encode(-math.huge), nil)
		t.eq(json.encode(0 / 0), nil)
	end)

	--------------------------------------------------------------------
	-- Разбор
	--------------------------------------------------------------------

	t.it("скаляры разбираются", function()
		t.eq(json.decode("42"), 42)
		t.eq(json.decode("-1.5"), -1.5)
		t.eq(json.decode("true"), true)
		t.eq(json.decode("false"), false)
		t.eq(json.decode('"hi"'), "hi")
	end)

	t.it("экспонента", function()
		t.eq(json.decode("1e3"), 1000)
		t.eq(json.decode("1.5e-2"), 0.015)
	end)

	t.it("массив", function()
		local v = json.decode("[1,2,3]")
		t.eq(#v, 3)
		t.eq(v[2], 2)
	end)

	t.it("объект", function()
		local v = json.decode('{"a":1,"b":"two"}')
		t.eq(v.a, 1)
		t.eq(v.b, "two")
	end)

	t.it("пустые массив и объект", function()
		t.eq(#json.decode("[]"), 0)
		t.eq(next(json.decode("{}")), nil)
	end)

	t.it("вложенность", function()
		local v = json.decode('{"a":{"b":[1,{"c":2}]}}')
		t.eq(v.a.b[2].c, 2)
	end)

	t.it("пробелы и переводы строк не мешают", function()
		local v = json.decode('  {\n  "a" : 1 ,\n "b":2\n}  ')
		t.eq(v.a, 1)
		t.eq(v.b, 2)
	end)

	t.it("escape-последовательности", function()
		t.eq(json.decode('"a\\nb"'), "a\nb")
		t.eq(json.decode('"a\\"b"'), 'a"b')
		t.eq(json.decode('"a\\\\b"'), "a\\b")
		t.eq(json.decode('"a\\/b"'), "a/b")
	end)

	t.it("\\u в UTF-8", function()
		t.eq(json.decode('"\\u0041"'), "A")
		t.eq(json.decode('"\\u043f"'), "п")
	end)

	-- Без склейки суррогатов эмодзи приезжает битым.
	t.it("суррогатная пара склеивается", function()
		local v = json.decode('"\\ud83d\\ude00"')
		t.eq(#v, 4, "должно быть 4 байта UTF-8")
	end)

	t.it("кириллица насквозь", function()
		t.eq(json.decode('"привет"'), "привет")
	end)

	--------------------------------------------------------------------
	-- null
	--------------------------------------------------------------------

	-- Если null становился бы nil, ключ просто исчезал из таблицы и
	-- "поля не было" было бы не отличить от "поле пришло пустым".
	t.it("null отличим от отсутствия ключа", function()
		local v = json.decode('{"a":null}')
		t.eq(v.a, json.null)
		t.no(v.a == nil)
		t.eq(v.b, nil)
	end)

	--------------------------------------------------------------------
	-- Битый ввод
	--------------------------------------------------------------------

	local function broken(text, what)
		local v, err = json.decode(text)
		t.eq(v, nil, what)
		t.ok(err, what)
	end

	t.it("обрезанный объект", function() broken('{"a":1', "обрезан") end)
	t.it("обрезанный массив", function() broken("[1,2", "обрезан") end)
	t.it("незакрытая строка", function() broken('"abc', "строка") end)
	t.it("мусор после значения", function() broken("1 2", "мусор") end)
	t.it("пустой ввод", function() broken("", "пусто") end)
	t.it("не строка на входе", function() broken(nil, "nil") end)
	t.it("ключ не строка", function() broken("{a:1}", "ключ") end)
	t.it("висящая запятая", function() broken('{"a":1,}', "запятая") end)
	t.it("одинарные кавычки", function() broken("{'a':1}", "кавычки") end)
	t.it("битый \\u", function() broken('"\\uZZZZ"', "\\u") end)
	t.it("неизвестный escape", function() broken('"a\\qb"', "escape") end)

	t.it("сообщение об ошибке называет позицию", function()
		local _, err = json.decode('{"a":1 "b":2}')
		t.ok(tostring(err):find("позиции", 1, true), tostring(err))
	end)

	--------------------------------------------------------------------
	-- Круг
	--------------------------------------------------------------------

	t.it("данные переживают encode -> decode", function()
		local original = {
			name = "котя",
			kills = 12,
			ratio = 1.5,
			alive = true,
			weapons = { "ak47", "deagle" },
			nested = { hits = { head = 3, chest = 7 } },
		}

		local back = json.decode(json.encode(original))
		t.eq(back.name, "котя")
		t.eq(back.kills, 12)
		t.eq(back.ratio, 1.5)
		t.eq(back.alive, true)
		t.eq(back.weapons[2], "deagle")
		t.eq(back.nested.hits.head, 3)
	end)

	t.it("похоже на ответ настоящего API", function()
		local body = [[
			{"ok":true,"result":[{"id":1,"name":"a"},{"id":2,"name":"b"}],"next":null}
		]]
		local v = json.decode(body)
		t.eq(v.ok, true)
		t.eq(#v.result, 2)
		t.eq(v.result[2].name, "b")
		t.eq(v.next, json.null)
	end)
end
