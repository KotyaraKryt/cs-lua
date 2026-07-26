---
title: ents.create
description: "Создаёт сущность по classname"
---

# ents.create

Создаёт сущность по classname.

```lua
ents.create(classname)
```

## Аргументы

| # | имя | тип |  |
|---|---|---|---|
| 1 | `classname` | string | `info_target`, `func_door`, … |

## Возвращает

| тип |  |
|---|---|
| `entity \| nil` | новая сущность |
| `string` | причина, если игра не знает classname |

## Пример

```lua
hook.add("round_start", "drops.spawn", function()
	local e = ents.create("info_target")
	e:model("models/w_ak47.mdl")
	e:origin(x, y, z)
	e:spawn()
end)
```

Возвращает голый edict: игра ещё не превратила его ни во что. Сначала модель и
позиция, потом [`e:spawn()`](spawn.md) — `Spawn` читает уже выставленные поля. В
обратном порядке получится сущность, которую игра игнорирует.

Модель обязана быть предкэширована через [`res.model`](../res/model.md), а это
делается в теле плагина.

> [!WARNING]
> Пока карта не загружена, сущностей не существует. Тело плагина
> выполняется задолго до неё, и здесь это ошибка. Создавай сущности
> из события: `round_start`, `player_spawn`, таймер, команда.
