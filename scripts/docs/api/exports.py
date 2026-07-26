# -*- coding: utf-8 -*-
NS = {
    'dir': 'exports',
    'label': 'export / import',
    'title': 'export / import',
    'brief': 'Вызов функций чужого плагина.',
    'intro': """
Плагины изолированы: глобальные переменные одного не видны другому.

| вопрос | чем |
|---|---|
| «выполни то, что умеет вон тот плагин» | эта страница |
| «сообщи всем, кому интересно» | [`hook.run`](../hook/run.md) |

```lua
-- в admin_manager
export("open", function(p) ui.root(p) end)

-- в любом другом
local admin = import("admin_manager")
local shop  = optional("shop")

admin.open(p)
if shop then shop.give(p, "vip") end
```

Прокси резолвит функцию на каждом вызове, поэтому в руках не останется указателя
на код исчезнувшего плагина. Ошибка внутри экспорта не роняет вызывающего: она
заворачивается в `[export плагин.имя] ...`.

Экспорты снимаются вместе с плагином, поэтому `lua_reload <plugin>` не
натыкается на «export уже зарегистрирован».

Кто что публикует — консольная команда [`lua_exports`](../console.md).
""",
    'groups': [
        {
            'items': [
                {
                    'name': 'export',
                    'brief': 'Публикует функцию плагина наружу.',
                    'sig': 'export(name, fn[, opts])',
                    'args': [('name', 'string', 'имя функции'),
                             ('fn', 'function', 'что вызывать'),
                             ('opts', 'table \\| nil', 'см. [Опции](#опции)')],
                    'fields': ('Опции', [
                        ('version', 'number', 'версия экспорта, по умолчанию `1`'),
                        ('override', 'boolean', 'разрешить перерегистрацию имени'),
                    ]),
                },
                {
                    'name': 'import',
                    'brief': 'Возвращает прокси к экспортам плагина; жёсткая зависимость.',
                    'sig': 'import(plugin)',
                    'args': [('plugin', 'string', 'id плагина — имя его папки')],
                    'returns': [('table', 'прокси к экспортам')],
                    'extra': 'Бросает ошибку, если плагина нет: зависимость падает при загрузке, а не через час на `nil`-вызове.',
                    'notes': [('note', 'Плагины грузятся по алфавиту, поэтому `import` на этапе загрузки\nвидит только тех, кто отсортирован раньше. Если порядок неудобен,\nбери зависимость лениво — внутри обработчика — или через\n[`optional`](optional.md).')],
                },
                {
                    'name': 'optional',
                    'brief': 'То же, что import, но `nil` вместо ошибки; мягкая зависимость.',
                    'sig': 'optional(plugin)',
                    'args': [('plugin', 'string', 'id плагина')],
                    'returns': [('table \\| nil', '`nil`, если плагина нет')],
                    'example': """
local shop = optional("shop")
if shop then
	shop.give(p, "vip")
end
""",
                },
            ],
        },
    ],
}
