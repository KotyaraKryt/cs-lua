---
title: bomb_exploded
description: "Бомба взорвалась"
---

# bomb_exploded

Бомба взорвалась.

```lua
hook.add("bomb_exploded", id, function(e)
	...
end)
```

## Поля события

| поле | тип |  |
|---|---|---|
| `e.x, e.y, e.z` | number | координаты взрыва |

Игрока в событии нет: запоминай заложившего в `bomb_planted`, если он нужен.

> [!WARNING]
> Событие приходит из ReGameDLL. На ванильном `mp.dll` оно не
> срабатывает никогда.
