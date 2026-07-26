---
title: bomb_planted
description: "Бомба заложена"
---

# bomb_planted

Бомба заложена.

```lua
hook.add("bomb_planted", id, function(e)
	...
end)
```

## Поля события

| поле | тип |  |
|---|---|---|
| `e.player` | player \| nil | кто заложил; `nil`, если игрока установить не удалось |

> [!WARNING]
> Событие приходит из ReGameDLL. На ванильном `mp.dll` оно не
> срабатывает никогда.
