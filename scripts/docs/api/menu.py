# -*- coding: utf-8 -*-
NS = {
    'dir': 'menu',
    'label': 'menu',
    'title': 'menu',
    'brief': 'Меню на клавишах 1–9 и 0.',
    'intro': """
```lua
local m = menu.new("Выбери оружие")
m:add("AK-47", function(p) p:give("weapon_ak47") end)
m:add("AWP",   function(p) p:give("weapon_awp") end)
m:show(p)
```

На странице до 8 пунктов, дальше меню разбивается само: `9` — вперёд, `0` —
назад, на первой странице `0` — выход.

Ответ игрока перехватывается до игры, поэтому нажатая клавиша не улетает в меню
покупки. Устаревший ответ и клавиша, которой в меню нет, игнорируются.

## Цвета

Панель умеет четыре цвета. Задаются как везде — именем из палитры, `"#rrggbb"`
или `{ r, g, b }`; берётся ближайший из четырёх. Сырой код AMX Mod X (`"\\r"`)
тоже принимается.

| ключ | что красит |
|---|---|
| `title` | заголовок |
| `number` | номера пунктов |
| `text` | текст пунктов |
| `nav` | номера `Next`, `Back`, `Exit`; по умолчанию как `number` |
| `disabled` | пункт целиком при `disabled = true`, по умолчанию серый |

Код цвета действует до следующего и переносится через строки, поэтому рендер
ставит коды только там, где цвет меняется: меню без `color` уходит на клиент
байт в байт таким же, как раньше.
""",
    'groups': [
        {
            'items': [
                {
                    'name': 'menu.new',
                    'brief': 'Создаёт меню.',
                    'sig': 'menu.new([title[, opts]])',
                    'args': [('title', 'string \\| nil', 'заголовок панели'),
                             ('opts', 'table \\| nil', 'см. [Опции](#опции)')],
                    'returns': [('menu', 'объект меню')],
                    'fields': ('Опции', [
                        ('exit', 'boolean', 'клавиша `0` закрывает меню; по умолчанию `true`'),
                        ('time', 'number', 'секунд на экране; `-1` — до ответа. По умолчанию `-1`'),
                        ('on_exit', 'function', 'вызывается при закрытии, получает игрока'),
                        ('color', 'string \\| table', 'см. [Цвета](index.md#цвета)'),
                    ]),
                    'example': """
local m = menu.new("Оружие", { color = { title = "yellow", number = "red" } })
""",
                },
                {
                    'name': 'm:add',
                    'brief': 'Добавляет пункт.',
                    'sig': 'm:add(text[, fn][, opts])',
                    'args': [('text', 'string', 'текст пункта'),
                             ('fn', 'function \\| nil', 'получает `(player, item)`'),
                             ('opts', 'table \\| nil', 'см. [Опции](#опции)')],
                    'returns': [('table', 'пункт — на него можно вешать свои поля')],
                    'fields': ('Опции', [
                        ('disabled', 'boolean', 'видно, нажать нельзя'),
                        ('value', 'any', 'своё значение на пункте'),
                        ('color', 'string \\| table', 'цвет пункта целиком или `{ number = , text = }`'),
                    ]),
                    'example': """
m:add("AWP", function(p, item)
	p:give("weapon_awp")
end, { color = "red", value = 4750 })

m:add("Пока недоступно", nil, { disabled = true })
""",
                },
                {
                    'name': 'm:show',
                    'brief': 'Показывает меню игроку.',
                    'sig': 'm:show(p[, page])',
                    'args': [('p', 'player', 'кому показать'),
                             ('page', 'number \\| nil', 'страница с нуля; по умолчанию первая')],
                },
                {
                    'name': 'm:close',
                    'brief': 'Убирает меню с экрана игрока.',
                    'sig': 'm:close(p)',
                    'args': [('p', 'player', 'у кого убрать')],
                },
                {
                    'name': 'm:count',
                    'brief': 'Количество пунктов.',
                    'sig': 'm:count()',
                    'args': [],
                    'returns': [('number', 'сколько пунктов добавлено')],
                },
                {
                    'name': 'm:color',
                    'brief': 'Перекрашивает меню целиком.',
                    'sig': 'm:color(spec)',
                    'args': [('spec', 'string \\| table', 'тот же формат, что `opts.color`')],
                    'returns': [('menu', 'сам объект, для цепочки')],
                    'extra': 'Работает до `m:show()`.',
                },
                {
                    'name': 'm:item_color',
                    'brief': 'Перекрашивает один пункт.',
                    'sig': 'm:item_color(item, spec)',
                    'args': [('item', 'table', 'то, что вернул `m:add`'),
                             ('spec', 'string \\| table', 'цвет')],
                    'returns': [('table', 'тот же пункт')],
                    'example': 'm:item_color(m.items[1], "grey")',
                },
            ],
        },
    ],
}
