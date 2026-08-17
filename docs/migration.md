---
description: "Перенос плагинов с cs-lua v1 на v2: таблица переименований, объект события, опции вместо булевых флагов."
---

# Переход с v1 на v2

v2 переименовал весь Lua-API и поменял форму обработчиков событий. Плагин с
`api_version = 1` не загружается — модуль останавливает его на строке `plugin{}`
и говорит почему, вместо того чтобы дать ему упасть глубже на `attempt to call a
nil value`.

Зачем ломали: в v1 события регистрировались одним способом, команды другим,
плагинные события третьим, и сорок глобальных функций лежали в `_G` без всякой
системы. Теперь всё через пространство имён, а любой обработчик получает ровно
одну таблицу.

## Что делать

1. Поднять `api_version` до `2`.
2. Прогнать переименования по таблице ниже.
3. Переписать обработчики событий на объект события.
4. Запустить, посмотреть `lua_list` и `lua_hooks`.

## Переименования

| v1 | v2 |
|---|---|
| `on(event, fn)` | `hook.add(event, id, fn)` |
| `emit(event, ...)` | `hook.run(event, { ... })` |
| `on_export(event, id, fn)` | `hook.add(event, id, fn)` |
| `command(name, fn, opts)` | `cmd.add(name, fn, opts)` |
| `after`, `every`, `cancel` | `timer.after`, `timer.every`, `timer.cancel` |
| `player(id)` | `players.get(id)` |
| `players([filter])` | `players.list([filter])` |
| `find_player(token)` | `players.find(token)` |
| `all` | `players.broadcast` |
| `player_method(name, fn)` | `players.method(name, fn)` |
| `create_entity`, `entities`, `find_in_sphere` | `ents.create`, `ents.find`, `ents.in_sphere` |
| `precache_sound`, `precache_model` | `res.sound`, `res.model` |
| `server_cmd`, `server_time`, `map` | `sv.cmd`, `sv.time`, `sv.map` |
| `cvar`, `cvar_register` | `sv.cvar`, `sv.cvar_register` |
| `sqlite(name)` | `db.open(name)` |
| `menu(title, opts)` | `menu.new(title, opts)` |
| `permission { "node", desc = }` | `access.declare("node", { desc = })` |
| `plugin_dir`, `plugin_id`, `plugin_data_dir` | `plugin.dir`, `plugin.id`, `plugin.data_dir` |
| `_CSLUA_VERSION`, `_CSLUA_API`, `_CSLUA_DIR` | `sv.version`, `sv.api`, `sv.dir` |
| `require("store")` | `require("datafile")` |
| `export`, `import`, `optional` | без изменений |

`require("store")` теперь другой модуль — key-value поверх SQLite, см.
[Хранилище](api/store/index.md). Старый, который пишет `.lua`-файлы, называется
`datafile`.

<Warning>
Автозамена по всему файлу опасна: `players` встречается в SQL как имя таблицы,
а `map`, `cancel`, `every` — в обычном тексте и в ключах конфигов. Меняй только
вызовы, потом проверь строковые литералы.
</Warning>

## Обработчики событий

Обработчик получает **одну таблицу**. Возврат не читается вообще: отменить —
`e:cancel()`, изменить — записать поле.

```lua
-- v1
on("player_hurt", function(victim, attacker, damage, bits)
	if god[victim.id] then return false end
	return damage * 2
end)

-- v2
hook.add("player_hurt", "myplugin.scale", function(e)
	if god[e.victim.id] then return e:cancel() end
	e.damage = e.damage * 2
end)
```

Один и тот же объект достаётся всей цепочке, поэтому второй обработчик видит
правки первого. `e:cancel()` обрывает цепочку.

Поля каждого события — в [`hook`](api/hook/index.md). Общая логика имён: участник
события это `e.player`, а если участников двое — `e.victim` и `e.attacker` (или
`e.killer`).

`e:cancel()` работает только у `client_connect`, `player_chat` и `player_hurt`.
У остальных он бросает ошибку с именем события: они сообщают о том, что уже
случилось, и останавливать там нечего.

## id обработчика

Второй аргумент `hook.add` — идентификатор, уникальный **внутри твоего плагина**.
Два разных плагина спокойно называют свои `"init"`.

Он решает три вещи:

- повторная регистрация той же пары заменяет обработчик, а не добавляет второй —
  поэтому `lua_reload <plugin>` не удваивает подписки;
- `hook.remove(event, id)` снимает подписку, чего в v1 не было совсем;
- в `lua_hooks` и в тексте ошибки видно конкретный обработчик, а не только плагин.

## Плагинные события

Отдельной системы больше нет. Своё событие — это имя с точкой:

```lua
-- v1
emit("shop.bought", p, item)
on_export("shop.bought", "Stats", function(p, item) end)

-- v2
hook.run("shop.bought", { player = p, item = item })
hook.add("shop.bought", "stats.count", function(e) end)
```

`hook.run` возвращает ту же таблицу, так что из неё можно прочитать результат:

```lua
local e = hook.run("shop.buying", { player = p, item = item })
if e.cancelled then return end
```

Точка в имени обязательна: она отличает своё событие от опечатки в движковом.
`hook.add("playr_spawn", ...)` — ошибка со списком известных событий, а не тихо
неработающая подписка.

## Опции вместо булевых флагов

```lua
p:money(500, true)          -- v1
p:money(500, { hud = true }) -- v2

p:team("CT", true)                  -- v1
p:team("CT", { force = true })      -- v2
```

Голый `true` вторым аргументом теперь ошибка с подсказкой, а не тихо принятое
значение.

## Цвет

Один тип на все каналы: имя из палитры, `"#rrggbb"` или `{ r, g, b }`.

```lua
p:hud(text, { color = "orange" })              -- было только { 255, 160, 0 }
menu.new("Оружие", { color = { title = "green" } })
```

Канал, который столько не умеет, берёт ближайшее: у панели меню четыре кода, у
чата три. Инлайновые теги в строке чата (`{green}`, `{team}`, `{default}`) не
менялись.

## Выгрузка плагина

```lua
plugin.on_unload(function()
	-- вернуть миру то, что плагин ему сделал
end)
```

Срабатывает и при `lua_reload <plugin>`, и при полной перезагрузке. Событие
`plugin_unload` осталось, но оно про всех: в нём `e.plugin` — id уходящего
плагина, либо `nil`, когда гасится всё состояние целиком.

## Перезагрузка по одному

```
lua_reload            всё состояние заново
lua_reload greeter    только этот плагин
```

Полная нужна после правок в `core/` и `include/` — их разделяют все плагины.
