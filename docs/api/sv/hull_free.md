---
title: sv.hull_free
description: "Влезет ли игрок в эту точку, не застряв в геометрии"
---

# sv.hull_free

Влезет ли игрок в эту точку, не застряв в геометрии.

```lua
sv.hull_free(x, y, z[, ducking])
```

## Аргументы

| # | имя | тип |  |
|---|---|---|---|
| 1 | `x` | number | |
| 2 | `y` | number | |
| 3 | `z` | number | точка — нога, не центр модели |
| 4 | `ducking` | boolean \| nil | хитбокс приседа вместо стоячего |

## Возвращает

| тип |  |
|---|---|
| `boolean` | `true` — точка свободна |

Нулевой `TRACE_HULL` в этой точке: тот же способ, которым сам движок проверяет
застревание. Игроков не задевает (`ignore_monsters`) — только мир и статичная
геометрия карты.

## Пример

```lua
if not sv.hull_free(x, y, z) then
	z = z + 16
end
p:origin(x, y, z)
```

## Смотри также

- [p:trace](../players/trace.md)
- [p:origin](../players/origin.md)
