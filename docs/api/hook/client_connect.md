---
title: client_connect
description: "Игрок стучится на сервер; его ещё можно не пустить"
---

# client_connect

Игрок стучится на сервер; его ещё можно не пустить.

```lua
hook.add("client_connect", id, function(e)
	...
end)
```

## Поля события

| поле | тип |  |
|---|---|---|
| `e.player` | player | кого касается событие |
| `e.name` | string | ник |
| `e.ip` | string | адрес |
| `e.reason` | string | запись: текст отказа, который увидит игрок |

## Пример

```lua
hook.add("client_connect", "bans.check", function(e)
	if banned[e.ip] then
		e.reason = "Вы забанены"
		e:cancel()
	end
end)
```

**Отмена.** `e:cancel()` не пускает игрока. Без `e.reason` он увидит общую фразу.

> [!WARNING]
> Здесь `e.player:steamid()` возвращает `STEAM_ID_PENDING`, а
> сообщения не доходят. Права и статистику вешай на
> `player_authorized`, приветствия — на `player_ready`.
