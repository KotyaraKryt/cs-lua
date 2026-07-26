---
title: bomb_defused
description: "Попытка разминирования завершилась"
---

# bomb_defused

Попытка разминирования завершилась.

```lua
hook.add("bomb_defused", id, function(e)
	...
end)
```

## Поля события

| поле | тип |  |
|---|---|---|
| `e.player` | player \| nil | кто разминировал |
| `e.success` | boolean | успел ли |

Событие приходит и при неудаче — проверяй `e.success`.

> [!WARNING]
> Событие приходит из ReGameDLL. На ванильном `mp.dll` оно не
> срабатывает никогда.
