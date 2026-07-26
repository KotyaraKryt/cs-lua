return function(t, support)
	local text = require("text")

	t.it("ascii считается побайтово", function()
		t.eq(text.width("hello"), 5)
	end)

	-- Ради этого модуль и существует: #s врёт и на тегах, и на кириллице.
	t.it("тег цвета занимает один байт на проводе, а не семь", function()
		t.eq(text.width("{green}"), 1)
		t.eq(#"{green}", 7)
	end)

	t.it("кириллица занимает один байт, а не два", function()
		t.eq(text.width("привет"), 6)
		t.eq(#"привет", 12)
	end)

	t.it("смешанная строка складывается верно", function()
		t.eq(text.width("{green}да{default}"), 1 + 2 + 1)
	end)

	t.it("fits меряет по каналу", function()
		t.ok(text.fits(("a"):rep(180), "chat"))
		t.no(text.fits(("a"):rep(200), "chat"))
		t.no(text.fits(("a"):rep(200), "dhud"))
	end)

	t.it("clip не трогает то, что влезает", function()
		t.eq(text.clip("коротко", "chat"), "коротко")
	end)

	t.it("clip режет длинное", function()
		local out = text.clip(("я"):rep(400), "chat")
		t.ok(text.fits(out, "chat"), "обрезанное всё ещё не влезает")
	end)

	t.it("clip не рвёт кириллический символ пополам", function()
		local out = text.clip(("я"):rep(400), "chat")
		-- Разорванный utf-8 дал бы нечётную длину в байтах.
		t.eq(#out % 2, 0)
	end)

	t.it("clip не рвёт тег цвета", function()
		local out = text.clip("{green}" .. ("a"):rep(400), "chat")
		t.no(out:find("{gree$"), "тег обрезан посередине")
	end)

	t.it("expand подставляет %name%", function()
		t.eq(text.expand("привет, %nick%", { nick = "котя" }), "привет, котя")
	end)

	t.it("expand оставляет неизвестный плейсхолдер как есть", function()
		t.eq(text.expand("%nope%", {}), "%nope%")
	end)

	t.it("mmss форматирует секунды", function()
		t.eq(text.mmss(125), "2:05")
		t.eq(text.mmss(0), "0:00")
		t.eq(text.mmss(60), "1:00")
	end)
end
