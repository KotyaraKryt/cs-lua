---
title: fx
description: "Временные эффекты: взрывы, лучи, шлейфы спрайтов"
---

# fx

Временные эффекты: взрывы, лучи, шлейфы спрайтов.

Разовая отправка, без объекта и без обратной связи — так же, как `res.sound`
и `res.model` это прекеш и забыть. Все три функции шлют то, что в HLSDK
называется temp entity: увидят игроки, у кого точка в PVS, ничего не
создаётся на сервере и не занимает слот сущности.

## fx.explosion {#explosion}

Спрайт взрыва в точке.

```lua
fx.explosion(x, y, z, opts)
```

### Аргументы

| # | имя | тип |  |
|---|---|---|---|
| 1 | `x, y, z` | number | точка взрыва |
| 2 | `opts` | table | см. Опции |

### Возвращает

Ничего.

### Пример

```lua
fx.explosion(x, y, z, { sprite = "sprites/reapi_healthnade/heal_explode.spr", flags = 4 })
```

### Опции

| поле | тип |  |
|---|---|---|
| `sprite` | string | путь спрайта, обязателен |
| `scale` | number | `30` по умолчанию |
| `framerate` | number | `20` по умолчанию |
| `flags` | number | `0` — обычный взрыв Half-Life; см. `TE_EXPLFLAG_*` в HLSDK, например `4` глушит родной звук взрыва |

<Warning>
Спрайт обязан быть уже прекеширован через
`res.model`, тем же путём. Функция не прекеширует сама — это тот же
контракт, что у `p:play_sound` и `res.sound`.
</Warning>

### Смотри также

- [res.model](../res/index.md)

## fx.beam_cylinder {#beam_cylinder}

Кольцо луча, расширяющееся от точки.

```lua
fx.beam_cylinder(x, y, z, opts)
```

### Аргументы

| # | имя | тип |  |
|---|---|---|---|
| 1 | `x, y, z` | number | центр |
| 2 | `opts` | table | см. Опции |

### Возвращает

Ничего.

### Пример

```lua
fx.beam_cylinder(x, y, z, {
	sprite = "sprites/shockwave.spr",
	color = { 120, 220, 120 },
	height = 150,
})
```

### Опции

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

<Warning>
Спрайт обязан быть уже прекеширован через
`res.model`, тем же путём. Функция не прекеширует сама — это тот же
контракт, что у `p:play_sound` и `res.sound`.
</Warning>

### Смотри также

- [res.model](../res/index.md)

## fx.sprite_trail {#sprite_trail}

Поток светящихся спрайтов между двумя точками.

```lua
fx.sprite_trail(x, y, z, opts)
```

### Аргументы

| # | имя | тип |  |
|---|---|---|---|
| 1 | `x, y, z` | number | начальная точка |
| 2 | `opts` | table | см. Опции |

### Возвращает

Ничего.

### Пример

```lua
fx.sprite_trail(x, y, z, { sprite = "sprites/reapi_healthnade/heal_shape.spr", height = 150 })
```

### Опции

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

<Warning>
Спрайт обязан быть уже прекеширован через
`res.model`, тем же путём. Функция не прекеширует сама — это тот же
контракт, что у `p:play_sound` и `res.sound`.
</Warning>

### Смотри также

- [res.model](../res/index.md)
