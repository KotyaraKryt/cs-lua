---
title: optional
description: "То же, что import, но `nil` вместо ошибки; мягкая зависимость"
---

# optional

То же, что import, но `nil` вместо ошибки; мягкая зависимость.

```lua
optional(plugin)
```

## Аргументы

| # | имя | тип |  |
|---|---|---|---|
| 1 | `plugin` | string | id плагина |

## Возвращает

| тип |  |
|---|---|
| `table \| nil` | `nil`, если плагина нет |

## Пример

```lua
local shop = optional("shop")
if shop then
	shop.give(p, "vip")
end
```
