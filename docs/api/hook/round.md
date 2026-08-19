---
title: hook — Раунд и бомба
description: "Одна система на движковые события и на свои"
---

# Раунд и бомба

## round_start {#round_start}

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

## round_end {#round_end}

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

## round_freeze_end {#round_freeze_end}

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

## bomb_planted {#bomb_planted}

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

## bomb_defuse_start {#bomb_defuse_start}

Игрок начал разминирование. Не путать с `bomb_defused` — то приходит один
раз в конце, успешно или нет; это — в момент начала попытки.

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

<Warning>
Событие приходит из ReGameDLL. На ванильном `mp.dll` оно не
срабатывает никогда.
</Warning>

## bomb_defused {#bomb_defused}

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

## bomb_exploded {#bomb_exploded}

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

Игрока в событии нет: запоминай заложившего в `bomb_planted`, если он нужен.

<Warning>
Событие приходит из ReGameDLL. На ванильном `mp.dll` оно не
срабатывает никогда.
</Warning>
