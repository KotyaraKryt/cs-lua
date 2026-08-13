---
title: grenade_throw
description: "HE- или дымовая граната вот-вот покинёт руку"
---

# grenade_throw

HE- или дымовая граната вот-вот покинёт руку.

```lua
hook.add("grenade_throw", id, function(e)
	...
end)
```

## Поля события

| поле | тип |  |
|---|---|---|
| `e.player` | player \| nil | кто бросает |
| `e.weapon` | string | `weapon_hegrenade` или `weapon_smokegrenade` |
| `e.fuse` | number | секунд до взрыва; запись подменяет таймер |

## Пример

```lua
hook.add("grenade_throw", "healnade.instant", function(e)
	if e.weapon == "weapon_smokegrenade" then
		e.fuse = 0.05
	end
end)
```

Приходит до того, как движок создаст сам снаряд — на этом этапе ещё нет сущности для `e:detonate_on_touch()`, только число. Взрыв по касанию, а не по таймеру — это [`e:detonate_on_touch()`](../ents/detonate_on_touch.md) в `grenade_thrown`, а не подмена `fuse` здесь.

> [!WARNING]
> Событие приходит из ReGameDLL. На ванильном `mp.dll` оно не
> срабатывает никогда.

## Смотри также

- [grenade_thrown](grenade_thrown.md)
