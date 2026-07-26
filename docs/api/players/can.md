---
title: p:can
description: "Есть ли у игрока право"
---

# p:can

Есть ли у игрока право.

```lua
p:can(node)
```

## Аргументы

| # | имя | тип |  |
|---|---|---|---|
| 1 | `node` | string | нода вида `admin.kick` |

## Возвращает

| тип |  |
|---|---|
| `boolean` | есть ли у игрока право |

## Пример

```lua
if p:can("shop.vip.buy") then ... end
```
