---
description: "Быстрый старт: манифест, права и команда за один плагин. Дальше — require, окружение, порядок загрузки, core-слой."
---

# Структура плагина

Плагин — папка внутри `addons/lua/plugins/` с двумя обязательными файлами:
`manifest.lua` и `init.lua`. Одиночный `.lua`-файл плагином не считается.

## Быстрый старт

Собери плагин с правом и командой — типовая связка для магазина, киков,
чего угодно с проверкой доступа.

`addons/lua/plugins/shop/manifest.lua` — грузится первым, объявляет право:

```lua
plugin { name = "Shop", version = "1.0", api_version = 1 }

access.declare("shop.buy", { desc = "Доступ к магазину", default = true })
```

`addons/lua/plugins/shop/init.lua` — грузится следом, использует его:

```lua
cmd.add("buy", function(ctx)
	ctx.reply("Куплено!")
end, { perm = "shop.buy" })
```

`lua_reload` — `!buy` в чате отвечает, у кого нет `shop.buy`, тому команда
молча недоступна. Дальше — [права и группы](api/access/index.md) и
[справочник команд](api/cmd/index.md).

## Раскладка плагина

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

Оба файла делят одно окружение и один кеш `require`, так что `require("config")`
в `init.lua` просто возвращает то, что уже загрузил `manifest.lua`.

Если `manifest.lua` не вызвал [`plugin{}`](api/plugin/index.md) — плагин не
грузится: `init.lua` не запускается вовсе, ошибка в консоль сразу после
первого файла.

<Warning>
Ошибка при загрузке снимает все хендлеры плагина. Плагин не запускается
наполовину — в `lua_list` он помечается `[FAILED]`.
</Warning>

## Порядок загрузки

1. `core/*.lua` — по алфавиту, в общем `_G`.
2. Плагины — по алфавиту имени папки, для каждого `manifest.lua` перед
   `init.lua`.

Нужен другой плагин раньше своей очереди — бери зависимость через
[`import`](api/exports/index.md#import), не через порядок имён.

## require

Модуль ищется сначала в папке плагина, потом в общей `addons/lua/include/`.

```lua
local cfg     = require("config")       -- greeter/config.lua
local Session = require("lib.sessions") -- greeter/lib/sessions.lua
local class   = require("class")        -- addons/lua/include/class.lua
```

Два плагина держат каждый свой `lib/util.lua` и не видят чужой. Кеш модулей
у каждого плагина свой и сбрасывается при его перезагрузке.

## Core-слой

`addons/lua/core/*.lua` грузятся до плагинов и определяют то, что видит
каждый.

| файл | что определяет |
|---|---|
| `access.lua` | [`access`](api/access/index.md), `p:can()`, группы, иммунитет |
| `commands.lua` | [`cmd.add`](api/cmd/index.md#add), `players.find` |
| `exports.lua` | [`export`](api/exports/index.md), `import`, `optional` |
| `hook.lua` | консольная команда `lua_hooks` |
| `perms.lua` | консольная команда `lua_perms` |
| `ui.lua` | [`menu.new`](api/menu/index.md#new), [`ui.color`](api/ui/index.md#color) |

Ошибки core логируются с пометкой `[core]`. После правок в `core/` нужна
полная `lua_reload`, а не одиночная.

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
