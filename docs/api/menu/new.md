---
title: menu.new
description: "Создаёт меню"
---

# menu.new

Создаёт меню.

```lua
menu.new([title[, opts]])
```

## Аргументы

| # | имя | тип |  |
|---|---|---|---|
| 1 | `title` | string \| nil | заголовок панели |
| 2 | `opts` | table \| nil | см. [Опции](#опции) |

## Возвращает

| тип |  |
|---|---|
| `menu` | объект меню |

## Опции

| поле | тип |  |
|---|---|---|
| `exit` | boolean | клавиша `0` закрывает меню; по умолчанию `true` |
| `time` | number | секунд на экране; `-1` — до ответа. По умолчанию `-1` |
| `on_exit` | function | вызывается при закрытии, получает игрока |
| `color` | string \| table | см. [Цвета](index.md#цвета) |

## Пример

```lua
local m = menu.new("Оружие", { color = { title = "yellow", number = "red" } })
```
