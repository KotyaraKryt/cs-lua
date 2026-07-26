---
title: p:play_sound
description: "Проигрывает игроку звук"
---

# p:play_sound

Проигрывает игроку звук.

```lua
p:play_sound(path[, opts])
```

## Аргументы

| # | имя | тип |  |
|---|---|---|---|
| 1 | `path` | string | путь от `cstrike/sound/`, обязан быть предкэширован |
| 2 | `opts` | table \| nil | см. [Опции](#опции) |

## Возвращает

Ничего.

## Опции

| поле | тип |  |
|---|---|---|
| `volume` | number | 0..1, по умолчанию `1.0` |
| `attenuation` | number | затухание с расстоянием; `0` — одинаково слышно везде. По умолчанию `0.8` |
| `channel` | number | канал звука, по умолчанию `0` |
| `pitch` | number | тон, по умолчанию `100` |

## Пример

```lua
p:play_sound("items/9mmclip1.wav", { attenuation = 0 })
```

## Смотри также

- [res.sound](../res/sound.md)
