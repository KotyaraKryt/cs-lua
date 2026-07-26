---
title: db:first
description: "Выполняет запрос и возвращает первую строку"
---

# db:first

Выполняет запрос и возвращает первую строку.

```lua
db:first(sql, ...)
```

## Аргументы

| # | имя | тип |  |
|---|---|---|---|
| 1 | `sql` | string | SQL |
| 2 | `...` | any | значения для `?` |

## Возвращает

| тип |  |
|---|---|
| `table \| nil` | строка или `nil` |

## Пример

```lua
local r = db:first("SELECT count(*) AS n FROM kills WHERE steamid = ?", id)
print(r.n)
```
