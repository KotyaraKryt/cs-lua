---
title: p:trace
description: "Пускает луч из глаз игрока туда, куда он смотрит"
---

# p:trace

Пускает луч из глаз игрока туда, куда он смотрит.

```lua
p:trace([distance])
```

## Аргументы

| # | имя | тип |  |
|---|---|---|---|
| 1 | `distance` | number \| nil | длина луча в юнитах, по умолчанию `8192` |

## Возвращает

| тип |  |
|---|---|
| `table` | что попалось, см. [Результат](#результат) |

## Результат

| поле | тип |  |
|---|---|---|
| `kind` | string | `player`, `entity`, `world`, `none` |
| `player` | player | только при `kind == "player"` |
| `classname` | string | classname цели; нет при `none` |
| `x, y, z` | number | точка попадания; при `none` — конец луча |
| `distance` | number | юнитов от глаз до точки |
| `hitgroup` | number | `1` — голова, `0` — тело |

## Пример

```lua
local t = p:trace()
if t.kind == "player" then
	p:chat(("%s, %d hp, %d юнитов"):format(t.player:name(), t.player:health(), t.distance))
end
```
