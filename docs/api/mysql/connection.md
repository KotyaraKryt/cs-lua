---
title: mysql — Объект соединения
description: "MySQL/MariaDB: удалённая база, не блокирующая кадр"
---

# Объект соединения

## conn:query {#query}

Выполняет запрос и отдаёт строки в коллбэк.

```lua
conn:query(sql[, ...], fn)
conn:query(sql, params, fn)
```

### Аргументы

| # | имя | тип |  |
|---|---|---|---|
| 1 | `sql` | string | SQL с `?` на месте значений |
| 2 | `...` | any | значения для `?`, ровно по числу знаков |
| 3 | `params` | table | массив значений для `?` |
| 4 | `fn` | function | получает объект ответа |

<Note>
Есть два способа передать значения для `?`.

Старый вариант передаёт их отдельными аргументами:
```lua
conn:query(
    "SELECT * FROM users WHERE server_id = ? AND steamid = ?",
    server_id,
    steamid,
    function(res)
        ...
    end
)
```

Для динамического количества параметров можно передать их одной таблицей:
```lua
conn:query(
    "SELECT * FROM users WHERE server_id = ? AND steamid = ?",
    { server_id, steamid },
    function(res)
        ...
    end
)
```

Оба варианта поддерживаются.
</Note>

### Возвращает

| тип |  |
|---|---|
| `number` | id запроса |

### Пример

Отдельные параметры
```lua
site:query(
    "SELECT id, shilings FROM users WHERE steam_id = ?",
    p:steamid(),
    function(res)
        if not res.ok then
            return print(res.error)
        end

        for _, row in ipairs(res.rows) do
            print(row.id, row.shilings)
        end
    end
)
```

Таблица параметров
```lua
local params = {
    server_id,
    steamid,
}

site:query(
    "SELECT id FROM users WHERE server_id = ? AND steam_id = ?",
    params,
    function(res)
        if not res.ok then
            return print(res.error)
        end

        local row = res.rows[1]
        if row then
            print(row.id)
        end
    end
)
```

<Note>
Форма с таблицей особенно удобна для запросов, которые собираются динамически:
```lua
local sets = {}
local params = {}

table.insert(sets, "name = ?")
table.insert(params, player:name())

table.insert(sets, "updated_at = ?")
table.insert(params, os.time())

table.insert(params, player:id())

conn:exec(
    "UPDATE users SET " .. table.concat(sets, ", ") .. " WHERE id = ?",
    params,
    function(res)
        if not res.ok then
            print(res.error)
        end
    end
)
```
</Note>

<Note>
Соединения переживают `lua_reload` и выгрузку плагина: сокет закрывается
только при остановке сервера, а не при перезагрузке — обрывать чужой
запрос на середине нельзя, см. ниже.
</Note>

## conn:exec {#exec}

`conn:exec` полностью эквивалентен `conn:query`. Отличается только
названием: `query` обычно используется для SELECT, а `exec` — для
INSERT, UPDATE и DELETE.

```lua
conn:exec(sql[, ...], fn)
conn:exec(sql, params, fn)
```

### Аргументы

| # | имя | тип |  |
|---|---|---|---|
| 1 | `sql` | string | SQL с `?` на месте значений |
| 2 | `...` | any | значения для `?` |
| 3 | `params` | table | массив значений для `?` |
| 4 | `fn` | function | получает объект ответа |

### Возвращает

| тип |  |
|---|---|
| `number` | id запроса |

### Пример

```lua
site:exec("UPDATE users SET shilings = shilings + ? WHERE id = ?", delta, id,
    function(res)
        if res.ok then print(res.affected_rows) end
    end)
```

## conn:close {#close}

Закрывает соединение.

```lua
conn:close()
```

### Возвращает

Ничего.

Помечает соединение закрытым сразу; сам сокет освобождается позже, когда рабочий поток до него дойдёт (или при остановке сервера, если запросов на нём больше нет). Запросы, уже отправленные до `:close()`, доигрывают и получают `res.error`.
