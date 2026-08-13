---
title: fx.sprite_trail
description: "Поток светящихся спрайтов между двумя точками"
---

# fx.sprite_trail

Поток светящихся спрайтов между двумя точками.

```lua
fx.sprite_trail(x, y, z, opts)
```

## Аргументы

| # | имя | тип |  |
|---|---|---|---|
| 1 | `x, y, z` | number | начальная точка |
| 2 | `opts` | table | см. Опции |

## Возвращает

Ничего.

## Пример

```lua
fx.sprite_trail(x, y, z, { sprite = "sprites/reapi_healthnade/heal_shape.spr", height = 150 })
```

## Опции

| поле | тип |  |
|---|---|---|
| `sprite` | string | путь спрайта, обязателен |
| `to` | table \| nil | конечная точка `{ x, y, z }` |
| `height` | number | если `to` нет, конец — точка + `height` по Z; `64` по умолчанию |
| `count` | number | сколько спрайтов; `20` по умолчанию |
| `life` | number | секунд; `1.0` по умолчанию |
| `scale` | number | `1.0` по умолчанию |
| `velocity` | number | случайный разброс скорости; `10.0` по умолчанию |
| `spread` | number | случайный разброс позиции; `20.0` по умолчанию |

Конечная точка — либо `opts.to = { x, y, z }`, либо, если его нет,
`{ x, y, z + opts.height }`.

> [!WARNING]
> Спрайт обязан быть уже прекеширован через
> `res.model`, тем же путём. Функция не прекеширует сама — это тот же
> контракт, что у `p:play_sound` и `res.sound`.

## Смотри также

- [res.model](../res/index.md)
