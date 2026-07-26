---
title: m:item_color
description: "Перекрашивает один пункт"
---

# m:item_color

Перекрашивает один пункт.

```lua
m:item_color(item, spec)
```

## Аргументы

| # | имя | тип |  |
|---|---|---|---|
| 1 | `item` | table | то, что вернул `m:add` |
| 2 | `spec` | string \| table | цвет |

## Возвращает

| тип |  |
|---|---|
| `table` | тот же пункт |

## Пример

```lua
m:item_color(m.items[1], "grey")
```
