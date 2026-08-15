---
title: conn:query
description: "Выполняет запрос и отдаёт строки в коллбэк"
---

# conn:query

Выполняет запрос и отдаёт строки в коллбэк.

```lua
conn:query(sql[, ...], fn)
```

## Аргументы

| # | имя | тип |  |
|---|---|---|---|
| 1 | `sql` | string | SQL с `?` на месте значений |
| 2 | `...` | any | значения для `?`, ровно по числу знаков |
| 3 | `fn` | function | получает объект ответа |

## Возвращает

| тип |  |
|---|---|
| `number` | id запроса |

## Пример

```lua
site:query("SELECT id, shilings FROM users WHERE steam_id = ?", p:steamid(),
    function(res)
        if not res.ok then return print(res.error) end
        for _, row in ipairs(res.rows) do
            print(row.id, row.shilings)
        end
    end)
```

> [!NOTE]
> Соединения переживают `lua_reload` и выгрузку плагина: сокет закрывается
> только при остановке сервера, а не при перезагрузке — обрывать чужой
> запрос на середине нельзя, см. ниже.
