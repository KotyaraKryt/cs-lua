---
title: round_start
description: "Раунд начался"
---

# round_start

Раунд начался.

```lua
hook.add("round_start", id, function(e)
	...
end)
```

## Поля события

| поле | тип |  |
|---|---|---|
| `—` | — | событие без полей |

> [!WARNING]
> Событие приходит из ReGameDLL. На ванильном `mp.dll` оно не
> срабатывает никогда.
