---
title: grenade_explode
description: "Граната HE вот-вот взорвётся"
---

# grenade_explode

Граната HE вот-вот взорвётся.

```lua
hook.add("grenade_explode", id, function(e)
	...
end)
```

## Поля события

| поле | тип |  |
|---|---|---|
| `e.player` | player \| nil | кто бросил; `nil`, если бросавшего не осталось |
| `e.x, e.y, e.z` | number | точка взрыва |

## Пример

```lua
hook.add("grenade_explode", "healnade.explode", function(e)
	if not e.player then return end
	e:cancel()
	for _, p in ipairs(players.list{ alive = true, team = e.player:team() }) do
		p:health(math.min(100, p:health() + 25))
	end
end)
```

**Отмена.** `e:cancel()` забирает взрыв целиком себе: ни урона, ни decal'ей, ни звука от игры — дальше плагин сам решает, что происходит в этой точке.

Только `weapon_hegrenade` — у флешки и дымовой свой взрыв без урона, событие их не трогает. Бомбу тоже не задевает: у неё отдельная цепочка, `bomb_exploded`.

> [!WARNING]
> Событие приходит из ReGameDLL. На ванильном `mp.dll` оно не
> срабатывает никогда.
