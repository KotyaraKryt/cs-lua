---
title: fx.beam_cylinder
description: "Кольцо луча, расширяющееся от точки"
---

# fx.beam_cylinder

Кольцо луча, расширяющееся от точки.

```lua
fx.beam_cylinder(x, y, z, opts)
```

## Аргументы

| # | имя | тип |  |
|---|---|---|---|
| 1 | `x, y, z` | number | центр |
| 2 | `opts` | table | см. Опции |

## Возвращает

Ничего.

## Пример

```lua
fx.beam_cylinder(x, y, z, {
	sprite = "sprites/shockwave.spr",
	color = { 120, 220, 120 },
	height = 150,
})
```

## Опции

| поле | тип |  |
|---|---|---|
| `sprite` | string | путь спрайта луча, обязателен |
| `to` | table \| nil | конечная точка `{ x, y, z }` |
| `height` | number | если `to` нет, конец — точка + `height` по Z; `128` по умолчанию |
| `life` | number | секунд; `1.0` по умолчанию |
| `width` | number | толщина луча; `10` по умолчанию |
| `amplitude` | number | дрожание; `0` по умолчанию |
| `color` | table | `{ r, g, b }`; `{255,255,255}` по умолчанию |
| `brightness` | number | `255` по умолчанию |
| `speed` | number | скорость анимации текстуры; `10` по умолчанию |
| `framerate` | number | `10` по умолчанию |

Конечная точка — либо `opts.to = { x, y, z }`, либо, если его нет,
`{ x, y, z + opts.height }`.

> [!WARNING]
> Спрайт обязан быть уже прекеширован через
> `res.model`, тем же путём. Функция не прекеширует сама — это тот же
> контракт, что у `p:play_sound` и `res.sound`.

## Смотри также

- [res.model](../res/index.md)
