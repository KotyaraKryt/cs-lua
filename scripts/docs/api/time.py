# -*- coding: utf-8 -*-

NS = {
    'dir': 'time',
    'label': 'time',
    'title': 'time',
    'brief': 'Разбор и форматирование длительностей, календарная арифметика, unixtime.',
    'intro': """
Длительности задаются строкой вроде `"3d"` или `"60s60m24h32d"` (суммируется) —
то, что иначе каждый плагин парсил бы сам: баны, муты, VIP на срок.

```lua
local expires = time.now() + time.parse("7d")
p:set_data("mute_until", expires)

if time.is_expired(expires) then
	p:chat("мут закончился")
end
```

Единицы: `s` секунда, `m` минута, `h` час, `d` день, `w` неделя, `mo` месяц
(фиксированные 30 дней), `y` год (фиксированные 365 дней) — оба приближение,
для реального календаря см. [`time.add_calendar`](add_calendar.md).
""",
    'groups': [
        {
            'title': 'Длительности',
            'slug': 'duration',
            'items': [
                {
                    'name': 'time.parse',
                    'brief': 'Разбирает строку длительности в секунды.',
                    'sig': 'time.parse(str)',
                    'args': [('str', 'string', 'например `"3d"`, `"1h30m"` или `"2mo"`')],
                    'returns': [('number', 'сумма в секундах')],
                    'example': """
time.parse("60s60m24h32d") --> 2854860
time.parse("7d")           --> 604800
""",
                    'extra': 'Число перед незнакомой единицей или числа без единицы в конце — ошибка\nLua, а не молчаливый `0`. `mo` — двухбуквенная единица, `m` без `o` следом —\nминута.',
                    'notes': [('warning', '`mo` и `y` — фиксированные 30 и 365 дней, не календарные месяц/год: срок,\nпересекающий короткий месяц или високосный год, съезжает на день-другие.\nДля «через месяц/год от даты» нужен [`time.add_calendar`](add_calendar.md).\nИспользование `mo`/`y` пишет предупреждение в консоль сервера с именем\nплагина.')],
                },
                {
                    'name': 'time.format',
                    'brief': 'Форматирует секунды обратно в строку длительности.',
                    'sig': 'time.format(seconds)',
                    'args': [('seconds', 'number', 'отрицательное считается как `0`')],
                    'returns': [('string', 'например `"32d24h60m60s"`; `"0s"` для нуля')],
                    'example': """
time.format(2854860) --> "32d24h60m60s"
""",
                    'extra': '`y` в вывод не попадает: год — 365-дневное приближение только на\nвходе, `"1y"` на выходе выглядело бы точнее, чем есть на самом деле.',
                },
                {
                    'name': 'time.add_calendar',
                    'brief': 'Сдвигает unixtime с учётом настоящего календаря.',
                    'sig': 'time.add_calendar(unixtime, str)',
                    'args': [('unixtime', 'number', 'точка отсчёта, UTC'),
                             ('str', 'string', 'длительность; `mo` — месяц, `m` — минута')],
                    'returns': [('number', 'unixtime + сдвиг')],
                    'example': """
-- через месяц с той же даты, а не +30 дней
local expires = time.add_calendar(time.now(), "1mo")
""",
                    'extra': """
`time.parse` считает год и месяц фиксированным числом секунд — годится для
таймаутов, но не для «забанен до того же числа следующего месяца»: в феврале
это не то же самое, что +30 дней. `add_calendar` сначала переносит `y` и `mo`
по календарю (с переносом через год и обрезкой дня, которого в целевом месяце
нет, до последнего существующего), а уже потом прибавляет остаток в секундах
(`s`/`m`/`h`/`d`/`w`) как есть.
""",
                },
            ],
        },
        {
            'title': 'Метки времени',
            'slug': 'timestamps',
            'items': [
                {
                    'name': 'time.now',
                    'brief': 'Текущее время.',
                    'sig': 'time.now()',
                    'args': [],
                    'returns': [('number', 'unixtime, UTC')],
                },
                {
                    'name': 'time.until_',
                    'brief': 'Сколько секунд осталось до момента.',
                    'sig': 'time.until_(unixtime)',
                    'args': [('unixtime', 'number', 'целевой момент')],
                    'returns': [('number', 'unixtime - time.now(); отрицательное, если момент уже прошёл')],
                    'extra': '`until` — зарезервированное слово Lua, отсюда `_` на конце.',
                },
                {
                    'name': 'time.is_expired',
                    'brief': 'Прошёл ли момент.',
                    'sig': 'time.is_expired(unixtime)',
                    'args': [('unixtime', 'number', 'проверяемый момент')],
                    'returns': [('boolean', '`true`, если `unixtime <= time.now()`')],
                    'example': """
if time.is_expired(p:get_data("mute_until")) then
	p:set_data("mute_until", nil)
end
""",
                },
            ],
        },
    ],
}
