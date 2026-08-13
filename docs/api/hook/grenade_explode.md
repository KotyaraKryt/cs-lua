---
title: grenade_explode
description: "HE- или дымовая граната вот-вот взорвётся"
---

# grenade_explode

HE- или дымовая граната вот-вот взорвётся.

```lua
hook.add("grenade_explode", id, function(e)
	...
end)
```

## Поля события

| поле | тип |  |
|---|---|---|
| `e.player` | player \| nil | кто бросил; `nil`, если бросавшего не осталось |
| `e.weapon` | string | `weapon_hegrenade` или `weapon_smokegrenade` |
| `e.entity` | entity \| nil | сама граната; `nil`, если уже пропала |
| `e.x, e.y, e.z` | number | точка взрыва |

## Пример

```lua
hook.add("grenade_explode", "healnade.explode", function(e)
	if e.weapon ~= "weapon_smokegrenade" or not e.player then return end
	e:cancel()
	for _, p in ipairs(players.list{ alive = true, team = e.player:team() }) do
		p:health(math.min(100, p:health() + 25))
	end
	if e.entity then e.entity:remove() end
end)
```

**Отмена.** `e:cancel()` забирает взрыв целиком себе: ни урона (у HE), ни дыма (у smoke), ни звука от игры — дальше плагин сам решает, что происходит в этой точке. Сущность при этом не убирается сама: если она больше не нужна, убирай через `e.entity:remove()`.

Бомбу не задевает: у неё отдельная цепочка, `bomb_exploded`. Флешку тоже — у неё свой взрыв без урона и дыма, событие его не трогает.

> [!WARNING]
> Событие приходит из ReGameDLL. На ванильном `mp.dll` оно не
> срабатывает никогда.
