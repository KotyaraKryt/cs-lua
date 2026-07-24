# Структура плагина

Плагин — это папка с `init.lua`. Всё остальное внутри папки на усмотрение
автора. Для мелочи на 20 строк хватит одиночного `.lua` в `plugins/`.

```
addons/lua/
  core/               базовый API, грузится ПЕРЕД плагинами
  data/               groups.lua, users.lua
  include/            общие библиотеки для require
  plugins/
    greeter/
      init.lua        точка входа, обязателен
      config.lua
      lib/sessions.lua
    hello.lua         одиночный файл — тоже плагин
    _wip/             имя с _ или . в начале пропускается
```

Все файлы одного плагина делят одно окружение: папка — одно пространство имён.
Глобалки плагина не видны другим плагинам, чтение `_G` работает как обычно.

**Ошибка при загрузке снимает все хендлеры плагина.** Половинчатый плагин не
запускается, в `lua_list` он помечается `[FAILED]`.

## require

Ищет сначала в папке плагина, потом в общей `addons/lua/include/`:

```lua
local cfg     = require("config")       -- greeter/config.lua
local Session = require("lib.sessions") -- greeter/lib/sessions.lua
local class   = require("class")        -- addons/lua/include/class.lua
```

Поэтому два плагина могут иметь свой `lib/util.lua` и не видеть чужой.

## Манифест

```lua
plugin {
	name        = "Shop",
	version     = "1.0",
	author      = "kotyarakryt",
	api_version = 1,
	requires    = { "class", "json" },
}
```

| поле | зачем |
|------|-------|
| `name`, `version`, `author` | метаданные, видны в `lua_list` |
| `api_version` | на какую версию Lua-API рассчитан плагин |
| `requires` | модули, которые должны резолвиться как `require()` |

`requires` проверяется при загрузке: если модуля нет, плагин падает на первой
строке с внятной ошибкой, а не где-то в глубине кода.

`api_version` растёт при каждом ломающем изменении API (текущее — в `_CSLUA_API`).
Плагин, просящий версию новее сборки, не загрузится с понятной ошибкой. Более
старую модуль грузит молча.

## Core-слой

`addons/lua/core/*.lua` грузятся до всех плагинов и определяют глобальные
функции, которые видит каждый плагин: роутер команд, права, меню, экспорты.

| файл | что даёт |
|------|----------|
| `access.lua` | `p:can()`, группы, иммунитет |
| `commands.lua` | `command()`, `find_player()` |
| `exports.lua` | `export()`, `import()`, `emit()` |
| `menu.lua` | `menu()` |
| `perms.lua` | консольная команда `lua_perms` |

Файлы core грузятся по алфавиту: `access.lua` определяет `access` до того, как
`commands.lua` начнёт им пользоваться. Добавляешь свой core-файл, зависящий от
чужого — имя решает. Ошибка в core логируется как `[core]`.

## Библиотеки в include/

Не часть API движка — обычные модули, которые можно не брать.

| модуль | что даёт |
|--------|----------|
| `class.lua` | минимальный класс с наследованием |
| `text.lua` | длина строки как её считает клиент, обрезка, шаблоны |
| `access_store.lua` | чтение и запись `data/*.lua` |

```lua
local class = require("class")

local Session = class("Session")
function Session:init(name) self.name = name; self.joined = os.time() end
function Session:seconds() return os.time() - self.joined end

local Timed = class("Timed", Session)          -- наследование
function Timed:seconds() return Timed.super.seconds(self) * 2 end
```

В Lua классы — это метатаблицы, поэтому ядро своего диалекта ООП не навязывает:
не нравится `class.lua` — пиши на голых метатаблицах или положи свою библиотеку
в `include/`.

## Глобальные функции

| | |
|---|---|
| `plugin{}`, `permission{}` | объявления |
| `on(event, fn)` | подписка на событие |
| `command(name, fn[, opts])` | команда в консоль и чат |
| `player(id)`, `players([filter])`, `find_player(token)`, `all` | игроки |
| `menu(title[, opts])` | меню |
| `after(sec, fn)`, `every(sec, fn)`, `cancel(id)` | таймеры |
| `cvar(name)`, `cvar_register(name, def[, flags])` | cvar'ы |
| `precache_sound(path)`, `precache_model(path)` | ресурсы |
| `server_time()`, `map()` | время сервера, текущая карта |
| `plugin_dir()`, `plugin_id()` | путь и id текущего плагина |
| `_CSLUA_VERSION`, `_CSLUA_API`, `_CSLUA_DIR` | константы |
