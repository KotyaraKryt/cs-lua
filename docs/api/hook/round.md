---
title: hook — Раунд и бомба
description: "Одна система на движковые события и на свои"
---

# Раунд и бомба

## round:start {#round_start}

Раунд начался.

```lua
hook.add("round:start", id, function(e)
	...
end)
```

### Поля события {#round_start-поля события}

| поле | тип |  |
|---|---|---|
| `—` | — | событие без полей |

<Warning>
Событие приходит из ReGameDLL. На ванильном `mp.dll` оно не
срабатывает никогда.
</Warning>

## round:end {#round_end}

Раунд закончился.

```lua
hook.add("round:end", id, function(e)
	...
end)
```

### Поля события {#round_end-поля события}

| поле | тип |  |
|---|---|---|
| `e.winner` | number | `1` — CT, `2` — T, `3` — ничья |

<Warning>
Событие приходит из ReGameDLL. На ванильном `mp.dll` оно не
срабатывает никогда.
</Warning>

## round:freeze_end {#round_freeze_end}

Заморозка кончилась, игроки могут двигаться.

```lua
hook.add("round:freeze_end", id, function(e)
	...
end)
```

### Поля события {#round_freeze_end-поля события}

| поле | тип |  |
|---|---|---|
| `—` | — | событие без полей |

<Warning>
Событие приходит из ReGameDLL. На ванильном `mp.dll` оно не
срабатывает никогда.
</Warning>

## round:balance_teams {#round_balance_teams}

Автобаланс только что раскидал игроков по командам.

```lua
hook.add("round:balance_teams", id, function(e)
	...
end)
```

### Поля события {#round_balance_teams-поля события}

| поле | тип |  |
|---|---|---|
| `—` | — | событие без полей |

Приходит уже после перемещения — кого именно перекинули, событие не говорит; команду каждого игрока смотри через `p:team()` или лови `player:team_change`.

<Warning>
Событие приходит из ReGameDLL. На ванильном `mp.dll` оно не
срабатывает никогда.
</Warning>

### Смотри также

- [player:team_change](gameplay.md#player_team_change)

## round:intermission {#round_intermission}

Карта закончилась, сервер вот-вот покажет табло перед сменой карты.

```lua
hook.add("round:intermission", id, function(e)
	...
end)
```

### Поля события {#round_intermission-поля события}

| поле | тип |  |
|---|---|---|
| `—` | — | событие без полей |

Раньше, чем `server:map_change`: мир ещё цел, сущности ещё на месте. Последняя точка для финального сохранения статистики перед сменой карты.

<Warning>
Событие приходит из ReGameDLL. На ванильном `mp.dll` оно не
срабатывает никогда.
</Warning>

### Смотри также

- [server:map_change](lifecycle.md#map_change)

## bomb:planted {#bomb_planted}

Бомба заложена.

```lua
hook.add("bomb:planted", id, function(e)
	...
end)
```

### Поля события {#bomb_planted-поля события}

| поле | тип |  |
|---|---|---|
| `e.player` | player \| nil | кто заложил; `nil`, если игрока установить не удалось |

<Warning>
Событие приходит из ReGameDLL. На ванильном `mp.dll` оно не
срабатывает никогда.
</Warning>

## bomb:defusing {#bomb_defuse_start}

Игрок начал разминирование.

```lua
hook.add("bomb:defusing", id, function(e)
	...
end)
```

### Поля события {#bomb_defuse_start-поля события}

| поле | тип |  |
|---|---|---|
| `e.player` | player | кто начал разминировать |
| `e.defuser` | boolean | с набором для разминирования (быстрее) или голыми руками |

Не путать с `bomb:defused` — то приходит один раз в конце, успешно или нет; это — в момент начала попытки.

<Warning>
Событие приходит из ReGameDLL. На ванильном `mp.dll` оно не
срабатывает никогда.
</Warning>

### Смотри также

- [bomb:defused](round.md#bomb_defused)

## bomb:defused {#bomb_defused}

Попытка разминирования завершилась.

```lua
hook.add("bomb:defused", id, function(e)
	...
end)
```

### Поля события {#bomb_defused-поля события}

| поле | тип |  |
|---|---|---|
| `e.player` | player \| nil | кто разминировал |
| `e.success` | boolean | успел ли |

Событие приходит и при неудаче — проверяй `e.success`.

<Warning>
Событие приходит из ReGameDLL. На ванильном `mp.dll` оно не
срабатывает никогда.
</Warning>

## bomb:exploded {#bomb_exploded}

Бомба взорвалась.

```lua
hook.add("bomb:exploded", id, function(e)
	...
end)
```

### Поля события {#bomb_exploded-поля события}

| поле | тип |  |
|---|---|---|
| `e.x, e.y, e.z` | number | координаты взрыва |

Игрока в событии нет: запоминай заложившего в `bomb:planted`, если он нужен.

<Warning>
Событие приходит из ReGameDLL. На ванильном `mp.dll` оно не
срабатывает никогда.
</Warning>
