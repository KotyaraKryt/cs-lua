# -*- coding: utf-8 -*-
NS = {
    'dir': 'sv',
    'label': 'sv',
    'title': 'sv',
    'brief': 'Сервер: часы, карта, консоль, переменные движка.',
    'groups': [
        {
            'title': 'Константы',
            'note': """
| поле | тип | |
|---|---|---|
| `sv.version` | string | версия модуля, `"2.0.0"` |
| `sv.api` | number | версия Lua-API |
| `sv.dir` | string | абсолютный путь `addons/lua` |
""",
            'items': [],
        },
        {
            'title': 'Сервер',
            'items': [
                {
                    'name': 'sv.time',
                    'brief': 'Серверные часы в секундах.',
                    'sig': 'sv.time()',
                    'args': [],
                    'returns': [('number', 'секунды с дробной частью')],
                    'extra': 'Те же часы, на которых идут [таймеры](../timer/index.md). Обнуляются при смене карты.',
                    'notes': [('note', '`os.clock()` — это CPU-время, оно сильно расходится на сервере,\nкоторый спит между кадрами. Для интервалов бери `sv.time()`.')],
                },
                {
                    'name': 'sv.map',
                    'brief': 'Имя текущей карты.',
                    'sig': 'sv.map()',
                    'args': [],
                    'returns': [('string', '`"de_dust2"`')],
                },
                {
                    'name': 'sv.cmd',
                    'brief': 'Ставит команду в очередь серверной консоли.',
                    'sig': 'sv.cmd(fmt, ...)',
                    'args': [('fmt', 'string', 'команда или формат для `string.format`'),
                             ('...', 'any', 'подстановки')],
                    'example': 'sv.cmd("changelevel %s", next_map)',
                    'extra': 'С одним аргументом строка уходит как есть — одинокий `%` в нике не превратится в директиву формата.',
                    'notes': [('warning', 'Длиннее 250 символов — ошибка: обрезанная команда выполнилась бы\nкак совсем другая.')],
                },
            ],
        },
        {
            'title': 'Cvar',
            'items': [
                {
                    'name': 'sv.cvar',
                    'brief': 'Возвращает объект существующей переменной движка.',
                    'sig': 'sv.cvar(name)',
                    'args': [('name', 'string', 'имя переменной')],
                    'returns': [('cvar \\| nil', '`nil`, если такой переменной нет')],
                    'example': """
local ft = sv.cvar("mp_freezetime")
if ft then print(ft:int()) end
""",
                },
                {
                    'name': 'sv.cvar_register',
                    'brief': 'Заводит свою переменную движка.',
                    'sig': 'sv.cvar_register(name, default[, flags])',
                    'args': [('name', 'string', 'имя переменной'),
                             ('default', 'string', 'значение по умолчанию, строкой'),
                             ('flags', 'table \\| nil', 'см. [Флаги](#флаги)')],
                    'returns': [('cvar', 'объект переменной')],
                    'fields': ('Флаги', [
                        ('archive', 'boolean', 'значение сохраняется в конфиг'),
                        ('server', 'boolean', 'игроки получают уведомление об изменении'),
                        ('protected', 'boolean', 'значение не отдаётся клиентам, для паролей'),
                    ]),
                    'example': """
local enabled = sv.cvar_register("greet_enabled", "1", { archive = true })
if enabled:int() ~= 0 then ... end
""",
                    'extra': 'Повторный вызов для существующего имени возвращает объект, не сбрасывая значение, поэтому `lua_reload` cvar\'ы не трогает.',
                },
                {
                    'name': 'c:int',
                    'brief': 'Значение переменной как целое.',
                    'sig': 'c:int()',
                    'args': [],
                    'returns': [('number', 'значение')],
                },
                {
                    'name': 'c:float',
                    'brief': 'Значение переменной как число.',
                    'sig': 'c:float()',
                    'args': [],
                    'returns': [('number', 'значение')],
                },
                {
                    'name': 'c:str',
                    'brief': 'Значение переменной как строка.',
                    'sig': 'c:str()',
                    'args': [],
                    'returns': [('string', 'значение')],
                },
                {
                    'name': 'c:set',
                    'brief': 'Записывает значение переменной.',
                    'sig': 'c:set(value)',
                    'args': [('value', 'string \\| number', 'новое значение')],
                    'example': 'sv.cvar("mp_friendlyfire"):set(1)',
                },
                {
                    'name': 'c:name',
                    'brief': 'Имя переменной.',
                    'sig': 'c:name()',
                    'args': [],
                    'returns': [('string', 'имя')],
                },
            ],
        },
    ],
}
