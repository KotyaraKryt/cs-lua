---
title: ents
description: "Сущности: дропы, точки интереса, зоны, маркеры"
---

# ents

Сущности: дропы, точки интереса, зоны, маркеры.

```lua
hook.add("round_start", "drops.spawn", function()
	local e = ents.create("info_target")
	e:model("models/w_ak47.mdl")
	e:origin(0, 0, 0)
	e:spawn()
end)
```

ReGameDLL не нужен — слой работает и на ванильном `mp.dll`.

Объект хранит индекс edict'а и его serial number. Движок переиспользует индексы,
поэтому каждый метод сверяет serial и при несовпадении бросает ошибку, а не
пишет в чужую сущность.

Объект защищён от записи: `e.foo = 1` бросит ошибку. Своё состояние держи в
своей таблице, ключом — `e.index`.

## Пространство имён

|  |  |
|---|---|
| [`ents.create`](namespace.md#create) | Создаёт сущность по classname |
| [`ents.find`](namespace.md#find) | Находит все сущности с заданным classname |
| [`ents.in_sphere`](namespace.md#in_sphere) | Находит все сущности в радиусе от точки |

## Объект сущности

|  |  |
|---|---|
| [`e.index`](entity.md#index) | Индекс edict'а |
| [`e:origin`](entity.md#origin) | Читает или задаёт позицию сущности |
| [`e:angles`](entity.md#angles) | Читает или задаёт поворот сущности |
| [`e:model`](entity.md#model) | Читает или задаёт модель сущности |
| [`e:classname`](entity.md#classname) | Classname сущности |
| [`e:valid`](entity.md#valid) | Жива ли сущность |
| [`e:spawn`](entity.md#spawn) | Запускает `Spawn` сущности |
| [`e:remove`](entity.md#remove) | Убирает сущность из мира |
| [`e:detonate_on_touch`](entity.md#detonate_on_touch) | Следующее касание сразу запускает think сущности |
| [`e:keyvalue`](entity.md#keyvalue) | Задаёт keyvalue — то же, что делает карта |
| [`e:solid`](entity.md#solid) | Читает или задаёт тип столкновений |
| [`e:movetype`](entity.md#movetype) | Читает или задаёт, как движок двигает сущность |
| [`e:size`](entity.md#size) | Читает или задаёт ограничивающий объём |
| [`e:render`](entity.md#render) | Читает или задаёт прозрачность и свечение |

## Границы слоя

Слоя нет для: своих think-функций, произвольных touch-обработчиков и своих
классов сущностей — всё это живёт в игре, а не в Lua.
[`e:detonate_on_touch`](entity.md#detonate_on_touch) не исключение, а частный случай:
он форсирует уже существующий think сущности, а не даёт Lua-коду решать,
что делать при касании.
