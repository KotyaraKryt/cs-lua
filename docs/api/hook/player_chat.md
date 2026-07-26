---
title: player_chat
description: "Игрок написал в чат"
---

# player_chat

Игрок написал в чат.

```lua
hook.add("player_chat", id, function(e)
	...
end)
```

## Поля события

| поле | тип |  |
|---|---|---|
| `e.player` | player | кого касается событие |
| `e.text` | string | что он написал |
| `e.team` | boolean | сообщение ушло в `say_team` |

## Пример

```lua
hook.add("player_chat", "myplugin.mute", function(e)
	if muted[e.player.id] then
		e.player:chat("Ты в муте")
		e:cancel()
	end
end)
```

**Отмена.** `e:cancel()` проглатывает сообщение — оно не доходит ни до кого. Так работает `!команда`, и так же чат-менеджер подменяет строку: отменить и разослать свою.
