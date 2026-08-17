---
title: timer — Анонимные
description: "Адресуются по id, который вернул вызов"
---

# Анонимные

Адресуются по id, который вернул вызов.

## timer.after {#after}

Вызывает функцию один раз через заданное время.

```lua
timer.after(seconds, fn[, opts])
```

### Аргументы

| # | имя | тип |  |
|---|---|---|---|
| 1 | `seconds` | number | задержка; `0` и меньше — на ближайшем кадре |
| 2 | `fn` | function | вызывается один раз |
| 3 | `opts` | table \| nil | см. [Опции](#after-опции) |

### Возвращает

| тип |  |
|---|---|
| `number` | id для [`timer.cancel`](anonymous.md#cancel) |

### Опции {#after-опции}

| поле | тип |  |
|---|---|---|
| `persist` | boolean | переживать смену карты; по умолчанию `true` |

### Пример

```lua
timer.after(0.5, function()
	if p:connected() then
		p:chat("прошло полсекунды")
	end
end)
```

<Warning>
Объект игрока, захваченный в замыкание, может пережить
самого игрока. В теле коллбэка проверяй `p:connected()`.
</Warning>

### Смотри также

- [timer.cancel](anonymous.md#cancel)
- [timer.create](named.md#create)

## timer.every {#every}

Вызывает функцию каждые N секунд, пока её не снимут.

```lua
timer.every(seconds, fn[, opts])
```

### Аргументы

| # | имя | тип |  |
|---|---|---|---|
| 1 | `seconds` | number | интервал, должен быть больше нуля |
| 2 | `fn` | function | вызывается каждые `seconds` |
| 3 | `opts` | table \| nil | см. [Опции](#every-опции) |

### Возвращает

| тип |  |
|---|---|
| `number` | id для [`timer.cancel`](anonymous.md#cancel) |

### Опции {#every-опции}

| поле | тип |  |
|---|---|---|
| `persist` | boolean | переживать смену карты; по умолчанию `true` |

### Пример

```lua
local tick = timer.every(1, function()
	left = left - 1
	if left <= 0 then timer.cancel(tick) end
end)
```

<Note>
Повторяющийся таймер, заведённый в теле плагина, лучше делать
именованным: [`timer.create`](named.md#create) не задваивается при
перезагрузке плагина.
</Note>

### Смотри также

- [timer.create](named.md#create)

## timer.cancel {#cancel}

Снимает таймер по id.

```lua
timer.cancel(id)
```

### Аргументы

| # | имя | тип |  |
|---|---|---|---|
| 1 | `id` | number | то, что вернул `after` или `every` |

### Возвращает

| тип |  |
|---|---|
| `boolean` | `true`, если таймер существовал и был снят |
