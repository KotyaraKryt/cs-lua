---
title: round_freeze_end
description: "Заморозка кончилась, игроки могут двигаться"
---

# round_freeze_end

Заморозка кончилась, игроки могут двигаться.

```lua
hook.add("round_freeze_end", id, function(e)
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
