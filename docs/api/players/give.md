---
title: p:give
description: "Выдаёт игроку предмет по classname"
---

# p:give

Выдаёт игроку предмет по classname.

```lua
p:give(classname)
```

## Аргументы

| # | имя | тип |  |
|---|---|---|---|
| 1 | `classname` | string | `weapon_ak47`, `item_assaultsuit`, `weapon_hegrenade` |

## Возвращает

Ничего.

## Пример

```lua
p:give("weapon_ak47")
```

> [!WARNING]
> Требует ReGameDLL. На ванильном `mp.dll` метод бросает ошибку.
