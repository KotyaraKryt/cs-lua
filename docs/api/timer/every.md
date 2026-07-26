---
title: timer.every
description: "Вызывает функцию каждые N секунд, пока её не снимут"
---

# timer.every

Вызывает функцию каждые N секунд, пока её не снимут.

```lua
timer.every(seconds, fn[, opts])
```

## Аргументы

| # | имя | тип |  |
|---|---|---|---|
| 1 | `seconds` | number | интервал, должен быть больше нуля |
| 2 | `fn` | function | вызывается каждые `seconds` |
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
local tick = timer.every(1, function()
	left = left - 1
	if left <= 0 then timer.cancel(tick) end
end)
```

> [!NOTE]
> Повторяющийся таймер, заведённый в теле плагина, лучше делать
> именованным: [`timer.create`](create.md) не задваивается при
> перезагрузке плагина.

## Смотри также

- [timer.create](create.md)
