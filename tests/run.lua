-- Прогон юнит-тестов Lua-слоя.
--
--     luajit tests/run.lua              всё
--     luajit tests/run.lua access       только tests/access_spec.lua
--
-- Тестируется то, что не требует игры: core/ и include/ — обычный Lua, а
-- пространства имён движка подменяются стабами из tests/support/stubs.lua.
-- Сюда попадает разрешение прав, роутер команд, разбор цвета, подсчёт длины
-- строки и запись данных — то есть код, который ломается молча.
--
-- C++-поверхность так не проверить: для неё есть plugins/_selftest.lua,
-- который гоняется на живом сервере.

local root = arg[0]:gsub("[/\\]tests[/\\]run%.lua$", "")
if root == arg[0] then
	root = "."
end

package.path = table.concat({
	root .. "/tests/?.lua",
	root .. "/scripts/include/?.lua",
	package.path,
}, ";")

-- Стабы подменяют глобальный print, чтобы ловить вывод тестируемого кода.
-- Свой отчёт харнесс печатает мимо них.
local out = print

local support = require("support.stubs")
support.root = root

--------------------------------------------------------------------------
-- Харнесс
--------------------------------------------------------------------------

local passed, failed, current = 0, 0, nil
local failures = {}

local function fail(message, level)
	failed = failed + 1
	local where = debug.getinfo(level or 3, "Sl")
	failures[#failures + 1] = ("%s\n      %s:%d: %s")
		:format(current, where.short_src, where.currentline, message)
end

local T = {}

-- Печатается только имя упавшего теста, поэтому оно должно читаться как
-- утверждение: «grant на карте не действует на другой».
function T.it(name, fn)
	current = name
	local ok, err = pcall(fn)
	if ok then
		passed = passed + 1
	else
		failed = failed + 1
		failures[#failures + 1] = ("%s\n      %s"):format(name, tostring(err))
	end
end

function T.eq(actual, expected, note)
	if actual ~= expected then
		fail(("ожидалось %s, получено %s%s"):format(
			tostring(expected), tostring(actual), note and (" (" .. note .. ")") or ""))
	end
end

function T.ok(value, note)
	if not value then
		fail("ожидалось истинное значение" .. (note and (": " .. note) or ""))
	end
end

function T.no(value, note)
	if value then
		fail(("ожидалось ложное значение, получено %s%s")
			:format(tostring(value), note and (": " .. note) or ""))
	end
end

-- Проверяет не только что упало, но и чем: сообщение об ошибке — часть API.
function T.raises(fn, pattern)
	local ok, err = pcall(fn)
	if ok then
		return fail("ожидалась ошибка, вызов прошёл")
	end
	if pattern and not tostring(err):find(pattern, 1, true) then
		fail(("ошибка не содержит %q: %s"):format(pattern, tostring(err)))
	end
end

--------------------------------------------------------------------------

local SPECS = {
	"color", "text", "json", "datafile", "access", "commands",
}

local only = arg[1]

for _, name in ipairs(SPECS) do
	if not only or name == only then
		support.reset()
		local spec = require(name .. "_spec")
		spec(T, support)
	end
end

if failed == 0 then
	out(("tests: %d passed"):format(passed))
	os.exit(0)
end

out(("tests: %d passed, %d FAILED"):format(passed, failed))
for _, f in ipairs(failures) do
	out("  - " .. f)
end
os.exit(1)
