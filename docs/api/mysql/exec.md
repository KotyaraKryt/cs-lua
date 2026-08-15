---
title: conn:exec
description: "То же самое, что conn:query — имя удобнее для запросов без строк"
---

# conn:exec

То же самое, что conn:query — имя удобнее для запросов без строк.

```lua
conn:exec(sql[, ...], fn)
```

## Аргументы

| # | имя | тип |  |
|---|---|---|---|
| 1 | `sql` | string | SQL с `?` на месте значений |
| 2 | `...` | any | значения для `?` |
| 3 | `fn` | function | получает объект ответа |

## Возвращает

| тип |  |
|---|---|
| `number` | id запроса |

## Пример

```lua
site:exec("UPDATE users SET shilings = shilings + ? WHERE id = ?", delta, id,
    function(res)
        if res.ok then print(res.affected_rows) end
    end)
```
