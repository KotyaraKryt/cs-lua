---
title: access — Выдача
description: "Права как именованные ноды, группы и иммунитет"
---

# Выдача

## access.grant {#grant}

Выдаёт права по ключу.

```lua
access.grant(key, spec)
```

### Аргументы

| # | имя | тип |  |
|---|---|---|---|
| 1 | `key` | string | steamid или `name:<ник>` |
| 2 | `spec` | table | см. [Поля](#grant-поля) |

### Возвращает

| тип |  |
|---|---|
| `boolean` | `true` при успехе |
| `string` | причина при ошибке |

### Поля {#grant-поля}

| поле | тип |  |
|---|---|---|
| `groups` | string \| table | группа или список групп |
| `allow` | string \| table | ноды, которые выдать |
| `deny` | string \| table | ноды, которые запретить |
| `until_` | string \| number | `"30d"`, `"12h"`, `"map"` или unix-время |
| `where` | table | `{ map = }` или `{ maps = {} }` |

<Note>
Действует сразу; в файл попадает только после
[`access.save`](grant.md#save).
</Note>

## access.revoke {#revoke}

Забирает запись, группу или ноду.

```lua
access.revoke(key[, group][, node])
```

### Аргументы

| # | имя | тип |  |
|---|---|---|---|
| 1 | `key` | string | steamid или `name:<ник>` |
| 2 | `group` | string \| nil | какую группу забрать |
| 3 | `node` | string \| nil | какую ноду забрать |

### Возвращает

| тип |  |
|---|---|
| `boolean` | `true` при успехе |
| `string` | причина при ошибке |

Без `group` и `node` удаляет запись целиком.

## access.save {#save}

Записывает `users.lua` на диск.

```lua
access.save()
```

### Возвращает

| тип |  |
|---|---|
| `boolean` | `true` при успехе |
| `string` | причина при ошибке |

Атомарно, с `.bak` — как [`datafile.save`](../store/datafile.md#save).

## access.reload {#reload}

Перечитывает `groups.lua` и `users.lua` с диска.

```lua
access.reload()
```

### Возвращает

Ничего.

Без полного `lua_reload`.

## access.invalidate {#invalidate}

Сбрасывает кеш прав.

```lua
access.invalidate([p])
```

### Аргументы

| # | имя | тип |  |
|---|---|---|---|
| 1 | `p` | player \| nil | чей кеш; без аргумента — всех |

### Возвращает

Ничего.

Нужен после `grant`/`revoke` из своего кода.
