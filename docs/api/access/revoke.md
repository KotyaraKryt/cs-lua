---
title: access.revoke
description: "Забирает запись, группу или ноду"
---

# access.revoke

Забирает запись, группу или ноду.

```lua
access.revoke(key[, group][, node])
```

## Аргументы

| # | имя | тип |  |
|---|---|---|---|
| 1 | `key` | string | steamid или `name:<ник>` |
| 2 | `group` | string \| nil | какую группу забрать |
| 3 | `node` | string \| nil | какую ноду забрать |

## Возвращает

| тип |  |
|---|---|
| `boolean` | `true` при успехе |
| `string` | причина при ошибке |

Без `group` и `node` удаляет запись целиком.
