# -*- coding: utf-8 -*-
NS = {
    'dir': 'timer',
    'label': 'timer',
    'title': 'timer',
    'brief': 'Отложенные и повторяющиеся вызовы.',
    'intro': """
Идут на серверном времени и тикают из `StartFrame`. Снимаются вместе с плагином:
при `lua_reload`, при одиночной перезагрузке и при отключении плагина за
превышение прекеша.
""",
    'groups': [
        {
            'title': 'Анонимные',
            'slug': 'anonymous',
            'note': 'Адресуются по id, который вернул вызов.',
            'items': [
                {
                    'name': 'timer.after',
                    'brief': 'Вызывает функцию один раз через заданное время.',
                    'sig': 'timer.after(seconds, fn[, opts])',
                    'args': [
                        ('seconds', 'number', 'задержка; `0` и меньше — на ближайшем кадре'),
                        ('fn', 'function', 'вызывается один раз'),
                        ('opts', 'table \\| nil', 'см. [Опции](#опции)'),
                    ],
                    'returns': [('number', 'id для [`timer.cancel`](cancel.md)')],
                    'fields': ('Опции', [
                        ('persist', 'boolean', 'переживать смену карты; по умолчанию `true`'),
                    ]),
                    'example': """
timer.after(0.5, function()
	if p:connected() then
		p:chat("прошло полсекунды")
	end
end)
""",
                    'notes': [
                        ('warning', 'Объект игрока, захваченный в замыкание, может пережить\nсамого игрока. В теле коллбэка проверяй `p:connected()`.'),
                    ],
                    'see': [('timer.cancel', 'cancel.md'), ('timer.create', 'create.md')],
                },
                {
                    'name': 'timer.every',
                    'brief': 'Вызывает функцию каждые N секунд, пока её не снимут.',
                    'sig': 'timer.every(seconds, fn[, opts])',
                    'args': [
                        ('seconds', 'number', 'интервал, должен быть больше нуля'),
                        ('fn', 'function', 'вызывается каждые `seconds`'),
                        ('opts', 'table \\| nil', 'см. [Опции](#опции)'),
                    ],
                    'returns': [('number', 'id для [`timer.cancel`](cancel.md)')],
                    'fields': ('Опции', [
                        ('persist', 'boolean', 'переживать смену карты; по умолчанию `true`'),
                    ]),
                    'example': """
local tick = timer.every(1, function()
	left = left - 1
	if left <= 0 then timer.cancel(tick) end
end)
""",
                    'notes': [
                        ('note', 'Повторяющийся таймер, заведённый в теле плагина, лучше делать\nименованным: [`timer.create`](create.md) не задваивается при\nперезагрузке плагина.'),
                    ],
                    'see': [('timer.create', 'create.md')],
                },
                {
                    'name': 'timer.cancel',
                    'brief': 'Снимает таймер по id.',
                    'sig': 'timer.cancel(id)',
                    'args': [('id', 'number', 'то, что вернул `after` или `every`')],
                    'returns': [('boolean', '`true`, если таймер существовал и был снят')],
                },
            ],
        },
        {
            'title': 'Именованные',
            'slug': 'named',
            'note': 'Адресуются по имени, уникальному внутри плагина.',
            'items': [
                {
                    'name': 'timer.create',
                    'brief': 'Заводит именованный таймер; повторный вызов заменяет старый.',
                    'sig': 'timer.create(name, seconds, fn[, opts])',
                    'args': [
                        ('name', 'string', 'уникален внутри этого плагина'),
                        ('seconds', 'number', 'интервал, должен быть больше нуля'),
                        ('fn', 'function', 'обработчик'),
                        ('opts', 'table \\| nil', 'см. [Опции](#опции)'),
                    ],
                    'fields': ('Опции', [
                        ('once', 'boolean', 'сработать один раз вместо повтора'),
                        ('persist', 'boolean', 'переживать смену карты; по умолчанию `true`'),
                    ]),
                    'example': """
timer.create("ads.chat", 60, show_message)
""",
                    'extra': """
Замена вместо добавления — это то, что нужно таймеру, который заводится в теле
плагина: `lua_reload <plugin>` выполнит эту строку второй раз и не оставит
второй таймер.
""",
                    'see': [('timer.destroy', 'destroy.md')],
                },
                {
                    'name': 'timer.destroy',
                    'brief': 'Снимает именованный таймер этого плагина.',
                    'sig': 'timer.destroy(name)',
                    'args': [('name', 'string', 'имя, под которым он создан')],
                    'returns': [('boolean', '`true`, если такой таймер был')],
                },
            ],
        },
        {
            'title': 'Смена карты',
            'note': """
Смена карты обнуляет серверные часы. Модуль переносит отложенные сроки на новые,
поэтому `timer.after(600, ...)` остаётся «через десять минут», а не срабатывает
пачкой на первом кадре следующей карты.

`persist = false` отменяет перенос: такой таймер снимается вместе с картой, на
которой был заведён.

```lua
-- инфраструктура плагина: должна пережить changelevel
timer.create("stats.flush", 60, flush)

-- привязано к текущей карте: на следующей уже бессмысленно
timer.after(30, warmup_over, { persist = false })
```

По умолчанию `persist = true`. Обратный вариант ломал бы плагины молча: таймер
сохранения статистики, заведённый один раз при загрузке, после первой же смены
карты перестал бы работать до конца жизни сервера.
""",
            'items': [],
        },
    ],
}
