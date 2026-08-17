---
title: access — Чтение
description: "Права как именованные ноды, группы и иммунитет"
---

# Чтение

## access.can {#can}

Есть ли у игрока право.

```lua
access.can(p, node)
```

### Аргументы

| # | имя | тип |  |
|---|---|---|---|
| 1 | `p` | player \| nil | `nil` — консоль, ей разрешено всё |
| 2 | `node` | string | нода |

### Возвращает

| тип |  |
|---|---|
| `boolean` | разрешено ли |

То же, что [`p:can`](../players/access.md#can), но принимает `nil`.

## access.permissions {#permissions}

Все объявленные ноды.

```lua
access.permissions()
```

### Возвращает

| тип |  |
|---|---|
| `table` | `{ [node] = { desc = , default = } }` |

## access.users {#users}

Все записи из `users.lua`.

```lua
access.users()
```

### Возвращает

| тип |  |
|---|---|
| `table` | `{ [key] = entry }` |

## access.user {#user}

Одна запись из `users.lua`.

```lua
access.user(key)
```

### Аргументы

| # | имя | тип |  |
|---|---|---|---|
| 1 | `key` | string | steamid или `name:<ник>` |

### Возвращает

| тип |  |
|---|---|
| `table \| nil` | запись |

## access.all_groups {#all_groups}

Все группы.

```lua
access.all_groups()
```

### Возвращает

| тип |  |
|---|---|
| `table` | `{ [name] = group }` |

## access.group {#group}

Одна группа по имени.

```lua
access.group(name)
```

### Аргументы

| # | имя | тип |  |
|---|---|---|---|
| 1 | `name` | string | имя группы |

### Возвращает

| тип |  |
|---|---|
| `table \| nil` | группа |
