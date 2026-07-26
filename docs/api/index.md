---
description: "Все пространства имён cs-lua: hook, cmd, players, timer, ents, res, sv, db, store, menu, ui, access, plugin."
---

# Справочник API

Всё регистрируется одинаково: пространство имён, глагол, обработчик на одну
таблицу.

```lua
plugin { name = "Example", api_version = 2 }

hook.add("player_spawn", "example.armor", function(e)
	e.player:armor(100)
end)

cmd.add("hp", function(ctx)
	ctx.reply(tostring(ctx.player:health()))
end)
```

| namespace | что даёт |
|---|---|
| [`hook`](hook/index.md) | события движка и свои |
| [`cmd`](cmd/index.md) | команды чата, консоли и rcon |
| [`players`](players/index.md) | объект игрока, поиск, рассылка |
| [`timer`](timer/index.md) | отложенные и повторяющиеся вызовы |
| [`ents`](ents/index.md) | сущности |
| [`res`](res/index.md) | прекеш звуков и моделей |
| [`sv`](sv/index.md) | сервер: время, карта, cvar'ы, консоль |
| [`db`](db/index.md) | SQLite |
| [`store`](store/index.md) | key-value на SQLite и `datafile` |
| [`menu`](menu/index.md) | меню |
| [`ui`](ui/index.md) | цвета, лимиты длины строк, кириллица |
| [`access`](access/index.md) | права, группы, иммунитет |
| [`plugin`](plugin/index.md) | манифест, пути, выгрузка |
| [`export` / `import`](exports/index.md) | вызов функций чужого плагина |

Плюс [консольные команды](console.md) модуля.

## Модули под require

| модуль | что даёт |
|---|---|
| [`store`](store/open.md) | key-value на SQLite |
| [`datafile`](store/at.md) | чтение и запись `data/*.lua` |
| [`color`](ui/color.md) | разбор цвета |
| [`text`](ui/index.md#длина-строки) | длина строки как её считает клиент |
| `class` | класс с наследованием |

## Соглашения

| | |
|---|---|
| `p:health()` | без аргумента читает |
| `p:health(100)` | с аргументом пишет |
| `p:money(500, { hud = true })` | всё необязательное — таблицей опций, а не позиционным флагом |
| `hook.add(event, id, fn)` | id уникален внутри плагина: делает регистрацию заменяемой и снимаемой |
| `nil, "причина"` | функции, которые могут не найти, возвращают текст вторым значением |

Объекты игрока и сущности защищены от записи: `p.foo = 1` бросит ошибку. Своё
состояние держи в таблице плагина, ключом — `p.id` или `e.index`.
