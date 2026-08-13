---
title: grenade_thrown
description: "HE-граната только что покинула руку"
---

# grenade_thrown

HE-граната только что покинула руку.

```lua
hook.add("grenade_thrown", id, function(e)
	...
end)
```

## Поля события

| поле | тип |  |
|---|---|---|
| `e.player` | player \| nil | кто бросил |
| `e.entity` | entity \| nil | сама граната; `nil`, если бросок не удался |

## Пример

```lua
hook.add("grenade_thrown", "healnade.reskin", function(e)
	if e.entity then
		e.entity:model("models/reapi_healthnade/w_hegrenade_v1.mdl")
	end
end)
```

`e.entity` — обычный объект сущности, тот же, что даёт `ents.create`: `e:model()`, `e:origin()` и весь остальной [`ents`](../ents/index.md) работают на нём как на любой другой.

> [!WARNING]
> Событие приходит из ReGameDLL. На ванильном `mp.dll` оно не
> срабатывает никогда.

## Смотри также

- [weapon_deploy](weapon_deploy.md)
- [ents](../ents/index.md)
