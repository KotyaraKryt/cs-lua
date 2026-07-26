---
description: "Раскладка папок плагина, require, окружение, порядок загрузки, core-слой и библиотеки include."
---

# Структура плагина

Плагин — папка с `init.lua` внутри `addons/lua/plugins/`. Для кода на два десятка
строк хватит одиночного `.lua` в той же папке.

```
addons/lua/
  core/               базовый Lua-слой, грузится до плагинов
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

## Окружение

Все файлы одного плагина делят одну таблицу глобальных переменных: папка — это
пространство имён. Записи наружу не видны, чтение `_G` работает как обычно.

> [!WARNING]
> Ошибка при загрузке снимает все хендлеры плагина. Плагин не запускается
> наполовину — в `lua_list` он помечается `[FAILED]`.

## Порядок загрузки

1. `core/*.lua` — по алфавиту, в общем `_G`;
2. плагины — по алфавиту имени папки или файла.

Плагин, которому нужен другой плагин, берёт зависимость через
[`import`](api/exports/import.md) — лениво, если алфавит неудобен.

## require

Модуль ищется сначала в папке плагина, потом в общей `addons/lua/include/`.

```lua
local cfg     = require("config")       -- greeter/config.lua
local Session = require("lib.sessions") -- greeter/lib/sessions.lua
local class   = require("class")        -- addons/lua/include/class.lua
```

Два плагина могут держать каждый свой `lib/util.lua` и не видеть чужой. Кеш
модулей у каждого плагина свой и сбрасывается при его перезагрузке.

## Манифест

См. [`plugin{}`](api/plugin/index.md).

```lua
plugin {
	name        = "Shop",
	version     = "1.0",
	author      = "kotyarakryt",
	api_version = 2,
	requires    = { "class" },
}
```

## Core-слой

`addons/lua/core/*.lua` грузятся до плагинов и определяют то, что видит каждый.

| файл | что определяет |
|---|---|
| `access.lua` | [`access`](api/access/index.md), `p:can()`, группы, иммунитет |
| `commands.lua` | [`cmd.add`](api/cmd/add.md), `players.find` |
| `exports.lua` | [`export`](api/exports/index.md), `import`, `optional` |
| `hook.lua` | консольная команда `lua_hooks` |
| `perms.lua` | консольная команда `lua_perms` |
| `ui.lua` | [`menu.new`](api/menu/new.md), [`ui.color`](api/ui/color.md) |

Ошибки core логируются с пометкой `[core]`. После правок в `core/` нужна полная
`lua_reload`, а не одиночная.

## include

Обычные модули под `require`, не часть API движка.

| модуль | |
|---|---|
| [`store`](api/store/index.md) | key-value на SQLite |
| [`datafile`](api/store/at.md) | чтение и запись `data/*.lua` |
| [`color`](api/ui/color.md) | разбор цвета |
| [`text`](api/ui/index.md#длина-строки) | длина строки как её считает клиент |
| `class` | класс с наследованием |

```lua
local class = require("class")

local Session = class("Session")
function Session:init(name) self.name = name; self.joined = os.time() end
function Session:seconds() return os.time() - self.joined end

local Timed = class("Timed", Session)
function Timed:seconds() return Timed.super.seconds(self) * 2 end
```

Классы в Lua — это метатаблицы, поэтому `class.lua` можно не брать: пиши на
голых метатаблицах или положи свою библиотеку в `include/`.
