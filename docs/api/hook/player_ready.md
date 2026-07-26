---
title: player_ready
description: "Игрок в игре, сообщения до него доходят"
---

# player_ready

Игрок в игре, сообщения до него доходят.

```lua
hook.add("player_ready", id, function(e)
	...
end)
```

## Поля события

| поле | тип |  |
|---|---|---|
| `e.player` | player | кого касается событие |

Место для приветствий и первого HUD.
