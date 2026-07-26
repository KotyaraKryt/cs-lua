# -*- coding: utf-8 -*-
NS = {
    'dir': 'cmd',
    'label': 'cmd',
    'title': 'cmd',
    'brief': 'Команды чата, серверной консоли и rcon.',
    'intro': 'Одна регистрация покрывает чат (`!name`, `/name`), консоль и rcon.',
    'groups': [
        {
            'items': [
                {
                    'name': 'cmd.add',
                    'brief': 'Регистрирует команду.',
                    'sig': 'cmd.add(name, fn[, opts])',
                    'args': [
                        ('name', 'string', 'имя без префикса'),
                        ('fn', 'function', 'получает [`ctx`](#ctx)'),
                        ('opts', 'table \\| nil', 'см. [Опции](#опции)'),
                    ],
                    'fields': ('Опции', [
                        ('source', 'string \\| table', '`console`, `chat`, `chat_team`; список или ничего — везде'),
                        ('perm', 'string', 'нода прав, проверяется до вызова обработчика'),
                        ('target', 'number', 'номер аргумента с именем игрока → `ctx.target`'),
                        ('immunity', 'boolean', '`false` отключает проверку иммунитета при `target`'),
                    ]),
                    'example': """
cmd.add("heal", function(ctx)
	local p = ctx.player
	if p then p:health(p:health() + 25) end
	ctx.reply("healed")
end)

cmd.add("kick",  fn, { source = "console" })
cmd.add("plant", fn, { source = "chat_team" })
cmd.add("slay",  fn, { perm = "admin.slay", target = 1 })
""",
                    'extra': """
## ctx

| поле | тип | |
|---|---|---|
| `ctx.player` | player \\| nil | `nil` для консоли и rcon |
| `ctx.args` | table | слова после имени команды |
| `ctx.name` | string | имя команды |
| `ctx.source` | string | `console`, `chat`, `chat_team` |
| `ctx.reply(text)` | function | ответ туда, откуда пришла команда |
| `ctx.target` | player | игрок из аргумента при `opts.target` |
| `ctx.can(node)` | function | проверить право вызвавшего |

Распознанная чат-команда в общий чат не попадает.

С `target` роутер сам находит игрока по слоту, `#userid` или части ника и
отказывает, если у цели иммунитет не ниже.
""",
                    'notes': [
                        ('note', 'У консоли и rcon прав нет — им разрешено всё. `ctx.player` там\n`nil`, и `perm` с `immunity` не проверяются.'),
                        ('warning', 'Имя команды регистрируется в движке навсегда. `lua_reload`\nменяет только Lua-обработчик: если плагин не объявил команду\nзаново, она становится пустышкой.'),
                    ],
                    'see': [('players.find', '../players/find.md'), ('access', '../access/index.md')],
                },
                {
                    'name': 'cmd.remove',
                    'brief': 'Снимает команду.',
                    'sig': 'cmd.remove(name)',
                    'args': [('name', 'string', 'имя команды')],
                    'returns': [('boolean', '`true`, если команда была')],
                },
                {
                    'name': 'cmd.list',
                    'brief': 'Возвращает имена зарегистрированных команд.',
                    'sig': 'cmd.list()',
                    'args': [],
                    'returns': [('table', 'отсортированный массив строк')],
                },
            ],
        },
    ],
}
