---
title: hook — Подключение
description: "Порядок: `client:connect` → `client:authorized` → `client:connected`"
---

# Подключение

Порядок: `client:connect` → `client:authorized` → `client:connected`.

## client:connect {#connect}

Игрок стучится на сервер; его ещё можно не пустить.

```lua
hook.add("client:connect", id, function(e)
	...
end)
```

### Поля события {#connect-поля события}

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
`client:authorized`, приветствия — на `client:connected`.
</Warning>

## client:disconnect {#disconnect}

Игрок отключился — уже был в игре, или его коннект отклонил Lua-плагин через
`client:connect`.

```lua
hook.add("client:disconnect", id, function(e)
	...
end)
```

### Поля события {#disconnect-поля события}

| поле | тип |  |
|---|---|---|
| `e.player` | player | кого касается событие |
| `e.name` | string | ник — сам объект уже пустеет |
| `e.reason` | string | причина отключения, см. ниже |
| `e.forced` | boolean | `true` — событие синтезировал модуль (движок его не вызывал), `false` — настоящий `ClientDisconnect` |

Место, где чистят своё состояние по `e.player.id`.

<Warning>
`e.reason` и `e.forced` приходят из двух разных источников в зависимости от
того, что случилось:

- **Игрок был в игре и отвалился** (кик, бан, таймаут, `disconnect` в
  консоли) — `e.forced == false`. Причина от движка через ReHLDS-хук
  `SV_DropClient`. Требует ReHLDS: на ванильном HLDS `e.reason` здесь пустая
  строка. Формат — как в логах сервера: `"Kicked"`, `"Disconnect by user"`,
  `"Timed out"` — движок не даёт фиксированный список кодов, только строку.
- **Коннект отклонил Lua-плагин** через `client:connect` + `e:cancel()` —
  `e.forced == true`. Причина всегда есть: это тот же текст, что увидел
  отказанный игрок (`e.reason`, выставленный в `client:connect`, либо
  дефолтная фраза, если плагин его не задал). Движок в этом случае
  `ClientDisconnect` вообще не вызывает, `client:disconnect` подставляется
  вместо него, чтобы игрок не "пропадал" молча для тех, кто слушает только
  это событие.

Проверяй `e.forced`, если логика зависит от того, был ли игрок реально в
игре.
</Warning>

## client:authorized {#authorized}

Steam ответил, steamid наконец известен.

```lua
hook.add("client:authorized", id, function(e)
	...
end)
```

### Поля события {#authorized-поля события}

| поле | тип |  |
|---|---|---|
| `e.player` | player | кого касается событие |
| `e.steamid` | string | настоящий authid |

Срабатывает один раз за подключение. Всё, что завязано на steamid — права, статистика — начинается здесь.

## client:connected {#connected}

Игрок в игре, сообщения до него доходят.

```lua
hook.add("client:connected", id, function(e)
	...
end)
```

### Поля события {#connected-поля события}

| поле | тип |  |
|---|---|---|
| `e.player` | player | кого касается событие |

Место для приветствий и первого HUD.

## player:chat {#chat}

Игрок написал в чат.

```lua
hook.add("player:chat", id, function(e)
	...
end)
```

### Поля события {#chat-поля события}

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

## menu:select {#select}

Игрок нажал клавишу в меню, открытом из Lua.

```lua
hook.add("menu:select", id, function(e)
	...
end)
```

### Поля события {#select-поля события}

| поле | тип |  |
|---|---|---|
| `e.player` | player | кого касается событие |
| `e.key` | number | номер клавиши, 1..10 |

Обычно не нужно: [`menu`](../menu/index.md) разбирает это сам.
