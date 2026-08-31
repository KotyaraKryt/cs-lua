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


## conn:find {#find}

SELECT по одной таблице. Собирает запрос сам; значения в `where` идут через `?`.

```lua
conn:find(table, opts, fn)
```

### Аргументы

| # | имя | тип |  |
|---|---|---|---|
| 1 | `table` | string | имя таблицы (`[A-Za-z0-9_]+`) |
| 2 | `opts` | table | см. ниже |
| 3 | `fn` | function | тот же объект ответа, что у `query` |

### opts

| поле | тип |  |
|---|---|---|
| `where` | table \| nil | `{ col = value, ... }` — только равенства, соединяются через `AND` |
| `select` | table \| nil | список имён колонок; нет или пусто — `*` |
| `order` | table \| nil | строки `"col"` или `"col ASC"` / `"col DESC"` |
| `limit` | number \| nil | `LIMIT n` |
| `offset` | number \| nil | `OFFSET n` |

### Возвращает

| тип |  |
|---|---|
| `number` | id запроса |

### Пример

```lua
site:find("users", {
    where  = { steam_id = p:steamid() },
    select = { "id", "shilings" },
    limit  = 1,
}, function(res)
    if not res.ok then return print(res.error) end
    local row = res.rows[1]
    if row then print(row.shilings) end
end)
```

---

## conn:create {#create}

INSERT одной строки. Ключи таблицы — имена колонок, значения — через `?`.

```lua
conn:create(table, data, fn)
```

### Аргументы

| # | имя | тип |  |
|---|---|---|---|
| 1 | `table` | string | имя таблицы |
| 2 | `data` | table | `{ col = value, ... }`, не пустая |
| 3 | `fn` | function | объект ответа (`insert_id`, `affected_rows`) |

### Возвращает

| тип |  |
|---|---|
| `number` | id запроса |

### Пример

```lua
site:create("users", {
    steam_id = steamid,
    name     = name,
    shilings = 0,
}, function(res)
    if res.ok then print("id", res.insert_id) end
end)
```

---

## conn:update {#update}

UPDATE: обязательны и `set`, и `where` (оба непустые).

```lua
conn:update(table, opts, fn)
```

### Аргументы

| # | имя | тип |  |
|---|---|---|---|
| 1 | `table` | string | имя таблицы |
| 2 | `opts` | table | `set` + `where` |
| 3 | `fn` | function | объект ответа |

### opts

| поле | тип |  |
|---|---|---|
| `set` | table | `{ col = value, ... }` |
| `where` | table | `{ col = value, ... }` — только `AND` равенств |

### Возвращает

| тип |  |
|---|---|
| `number` | id запроса |

### Пример

```lua
site:update("users", {
    set   = { shilings = 100 },
    where = { id = 42 },
}, function(res)
    if res.ok then print(res.affected_rows) end
end)
```

---

## conn:delete {#delete}

DELETE. `where` обязателен и непустой (защита от «удалить всё»).

```lua
conn:delete(table, opts, fn)
```

### Аргументы

| # | имя | тип |  |
|---|---|---|---|
| 1 | `table` | string | имя таблицы |
| 2 | `opts` | table | `where` |
| 3 | `fn` | function | объект ответа |

### opts

| поле | тип |  |
|---|---|---|
| `where` | table | `{ col = value, ... }` |

### Возвращает

| тип |  |
|---|---|
| `number` | id запроса |

### Пример

```lua
site:delete("users", { where = { id = 42 } }, function(res)
    if res.ok then print(res.affected_rows) end
end)
```

---

## conn:migrate {#migrate}

Прогоняет миграции **последовательно** на этом соединении (под одним
`use_lock`). Уже применённые id пропускаются. Книга учёта —

```sql
_cslua_migrations (id VARCHAR(128) PRIMARY KEY, applied_at TIMESTAMP)
```

создаётся сама при первом вызове.

```lua
conn:migrate(opts, fn)
```

### Аргументы

| # | имя | тип |  |
|---|---|---|---|
| 1 | `opts` | table | `migrations` и/или `files` |
| 2 | `fn` | function | объект ответа + `res.applied` |

### opts

**`migrations`** — массив шагов:

| поле шага | тип |  |
|---|---|---|
| `id` | string | уникальный id (`[A-Za-z0-9_.-]+`, до 128) |
| `sql` | string \| nil | текст SQL |
| `file` | string \| nil | имя файла в `plugin.data_dir()` (те же правила, что у `file.read`) |

Нужен `sql` **или** `file`.

**`files`** — массив имён файлов; id миграции = имя файла целиком
(`001_init.sql`).

Можно передать оба списка: сначала `migrations`, потом `files`.

### Возвращает

| тип |  |
|---|---|
| `number` | id запроса |

### Ответ

Как у `query`, плюс:

| поле | тип |  |
|---|---|---|
| `res.applied` | table | id, которые реально выполнились в этом вызове (пустой, если всё уже было) |

### Пример

```lua
site:migrate({
    migrations = {
        {
            id = "001_users",
            sql = [[
                CREATE TABLE IF NOT EXISTS users (
                    id INT AUTO_INCREMENT PRIMARY KEY,
                    steam_id VARCHAR(32) NOT NULL UNIQUE,
                    shilings INT NOT NULL DEFAULT 0
                )
            ]],
        },
        { id = "002_name", file = "002_name.sql" },
    },
}, function(res)
    if not res.ok then return print(res.error) end
    for _, id in ipairs(res.applied or {}) do
        print("applied", id)
    end
end)
```

```lua
-- только файлы из data_dir плагина
site:migrate({
    files = { "001_init.sql", "002_vip.sql" },
}, function(res)
    if not res.ok then return print(res.error) end
end)
```

<Warning>
Файлы читаются только из каталога плагина (песочница `file.*`): без `../`,
без подпапок. SQL из файла выполняется как есть — это код плагина.
</Warning>

<Note>
Повторный вызов с тем же `id` ничего не делает. Новый шаг — новый `id`.
Весь batch одной миграции идёт строго по очереди; параллелить шаги нельзя.
</Note>

---

## conn:close {#close}

Закрывает соединение.

```lua
conn:close()
```

### Возвращает

Ничего.

Помечает соединение закрытым сразу; сам сокет освобождается позже, когда рабочий поток до него дойдёт (или при остановке сервера, если запросов на нём больше нет). Запросы, уже отправленные до `:close()`, доигрывают и получают `res.error`.