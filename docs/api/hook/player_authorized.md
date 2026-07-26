---
title: player_authorized
description: "Steam ответил, steamid наконец известен"
---

# player_authorized

Steam ответил, steamid наконец известен.

```lua
hook.add("player_authorized", id, function(e)
	...
end)
```

## Поля события

| поле | тип |  |
|---|---|---|
| `e.player` | player | кого касается событие |
| `e.steamid` | string | настоящий authid |

Срабатывает один раз за подключение. Всё, что завязано на steamid — права, статистика — начинается здесь.
