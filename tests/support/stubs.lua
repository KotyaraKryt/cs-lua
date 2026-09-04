-- Подмена пространств имён движка, чтобы core/ и lib/ грузились без игры.
--
-- Стабы намеренно тупые: они не изображают движок, а только дают коду то, на
-- что он опирается, и записывают, что он с этим сделал. Всё, что проверяется в
-- тестах — это логика самого Lua-слоя, а не точность стабов.

local support = {}

support.root = "."

--------------------------------------------------------------------------
-- Временные файлы
--------------------------------------------------------------------------

local tmp_roots = {}

-- package.config начинается с разделителя путей: "\\" на Windows, "/" везде ещё.
local windows = package.config:sub(1, 1) == "\\"

local function mkdir(path)
	if windows then
		os.execute(('mkdir "%s" >nul 2>nul'):format(path:gsub("/", "\\")))
	else
		os.execute(('mkdir -p "%s" 2>/dev/null'):format(path))
	end
end

-- Каталог живёт до конца прогона: тестов немного, а удалять его между ними
-- значило бы гасить ошибки, которые видно только по оставшимся файлам.
function support.tmpdir()
	local base = os.getenv("TMPDIR") or os.getenv("TEMP") or "/tmp"
	local path = ("%s/cslua-test-%d-%d"):format(base, os.time(), math.random(1e6))
	path = path:gsub("\\", "/")
	mkdir(path)
	tmp_roots[#tmp_roots + 1] = path
	return path
end

function support.write(path, text)
	local fh = assert(io.open(path, "w"))
	fh:write(text)
	fh:close()
end

--------------------------------------------------------------------------
-- Игроки
--------------------------------------------------------------------------

local Player = {}
Player.__index = Player

-- Методы, которые доклеивает core/access.lua через players.method().
local extra = {}

function Player:name()    return self._name end
function Player:steamid() return self._steamid end
function Player:userid()  return self._userid end
function Player:team()    return self._team end
function Player:alive()   return self._alive end
function Player:chat(text) self.said[#self.said + 1] = text end
function Player:console(text) self.consoled[#self.consoled + 1] = text end

-- p:can и остальное появляются через players.method, поэтому промах ищем
-- сначала там.
setmetatable(Player, {
	__index = function(_, key)
		return extra[key]
	end,
})

function support.player(id, opts)
	opts = opts or {}
	local p = setmetatable({
		id       = id,
		_name    = opts.name or ("player" .. id),
		_steamid = opts.steamid or ("STEAM_0:0:" .. id),
		_userid  = opts.userid or (100 + id),
		_team    = opts.team or "CT",
		_alive   = opts.alive ~= false,
		said     = {},
		consoled = {},
	}, Player)

	support.connected[id] = p
	return p
end

--------------------------------------------------------------------------
-- Сброс между спеками
--------------------------------------------------------------------------

function support.reset()
	support.connected = {}
	support.hooks = {}
	support.server_commands = {}
	support.client_commands = {}
	support.printed = {}

	for _, name in ipairs({ "access", "cmd", "hook", "players", "sv", "ui",
	                        "plugin", "db", "menu", "timer", "ents", "res" }) do
		_G[name] = nil
	end
	for _, name in ipairs({ "datafile", "color", "text", "store", "json" }) do
		package.loaded[name] = nil
	end

	_G.print = function(...)
		local parts = {}
		for i = 1, select("#", ...) do
			parts[i] = tostring((select(i, ...)))
		end
		support.printed[#support.printed + 1] = table.concat(parts, "\t")
	end

	_G.hook = {
		add = function(event, id, fn)
			support.hooks[event] = support.hooks[event] or {}
			local list = support.hooks[event]
			for i = 1, #list do
				if list[i].id == id then
					list[i].fn = fn
					return
				end
			end
			list[#list + 1] = { id = id, fn = fn }
		end,
		remove = function(event, id)
			local list = support.hooks[event] or {}
			for i = #list, 1, -1 do
				if list[i].id == id then
					table.remove(list, i)
					return true
				end
			end
			return false
		end,
	}

	_G.cmd = {
		_register_server = function(name, fn)
			support.server_commands[name] = fn
		end,
		_register_console = function(name, fn)
			support.client_commands[name] = fn
		end,
	}

	_G.players = {
		get = function(id) return support.connected[id] end,
		list = function()
			local out = {}
			for _, p in pairs(support.connected) do
				out[#out + 1] = p
			end
			table.sort(out, function(a, b) return a.id < b.id end)
			return out
		end,
		method = function(name, fn)
			extra[name] = fn
		end,
		broadcast = { chat = function() end },
	}

	_G.sv = {
		map = function() return support.map end,
		time = function() return support.time end,
		dir = support.data_root,
		version = "test",
		api = 2,
	}

	_G.ui = {
		palette = {
			white  = { 255, 255, 255 },
			black  = { 0, 0, 0 },
			red    = { 255, 64, 64 },
			green  = { 0, 255, 0 },
			blue   = { 80, 160, 255 },
			yellow = { 255, 208, 0 },
			orange = { 255, 160, 0 },
			grey   = { 160, 160, 160 },
		},
	}

	_G.plugin = { id = function() return "test" end }

	support.map = "de_dust2"
	support.time = 0
	extra = {}
end

--------------------------------------------------------------------------
-- Загрузка тестируемого кода
--------------------------------------------------------------------------

function support.load_core(name)
	local path = ("%s/scripts/core/%s.lua"):format(support.root, name)
	local chunk = assert(loadfile(path), "не открылся " .. path)
	chunk()
end

-- Кладёт groups.lua и users.lua во временный каталог и поднимает core/access
-- поверх них. Возвращает путь, чтобы тест мог заглянуть в записанный файл.
function support.load_access(groups, users)
	local root = support.tmpdir()
	mkdir(root .. "/data")

	if groups then
		support.write(root .. "/data/groups.lua", "return " .. groups)
	end
	if users then
		support.write(root .. "/data/users.lua", "return " .. users)
	end

	_G.sv.dir = root
	package.loaded["datafile"] = nil
	support.load_core("access")
	return root
end

-- Вызывает всех подписчиков события так же, как это делает движок: одна
-- таблица на цепочку, отмена обрывает её.
function support.fire(event, fields)
	local e = fields or {}
	e.name = event
	e.cancelled = e.cancelled or false
	e.cancel = function(self) self.cancelled = true end

	for _, h in ipairs(support.hooks[event] or {}) do
		h.fn(e)
		if e.cancelled then
			break
		end
	end
	return e
end

support.reset()

return support
