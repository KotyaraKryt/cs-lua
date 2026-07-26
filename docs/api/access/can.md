---
title: access.can
description: "Есть ли у игрока право"
---

# access.can

Есть ли у игрока право.

```lua
access.can(p, node)
```

## Аргументы

| # | имя | тип |  |
|---|---|---|---|
| 1 | `p` | player \| nil | `nil` — консоль, ей разрешено всё |
| 2 | `node` | string | нода |

## Возвращает

| тип |  |
|---|---|
| `boolean` | разрешено ли |

То же, что [`p:can`](../players/can.md), но принимает `nil`.
