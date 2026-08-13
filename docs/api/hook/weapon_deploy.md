---
title: weapon_deploy
description: "Оружие вот-вот покажет вьюмодель и модель в руках"
---

# weapon_deploy

Оружие вот-вот покажет вьюмодель и модель в руках.

```lua
hook.add("weapon_deploy", id, function(e)
	...
end)
```

## Поля события

| поле | тип |  |
|---|---|---|
| `e.player` | player | кого касается событие |
| `e.weapon` | string | classname оружия |
| `e.view_model` | string | путь вьюмодели; запись подменяет её |
| `e.world_model` | string | путь модели в руках; запись подменяет её |

## Пример

```lua
hook.add("weapon_deploy", "healnade.reskin", function(e)
	if e.weapon == "weapon_hegrenade" then
		e.view_model = "models/reapi_healthnade/v_hegrenade_v1.mdl"
		e.world_model = "models/reapi_healthnade/p_hegrenade_v1.mdl"
	end
end)
```

Оба поля приходят уже заполненными тем, что показал бы движок — меняет их только тот, кто хочет реснуть оружие. Не заменяет модель на земле/в полёте: это `grenade_thrown` про `weapon_hegrenade`.

> [!WARNING]
> Событие приходит из ReGameDLL. На ванильном `mp.dll` оно не
> срабатывает никогда.

## Смотри также

- [grenade_thrown](grenade_thrown.md)
