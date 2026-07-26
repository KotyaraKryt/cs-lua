---
title: e:classname
description: "Classname сущности"
---

# e:classname

Classname сущности.

```lua
e:classname()
```

## Возвращает

| тип |  |
|---|---|
| `string` | classname |

> [!WARNING]
> Объект от удалённой сущности — или переживший `changelevel` —
> бросает `entity #N is gone`. Проверяй [`e:valid()`](valid.md).
