---
title: timer.after
description: "Вызывает функцию один раз через заданное время"
---

# timer.after

Вызывает функцию один раз через заданное время.

```lua
timer.after(seconds, fn[, opts])
```

## Аргументы

| # | имя | тип |  |
|---|---|---|---|
| 1 | `seconds` | number | задержка; `0` и меньше — на ближайшем кадре |
| 2 | `fn` | function | вызывается один раз |
| 3 | `opts` | table \| nil | см. [Опции](#опции) |

## Возвращает

| тип |  |
|---|---|
| `number` | id для [`timer.cancel`](cancel.md) |

## Опции

| поле | тип |  |
|---|---|---|
| `persist` | boolean | переживать смену карты; по умолчанию `true` |

## Пример

```lua
timer.after(0.5, function()
	if p:connected() then
		p:chat("прошло полсекунды")
	end
end)
```

> [!WARNING]
> Объект игрока, захваченный в замыкание, может пережить
> самого игрока. В теле коллбэка проверяй `p:connected()`.

## Смотри также

- [timer.cancel](cancel.md)
- [timer.create](create.md)
