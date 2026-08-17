---
description: "Раскладка папок плагина, require, окружение, порядок загрузки, core-слой и библиотеки include."
---

# Структура плагина

Плагин — папка внутри `addons/lua/plugins/` с двумя обязательными файлами:
`manifest.lua` и `init.lua`. Одиночный `.lua`-файл плагином не считается —
структура строгая, папка обязательна.

```
addons/lua/
  core/               базовый Lua-слой, грузится до плагинов
  data/               groups.lua, users.lua
  include/            общие библиотеки для require
  plugins/
    greeter/
      manifest.lua    корень: plugin{}, права, конфиг — грузится первым
      init.lua        код плагина — грузится после manifest.lua
      config.lua
      lib/sessions.lua
    _wip/             имя с _ или . в начале пропускается
```

## manifest.lua

Каждый плагин грузится в два шага, и порядок фиксирован:

1. `manifest.lua` — задаёт правила плагина: вызывает [`plugin{}`](api/plugin/index.md),
   объявляет права через `access.declare`, поднимает `config.lua`. Ничего
   игрового тут быть не должно.
2. `init.lua` — сам плагин: хендлеры, команды, состояние.

Оба файла делят одно окружение и один кеш `require`, так что `require("config")`
в `init.lua` просто возвращает то, что уже загрузил `manifest.lua`.

```lua
-- manifest.lua
plugin {
	name        = "Shop",
	version     = "1.0",
	author      = "kotyarakryt",
	api_version = 2,
	requires    = { "class" },
}

access.declare("shop.buy", { desc = "Доступ к магазину", default = true })

require("config")
```

Если `manifest.lua` не вызвал `plugin{}` — плагин не грузится: `init.lua` не
запускается вовсе, ошибка в консоль сразу после первого файла.

## Окружение

Все файлы одного плагина делят одну таблицу глобальных переменных: папка — это
пространство имён. Записи наружу не видны, чтение `_G` работает как обычно.

<Warning>
Ошибка при загрузке снимает все хендлеры плагина. Плагин не запускается
наполовину — в `lua_list` он помечается `[FAILED]`.
</Warning>

## Порядок загрузки

1. `core/*.lua` — по алфавиту, в общем `_G`;
2. плагины — по алфавиту имени папки, для каждого `manifest.lua` перед `init.lua`.

Плагин, которому нужен другой плагин, берёт зависимость через
[`import`](api/exports/index.md#import) — лениво, если алфавит неудобен.

## require

Модуль ищется сначала в папке плагина, потом в общей `addons/lua/include/`.

```lua
local cfg     = require("config")       -- greeter/config.lua
local Session = require("lib.sessions") -- greeter/lib/sessions.lua
local class   = require("class")        -- addons/lua/include/class.lua
```

Два плагина могут держать каждый свой `lib/util.lua` и не видеть чужой. Кеш
модулей у каждого плагина свой и сбрасывается при его перезагрузке.

## Core-слой

`addons/lua/core/*.lua` грузятся до плагинов и определяют то, что видит каждый.

| файл | что определяет |
|---|---|
| `access.lua` | [`access`](api/access/index.md), `p:can()`, группы, иммунитет |
| `commands.lua` | [`cmd.add`](api/cmd/index.md#add), `players.find` |
| `exports.lua` | [`export`](api/exports/index.md), `import`, `optional` |
| `hook.lua` | консольная команда `lua_hooks` |
| `perms.lua` | консольная команда `lua_perms` |
| `ui.lua` | [`menu.new`](api/menu/index.md#new), [`ui.color`](api/ui/index.md#color) |

Ошибки core логируются с пометкой `[core]`. После правок в `core/` нужна полная
`lua_reload`, а не одиночная.

## include

Обычные модули под `require`, не часть API движка.

| модуль | |
|---|---|
| [`store`](api/store/index.md) | key-value на SQLite |
| [`datafile`](api/store/datafile.md#at) | чтение и запись `data/*.lua` |
| [`json`](api/http/index.md) | разбор и сборка JSON, для ответов [`http`](api/http/index.md) |
| [`color`](api/ui/index.md#color) | разбор цвета |
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
