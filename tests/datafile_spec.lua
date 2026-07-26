return function(t, support)
	local datafile = require("datafile")

	local function fresh()
		return datafile.at(support.tmpdir())
	end

	local function read(path)
		local fh = io.open(path, "r")
		if not fh then return nil end
		local text = fh:read("*a")
		fh:close()
		return text
	end

	t.it("отсутствующий файл отдаёт fallback, а не ошибку", function()
		local files = fresh()
		local data = files.load("nope", { default = true })
		t.eq(data.default, true)
	end)

	t.it("записанное читается обратно", function()
		local files = fresh()
		t.ok(files.save("stock", { ak = 2500, awp = 4750 }))

		local data = files.load("stock")
		t.eq(data.ak, 2500)
		t.eq(data.awp, 4750)
	end)

	t.it("вложенные таблицы переживают круг", function()
		local files = fresh()
		files.save("cfg", { hud = { x = -1, color = { 255, 0, 0 } } })

		local data = files.load("cfg")
		t.eq(data.hud.x, -1)
		t.eq(data.hud.color[1], 255)
	end)

	t.it("массив остаётся массивом", function()
		local files = fresh()
		files.save("list", { "a", "b", "c" })

		local data = files.load("list")
		t.eq(#data, 3)
		t.eq(data[2], "b")
	end)

	t.it("строки с кавычками и переводами строк не ломают файл", function()
		local files = fresh()
		files.save("weird", { s = 'он сказал "да"\nи ушёл' })

		local data = files.load("weird")
		t.eq(data.s, 'он сказал "да"\nи ушёл')
	end)

	-- Иначе каждая запись выглядела бы в git как изменение всего файла.
	t.it("две записи одних данных дают побайтово одинаковый файл", function()
		local files = fresh()
		local payload = { zebra = 1, alpha = 2, middle = 3, nested = { b = 1, a = 2 } }

		files.save("same", payload)
		local first = read(files.dir() .. "/same.lua")
		files.save("same", payload)
		local second = read(files.dir() .. "/same.lua")

		t.eq(first, second)
	end)

	t.it("ключи сортируются", function()
		local files = fresh()
		files.save("ordered", { zebra = 1, alpha = 2 })

		local text = read(files.dir() .. "/ordered.lua")
		t.ok(text:find("alpha") < text:find("zebra"), "alpha должен идти первым")
	end)

	t.it("header попадает в файл комментарием", function()
		local files = fresh()
		files.save("titled", { a = 1 }, "Заголовок файла")

		local text = read(files.dir() .. "/titled.lua")
		t.ok(text:find("Заголовок файла", 1, true))
	end)

	-- Падение сервера посреди записи не должно оставлять полфайла.
	t.it("после записи остаётся .bak с прошлой версией", function()
		local files = fresh()
		files.save("hist", { v = 1 })
		files.save("hist", { v = 2 })

		t.eq(files.load("hist").v, 2)
		t.ok(read(files.dir() .. "/hist.lua.bak"), ".bak не создан")
	end)

	t.it("временный файл после записи не остаётся", function()
		local files = fresh()
		files.save("clean", { a = 1 })
		t.eq(read(files.dir() .. "/clean.lua.tmp"), nil)
	end)

	t.it("функцию сохранить нельзя — это ошибка, а не тихая потеря", function()
		local files = fresh()
		local ok, err = files.save("bad", { fn = function() end })
		t.no(ok)
		t.ok(tostring(err):find("function", 1, true))
	end)

	t.it("битый файл — ошибка вторым значением, а не пустая таблица", function()
		local files = fresh()
		support.write(files.dir() .. "/broken.lua", "return { this is not lua")

		local data, err = files.load("broken")
		t.eq(data, nil)
		t.ok(err)
	end)

	t.it("файл не с таблицей — ошибка", function()
		local files = fresh()
		support.write(files.dir() .. "/scalar.lua", "return 42")

		local data, err = files.load("scalar")
		t.eq(data, nil)
		t.ok(tostring(err):find("table", 1, true))
	end)

	-- Файл данных — данные: он выполняется, поэтому не должен дотягиваться до io.
	t.it("файл данных выполняется в пустом окружении", function()
		local files = fresh()
		support.write(files.dir() .. "/evil.lua",
			"os.exit(1) return {}")

		local data, err = files.load("evil")
		t.eq(data, nil)
		t.ok(err, "обращение к os должно было упасть")
	end)

	t.it("serialize отдаёт текст, ничего не записывая", function()
		local files = fresh()
		local text = files.serialize({ a = 1 })
		t.ok(text:find("a = 1", 1, true))
		t.eq(read(files.dir() .. "/a.lua"), nil)
	end)
end
