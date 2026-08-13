-- JSON: разбор и сборка.
--
--   local json = require("json")
--
--   local data, err = json.decode(res.body)
--   local text = json.encode({ name = p:name(), kills = 12 })
--
-- Существует ради [`http`](../docs/api/http/index.md): почти всё, что отвечает
-- по сети, отвечает JSON'ом, а без разбора ответ остаётся строкой.
--
-- Обе стороны не бросают исключений: `decode` возвращает `nil, причина`,
-- `encode` — `nil, причина`. Разбирать приходится то, что прислал чужой сервер,
-- и падать на этом плагину незачем.

local json = {}

--------------------------------------------------------------------------
-- Сборка
--------------------------------------------------------------------------

local ESCAPE = {
	['"']    = '\\"',
	['\\']   = '\\\\',
	['\b']   = '\\b',
	['\f']   = '\\f',
	['\n']   = '\\n',
	['\r']   = '\\r',
	['\t']   = '\\t',
}

local function escape(c)
	return ESCAPE[c] or ("\\u%04x"):format(c:byte())
end

local function quote(s)
	-- Управляющие символы обязаны быть экранированы, иначе получится JSON,
	-- который никто на той стороне не прочитает.
	return '"' .. s:gsub('[%c"\\]', escape) .. '"'
end

-- Массив или объект. Пустая таблица уходит объектом: {} читается обеими
-- сторонами, а [] сузило бы тип на ровном месте.
local function is_array(t)
	local n = 0
	for k in pairs(t) do
		if type(k) ~= "number" then
			return false
		end
		n = n + 1
	end
	return n == #t and n > 0
end

local encode_value

