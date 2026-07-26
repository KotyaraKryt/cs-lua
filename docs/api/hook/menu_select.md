---
title: menu_select
description: "Игрок нажал клавишу в меню, открытом из Lua"
---

# menu_select

Игрок нажал клавишу в меню, открытом из Lua.

```lua
hook.add("menu_select", id, function(e)
	...
end)
```

## Поля события

| поле | тип |  |
|---|---|---|
| `e.player` | player | кого касается событие |
| `e.key` | number | номер клавиши, 1..10 |

Обычно не нужно: [`menu`](../menu/index.md) разбирает это сам.
