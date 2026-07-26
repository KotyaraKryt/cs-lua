---
title: access.invalidate
description: "Сбрасывает кеш прав"
---

# access.invalidate

Сбрасывает кеш прав.

```lua
access.invalidate([p])
```

## Аргументы

| # | имя | тип |  |
|---|---|---|---|
| 1 | `p` | player \| nil | чей кеш; без аргумента — всех |

## Возвращает

Ничего.

Нужен после `grant`/`revoke` из своего кода.
