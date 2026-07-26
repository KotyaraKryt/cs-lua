---
title: e:keyvalue
description: "Задаёт keyvalue — то же, что делает карта"
---

# e:keyvalue

Задаёт keyvalue — то же, что делает карта.

```lua
e:keyvalue(key, value)
```

## Аргументы

| # | имя | тип |  |
|---|---|---|---|
| 1 | `key` | string | имя, например `targetname` |
| 2 | `value` | string | значение |

## Возвращает

| тип |  |
|---|---|
| `boolean` | приняла ли игра это поле |

## Пример

```lua
e:keyvalue("targetname", "door1")
```

Всё, что дизайнер уровня выставляет на сущности в редакторе, доступно отсюда.

> [!WARNING]
> Только до [`e:spawn()`](spawn.md): значения читает именно Spawn.
