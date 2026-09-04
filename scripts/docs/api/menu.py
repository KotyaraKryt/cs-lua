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
назад, на первой странице `0` — выход. Порог задаётся `per_page` — поставь `5`,
и меню из 6 пунктов уже листается.

Пункты можно передать сразу в `menu.new`, а не добавлять по одному:

```lua
local m = menu.new("Оружие", { items = {
	{ "AK-47", give_ak },
	{ "AWP", give_awp, color = "red" },
	"---",                              -- строка = разделитель
} })
```

## Вложенные меню

Вторым аргументом `m:add` (или через [`m:submenu`](#submenu)) можно дать не
функцию, а другое меню — пункт открывает его, а клавиша `0` внутри возвращает
назад, а не закрывает. Цепочка любой глубины.

```lua
local guns = menu.new("Оружие")
guns:add("AK-47", give_ak)
guns:add("AWP",   give_awp)

local root = menu.new("Магазин")
root:submenu("Оружие", guns)
root:add("Броня", give_armor)
root:show(p)
```

Ответ игрока перехватывается до игры, поэтому нажатая клавиша не улетает в меню
покупки. Устаревший ответ и клавиша, которой в меню нет, игнорируются.

## Всем сразу

`m:show` и `m:close` (и `menu.custom`, `menu.confirm`) принимают
[`players.broadcast`](../players/broadcast.md) вместо игрока.

```lua
m:show(players.broadcast)   -- откроется у всех подключённых
m:close(players.broadcast)  -- закроется у тех, у кого сейчас открыто именно это меню
```

Показ идёт отдельным вызовом на каждого — `text` и `disabled` пункта, заданные
как `function(player)`, у разных игроков честно могут отличаться (счётчик
патронов, серый пункт при нехватке денег). Закрытие через `players.broadcast`
трогает только тех, у кого прямо сейчас открыто это самое меню — чужое меню
(в том числе другого плагина) не гасит.

Закрыть что угодно, не зная, чьё это меню — [`menu.close_all`](#close_all):
без аргумента или с `players.broadcast` гасит экран у всех, с конкретным
игроком — только у него.

## Проверка, что открыто

[`menu.is_open`](#is_open) — что-то вообще открыто у игрока прямо сейчас (в
том числе `menu.raw_show`). [`m:is_open`](#m_is_open) — открыто именно это
меню, а не любое другое.

```lua
if not shop_menu:is_open(p) then
	shop_menu:show(p)
end
```

`text` пункта и `disabled` можно задать функцией `function(player)` — она
считается при каждом показе, и один объект меню обслуживает всех игроков и сам
держится в актуальном состоянии (живой счётчик в подписи, пункт сереет, когда
не хватает денег), пересобирать ничего не надо.

## Своё меню

Когда пунктов нет — крестики-нолики, доска, произвольная панель — рисуется
руками через [`menu.custom`](#custom): `render` возвращает текст и список живых
клавиш, `on_key` получает нажатую. Простое «да / нет» — [`menu.confirm`](#confirm).
Для совсем ручного случая есть `menu.raw_show(id, keys, time, text)` в паре с
`hook.add("menu:select", ...)` — форма один в один как `show_menu()` /
`register_menucmd()` в AMX Mod X.

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

## Стиль

`opts.layout` меняет разметку строк, которые меню рисует само. Любое поле можно
опустить — строка выглядит как раньше.

| поле | что задаёт |
|---|---|
| `prefix` | префикс перед пунктом: строка с `%d` или `function(slot)`. По умолчанию `"%d. "`. Идёт и на клавиши `9` / `0` |
| `title` | заголовок: строка с `%s` или `function(text)` |
| `counter` | метка страницы после заголовка, когда страниц больше одной: строка с двумя `%d` (страница, всего) или `true` для `" [%d/%d]"` |
| `exit_label` / `back_label` / `next_label` | подписи навигации; по умолчанию `Exit` / `Back` / `Next` |

```lua
local m = menu.new("Меню", { layout = {
	prefix     = "[%d] ",
	exit_label = "Выйти",
	counter    = true,
} })
-- Меню [1/2]
--
-- [1] Первый
-- [0] Выйти
```

У отдельного пункта свой префикс — `m:add(text, fn, { prefix = ... })`, строкой
или `function(slot)`. Например, заблокированный пункт:

```lua
m:add("Доступен")
m:add("Недоступен", nil, { disabled = true, prefix = "[#] " })
-- [1] Доступен
-- [#] Недоступен
```

Старые имена `number` (вместо `prefix`), `exit` / `back` / `next` (вместо
`*_label`) всё ещё принимаются.
""",
    'groups': [
        {
            'items': [
                {
                    'name': 'menu.new',
                    'brief': 'Создаёт меню.',
                    'sig': 'menu.new([title[, opts]])',
                    'args': [('title', 'string \\| function \\| nil', 'заголовок панели; функция получает игрока'),
                             ('opts', 'table \\| nil', 'см. [Опции](#опции)')],
                    'returns': [('menu', 'объект меню')],
                    'fields': ('Опции', [
                        ('items', 'table', 'пункты списком: `{ text, fn, ...opts }` или строка-разделитель'),
                        ('closable', 'boolean', 'клавиша `0` закрывает меню; по умолчанию `true`. Раньше `exit`'),
                        ('per_page', 'number', 'пунктов на странице до разбивки; по умолчанию `8`, максимум `8`'),
                        ('timeout', 'number', 'секунд на экране; `-1` — до ответа. По умолчанию `-1`. Раньше `time`'),
                        ('on_close', 'function', 'вызывается при закрытии, получает игрока. Раньше `on_exit` (и `m.on_exit = fn` тоже работает)'),
                        ('on_select', 'function', '`(player, item)` на любой выбор, до личного обработчика пункта'),
                        ('color', 'string \\| table', 'см. [Цвета](index.md#цвета)'),
                        ('layout', 'table', 'подписи и префиксы строк, см. [Стиль](index.md#стиль)'),
                    ]),
                    'example': """
local m = menu.new("Оружие", {
	color  = { title = "yellow", number = "red" },
	layout = { prefix = "[%d] ", exit_label = "Выход" },
})
""",
                },
                {
                    'name': 'm:add',
                    'brief': 'Добавляет пункт.',
                    'sig': 'm:add(text[, fn][, opts])',
                    'args': [('text', 'string \\| function', 'текст пункта; функция получает игрока'),
                             ('fn', 'function \\| menu \\| nil', '`(player, item)`, либо меню — тогда пункт открывает его'),
                             ('opts', 'table \\| nil', 'см. [Опции](#опции)')],
                    'returns': [('table', 'пункт — на него можно вешать свои поля')],
                    'fields': ('Опции', [
                        ('disabled', 'boolean \\| function', 'видно, нажать нельзя; `function(player)` считается при показе'),
                        ('prefix', 'string \\| function', 'свой префикс вместо `1. `; строка или `function(slot)`. Можно и `item.prefix = ...` позже. Раньше `number`'),
                        ('value', 'any', 'своё значение на пункте'),
                        ('color', 'string \\| table', 'цвет пункта целиком или `{ number = , text = }`'),
                    ]),
                    'example': """
m:add("AWP", function(p, item)
	p:give("weapon_awp")
end, { color = "red", value = 4750 })

m:add(function(p) return "Патроны: " .. p:ammo("awp") end, nil, {
	disabled = function(p) return not p:alive() end,
})
""",
                },
                {
                    'name': 'm:separator',
                    'brief': 'Строка без клавиши — заголовок группы или отступ.',
                    'sig': 'm:separator([text])',
                    'args': [('text', 'string \\| function \\| nil', 'текст строки; по умолчанию пусто')],
                    'returns': [('table', 'та же запись')],
                    'example': """
m:separator("\\yОружие")
m:add("AK-47", give_ak)
m:separator()
m:add("Выход", nil, { disabled = true })
""",
                },
                {
                    'name': 'm:submenu',
                    'brief': 'Пункт, открывающий вложенное меню.',
                    'sig': 'm:submenu(text, child[, opts])',
                    'args': [('text', 'string \\| function', 'текст пункта'),
                             ('child', 'menu', 'меню, которое откроется; `0` в нём — назад'),
                             ('opts', 'table \\| nil', 'как у `m:add`')],
                    'returns': [('table', 'пункт')],
                    'extra': 'То же, что `m:add(text, child, opts)` — просто читается яснее.',
                },
                {
                    'name': 'm:show',
                    'brief': 'Показывает меню игроку.',
                    'sig': 'm:show(p[, page])',
                    'args': [('p', 'player \\| players.broadcast', 'кому показать; broadcast — всем подключённым'),
                             ('page', 'number \\| nil', 'страница с нуля; по умолчанию первая')],
                    'extra': 'С `players.broadcast` рендерится и отправляется отдельно каждому — см.\n[Всем сразу](index.md#всем-сразу).',
                },
                {
                    'name': 'm:close',
                    'brief': 'Убирает меню с экрана игрока.',
                    'sig': 'm:close(p)',
                    'args': [('p', 'player \\| players.broadcast', 'у кого убрать; broadcast — у всех, у кого открыто именно это меню')],
                },
                {
                    'name': 'm:is_open',
                    'slug': 'm_is_open',
                    'brief': 'Открыто ли именно это меню у игрока прямо сейчас.',
                    'sig': 'm:is_open(p)',
                    'args': [('p', 'player', 'конкретный игрок, не `players.broadcast`')],
                    'returns': [('boolean', '`true`, если у игрока на экране сейчас это меню (не чужое и не другая\nстраница другого меню)')],
                },
                {
                    'name': 'm:count',
                    'brief': 'Количество пунктов.',
                    'sig': 'm:count()',
                    'args': [],
                    'returns': [('number', 'сколько записей добавлено')],
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
                {
                    'name': 'menu.custom',
                    'brief': 'Меню, которое рисуешь сам.',
                    'sig': 'menu.custom(opts)',
                    'args': [('opts', 'table', 'см. [Опции](#custom-опции)')],
                    'returns': [('menu', 'объект меню, у него `:show(p)` и `:close(p)`')],
                    'fields': ('Опции', [
                        ('render', 'function', '`render(p)` -> `text, keys`; обязательна'),
                        ('on_key', 'function \\| nil', '`on_key(p, key)` — нажатая клавиша, `0`–`9`'),
                        ('time', 'number', 'секунд на экране; `-1` — до ответа. По умолчанию `-1`'),
                    ]),
                    'extra': (
                        '`render` возвращает готовый текст панели (переводы строк `\\n`, '
                        'цветовые коды AMX по желанию) и список живых клавиш — '
                        '`{ 1, 2, 3, 0 }` или битовую маску; `nil` — все `1`–`9` и `0`. '
                        'Число в списке — это клавиша на клавиатуре. Панель одноразовая: '
                        'после нажатия она закрыта, в `on_key` вызови `:show(p)` снова, '
                        'чтобы держать её на экране.'
                    ),
                    'example': """
local board = menu.custom({
	render = function(p)
		return draw(game), free_cells(game)
	end,
	on_key = function(p, key)
		if key == 0 then return give_up(p, game) end
		play(game, key)
		board:show(p)
	end,
})
board:show(p)
""",
                },
                {
                    'name': 'menu.confirm',
                    'brief': 'Окно «да / нет».',
                    'sig': 'menu.confirm(p, text, on_yes[, on_no][, opts])',
                    'args': [('p', 'player', 'кому показать'),
                             ('text', 'string', 'вопрос'),
                             ('on_yes', 'function', '`function(player)` на «да»'),
                             ('on_no', 'function \\| nil', '`function(player)` на «нет»'),
                             ('opts', 'table \\| nil', '`yes` / `no` — подписи, `timeout` — секунд на экране')],
                    'returns': [('menu', 'объект панели')],
                    'example': """
menu.confirm(p, "Выдать VIP игроку " .. t:name() .. "?", function()
	grant_vip(t)
end, { yes = "Выдать", no = "Отмена" })
""",
                },
                {
                    'name': 'menu.raw_show',
                    'brief': 'Сырая панель: текст и маска клавиш как в AMX Mod X.',
                    'sig': 'menu.raw_show(id, keys, time, text)',
                    'args': [('id', 'number', 'индекс игрока'),
                             ('keys', 'number', 'битовая маска: бит 0 — клавиша `1`'),
                             ('time', 'number', 'секунд на экране; `-1` — до ответа'),
                             ('text', 'string', 'текст панели')],
                    'extra': 'Ответ — через `hook.add("menu:select", name, fn)`, у события `e.player` и `e.key`.',
                    'example': """
menu.raw_show(p.id, 2^0 + 2^1, -1, "Выбор:\\n1. Да\\n2. Нет")
hook.add("menu:select", "my_plugin", function(e)
	if e.key == 1 then ... end
end)
""",
                },
                {
                    'name': 'menu.raw_close',
                    'brief': 'Убирает сырую панель.',
                    'sig': 'menu.raw_close(id)',
                    'args': [('id', 'number', 'индекс игрока')],
                },
                {
                    'name': 'menu.is_open',
                    'brief': 'Открыто ли у игрока хоть какое-то меню.',
                    'sig': 'menu.is_open(p)',
                    'args': [('p', 'player', 'конкретный игрок, не `players.broadcast`')],
                    'returns': [('boolean', '`true`, если на экране сейчас что-либо: `menu.new`, `menu.custom`,\n`menu.confirm` или сырая панель')],
                    'extra': 'Не спрашивает, какое именно меню — для этого [`m:is_open`](#m_is_open).',
                },
                {
                    'name': 'menu.close_all',
                    'brief': 'Убирает с экрана что угодно, не зная, чьё это меню.',
                    'sig': 'menu.close_all([p])',
                    'args': [('p', 'player \\| players.broadcast \\| nil', 'у кого убрать; без аргумента или\n`players.broadcast` — у всех подключённых')],
                    'example': """
menu.close_all() -- у всех на сервере гаснет любая открытая панель
""",
                },
            ],
        },
    ],
}