local function encode_table(t, seen, out)
	if seen[t] then
		error("json.encode: таблица ссылается сама на себя", 0)
	end
	seen[t] = true

	if is_array(t) then
		out[#out + 1] = "["
		for i = 1, #t do
			if i > 1 then out[#out + 1] = "," end
			encode_value(t[i], seen, out)
		end
		out[#out + 1] = "]"
	else
		-- Ключи сортируются: две сборки одних данных дают одинаковый текст,
		-- что важно и для тестов, и для diff'а в логе.
		--
		-- Значение хранится тут же, а не восстанавливается по ключу после
		-- сортировки: `t[k] ~= nil and t[k] or t[tonumber(k)]` держал ровно
		-- одно значение - false - которое `and/or` не отличает от отсутствия,
		-- и превращал его в null.
		local keys = {}
		for k, v in pairs(t) do
			if type(k) == "string" or type(k) == "number" then
				keys[#keys + 1] = { text = tostring(k), value = v }
			else
				error("json.encode: ключ типа " .. type(k), 0)
			end
		end
		table.sort(keys, function(a, b) return a.text < b.text end)

		out[#out + 1] = "{"
		for i, entry in ipairs(keys) do
			if i > 1 then out[#out + 1] = "," end
			out[#out + 1] = quote(entry.text)
			out[#out + 1] = ":"
			encode_value(entry.value, seen, out)
		end
		out[#out + 1] = "}"
	end

	seen[t] = nil
end

encode_value = function(v, seen, out)
	local kind = type(v)

	if v == nil then
		out[#out + 1] = "null"
	elseif kind == "boolean" then
		out[#out + 1] = tostring(v)
	elseif kind == "number" then
		-- В JSON нет ни inf, ни nan: молча превратить их в null означало бы
		-- отправить чужому серверу не то, что просили.
		if v ~= v or v == math.huge or v == -math.huge then
			error("json.encode: " .. tostring(v) .. " в JSON не представим", 0)
		end
		-- %.14g отдаёт целые без хвоста ".0" и не теряет точность double.
		out[#out + 1] = ("%.14g"):format(v)
	elseif kind == "string" then
		out[#out + 1] = quote(v)
	elseif kind == "table" then
		encode_table(v, seen, out)
	else
		error("json.encode: " .. kind .. " не сериализуется", 0)
	end
end

-- json.encode(value) -> string | nil, причина
function json.encode(value)
	local out = {}
	local ok, err = pcall(encode_value, value, {}, out)
	if not ok then
		return nil, err
	end
	return table.concat(out)
end

--------------------------------------------------------------------------
-- Разбор
--------------------------------------------------------------------------

local Parser = {}
Parser.__index = Parser

function Parser:fail(what)
	error(("json.decode: %s на позиции %d"):format(what, self.pos), 0)
end

function Parser:skip()
	local _, stop = self.text:find("^[ \t\r\n]+", self.pos)
	if stop then
		self.pos = stop + 1
	end
end

function Parser:literal(word, value)
	if self.text:sub(self.pos, self.pos + #word - 1) ~= word then
		self:fail("ожидалось " .. word)
	end
	self.pos = self.pos + #word
	return value
end

function Parser:number()
	local text = self.text:match("^-?%d+%.?%d*[eE]?[-+]?%d*", self.pos)
	if not text or text == "" then
		self:fail("не число")
	end

	local value = tonumber(text)
	if not value then
		self:fail("не число")
	end

	self.pos = self.pos + #text
	return value
end

local UNESCAPE = {
	['"'] = '"', ['\\'] = '\\', ['/'] = '/',
	b = '\b', f = '\f', n = '\n', r = '\r', t = '\t',
}

-- \uXXXX в UTF-8. Суррогатные пары склеиваются: без этого эмодзи и всё за
-- пределами BMP приезжает битым.
function Parser:utf8_escape()
	local hex = self.text:sub(self.pos, self.pos + 3)
	if not hex:match("^%x%x%x%x$") then
		self:fail("битый \\u")
	end
	self.pos = self.pos + 4

	local cp = tonumber(hex, 16)

	if cp >= 0xD800 and cp <= 0xDBFF then
		if self.text:sub(self.pos, self.pos + 1) == "\\u" then
			local low = self.text:sub(self.pos + 2, self.pos + 5)
			if low:match("^%x%x%x%x$") then
				local lo = tonumber(low, 16)
				if lo >= 0xDC00 and lo <= 0xDFFF then
					self.pos = self.pos + 6
					cp = 0x10000 + (cp - 0xD800) * 0x400 + (lo - 0xDC00)
				end
			end
		end
	end

	if cp < 0x80 then
		return string.char(cp)
	elseif cp < 0x800 then
		return string.char(0xC0 + math.floor(cp / 0x40), 0x80 + cp % 0x40)
	elseif cp < 0x10000 then
		return string.char(
			0xE0 + math.floor(cp / 0x1000),
			0x80 + math.floor(cp / 0x40) % 0x40,
			0x80 + cp % 0x40)
	end

	return string.char(
		0xF0 + math.floor(cp / 0x40000),
		0x80 + math.floor(cp / 0x1000) % 0x40,
		0x80 + math.floor(cp / 0x40) % 0x40,
		0x80 + cp % 0x40)
end

function Parser:string()
	self.pos = self.pos + 1				-- открывающая кавычка

	local parts = {}
	while true do
		local c = self.text:sub(self.pos, self.pos)

		if c == "" then
			self:fail("строка не закрыта")
		elseif c == '"' then
			self.pos = self.pos + 1
			return table.concat(parts)
		elseif c == "\\" then
			self.pos = self.pos + 1
			local esc = self.text:sub(self.pos, self.pos)

			if esc == "u" then
				self.pos = self.pos + 1
				parts[#parts + 1] = self:utf8_escape()
			elseif UNESCAPE[esc] then
				parts[#parts + 1] = UNESCAPE[esc]
				self.pos = self.pos + 1
			else
				self:fail("неизвестная escape-последовательность \\" .. esc)
			end
		else
			-- Кусками, а не посимвольно: тело строки обычно без экранирования.
			local plain = self.text:match('^[^"\\]+', self.pos)
			parts[#parts + 1] = plain
			self.pos = self.pos + #plain
		end
	end
end

function Parser:array()
	self.pos = self.pos + 1
	local out = {}

	self:skip()
	if self.text:sub(self.pos, self.pos) == "]" then
		self.pos = self.pos + 1
		return out
	end

	while true do
		out[#out + 1] = self:value()
		self:skip()

		local c = self.text:sub(self.pos, self.pos)
		self.pos = self.pos + 1

		if c == "]" then return out end
		if c ~= "," then self:fail("ожидалась , или ]") end
		self:skip()
	end
end

function Parser:object()
	self.pos = self.pos + 1
	local out = {}

	self:skip()
	if self.text:sub(self.pos, self.pos) == "}" then
		self.pos = self.pos + 1
		return out
	end

	while true do
		self:skip()
		if self.text:sub(self.pos, self.pos) ~= '"' then
			self:fail("ключ должен быть строкой")
		end

		local key = self:string()
		self:skip()

		if self.text:sub(self.pos, self.pos) ~= ":" then
			self:fail("ожидалось :")
		end
		self.pos = self.pos + 1

		out[key] = self:value()
		self:skip()

		local c = self.text:sub(self.pos, self.pos)
		self.pos = self.pos + 1

		if c == "}" then return out end
		if c ~= "," then self:fail("ожидалась , или }") end
	end
end

function Parser:value()
	self:skip()
	local c = self.text:sub(self.pos, self.pos)

	if c == "" then self:fail("текст кончился") end
	if c == "{" then return self:object() end
	if c == "[" then return self:array() end
	if c == '"' then return self:string() end
	if c == "t" then return self:literal("true", true) end
	if c == "f" then return self:literal("false", false) end
	-- null становится json.null, а не nil: иначе ключ просто исчезает из
	-- таблицы и "поля не было" не отличить от "поле пришло пустым".
	if c == "n" then return self:literal("null", json.null) end

	return self:number()
end

-- Уникальное значение под JSON-овский null.
json.null = setmetatable({}, { __tostring = function() return "null" end })

-- json.decode(text) -> value | nil, причина
function json.decode(text)
	if type(text) ~= "string" then
		return nil, "json.decode: ожидалась строка, получено " .. type(text)
	end

	local parser = setmetatable({ text = text, pos = 1 }, Parser)

	local ok, result = pcall(function()
		local value = parser:value()
		parser:skip()
		if parser.pos <= #parser.text then
			parser:fail("мусор после значения")
		end
		return value
	end)

	if not ok then
		return nil, result
	end
	return result
end

return json
