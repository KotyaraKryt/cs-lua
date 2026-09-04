# -*- coding: utf-8 -*-
NS = {
    'dir': 'plugin',
    'label': 'plugin',
    'title': 'plugin',
    'brief': 'Манифест, пути плагина и его выгрузка.',
    'intro': '`plugin` — одновременно вызов манифеста и пространство имён.',
    'groups': [
        {
            'items': [
                {
                    'name': 'plugin{}',
                    'slug': 'manifest',
                    'brief': 'Объявляет метаданные и требования плагина.',
                    'sig': 'plugin { name = , version = , author = , api_version = , min_engine_version = , max_engine_version = , requires = }',
                    'args': None,
                    'fields': ('Поля', [
                        ('name', 'string', 'имя в `lua_list`'),
                        ('version', 'string', 'версия плагина'),
                        ('author', 'string', 'автор'),
                        ('api_version', 'number', 'версия Lua-API, под которую написан плагин'),
                        ('min_engine_version', 'string', 'минимальная сборка cs-lua, `"X.Y.Z"`'),
                        ('max_engine_version', 'string', 'максимальная проверенная сборка cs-lua, `"X.Y.Z"`'),
                        ('requires', 'table', 'модули, которые должны резолвиться через `require`'),
                    ]),
                    'example': """
plugin {
	name               = "Shop",
	version            = "1.0",
	author             = "kotyarakryt",
	api_version        = 1,
	min_engine_version = "1.1.0",
	requires           = { "class" },
}
""",
                    'extra': """
Вызывается один раз, в `manifest.lua` — это единственная обязанность этого
файла. Движок грузит `manifest.lua` раньше `init.lua` и требует, чтобы `plugin{}`
там прозвучал: если нет, плагин не запускается и `init.lua` не выполняется вовсе.
См. [структуру плагина](../../plugins.md).

`requires` проверяется при загрузке: отсутствующий модуль останавливает плагин на
первой строке `manifest.lua` с указанием, чего не хватает.

`api_version` меняется только на ломающем изменении API — новый натив
(например `http_server`) может появиться в сборке, которая всё ещё говорит
`api_version = 1`. Для этого `min_engine_version`: точнее, чем `api_version`, и не
требует ждать следующего v2. `max_engine_version` — обратный случай, «плагин
проверен только по эту сборку», отказ загрузки на более новой. Сравниваются
как `X.Y.Z`, отсутствующая часть читается как `0`. Текущая сборка — `sv.version`.
""",
                    'notes': [
                        ('warning', '`api_version` выше текущего — отказ загрузки: сборка ещё не\nзнает этот API.'),
                        ('note', 'Лёгкая опечатка: `plugin = { ... }` вместо `plugin { ... }`\nприсваивает таблицу и затирает пространство имён — движок решит, что\nманифест ничего не объявил, и остановит загрузку с ошибкой в консоли.'),
                    ],
                },
                {
                    'name': 'plugin.id',
                    'brief': 'Имя папки плагина.',
                    'sig': 'plugin.id()',
                    'args': [],
                    'returns': [('string \\| nil', '`nil` в core-слое')],
                    'extra': 'Это и есть идентичность плагина — на неё ключуется реестр [`export`](../exports/index.md).',
                },
                {
                    'name': 'plugin.dir',
                    'brief': 'Абсолютный путь к папке плагина.',
                    'sig': 'plugin.dir()',
                    'args': [],
                    'returns': [('string \\| nil', '`nil` в core-слое')],
                    'notes': [('warning', 'Здесь лежит код, и установщик перезаписывает его при обновлении.\nДанные клади в [`plugin.data_dir()`](data_dir.md).')],
                },
                {
                    'name': 'plugin.data_dir',
                    'brief': 'Каталог плагина под данные, переживающий обновление.',
                    'sig': 'plugin.data_dir()',
                    'args': [],
                    'returns': [('string', '`addons/lua/data/<plugin_id>/`')],
                    'extra': 'Создаётся при первом вызове. Туда же кладут файлы [`datafile`](../datafile/index.md) и [`db.open`](../db/open.md).',
                },
                {
                    'name': 'plugin.on_unload',
                    'brief': 'Регистрирует обработчик выгрузки этого плагина.',
                    'sig': 'plugin.on_unload(fn)',
                    'args': [('fn', 'function', 'что выполнить перед выгрузкой')],
                    'example': """
plugin.on_unload(function()
	for _, p in ipairs(players.list()) do
		p:gravity(1.0)
	end
end)
""",
                    'extra': """
Срабатывает и при `lua_reload`, и при `lua_reload <plugin>`. Обработчики
выполняются в обратном порядке регистрации — плагин разбирает себя так же, как
собирал.

Выполняются **до** того, как снимаются хендлеры, таймеры и базы плагина, поэтому
внутри всё ещё работает.
""",
                    'see': [('plugin_unload', '../hook/plugin_unload.md')],
                },
            ],
        },
    ],
}
