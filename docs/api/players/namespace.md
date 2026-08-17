---
title: players — Пространство имён
description: "Поиск игроков, рассылка и объект игрока"
---

# Пространство имён

## players.get {#get}

Возвращает игрока по номеру слота.

```lua
players.get(id)
```

### Аргументы

| # | имя | тип |  |
|---|---|---|---|
| 1 | `id` | number | номер слота, 1..32 |

### Возвращает

| тип |  |
|---|---|
| `player \| nil` | `nil`, если слот пуст |

## players.list {#list}

Возвращает массив подключённых игроков.

```lua
players.list([filter])
```

### Аргументы

| # | имя | тип |  |
|---|---|---|---|
| 1 | `filter` | table \| nil | см. [Фильтр](#list-фильтр) |

### Возвращает

| тип |  |
|---|---|
| `table` | массив объектов игроков |

### Фильтр {#list-фильтр}

| поле | тип |  |
|---|---|---|
| `alive` | boolean | только живые или только мёртвые |
| `team` | string | `CT`, `T`, `SPEC` |

### Пример

```lua
for _, p in ipairs(players.list{ alive = true, team = "CT" }) do
	p:chat("живой контр")
end
```

<Warning>
Фильтры читают живое CS-состояние и требуют ReGameDLL.
Без фильтра метод работает и на ванильном `mp.dll`.
</Warning>

## players.find {#find}

Ищет игрока по слоту, userid или части ника.

```lua
players.find(token)
```

### Аргументы

| # | имя | тип |  |
|---|---|---|---|
| 1 | `token` | string | см. [Форматы](#find-форматы) |

### Возвращает

| тип |  |
|---|---|
| `player \| nil` | найденный игрок |
| `string` | причина, если не найден |

### Форматы {#find-форматы}

| поле | тип |  |
|---|---|---|
| `"3"` | string | номер слота |
| `"#12"` | string | userid |
| `"kotya"` | string | часть ника, регистр не важен; совпадение должно быть одно |

### Пример

```lua
local target, why = players.find(ctx.args[1])
if not target then
	return ctx.reply(why)
end
```

Тот же поиск делает `opts.target` в [`cmd.add`](../cmd/index.md#add).

## players.broadcast {#broadcast}

Приёмник «всем сразу»: только отправка сообщений.

```lua
players.broadcast:chat(text[, opts])
```

### Пример

```lua
players.broadcast:chat("{green}[Server]{default} раунд начался")
players.broadcast:play_sound("items/9mmclip1.wav")
```

Понимает те же методы отправки, что и обычный игрок:
[`chat`](messages.md#chat), [`console`](messages.md#console), [`center`](messages.md#center),
[`hud`](messages.md#hud), [`dhud`](messages.md#dhud), [`play_sound`](messages.md#play_sound).

`play_sound` тут — один `EMIT_SOUND` от первого подключённого игрока, а не
цикл по всем: `EMIT_SOUND` и так слышен всем, у кого PAS накрывает точку
излучения, повторять его на каждого — значит дать части слушателей услышать
один и тот же клип по два-три раза подряд.

<Warning>
Состояния у него нет: `players.broadcast:alive()` бросает ошибку
с объяснением. Для чтения и записи состояния перебирай
`players.list()`.
</Warning>

## players.method {#method}

Добавляет свой метод всем объектам игроков.

```lua
players.method(name, fn)
```

### Аргументы

| # | имя | тип |  |
|---|---|---|---|
| 1 | `name` | string | имя метода |
| 2 | `fn` | function | получает `(self, ...)` |

### Возвращает

Ничего.

### Пример

```lua
players.method("playtime", function(self)
	return sessions[self.id] and sessions[self.id]:seconds() or 0
end)
```

<Warning>
Занятое имя переопределить нельзя — это то, что не даёт одному
плагину подменить `p:health()` для всех остальных.
</Warning>
