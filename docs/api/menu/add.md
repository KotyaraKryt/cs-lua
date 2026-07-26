---
title: m:add
description: "Добавляет пункт"
---

# m:add

Добавляет пункт.

```lua
m:add(text[, fn][, opts])
```

## Аргументы

| # | имя | тип |  |
|---|---|---|---|
| 1 | `text` | string | текст пункта |
| 2 | `fn` | function \| nil | получает `(player, item)` |
| 3 | `opts` | table \| nil | см. [Опции](#опции) |

## Возвращает

| тип |  |
|---|---|
| `table` | пункт — на него можно вешать свои поля |

## Опции

| поле | тип |  |
|---|---|---|
| `disabled` | boolean | видно, нажать нельзя |
| `value` | any | своё значение на пункте |
| `color` | string \| table | цвет пункта целиком или `{ number = , text = }` |

## Пример

```lua
m:add("AWP", function(p, item)
	p:give("weapon_awp")
end, { color = "red", value = 4750 })

m:add("Пока недоступно", nil, { disabled = true })
```
