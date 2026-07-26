---
title: client_disconnect
description: "Игрок отключился"
---

# client_disconnect

Игрок отключился.

```lua
hook.add("client_disconnect", id, function(e)
	...
end)
```

## Поля события

| поле | тип |  |
|---|---|---|
| `e.player` | player | кого касается событие |
| `e.name` | string | ник — сам объект уже пустеет |

Место, где чистят своё состояние по `e.player.id`.
