---
title: player_death
description: "Игрок погиб"
---

# player_death

Игрок погиб.

```lua
hook.add("player_death", id, function(e)
	...
end)
```

## Поля события

| поле | тип |  |
|---|---|---|
| `e.victim` | player | погибший |
| `e.killer` | player \| nil | `nil` при падении, уроне мира и окружения |
| `e.headshot` | boolean | попадание в голову |
| `e.weapon` | string \| nil | classname того, что было у убийцы в руках |
| `e.distance` | number \| nil | юнитов между убийцей и жертвой |

## Пример

```lua
hook.add("player_death", "myplugin.bounty", function(e)
	if not e.killer or e.killer.id == e.victim.id then return end
	e.killer:money(e.killer:money() + (e.headshot and 600 or 300), { hud = true })
end)
```

Суицид приходит как `e.killer.id == e.victim.id`. Вместе с `e.killer` в `nil` уходят `e.weapon` и `e.distance`.

> [!NOTE]
> Граната и взрыв C4 приходят как оружие в руках убийцы, а не как
> `hegrenade`. Отличить снаряд можно по `e.bits` в `player_hurt`.

> [!WARNING]
> Событие приходит из ReGameDLL. На ванильном `mp.dll` оно не
> срабатывает никогда.
