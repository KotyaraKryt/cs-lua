# -*- coding: utf-8 -*-
NS = {
    'dir': 'access',
    'label': 'access',
    'title': 'access',
    'brief': 'Права как именованные ноды, группы и иммунитет.',
    'intro': """
Право — именованная нода: `admin.kick`, `shop.vip.buy`.

```lua
if p:can("shop.vip.buy") then ... end

cmd.add("slay", function(ctx)
	ctx.target:slay()
end, { perm = "admin.slay", target = 1 })
```

> [!NOTE]
> У консоли и rcon прав нет — им разрешено всё. `ctx.player` там `nil`, любая
> проверка проходит.

Методы игрока — [`p:can`](../players/can.md), [`p:groups`](../players/groups.md),
[`p:group`](../players/group.md), [`p:weight`](../players/weight.md),
[`p:outranks`](../players/outranks.md).

## Файлы

`data/groups.lua` правится руками, модуль его только читает.

```lua
return {
	default   = { weight = 0,   allow = { "chat.say" } },
	vip       = { weight = 10,  inherit = "default", allow = { "shop.vip.*" } },
	moderator = { weight = 30,  inherit = "vip", allow = { "admin.kick" } },
	admin     = { weight = 50,  inherit = "moderator", allow = { "admin.*" },
	              deny = { "admin.rcon" } },
	owner     = { weight = 100, inherit = "admin", allow = { "*" } },
}
```

`data/users.lua` — кто есть кто. Ключ записи: steamid или `name:<ник>`.

```lua
return {
	["STEAM_0:1:12345"] = { name = "kotyarakryt", groups = { "owner" } },
	["STEAM_0:0:99999"] = { groups = { "vip" }, until_ = 1785000000 },
	["name:Helper"]     = { password = "secret", groups = { "moderator" },
	                        where = { map = "de_dust2" } },
}
```

| поле | |
|---|---|
| `until_` | unix-время; при выдаче из кода — `"30d"`, `"12h"` или `"map"` |
| `where` | `{ map = "de_dust2" }` или `{ maps = {...} }`; есть и у групп |
| `password` | сверяется с `setinfo _pw` на клиенте |

Оба файла необязательны: без `groups.lua` работают встроенные `default`, `admin`
и `owner`. Битый файл даёт ошибку в консоль и пустую половину прав, а не
половинчатые права.

> [!WARNING]
> Вход по нику годится только для LAN: ник подделывается тривиально, а пароль
> передаётся открытым текстом. На публичном сервере выдавай права по steamid.

## Разрешение конфликтов

Нода в выдаче может быть с хвостовой звёздочкой (`shop.vip.*`, `*`) и с минусом
впереди — `-admin.ban` означает явный запрет.

Порядок в файле роли не играет. Побеждает запись, которая выигрывает по
признакам, в этом порядке:

1. **источник** — личная запись > своя группа > унаследованная > `default` ноды;
2. **вес** — среди групп одного уровня выигрывает более тяжёлая;
3. **точность** — `shop.vip.buy` > `shop.vip.*` > `shop.*` > `*`;
4. **запрет** — при полном равенстве выигрывает `deny`.

Поэтому `admin` с `allow = {"admin.*"}` и `deny = {"admin.rcon"}` получает всё
кроме rcon, а личный `allow = {"admin.rcon"}` в записи игрока это перебивает.

## Иммунитет

Иммунитет — это `weight` группы. Действие против другого игрока требует **строго
большего** веса, поэтому два админа одного ранга друг друга не тронут.

Вручную — [`p:outranks`](../players/outranks.md). Декларативно — `opts.target` в
[`cmd.add`](../cmd/add.md).

Права считаются лениво и кешируются на слот. Кеш сбрасывается сам: на
`player_authorized`, при смене карты, по истечении срока и при любом изменении
прав.
""",
    'groups': [
        {
            'title': 'Объявление',
            'items': [
                {
                    'name': 'access.declare',
                    'brief': 'Объявляет ноду прав.',
                    'sig': 'access.declare(node[, opts])',
                    'args': [('node', 'string', 'конкретная нода, без звёздочек'),
                             ('opts', 'table \\| nil', 'см. [Опции](#опции)')],
                    'fields': ('Опции', [
                        ('desc', 'string', 'описание для `lua_perms list`'),
                        ('default', 'string \\| boolean', 'группа, которой нода выдана по умолчанию; `true` — всем'),
                        ('plugin', 'string', 'владелец в `lua_perms list`; по умолчанию `plugin.id()`'),
                    ]),
                    'example': """
access.declare("shop.vip.buy", { desc = "Покупка в VIP-магазине", default = "vip" })
""",
                    'extra': """
Объявление необязательно, но даёт две вещи: нода попадает в `lua_perms list` с
именем плагина-владельца, а опечатка в `p:can("shop.vip.by")` выводит
предупреждение в консоль вместо тихого отказа.

`default` действует с самым низким приоритетом: любой `deny` в файлах забирает
ноду обратно.

`plugin` указывать почти никогда не нужно: раз `access.declare` вызывается из
`manifest.lua`, движок и так знает, чей это плагин, через `plugin.id()`. Опция
существует для нод, которые объявляет `core/`, где своего плагина нет.
""",
                },
                {
                    'name': 'access.rule',
                    'brief': 'Задаёт динамическое правило для ноды.',
                    'sig': 'access.rule(node, fn)',
                    'args': [('node', 'string', 'нода'),
                             ('fn', 'function', 'получает `(p, ctx)`, возвращает boolean')],
                    'example': """
access.rule("shop.buy", function(p, ctx)
	return p:playtime() > 3600
end)
""",
                    'extra': 'Для случаев, когда правом управляет логика, а не таблица.',
                    'notes': [('note', 'Правило вызывается **только если ноду не покрыл никто**. Явный\n`deny` в файлах всегда сильнее.')],
                },
            ],
        },
        {
            'title': 'Выдача',
            'items': [
                {
                    'name': 'access.grant',
                    'brief': 'Выдаёт права по ключу.',
                    'sig': 'access.grant(key, spec)',
                    'args': [('key', 'string', 'steamid или `name:<ник>`'),
                             ('spec', 'table', 'см. [Поля](#поля)')],
                    'returns': [('boolean', '`true` при успехе'),
                                ('string', 'причина при ошибке')],
                    'fields': ('Поля', [
                        ('groups', 'string \\| table', 'группа или список групп'),
                        ('allow', 'string \\| table', 'ноды, которые выдать'),
                        ('deny', 'string \\| table', 'ноды, которые запретить'),
                        ('until_', 'string \\| number', '`"30d"`, `"12h"`, `"map"` или unix-время'),
                        ('where', 'table', '`{ map = }` или `{ maps = {} }`'),
                    ]),
                    'notes': [('note', 'Действует сразу; в файл попадает только после\n[`access.save`](save.md).')],
                },
                {
                    'name': 'access.revoke',
                    'brief': 'Забирает запись, группу или ноду.',
                    'sig': 'access.revoke(key[, group][, node])',
                    'args': [('key', 'string', 'steamid или `name:<ник>`'),
                             ('group', 'string \\| nil', 'какую группу забрать'),
                             ('node', 'string \\| nil', 'какую ноду забрать')],
                    'returns': [('boolean', '`true` при успехе'),
                                ('string', 'причина при ошибке')],
                    'extra': 'Без `group` и `node` удаляет запись целиком.',
                },
                {
                    'name': 'access.save',
                    'brief': 'Записывает `users.lua` на диск.',
                    'sig': 'access.save()',
                    'args': [],
                    'returns': [('boolean', '`true` при успехе'),
                                ('string', 'причина при ошибке')],
                    'extra': 'Атомарно, с `.bak` — как [`datafile.save`](../store/save.md).',
                },
                {
                    'name': 'access.reload',
                    'brief': 'Перечитывает `groups.lua` и `users.lua` с диска.',
                    'sig': 'access.reload()',
                    'args': [],
                    'extra': 'Без полного `lua_reload`.',
                },
                {
                    'name': 'access.invalidate',
                    'brief': 'Сбрасывает кеш прав.',
                    'sig': 'access.invalidate([p])',
                    'args': [('p', 'player \\| nil', 'чей кеш; без аргумента — всех')],
                    'extra': 'Нужен после `grant`/`revoke` из своего кода.',
                },
            ],
        },
        {
            'title': 'Чтение',
            'items': [
                {
                    'name': 'access.can',
                    'brief': 'Есть ли у игрока право.',
                    'sig': 'access.can(p, node)',
                    'args': [('p', 'player \\| nil', '`nil` — консоль, ей разрешено всё'),
                             ('node', 'string', 'нода')],
                    'returns': [('boolean', 'разрешено ли')],
                    'extra': 'То же, что [`p:can`](../players/can.md), но принимает `nil`.',
                },
                {
                    'name': 'access.permissions',
                    'brief': 'Все объявленные ноды.',
                    'sig': 'access.permissions()',
                    'args': [],
                    'returns': [('table', '`{ [node] = { desc = , default = } }`')],
                },
                {
                    'name': 'access.users',
                    'brief': 'Все записи из `users.lua`.',
                    'sig': 'access.users()',
                    'args': [],
                    'returns': [('table', '`{ [key] = entry }`')],
                },
                {
                    'name': 'access.user',
                    'brief': 'Одна запись из `users.lua`.',
                    'sig': 'access.user(key)',
                    'args': [('key', 'string', 'steamid или `name:<ник>`')],
                    'returns': [('table \\| nil', 'запись')],
                },
                {
                    'name': 'access.all_groups',
                    'brief': 'Все группы.',
                    'sig': 'access.all_groups()',
                    'args': [],
                    'returns': [('table', '`{ [name] = group }`')],
                },
                {
                    'name': 'access.group',
                    'brief': 'Одна группа по имени.',
                    'sig': 'access.group(name)',
                    'args': [('name', 'string', 'имя группы')],
                    'returns': [('table \\| nil', 'группа')],
                },
            ],
        },
    ],
}
