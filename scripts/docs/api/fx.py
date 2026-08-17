# -*- coding: utf-8 -*-

PRECACHED = ('warning', 'Спрайт обязан быть уже прекеширован через\n`res.model`, тем же путём. Функция не прекеширует сама — это тот же\nконтракт, что у `p:play_sound` и `res.sound`.')

TO_HEIGHT = """
Конечная точка — либо `opts.to = { x, y, z }`, либо, если его нет,
`{ x, y, z + opts.height }`.
"""

NS = {
    'dir': 'fx',
    'label': 'fx',
    'title': 'fx',
    'brief': 'Временные эффекты: взрывы, лучи, шлейфы спрайтов.',
    'intro': """
Разовая отправка, без объекта и без обратной связи — так же, как `res.sound`
и `res.model` это прекеш и забыть. Все три функции шлют то, что в HLSDK
называется temp entity: увидят игроки, у кого точка в PVS, ничего не
создаётся на сервере и не занимает слот сущности.
""",
    'groups': [
        {
            'items': [
                {
                    'name': 'fx.explosion',
                    'brief': 'Спрайт взрыва в точке.',
                    'sig': 'fx.explosion(x, y, z, opts)',
                    'args': [
                        ('x, y, z', 'number', 'точка взрыва'),
                        ('opts', 'table', 'см. Опции'),
                    ],
                    'extra': """
### Опции

| поле | тип |  |
|---|---|---|
| `sprite` | string | путь спрайта, обязателен |
| `scale` | number | `30` по умолчанию |
| `framerate` | number | `20` по умолчанию |
| `flags` | number | `0` — обычный взрыв Half-Life; см. `TE_EXPLFLAG_*` в HLSDK, например `4` глушит родной звук взрыва |
""",
                    'example': """
fx.explosion(x, y, z, { sprite = "sprites/reapi_healthnade/heal_explode.spr", flags = 4 })
""",
                    'notes': [PRECACHED],
                    'see': [('res.model', '../res/index.md')],
                },
                {
                    'name': 'fx.beam_cylinder',
                    'brief': 'Кольцо луча, расширяющееся от точки.',
                    'sig': 'fx.beam_cylinder(x, y, z, opts)',
                    'args': [
                        ('x, y, z', 'number', 'центр'),
                        ('opts', 'table', 'см. Опции'),
                    ],
                    'extra': """
### Опции

| поле | тип |  |
|---|---|---|
| `sprite` | string | путь спрайта луча, обязателен |
| `to` | table \\| nil | конечная точка `{ x, y, z }` |
| `height` | number | если `to` нет, конец — точка + `height` по Z; `128` по умолчанию |
| `life` | number | секунд; `1.0` по умолчанию |
| `width` | number | толщина луча; `10` по умолчанию |
| `amplitude` | number | дрожание; `0` по умолчанию |
| `color` | table | `{ r, g, b }`; `{255,255,255}` по умолчанию |
| `brightness` | number | `255` по умолчанию |
| `speed` | number | скорость анимации текстуры; `10` по умолчанию |
| `framerate` | number | `10` по умолчанию |
""" + TO_HEIGHT,
                    'example': """
fx.beam_cylinder(x, y, z, {
	sprite = "sprites/shockwave.spr",
	color = { 120, 220, 120 },
	height = 150,
})
""",
                    'notes': [PRECACHED],
                    'see': [('res.model', '../res/index.md')],
                },
                {
                    'name': 'fx.sprite_trail',
                    'brief': 'Поток светящихся спрайтов между двумя точками.',
                    'sig': 'fx.sprite_trail(x, y, z, opts)',
                    'args': [
                        ('x, y, z', 'number', 'начальная точка'),
                        ('opts', 'table', 'см. Опции'),
                    ],
                    'extra': """
### Опции

| поле | тип |  |
|---|---|---|
| `sprite` | string | путь спрайта, обязателен |
| `to` | table \\| nil | конечная точка `{ x, y, z }` |
| `height` | number | если `to` нет, конец — точка + `height` по Z; `64` по умолчанию |
| `count` | number | сколько спрайтов; `20` по умолчанию |
| `life` | number | секунд; `1.0` по умолчанию |
| `scale` | number | `1.0` по умолчанию |
| `velocity` | number | случайный разброс скорости; `10.0` по умолчанию |
| `spread` | number | случайный разброс позиции; `20.0` по умолчанию |
""" + TO_HEIGHT,
                    'example': """
fx.sprite_trail(x, y, z, { sprite = "sprites/reapi_healthnade/heal_shape.spr", height = 150 })
""",
                    'notes': [PRECACHED],
                    'see': [('res.model', '../res/index.md')],
                },
            ],
        },
    ],
}
