---
title: hook — Подключение
description: "Порядок: `client_connect` → `player_authorized` → `player_ready`"
---

# Подключение

Порядок: `client_connect` → `player_authorized` → `player_ready`.

## client_connect {#client_connect}

Игрок стучится на сервер; его ещё можно не пустить.

```lua
hook.add("client:connect", id, function(e)
	...
end)
```

### Поля события {#client_connect-поля события}

| поле | тип |  |
|---|---|---|
| `e.player` | player | кого касается событие |
| `e.name` | string | ник |
| `e.ip` | string | адрес |
| `e.reason` | string | запись: текст отказа, который увидит игрок |

### Пример

```lua
hook.add("client:connect", "bans.check", function(e)
	if banned[e.ip] then
		e.reason = "Вы забанены"
		e:cancel()
	end
end)
```

**Отмена.** `e:cancel()` не пускает игрока. Без `e.reason` он увидит общую фразу.

<Warning>
Здесь `e.player:steamid()` возвращает `STEAM_ID_PENDING`, а
сообщения не доходят. Права и статистику вешай на
`player_authorized`, приветствия — на `player_ready`.
</Warning>

## client_disconnect {#client_disconnect}

Игрок отключился.

```lua
hook.add("client:disconnect", id, function(e)
	...
end)
```

### Поля события {#client_disconnect-поля события}

| поле | тип |  |
|---|---|---|
| `e.player` | player | кого касается событие |
| `e.name` | string | ник — сам объект уже пустеет |

Место, где чистят своё состояние по `e.player.id`.

## player_authorized {#player_authorized}

Steam ответил, steamid наконец известен.

```lua
hook.add("client:authorized", id, function(e)
	...
end)
```

### Поля события {#player_authorized-поля события}

| поле | тип |  |
|---|---|---|
| `e.player` | player | кого касается событие |
| `e.steamid` | string | настоящий authid |

Срабатывает один раз за подключение. Всё, что завязано на steamid — права, статистика — начинается здесь.

## player_ready {#player_ready}

Игрок в игре, сообщения до него доходят.

```lua
hook.add("client:connected", id, function(e)
	...
end)
```

### Поля события {#player_ready-поля события}

| поле | тип |  |
|---|---|---|
| `e.player` | player | кого касается событие |

Место для приветствий и первого HUD.

## player_chat {#player_chat}

Игрок написал в чат.

```lua
hook.add("player:chat", id, function(e)
	...
end)
```

### Поля события {#player_chat-поля события}

| поле | тип |  |
|---|---|---|
| `e.player` | player | кого касается событие |
| `e.text` | string | что он написал |
| `e.team` | boolean | сообщение ушло в `say_team` |

### Пример

```lua
hook.add("player:chat", "myplugin.mute", function(e)
	if muted[e.player.id] then
		e.player:chat("Ты в муте")
		e:cancel()
	end
end)
```

**Отмена.** `e:cancel()` проглатывает сообщение — оно не доходит ни до кого. Так работает `!команда`, и так же чат-менеджер подменяет строку: отменить и разослать свою.

## menu_select {#menu_select}

Игрок нажал клавишу в меню, открытом из Lua.

```lua
hook.add("menu:select", id, function(e)
	...
end)
```

### Поля события {#menu_select-поля события}

| поле | тип |  |
|---|---|---|
| `e.player` | player | кого касается событие |
| `e.key` | number | номер клавиши, 1..10 |

Обычно не нужно: [`menu`](../menu/index.md) разбирает это сам.
