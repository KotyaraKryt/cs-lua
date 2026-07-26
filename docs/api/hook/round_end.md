---
title: round_end
description: "Раунд закончился"
---

# round_end

Раунд закончился.

```lua
hook.add("round_end", id, function(e)
	...
end)
```

## Поля события

| поле | тип |  |
|---|---|---|
| `e.winner` | number | `1` — CT, `2` — T, `3` — ничья |

> [!WARNING]
> Событие приходит из ReGameDLL. На ванильном `mp.dll` оно не
> срабатывает никогда.
