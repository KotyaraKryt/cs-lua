---
title: mysql — Объект соединения
description: "MySQL/MariaDB: удалённая база, не блокирующая кадр"
---

# Объект соединения

## conn:query {#query}

Выполняет запрос и отдаёт строки в коллбэк.

```lua
conn:query(sql[, ...], fn)
```

### Аргументы

| # | имя | тип |  |
|---|---|---|---|
| 1 | `sql` | string | SQL с `?` на месте значений |
| 2 | `...` | any | значения для `?`, ровно по числу знаков |
| 3 | `fn` | function | получает объект ответа |

### Возвращает

| тип |  |
|---|---|
| `number` | id запроса |

### Пример

```lua
site:query("SELECT id, shilings FROM users WHERE steam_id = ?", p:steamid(),
    function(res)
        if not res.ok then return print(res.error) end
        for _, row in ipairs(res.rows) do
            print(row.id, row.shilings)
        end
    end)
```

<Note>
Соединения переживают `lua_reload` и выгрузку плагина: сокет закрывается
только при остановке сервера, а не при перезагрузке — обрывать чужой
запрос на середине нельзя, см. ниже.
</Note>

## conn:exec {#exec}

То же самое, что conn:query — имя удобнее для запросов без строк.

```lua
conn:exec(sql[, ...], fn)
```

### Аргументы

| # | имя | тип |  |
|---|---|---|---|
| 1 | `sql` | string | SQL с `?` на месте значений |
| 2 | `...` | any | значения для `?` |
| 3 | `fn` | function | получает объект ответа |

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
