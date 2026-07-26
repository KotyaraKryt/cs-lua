# -*- coding: utf-8 -*-

REGAMEDLL = ('warning', 'Событие приходит из ReGameDLL. На ванильном `mp.dll` оно не\nсрабатывает никогда.')


def event(name, brief, fields, cancel=None, example=None, extra=None,
          notes=None, regamedll=False, see=None):
    n = list(notes or [])
    if regamedll:
        n.append(REGAMEDLL)
    return {
        'name': name,
        'brief': brief,
        'sig': 'hook.add("%s", id, function(e)\n\t...\nend)' % name,
        'args': None,
        'fields': ('Поля события', fields or [('—', '—', 'событие без полей')]),
        'example': example,
        'extra': (('**Отмена.** ' + cancel + '\n\n') if cancel else '') + (extra or '') or None,
        'notes': n,
        'see': see,
    }


PLAYER = ('e.player', 'player', 'кого касается событие')

NS = {
    'dir': 'hook',
    'label': 'hook',
    'title': 'hook',
    'brief': 'Одна система на движковые события и на свои.',
    'intro': """
Обработчик получает **одну таблицу** — объект события. Возврат не читается:
чтобы что-то изменить, пиши в поле, чтобы отменить — вызови `e:cancel()`.

```lua
hook.add("player_hurt", "myplugin.double", function(e)
	e.damage = e.damage * 2
end)
```

Одна и та же таблица достаётся всей цепочке, поэтому второй обработчик видит
правки первого. У каждого события есть `e.name` и `e.cancelled`.
""",
    'groups': [
        {
            'title': 'Пространство имён',
            'items': [
                {
                    'name': 'hook.add',
                    'brief': 'Подписывает функцию на событие.',
                    'sig': 'hook.add(event, id, fn)',
                    'args': [
                        ('event', 'string', 'имя события; своё обязано содержать точку'),
                        ('id', 'string', 'уникален внутри этого плагина'),
                        ('fn', 'function', 'получает объект события'),
                    ],
                    'example': """
hook.add("player_spawn", "myplugin.armor", function(e)
	e.player:armor(100)
end)
""",
                    'extra': """
Повторная регистрация той же пары `(event, id)` **заменяет** обработчик. Это то,
что не даёт `lua_reload <plugin>` удвоить подписки.

Id уникален внутри плагина — два разных плагина спокойно называют свои `"init"`.

## Порядок вызова

Обработчики идут в порядке загрузки плагинов. Обычно это неважно, но на
отменяемом событии решает, кто выиграет: `e:cancel()` обрывает цепочку.

Числа приоритета в `hook.add` тут не помогли бы: как только их начнут ставить
все, они снова перестанут что-либо значить. Порядком распоряжается владелец
сервера — через `addons/lua/load_order.txt`:

```
# грузятся первыми и в этом порядке
godmode
damager
stats
```

Всё, чего в файле нет, идёт после по алфавиту, поэтому список не обязан быть
полным. Текущий порядок видно в [`lua_list`](../console.md) и
[`lua_hooks`](../console.md).
""",
                    'notes': [('warning', 'Неизвестное имя без точки — ошибка со списком движковых событий.\nТак ловится опечатка, которая иначе дала бы молча неработающую\nподписку.')],
                    'see': [('hook.remove', 'remove.md'), ('lua_hooks', '../console.md')],
                },
                {
                    'name': 'hook.remove',
                    'brief': 'Снимает подписку по имени события и id.',
                    'sig': 'hook.remove(event, id)',
                    'args': [('event', 'string', 'имя события'),
                             ('id', 'string', 'id, под которым подписывались')],
                    'returns': [('boolean', '`true`, если обработчик был')],
                    'extra': 'Снимает только собственный обработчик плагина: один плагин не может тихо отписать чужой.',
                },
                {
                    'name': 'hook.run',
                    'brief': 'Запускает своё событие плагина.',
                    'sig': 'hook.run(event[, data])',
                    'args': [('event', 'string', 'имя, обязательно с точкой'),
                             ('data', 'table \\| nil', 'становится объектом события')],
                    'returns': [('table', 'та же таблица — из неё читается результат')],
                    'example': """
local e = hook.run("shop.buying", { player = p, item = item })
if e.cancelled then return end
""",
                    'extra': """
Точка в имени обязательна: она отличает своё событие от опечатки в движковом.

Движковое событие через `hook.run` запустить нельзя — это делает модуль.
""",
                    'see': [('export / import', '../exports/index.md')],
                },
                {
                    'name': 'hook.list',
                    'brief': 'Возвращает список подписок в порядке вызова.',
                    'sig': 'hook.list([event])',
                    'args': [('event', 'string \\| nil', 'только по одному событию')],
                    'returns': [('table', 'массив `{ event = , id = , plugin = }`')],
                    'extra': 'Консольная обёртка — [`lua_hooks`](../console.md).',
                },
            ],
        },
        {
            'title': 'Подключение',
            'note': 'Порядок: `client_connect` → `player_authorized` → `player_ready`.',
            'items': [
                event('client_connect',
                      'Игрок стучится на сервер; его ещё можно не пустить.',
                      [PLAYER, ('e.name', 'string', 'ник'),
                       ('e.ip', 'string', 'адрес'),
                       ('e.reason', 'string', 'запись: текст отказа, который увидит игрок')],
                      cancel='`e:cancel()` не пускает игрока. Без `e.reason` он увидит общую фразу.',
                      example="""
hook.add("client_connect", "bans.check", function(e)
	if banned[e.ip] then
		e.reason = "Вы забанены"
		e:cancel()
	end
end)
""",
                      notes=[('warning', 'Здесь `e.player:steamid()` возвращает `STEAM_ID_PENDING`, а\nсообщения не доходят. Права и статистику вешай на\n`player_authorized`, приветствия — на `player_ready`.')]),
                event('client_disconnect', 'Игрок отключился.',
                      [PLAYER, ('e.name', 'string', 'ник — сам объект уже пустеет')],
                      extra='Место, где чистят своё состояние по `e.player.id`.'),
                event('player_authorized', 'Steam ответил, steamid наконец известен.',
                      [PLAYER, ('e.steamid', 'string', 'настоящий authid')],
                      extra='Срабатывает один раз за подключение. Всё, что завязано на steamid — права, статистика — начинается здесь.'),
                event('player_ready', 'Игрок в игре, сообщения до него доходят.',
                      [PLAYER],
                      extra='Место для приветствий и первого HUD.'),
                event('player_chat', 'Игрок написал в чат.',
                      [PLAYER, ('e.text', 'string', 'что он написал'),
                       ('e.team', 'boolean', 'сообщение ушло в `say_team`')],
                      cancel='`e:cancel()` проглатывает сообщение — оно не доходит ни до кого. Так работает `!команда`, и так же чат-менеджер подменяет строку: отменить и разослать свою.',
                      example="""
hook.add("player_chat", "myplugin.mute", function(e)
	if muted[e.player.id] then
		e.player:chat("Ты в муте")
		e:cancel()
	end
end)
"""),
                event('menu_select', 'Игрок нажал клавишу в меню, открытом из Lua.',
                      [PLAYER, ('e.key', 'number', 'номер клавиши, 1..10')],
                      extra='Обычно не нужно: [`menu`](../menu/index.md) разбирает это сам.'),
            ],
        },
        {
            'title': 'Жизнь сервера',
            'items': [
                event('map_change', 'Карта заканчивается.',
                      [('e.map', 'string', 'имя карты, которая заканчивается')],
                      extra='Lua-состояние живёт дальше. Место, где пишут данные на диск и снимают эффекты с мира.'),
                event('plugin_unload', 'Плагин или всё состояние уходит.',
                      [('e.plugin', 'string \\| nil', 'id уходящего плагина; `nil` — гасится всё состояние')],
                      extra='Подписываться стоит, если держишь ссылки на чужой плагин. Для снятия собственных эффектов есть [`plugin.on_unload`](../plugin/on_unload.md) — он про твой плагин.'),
            ],
        },
        {
            'title': 'Геймплей',
            'note': 'Всё в этом разделе приходит из ReGameDLL.',
            'items': [
                event('player_spawn', 'Игрок появился в раунде живым.', [PLAYER],
                      regamedll=True,
                      extra='Спавн сбрасывает гравитацию, скорость, заморозку, godmode и noclip — ставь их здесь.'),
                event('player_hurt', 'Игроку наносят урон; урон можно изменить или погасить.',
                      [('e.victim', 'player', 'кому прилетело'),
                       ('e.attacker', 'player \\| nil', '`nil` при падении и уроне мира'),
                       ('e.damage', 'number', 'запись: сколько урона применить'),
                       ('e.bits', 'number', 'маска типа урона: `DMG_FALL`, `DMG_BULLET`'),
                       ('e.hitgroup', 'number \\| nil', 'куда попали: `1` голова, `2` грудь, `3` живот, `4`/`5` руки, `6`/`7` ноги; `nil`, если урон не от попадания')],
                      cancel='`e:cancel()` гасит урон полностью: ни звука боли, ни брони, ни смерти. Цепочка на этом обрывается.',
                      example="""
hook.add("player_hurt", "myplugin.double", function(e)
	e.damage = e.damage * 2
end)
""",
                      extra='''Единственное событие, которое меняет игру. Каждый следующий обработчик видит
`e.damage` после предыдущего.

`e.hitgroup` заполнен только когда урон пришёл от попадания — пуля, нож. У
падения, взрыва и урона мира его нет: движок оставил бы там зону от прошлого
попадания, а это враньё.''',
                      regamedll=True,
                      see=[('player_hurt_post', 'player_hurt_post.md')]),
                event('player_hurt_post', 'Урон уже применён; только для наблюдателей.',
                      [('e.victim', 'player', 'кому прилетело'),
                       ('e.attacker', 'player \\| nil', '`nil` при падении и уроне мира'),
                       ('e.damage', 'number', 'сколько урона применилось'),
                       ('e.bits', 'number', 'маска типа урона'),
                       ('e.hitgroup', 'number \\| nil', 'куда попали: `1` голова, `2` грудь, `3` живот, `4`/`5` руки, `6`/`7` ноги; `nil`, если урон не от попадания')],
                      extra='`e.victim:health()` здесь уже актуальное. Менять нечего — событие неотменяемое.',
                      regamedll=True),
                event('player_death', 'Игрок погиб.',
                      [('e.victim', 'player', 'погибший'),
                       ('e.killer', 'player \\| nil', '`nil` при падении, уроне мира и окружения'),
                       ('e.headshot', 'boolean', 'попадание в голову'),
                       ('e.weapon', 'string \\| nil', 'classname того, что было у убийцы в руках'),
                       ('e.distance', 'number \\| nil', 'юнитов между убийцей и жертвой')],
                      example="""
hook.add("player_death", "myplugin.bounty", function(e)
	if not e.killer or e.killer.id == e.victim.id then return end
	e.killer:money(e.killer:money() + (e.headshot and 600 or 300), { hud = true })
end)
""",
                      extra='Суицид приходит как `e.killer.id == e.victim.id`. Вместе с `e.killer` в `nil` уходят `e.weapon` и `e.distance`.',
                      regamedll=True,
                      notes=[('note', 'Граната и взрыв C4 приходят как оружие в руках убийцы, а не как\n`hegrenade`. Отличить снаряд можно по `e.bits` в `player_hurt`.')]),
                event('player_team_change', 'Игрок сменил сторону.',
                      [PLAYER, ('e.old_team', 'string', '`CT`, `T`, `SPEC`, `NONE`'),
                       ('e.new_team', 'string', 'куда перешёл')],
                      extra='Команда опрашивается каждый кадр, поэтому ловится любая смена: меню, автобаланс, `p:team()`, сторонний мод. Событие приходит на следующем кадре. Первый заход игрока событием не считается.',
                      regamedll=True),
                event('weapon_fire', 'Из ствола вышел выстрел.',
                      [PLAYER, ('e.weapon', 'string', 'classname оружия'),
                       ('e.clip', 'number', 'патронов в магазине после выстрела')],
                      extra='Только огнестрел: нож бьёт через `TraceAttack`, гранаты идут своими цепочками. Дробовик даёт одно событие на выстрел, а не на дробину.',
                      regamedll=True),
            ],
        },
        {
            'title': 'Раунд и бомба',
            'items': [
                event('round_start', 'Раунд начался.', [], regamedll=True),
                event('round_end', 'Раунд закончился.',
                      [('e.winner', 'number', '`1` — CT, `2` — T, `3` — ничья')],
                      regamedll=True),
                event('round_freeze_end', 'Заморозка кончилась, игроки могут двигаться.', [],
                      regamedll=True),
                event('bomb_planted', 'Бомба заложена.',
                      [('e.player', 'player \\| nil', 'кто заложил; `nil`, если игрока установить не удалось')],
                      regamedll=True),
                event('bomb_defused', 'Попытка разминирования завершилась.',
                      [('e.player', 'player \\| nil', 'кто разминировал'),
                       ('e.success', 'boolean', 'успел ли')],
                      extra='Событие приходит и при неудаче — проверяй `e.success`.',
                      regamedll=True),
                event('bomb_exploded', 'Бомба взорвалась.',
                      [('e.x, e.y, e.z', 'number', 'координаты взрыва')],
                      extra='Игрока в событии нет: запоминай заложившего в `bomb_planted`, если он нужен.',
                      regamedll=True),
            ],
        },
    ],
}
