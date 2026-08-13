---
title: fx.explosion
description: "Спрайт взрыва в точке"
---

# fx.explosion

Спрайт взрыва в точке.

```lua
fx.explosion(x, y, z, opts)
```

## Аргументы

| # | имя | тип |  |
|---|---|---|---|
| 1 | `x, y, z` | number | точка взрыва |
| 2 | `opts` | table | см. Опции |

## Возвращает

Ничего.

## Пример

```lua
fx.explosion(x, y, z, { sprite = "sprites/reapi_healthnade/heal_explode.spr", flags = 4 })
```

## Опции

| поле | тип |  |
|---|---|---|
| `sprite` | string | путь спрайта, обязателен |
| `scale` | number | `30` по умолчанию |
| `framerate` | number | `20` по умолчанию |
| `flags` | number | `0` — обычный взрыв Half-Life; см. `TE_EXPLFLAG_*` в HLSDK, например `4` глушит родной звук взрыва |

> [!WARNING]
> Спрайт обязан быть уже прекеширован через
> `res.model`, тем же путём. Функция не прекеширует сама — это тот же
> контракт, что у `p:play_sound` и `res.sound`.

## Смотри также

- [res.model](../res/index.md)
