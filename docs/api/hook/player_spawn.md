---
title: player_spawn
description: "Игрок появился в раунде живым"
---

# player_spawn

Игрок появился в раунде живым.

```lua
hook.add("player_spawn", id, function(e)
	...
end)
```

## Поля события

| поле | тип |  |
|---|---|---|
| `e.player` | player | кого касается событие |

Спавн сбрасывает гравитацию, скорость, заморозку, godmode и noclip — ставь их здесь.

> [!WARNING]
> Событие приходит из ReGameDLL. На ванильном `mp.dll` оно не
> срабатывает никогда.
