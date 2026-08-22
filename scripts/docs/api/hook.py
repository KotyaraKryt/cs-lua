# -*- coding: utf-8 -*-

REGAMEDLL = ('warning', 'Событие приходит из ReGameDLL. На ванильном `mp.dll` оно не\nсрабатывает никогда.')
REHLDS = ('warning', 'Событие требует ReHLDS. На движке без него — тихо никогда не\nсрабатывает, без ошибок и предупреждений.')


def event(name, brief, fields, cancel=None, example=None, extra=None,
          notes=None, regamedll=False, see=None, slug=None):
    n = list(notes or [])
    if regamedll:
        n.append(REGAMEDLL)
    return {
        'name': name,
        'slug': slug,
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
hook.add("player:hurt", "myplugin.double", function(e)
	e.damage = e.damage * 2
end)
```

Одна и та же таблица достаётся всей цепочке, поэтому второй обработчик видит
правки первого. У каждого события есть `e.name` и `e.cancelled`.

Имя события — `subject:verb` через двоеточие (`player:spawn`, `weapon:buy`).
Своё событие плагина — через точку (`shop.bought`): так модуль отличает
опечатку в движковом имени от чужого события, которое просто ещё не
зарегистрировано.
""",
    'groups': [
        {
            'title': 'Пространство имён',
            'slug': 'namespace',
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
hook.add("player:spawn", "myplugin.armor", function(e)
	e.player:armor(100)
end)
""",
                    'extra': """
Повторная регистрация той же пары `(event, id)` **заменяет** обработчик. Это то,
что не даёт `lua_reload <plugin>` удвоить подписки.

Id уникален внутри плагина — два разных плагина спокойно называют свои `"init"`.

### Приём сетевых сообщений — `msg:Name`

Особый вид имени, без точки: `hook.add("msg:TextMsg", id, fn)` подписывается
на сетевые сообщения, которые движок отправляет клиентам — приёмная сторона
к [`msg.send`](../msg/send.md). Свои подробности — у [`msg`](../msg/index.md).

### Порядок вызова

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
                    'notes': [('warning', 'Неизвестное имя без точки и без `msg:` — ошибка со списком\nдвижковых событий. Так ловится опечатка, которая иначе дала бы\nмолча неработающую подписку.')],
                    'see': [('hook.remove', 'remove.md'), ('msg.send', '../msg/send.md'), ('lua_hooks', '../console.md')],
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
            'slug': 'connection',
            'note': 'Порядок: `client:connect` → `client:authorized` → `client:connected`.',
            'items': [
                event('client:connect',
                      'Игрок стучится на сервер; его ещё можно не пустить.',
                      [PLAYER, ('e.name', 'string', 'ник'),
                       ('e.ip', 'string', 'адрес'),
                       ('e.reason', 'string', 'запись: текст отказа, который увидит игрок')],
                      slug='connect',
                      cancel='`e:cancel()` не пускает игрока. Без `e.reason` он увидит общую фразу.',
                      example="""
hook.add("client:connect", "bans.check", function(e)
	if banned[e.ip] then
		e.reason = "Вы забанены"
		e:cancel()
	end
end)
""",
                      notes=[('warning', 'Здесь `e.player:steamid()` возвращает `STEAM_ID_PENDING`, а\nсообщения не доходят. Права и статистику вешай на\n`client:authorized`, приветствия — на `client:connected`.')]),
                event('client:disconnect', 'Игрок отключился.',
                      [PLAYER, ('e.name', 'string', 'ник — сам объект уже пустеет')],
                      slug='disconnect',
                      extra='Место, где чистят своё состояние по `e.player.id`.'),
                event('client:authorized', 'Steam ответил, steamid наконец известен.',
                      [PLAYER, ('e.steamid', 'string', 'настоящий authid')],
                      slug='authorized',
                      extra='Срабатывает один раз за подключение. Всё, что завязано на steamid — права, статистика — начинается здесь.'),
                event('client:connected', 'Игрок в игре, сообщения до него доходят.',
                      [PLAYER],
                      slug='connected',
                      extra='Место для приветствий и первого HUD.'),
                event('player:chat', 'Игрок написал в чат.',
                      [PLAYER, ('e.text', 'string', 'что он написал'),
                       ('e.team', 'boolean', 'сообщение ушло в `say_team`')],
                      slug='chat',
                      cancel='`e:cancel()` проглатывает сообщение — оно не доходит ни до кого. Так работает `!команда`, и так же чат-менеджер подменяет строку: отменить и разослать свою.',
                      example="""
hook.add("player:chat", "myplugin.mute", function(e)
	if muted[e.player.id] then
		e.player:chat("Ты в муте")
		e:cancel()
	end
end)
"""),
                event('menu:select', 'Игрок нажал клавишу в меню, открытом из Lua.',
                      [PLAYER, ('e.key', 'number', 'номер клавиши, 1..10')],
                      slug='select',
                      extra='Обычно не нужно: [`menu`](../menu/index.md) разбирает это сам.'),
            ],
        },
        {
            'title': 'Жизнь сервера',
            'slug': 'lifecycle',
            'items': [
                event('server:map_change', 'Карта заканчивается.',
                      [('e.map', 'string', 'имя карты, которая заканчивается')],
                      slug='map_change',
                      extra='Lua-состояние живёт дальше. Место, где пишут данные на диск и снимают эффекты с мира.'),
                event('module:plugin_unload', 'Плагин или всё состояние уходит.',
                      [('e.plugin', 'string \\| nil', 'id уходящего плагина; `nil` — гасится всё состояние')],
                      slug='plugin_unload',
                      extra='Подписываться стоит, если держишь ссылки на чужой плагин. Для снятия собственных эффектов есть [`plugin.on_unload`](../plugin/on_unload.md) — он про твой плагин.'),
            ],
        },
        {
            'title': 'Геймплей',
            'slug': 'gameplay',
            'note': 'Всё в этом разделе приходит из ReGameDLL.',
            'items': [
                event('player:spawn', 'Игрок появился в раунде живым.', [PLAYER],
                      slug='player_spawn',
                      regamedll=True,
                      extra='Спавн сбрасывает гравитацию, скорость, заморозку, godmode и noclip — ставь их здесь.'),
                event('player:hurt', 'Игроку наносят урон; урон можно изменить или погасить.',
                      [('e.victim', 'player', 'кому прилетело'),
                       ('e.attacker', 'player \\| nil', '`nil` при падении и уроне мира'),
                       ('e.damage', 'number', 'запись: сколько урона применить'),
                       ('e.bits', 'number', 'маска типа урона: `DMG_FALL`, `DMG_BULLET`'),
                       ('e.hitgroup', 'number \\| nil', 'куда попали: `1` голова, `2` грудь, `3` живот, `4`/`5` руки, `6`/`7` ноги; `nil`, если урон не от попадания')],
                      slug='player_hurt',
                      cancel='`e:cancel()` гасит урон полностью: ни звука боли, ни брони, ни смерти. Цепочка на этом обрывается.',
                      example="""
hook.add("player:hurt", "myplugin.double", function(e)
	e.damage = e.damage * 2
end)
""",
                      extra='''Единственное событие, которое меняет игру. Каждый следующий обработчик видит
`e.damage` после предыдущего.

`e.hitgroup` заполнен только когда урон пришёл от попадания — пуля, нож. У
падения, взрыва и урона мира его нет: движок оставил бы там зону от прошлого
попадания, а это враньё.''',
                      regamedll=True,
                      see=[('player:hurt_post', 'player_hurt_post.md'),
                           ('player:trace_attack', 'player_trace_attack.md')]),
                event('player:hurt_post', 'Урон уже применён; только для наблюдателей.',
                      [('e.victim', 'player', 'кому прилетело'),
                       ('e.attacker', 'player \\| nil', '`nil` при падении и уроне мира'),
                       ('e.damage', 'number', 'сколько урона применилось'),
                       ('e.bits', 'number', 'маска типа урона'),
                       ('e.hitgroup', 'number \\| nil', 'куда попали: `1` голова, `2` грудь, `3` живот, `4`/`5` руки, `6`/`7` ноги; `nil`, если урон не от попадания')],
                      slug='player_hurt_post',
                      extra='`e.victim:health()` здесь уже актуальное. Менять нечего — событие неотменяемое.',
                      regamedll=True),
                event('player:trace_attack', 'Один хитбокс получил попадание — раньше и точнее, чем `player:hurt`: до брони, до множителей, до суммирования дроби дробовика в одно число.',
                      [('e.victim', 'player', 'кому прилетело'),
                       ('e.attacker', 'player \\| nil', '`nil` при уроне мира'),
                       ('e.damage', 'number', 'запись: сырой урон этого попадания, до брони и множителей'),
                       ('e.bits', 'number', 'маска типа урона'),
                       ('e.hitgroup', 'number \\| nil', 'куда попали: `1` голова, `2` грудь, `3` живот, `4`/`5` руки, `6`/`7` ноги'),
                       ('e.x, e.y, e.z', 'number', 'точка попадания')],
                      slug='player_trace_attack',
                      cancel='`e:cancel()` (или `e.damage = 0`) убирает это попадание целиком: ни крови, ни вклада в итоговый урон, который дальше увидит `player:hurt`.',
                      extra='Название — по движковому хуку `CBasePlayer::TraceAttack` (тот же, что `RG_CBasePlayer_TraceAttack` в ReAPI). Дробовик даёт одно `player:trace_attack` на каждую долетевшую дробину и одно `player:hurt`/`weapon:fire` в сумме — здесь урон ещё не сложен.',
                      regamedll=True,
                      see=[('player:hurt', 'player_hurt.md')]),
                event('player:heal', 'Игроку вот-вот дадут здоровье — аптечка, админ-команда; своей регенерации в CS нет.',
                      [PLAYER, ('e.amount', 'number', 'запись: сколько здоровья дать'),
                       ('e.bits', 'number', 'маска, с которой движок передал лечение')],
                      slug='player_heal',
                      cancel='`e:cancel()` (или `e.amount = 0`) — здоровье не даётся вообще.',
                      regamedll=True),
                event('player:death', 'Игрок погиб.',
                      [('e.victim', 'player', 'погибший'),
                       ('e.killer', 'player \\| nil', '`nil` при падении, уроне мира и окружения'),
                       ('e.headshot', 'boolean', 'попадание в голову'),
                       ('e.weapon', 'string \\| nil', 'classname того, что было у убийцы в руках'),
                       ('e.distance', 'number \\| nil', 'юнитов между убийцей и жертвой')],
                      slug='player_death',
                      example="""
hook.add("player:death", "myplugin.bounty", function(e)
	if not e.killer or e.killer.id == e.victim.id then return end
	e.killer:money(e.killer:money() + (e.headshot and 600 or 300), { hud = true })
end)
""",
                      extra='Суицид приходит как `e.killer.id == e.victim.id`. Вместе с `e.killer` в `nil` уходят `e.weapon` и `e.distance`.',
                      regamedll=True,
                      notes=[('note', 'Граната и взрыв C4 приходят как оружие в руках убийцы, а не как\n`hegrenade`. Отличить снаряд можно по `e.bits` в `player:hurt`.')]),
                event('player:team_change', 'Игрок сменил сторону.',
                      [PLAYER, ('e.old_team', 'string', '`CT`, `T`, `SPEC`, `NONE`'),
                       ('e.new_team', 'string', 'куда перешёл')],
                      slug='player_team_change',
                      extra='Команда опрашивается каждый кадр, поэтому ловится любая смена: меню, автобаланс, `p:team()`, сторонний мод. Событие приходит на следующем кадре. Первый заход игрока событием не считается.',
                      regamedll=True),
                event('player:jump', 'Игрок прыгнул.', [PLAYER],
                      slug='player_jump',
                      regamedll=True,
                      extra='Не отменяемое: к моменту события движок уже применил прыжок к движению игрока, отменять нечего.'),
                event('player:duck', 'Игрок присел.', [PLAYER],
                      slug='player_duck',
                      regamedll=True,
                      extra='Не отменяемое, по той же причине, что и `player:jump`.'),
                event('player:spectate', 'Игрок перешёл в режим наблюдателя.', [PLAYER],
                      slug='player_spectate',
                      regamedll=True),
                event('player:radio', 'Игрок использовал радиокоманду.',
                      [PLAYER, ('e.sentence', 'string', 'имя произносимой фразы движка'),
                       ('e.sample', 'string', 'звуковой файл')],
                      slug='player_radio',
                      cancel='`e:cancel()` — ни звука, ни строки «Radio: ...» в консоли у остальных.',
                      regamedll=True),
                event('player:respawn_check', 'Игра вот-вот решит, можно ли игроку возродиться.', [PLAYER],
                      slug='player_can_respawn',
                      cancel='`e:cancel()` принудительно запрещает респаун — независимо от того, что решили бы правила игры сами. Разрешить респаун, который правила и так запретили бы, этим событием нельзя: только забрать разрешение, не выдать его.',
                      regamedll=True),
                event('player:money_change', 'У игрока вот-вот изменятся деньги — раундовый бонус, награда за фраг, покупка, действие с заложником и т.д.',
                      [PLAYER, ('e.amount', 'number', 'на сколько меняется баланс; отрицательное — трата'),
                       ('e.reason', 'string', '`round_bonus`, `enemy_killed`, `bought_something`, `hostage_rescued` и т.д.')],
                      slug='money_change',
                      cancel='`e:cancel()` — баланс не меняется вообще.',
                      extra='`weapon:buy`/`ammo:buy`/`item:buy` уже покрывают саму покупку; это событие — про деньги в отрыве от того, что их вызвало, включая раундовый бонус и награды за фраги, которые через магазинные события не видны.',
                      regamedll=True),
                event('weapon:fire', 'Из ствола вышел выстрел.',
                      [PLAYER, ('e.weapon', 'string', 'classname оружия'),
                       ('e.clip', 'number', 'патронов в магазине после выстрела')],
                      slug='weapon_fire',
                      extra='Только огнестрел: нож бьёт через `TraceAttack`, гранаты идут своими цепочками. Дробовик даёт одно событие на выстрел, а не на дробину.',
                      regamedll=True),
                event('weapon:deploy', 'Оружие вот-вот покажет вьюмодель и модель в руках.',
                      [PLAYER, ('e.weapon', 'string', 'classname оружия'),
                       ('e.view_model', 'string', 'путь вьюмодели; запись подменяет её'),
                       ('e.world_model', 'string', 'путь модели в руках; запись подменяет её')],
                      slug='weapon_deploy',
                      example="""
hook.add("weapon:deploy", "healnade.reskin", function(e)
	if e.weapon == "weapon_hegrenade" then
		e.view_model = "models/reapi_healthnade/v_hegrenade_v1.mdl"
		e.world_model = "models/reapi_healthnade/p_hegrenade_v1.mdl"
	end
end)
""",
                      extra='Оба поля приходят уже заполненными тем, что показал бы движок — меняет их только тот, кто хочет реснуть оружие. Не заменяет модель на земле/в полёте: это `grenade:thrown` про `weapon_hegrenade`.',
                      regamedll=True,
                      see=[('grenade:thrown', 'grenade_thrown.md')]),
                event('weapon:reload', 'Началась настоящая перезарядка.',
                      [PLAYER, ('e.weapon', 'string', 'classname оружия'),
                       ('e.clip', 'number', 'патронов в магазине на момент начала перезарядки'),
                       ('e.delay', 'number', 'секунд до конца анимации перезарядки'),
                       ('e.max_clip', 'number \\| nil', 'вместимость магазина; `nil`, если движок её не отдал')],
                      slug='weapon_reload',
                      example="""
hook.add("weapon:reload", "myplugin.reload_log", function(e)
	e.player:hud(("перезаряжаешь %s (%d/%s патронов), готово через %.1fс")
		:format(e.weapon, e.clip, e.max_clip or "?", e.delay))
end)
""",
                      extra='Приходит из `DefaultReload`/`DefaultShotgunReload` — но только когда те реально начинают перезарядку. Сами хелперы вызываются при каждой попытке (в том числе от зажатой или забинженной клавиши), а внутри молча ничего не делают, если магазин уже полон или патронов в запасе нет — так что это не то же самое, что чтение кнопки `reload` на клиенте: событие приходит, только когда перезарядка правда стартовала.\n\n`e.clip` — это ещё старое значение, до заполнения: сколько патронов оставалось в магазине в момент, когда игрок начал перезарядку.\n\n`e.delay` — время, которое движок передал самой анимации, а не пересчитанный остаток; для дробовиков (перезарядка патрон за патроном) это время именно текущего патрона, а не всей перезарядки целиком.\n\n`e.max_clip` — вместимость магазина этого оружия. Для обычного оружия это тот же `iClipSize`, что движок передаёт в `DefaultReload`; для дробовиков (у `DefaultShotgunReload` такого параметра нет) читается через `GetItemInfo()`. Пригождается, когда нужно посчитать, сколько патронов реально уйдёт из запаса — например, для механики, где недострелянные патроны в стволе не досыпаются к новому магазину, а теряются.',
                      regamedll=True,
                      see=[('weapon:deploy', 'weapon_deploy.md')]),
                event('item:give', 'Движок вот-вот выдаст игроку предмет по имени classname.',
                      [PLAYER, ('e.item', 'string', 'classname выдаваемого предмета')],
                      slug='item_give',
                      cancel='`e:cancel()` — ничего не создаётся и не выдаётся.',
                      extra='Не тот же путь, что `p:give()` (тот идёт через `GiveNamedItemEx`, отдельный вызов ReGameDLL, который SDK не документирует как обёртку над этим). Это всё остальное, что раздаёт предметы по имени: стартовое снаряжение раунда, `give` из rcon, сторонний мод.',
                      regamedll=True),
                event('player:strip', 'У игрока только что забрали весь инвентарь.',
                      [PLAYER, ('e.remove_suit', 'boolean', 'забрали и костюм тоже')],
                      slug='player_strip',
                      extra='Не отменяемое: к моменту события инвентарь уже пуст. Срабатывает и на `p:strip()` из любого плагина — хук общий для всех путей, которые ведут к `RemoveAllItems`.',
                      regamedll=True),
                event('player:can_have_item', 'Игра вот-вот решит, может ли игрок вообще получить этот предмет.',
                      [PLAYER, ('e.item', 'string', 'classname предмета')],
                      slug='player_can_have_item',
                      cancel='`e:cancel()` принудительно запрещает — независимо от того, что решили бы правила игры сами. Разрешить то, что правила и так запретили бы, этим событием нельзя: только забрать разрешение, не выдать его.',
                      extra='Комментарий SDK для этого хука дословно: «the player is touching an item, do I give it to him» — шире, чем просто подбор с земли: сюда же попадает и `item:give`, и всё остальное, что спрашивает разрешения выдать предмет.',
                      regamedll=True,
                      see=[('weapon:pickup', 'weapon_pickup.md')]),
                event('weapon:pickup', 'Игрок подобрал оружие с земли.',
                      [PLAYER, ('e.weapon', 'string', 'classname подобранного оружия')],
                      slug='weapon_pickup',
                      extra='Комментарий SDK для этого хука дословно: «called each time a player picks up a weapon from the ground» — именно подбор с земли, не `item:give` и не покупка.',
                      regamedll=True,
                      see=[('ammo:pickup', 'ammo_pickup.md'), ('weapon:drop', 'weapon_drop.md')]),
                event('weapon:throw', 'Любой гранатный слот вот-вот покинёт руку, до того как движок решил, HE это, дымовая или флешка.',
                      [PLAYER, ('e.weapon', 'string', 'classname брошенного предмета'),
                       ('e.ammo_type', 'number', '`m_iPrimaryAmmoType` предмета — отличает переодетый предмет от настоящей гранаты того же classname'),
                       ('e.x, e.y, e.z', 'number', 'точка броска'),
                       ('e.vx, e.vy, e.vz', 'number', 'вектор броска'),
                       ('e.time', 'number', 'таймер, который движок передаст дальше')],
                      slug='weapon_throw',
                      cancel='`e:cancel()` забирает бросок целиком себе: движковый `CGrenade` не создаётся вообще. Обработчик, который отменяет, обычно уже успел построить свой снаряд сам (`ents.create` + `e:movetype(8)`) — иначе ничего не полетит.',
                      extra='Общий перехватчик перед тем, как движок решит, какая это граната — `grenade:throw` про конкретно HE/дымовую/флешку идёт дальше по цепочке отдельно.',
                      regamedll=True,
                      see=[('grenade:throw', 'grenade_throw.md')]),
                event('weapon:secondary_attack', 'Игрок нажал правую кнопку мыши, держа это оружие.',
                      [PLAYER, ('e.weapon', 'string', 'classname оружия'),
                       ('e.ammo_type', 'number', '`m_iPrimaryAmmoType` оружия')],
                      slug='weapon_secondary_attack',
                      extra='Срабатывает на каждое нажатие, не на каждый кадр зажатой кнопки. Нет отдельного движкового хука на secondary attack (в отличие от primary, за которым стоит `weapon:fire`) — событие собрано из детектора фронта кнопки в `PlayerPreThink`.',
                      regamedll=True),
                event('grenade:throw', 'HE, дымовая или флешка вот-вот покинёт руку.',
                      [('e.player', 'player \\| nil', 'кто бросает'),
                       ('e.weapon', 'string', '`weapon_hegrenade`, `weapon_smokegrenade` или `weapon_flashbang`'),
                       ('e.fuse', 'number', 'секунд до взрыва; запись подменяет таймер')],
                      slug='grenade_throw',
                      example="""
hook.add("grenade:throw", "healnade.instant", function(e)
	if e.weapon == "weapon_smokegrenade" then
		e.fuse = 0.05
	end
end)
""",
                      extra='Приходит до того, как движок создаст сам снаряд — на этом этапе ещё нет сущности для `e:detonate_on_touch()`, только число. Взрыв по касанию, а не по таймеру — это [`e:detonate_on_touch()`](../ents/detonate_on_touch.md) в `grenade:thrown`, а не подмена `fuse` здесь.',
                      regamedll=True,
                      see=[('grenade:thrown', 'grenade_thrown.md')]),
                event('grenade:thrown', 'HE, дымовая или флешка только что покинула руку.',
                      [('e.player', 'player \\| nil', 'кто бросил'),
                       ('e.weapon', 'string', '`weapon_hegrenade`, `weapon_smokegrenade` или `weapon_flashbang`'),
                       ('e.entity', 'entity \\| nil', 'сама граната; `nil`, если бросок не удался')],
                      slug='grenade_thrown',
                      example="""
hook.add("grenade:thrown", "healnade.reskin", function(e)
	if e.weapon == "weapon_smokegrenade" and e.entity then
		e.entity:model("models/reapi_healthnade/w_hegrenade_v1.mdl")
		e.entity:detonate_on_touch()
	end
end)
""",
                      extra='`e.entity` — обычный объект сущности, тот же, что даёт `ents.create`: `e:model()`, `e:origin()`, [`e:detonate_on_touch()`](../ents/detonate_on_touch.md) и весь остальной [`ents`](../ents/index.md) работают на нём как на любой другой.',
                      regamedll=True,
                      see=[('weapon:deploy', 'weapon_deploy.md'), ('ents', '../ents/index.md')]),
                event('player:disappear', 'Сущность игрока временно убирается из мира.',
                      [PLAYER],
                      slug='player_disappear',
                      extra='Название и место — по движковому хуку `CBasePlayer::Disappear`. Не отменяемое: к моменту события сущность уже убрана.',
                      regamedll=True),
                event('player:can_switch_team', 'Игра вот-вот решит, может ли игрок перейти в команду team.',
                      [PLAYER, ('e.team', 'string', '`CT`, `T`, `SPEC` или `NONE` — куда переходит')],
                      slug='player_can_switch_team',
                      cancel='`e:cancel()` принудительно запрещает переход — независимо от того, что решили бы правила игры сами. Разрешить переход, который правила и так запретили бы, этим событием нельзя: только забрать разрешение, не выдать его.',
                      regamedll=True,
                      see=[('player:team_change', 'player_team_change.md')]),
                event('player:shield_give', 'Игроку вот-вот выдадут щит.',
                      [PLAYER, ('e.deploy', 'boolean', 'щит достаётся из рук сразу же')],
                      slug='player_shield_give',
                      regamedll=True,
                      see=[('player:shield_drop', 'player_shield_drop.md')]),
                event('player:shield_drop', 'У игрока только что забрали щит.',
                      [PLAYER, ('e.deploy', 'boolean', 'то же значение deploy, что было при выдаче')],
                      slug='player_shield_drop',
                      extra='Не отменяемое: к моменту события щит уже убран.',
                      regamedll=True,
                      see=[('player:shield_give', 'player_shield_give.md')]),
                event('player:observer_next', 'Наблюдатель вот-вот переключится на другую цель.',
                      [PLAYER, ('e.reverse', 'boolean', 'направление переключения'),
                       ('e.target', 'string \\| nil', 'конкретное имя цели, если оно было задано')],
                      slug='player_observer_next',
                      extra='Не отменяемое: к моменту события движок уже выбрал следующую цель.',
                      regamedll=True,
                      see=[('player:observer_mode', 'player_observer_mode.md')]),
                event('player:observer_mode', 'У наблюдателя только что сменился режим камеры.',
                      [PLAYER, ('e.mode', 'number', '`0` свободный полёт выключен, `1` слежение без поворота камеры, `2` слежение со свободной камерой, `3` роуминг, `4` вид от первого лица, `5` карта свободно, `6` карта со слежением — см. `OBS_*` в SDK')],
                      slug='player_observer_mode',
                      extra='Не отменяемое.',
                      regamedll=True,
                      see=[('player:observer_next', 'player_observer_next.md')]),
                event('player:score_add', 'Игроку вот-вот изменят личный счёт очков.',
                      [PLAYER, ('e.score', 'number', 'запись: на сколько изменить счёт'),
                       ('e.allow_negative', 'boolean', 'запись: разрешить ли счёту уйти в минус')],
                      slug='player_score_add',
                      cancel='`e:cancel()` — счёт не меняется вообще.',
                      regamedll=True,
                      see=[('team:score_add', 'team_score_add.md')]),
                event('team:score_add', 'Команде вот-вот изменят счёт очков.',
                      [PLAYER, ('e.score', 'number', 'запись: на сколько изменить счёт'),
                       ('e.allow_negative', 'boolean', 'запись: разрешить ли счёту уйти в минус')],
                      slug='team_score_add',
                      cancel='`e:cancel()` — счёт не меняется вообще.',
                      extra='`e.player` — не «кому идут очки», а тот, через кого движок дёрнул вызов; какой команде считать очки, смотри через `e.player:team()`.',
                      regamedll=True,
                      see=[('player:score_add', 'player_score_add.md')]),
                event('player:userinfo_change', 'У игрока изменился один из userinfo-ключей (модель, имя и т.д.).',
                      [PLAYER],
                      slug='player_userinfo_change',
                      extra='Сырой буфер движка в событие не попадает — сразу читай нужный ключ через [`p:info(key)`](../players/identity.md#info).',
                      regamedll=True),
                event('player:can_hear', 'Игра вот-вот решит, слышит ли listener голос speaker.',
                      [('e.listener', 'player', 'кто слушает'),
                       ('e.speaker', 'player', 'кто говорит')],
                      slug='player_can_hear',
                      cancel='`e:cancel()` принудительно запрещает — независимо от того, что решили бы правила игры сами. Разрешить то, что правила и так запретили бы, этим событием нельзя: только забрать разрешение, не выдать его.',
                      regamedll=True),
                event('player:choose_model', 'Игрок выбрал пункт в меню внешности.',
                      [PLAYER, ('e.slot', 'number', 'номер пункта меню, не имя модели')],
                      slug='player_choose_model',
                      extra='Не отменяемое: к моменту события выбор уже применён.',
                      regamedll=True,
                      see=[('player:choose_team', 'player_choose_team.md')]),
                event('player:choose_team', 'Игрок выбрал пункт в меню команды.',
                      [PLAYER, ('e.slot', 'number', 'номер пункта меню, не имя команды')],
                      slug='player_choose_team',
                      cancel='`e:cancel()` блокирует сам переход в команду: выбор из этого меню применяется внутри того же вызова, так что отмена до него срабатывает как полный запрет, а не откат задним числом.',
                      regamedll=True,
                      see=[('player:choose_model', 'player_choose_model.md'), ('player:can_switch_team', 'player_can_switch_team.md')]),
                event('grenade:explode', 'HE, дымовая или флешка вот-вот взорвётся.',
                      [('e.player', 'player \\| nil', 'кто бросил; `nil`, если бросавшего не осталось'),
                       ('e.weapon', 'string', '`weapon_hegrenade`, `weapon_smokegrenade` или `weapon_flashbang`'),
                       ('e.entity', 'entity \\| nil', 'сама граната; `nil`, если уже пропала'),
                       ('e.x, e.y, e.z', 'number', 'точка взрыва')],
                      slug='grenade_explode',
                      cancel='`e:cancel()` забирает взрыв целиком себе: ни урона (у HE), ни дыма (у smoke), ни ослепления/звона в ушах (у флешки), ни звука от игры — дальше плагин сам решает, что происходит в этой точке. Сущность при этом не убирается сама: если она больше не нужна, убирай через `e.entity:remove()`.',
                      example="""
hook.add("grenade:explode", "healnade.explode", function(e)
	if e.weapon ~= "weapon_smokegrenade" or not e.player then return end
	e:cancel()
	for _, p in ipairs(players.list{ alive = true, team = e.player:team() }) do
		p:health(math.min(100, p:health() + 25))
	end
	if e.entity then e.entity:remove() end
end)
""",
                      extra='Бомбу не задевает: у неё отдельная цепочка, `bomb:exploded`.',
                      regamedll=True),
                event('player:round_respawn', 'Игрока вот-вот сбросят на новый раунд — позиция, здоровье, оружие (оставить или нет), наказание за тимкилл.',
                      [PLAYER],
                      slug='player_round_respawn',
                      cancel='`e:cancel()` — для этого игрока сброс на новый раунд не происходит вообще, он остаётся как есть.',
                      extra='Не путать с `player:spawn` — тот срабатывает и на самый первый спавн игрока тоже, это событие — именно про сброс между раундами.',
                      regamedll=True,
                      see=[('player:spawn', 'player_spawn.md')]),
                event('player:give_default_items', 'Игроку вот-вот выдадут стандартный набор на раунд — нож, дефолтный пистолет, всё, что задано cvar-ами команды.',
                      [PLAYER],
                      slug='player_give_default_items',
                      cancel='`e:cancel()` — движок сначала снимает с игрока всё, что у него есть, и только потом выдаёт стандартный набор; отмена пропускает оба шага, инвентарь остаётся как был.',
                      regamedll=True),
            ],
        },
        {
            'title': 'Раунд и бомба',
            'slug': 'round',
            'items': [
                event('round:start', 'Раунд начался.', [], slug='round_start', regamedll=True),
                event('round:end', 'Раунд закончился.',
                      [('e.winner', 'number', '`1` — CT, `2` — T, `3` — ничья')],
                      slug='round_end',
                      regamedll=True),
                event('round:freeze_end', 'Заморозка кончилась, игроки могут двигаться.', [],
                      slug='round_freeze_end',
                      regamedll=True),
                event('round:balance_teams', 'Автобаланс только что раскидал игроков по командам.', [],
                      slug='round_balance_teams',
                      extra='Приходит уже после перемещения — кого именно перекинули, событие не говорит; команду каждого игрока смотри через `p:team()` или лови `player:team_change`.',
                      regamedll=True,
                      see=[('player:team_change', 'player_team_change.md')]),
                event('round:intermission', 'Карта закончилась, сервер вот-вот покажет табло перед сменой карты.', [],
                      slug='round_intermission',
                      extra='Раньше, чем `server:map_change`: мир ещё цел, сущности ещё на месте. Последняя точка для финального сохранения статистики перед сменой карты.',
                      regamedll=True,
                      see=[('server:map_change', 'map_change.md')]),
                event('bomb:planted', 'Бомба заложена.',
                      [('e.player', 'player \\| nil', 'кто заложил; `nil`, если игрока установить не удалось')],
                      slug='bomb_planted',
                      regamedll=True),
                event('bomb:defusing', 'Игрок начал разминирование.',
                      [('e.player', 'player', 'кто начал разминировать'),
                       ('e.defuser', 'boolean', 'с набором для разминирования (быстрее) или голыми руками')],
                      slug='bomb_defuse_start',
                      extra='Не путать с `bomb:defused` — то приходит один раз в конце, успешно или нет; это — в момент начала попытки.',
                      regamedll=True,
                      see=[('bomb:defused', 'bomb_defused.md')]),
                event('bomb:defused', 'Попытка разминирования завершилась.',
                      [('e.player', 'player \\| nil', 'кто разминировал'),
                       ('e.success', 'boolean', 'успел ли')],
                      slug='bomb_defused',
                      extra='Событие приходит и при неудаче — проверяй `e.success`.',
                      regamedll=True),
                event('bomb:exploded', 'Бомба взорвалась.',
                      [('e.x, e.y, e.z', 'number', 'координаты взрыва')],
                      slug='bomb_exploded',
                      extra='Игрока в событии нет: запоминай заложившего в `bomb:planted`, если он нужен.',
                      regamedll=True),
                event('bomb:carrier', 'Игра только что выбрала, кому достанется бомба в этом раунде.',
                      [('e.player', 'player \\| nil', 'кто несёт бомбу; `nil`, если игра никого не выбрала')],
                      slug='bomb_carrier',
                      extra='Не отменяемое: кто несёт бомбу, событие не меняет, только сообщает.',
                      regamedll=True,
                      see=[('bomb:planted', 'bomb_planted.md')]),
                event('round:remove_guns', 'У всех игроков вот-вот заберут оружие.', [],
                      slug='round_remove_guns',
                      regamedll=True),
                event('round:dead_weapons', 'Игра только что решила, что делать с оружием погибшего.',
                      [PLAYER, ('e.mode', 'number', 'запись: `GR_PLR_DROP_GUN_ALL`, `GR_PLR_DROP_GUN_ACTIVE` или `GR_PLR_DROP_GUN_NO` из SDK')],
                      slug='round_dead_weapons',
                      extra='Не про разрешить/запретить — про выбор одного из вариантов, поэтому `e:cancel()` тут нет: меняй `e.mode` напрямую.',
                      regamedll=True),
                event('round:cleanup', 'Карта вот-вот сбросит мир между раундами.', [],
                      slug='round_cleanup',
                      regamedll=True),
            ],
        },
        {
            'title': 'Магазин',
            'slug': 'shop',
            'items': [
                event('weapon:buy', 'Игрок покупает оружие в магазине.',
                      [PLAYER, ('e.weapon', 'string', 'classname покупаемого оружия')],
                      slug='weapon_buy',
                      cancel='`e:cancel()` отменяет покупку целиком: деньги не списываются, оружие не выдаётся — как будто игрок ничего не покупал.',
                      example="""
hook.add("weapon:buy", "myplugin.no_awp", function(e)
	if e.weapon == "weapon_awp" then
		e.player:hud("AWP запрещён на этой карте")
		e:cancel()
	end
end)
""",
                      extra='Приходит на любую покупку оружия — ручную, ребай и автобай, все идут через один и тот же вызов движка. Сама сущность оружия на этот момент ещё не создана, поэтому в событии нет `e.entity`.',
                      regamedll=True,
                      see=[('ammo:buy', 'ammo_buy.md'), ('item:buy', 'item_buy.md')]),
                event('ammo:buy', 'Игрок покупает патроны для оружия в руках.',
                      [PLAYER, ('e.weapon', 'string', 'classname оружия, для которого докупают патроны')],
                      slug='ammo_buy',
                      cancel='`e:cancel()` отменяет покупку: деньги не списываются, патроны не добавляются.',
                      regamedll=True,
                      see=[('weapon:buy', 'weapon_buy.md')]),
                event('item:buy', 'Игрок покупает не-оружие в магазине: броню, прибор ночного видения, набор для разминирования, щит или гранату.',
                      [PLAYER, ('e.item', 'string', '`vest`, `vesthelm`, `flashbang`, `hegrenade`, `smokegrenade`, `nvg`, `defusekit` или `shield`')],
                      slug='item_buy',
                      cancel='`e:cancel()` отменяет покупку: деньги не списываются, предмет не выдаётся.',
                      example="""
hook.add("item:buy", "myplugin.no_defuse_for_t", function(e)
	if e.item == "defusekit" and e.player:team() == "t" then
		e:cancel()
	end
end)
""",
                      extra='Гранаты (`hegrenade`, `flashbang`, `smokegrenade`) идут через тот же пункт меню, что и броня/щит — это не то же самое, что бросок: `weapon:buy` про оружие, `item:buy` про всё остальное в магазине.',
                      regamedll=True,
                      see=[('weapon:buy', 'weapon_buy.md')]),
                event('ammo:pickup', 'Игроку вот-вот добавят патронов в запас — подбор с земли или дозарядка через магазин (отдельно от `ammo:buy`, которое про сам факт покупки).',
                      [PLAYER, ('e.weapon', 'string', 'classname оружия, для которого патроны'),
                       ('e.count', 'number', 'сколько добавляется'),
                       ('e.max', 'number', 'потолок запаса для этого оружия')],
                      slug='ammo_pickup',
                      cancel='`e:cancel()` — патроны не добавляются.',
                      regamedll=True,
                      see=[('ammo:buy', 'ammo_buy.md')]),
                event('weapon:drop', 'Игрок вот-вот выбросит оружие на пол — командой `drop`, из `p:drop()` или при потере слота.',
                      [PLAYER, ('e.weapon', 'string', 'classname выбрасываемого оружия')],
                      slug='weapon_drop',
                      cancel='`e:cancel()` — оружие остаётся в руках.',
                      regamedll=True),
            ],
        },
        {
            'title': 'Движок и ReHLDS',
            'slug': 'engine',
            'note': 'В отличие от «Геймплея» эти события не требуют ReGameDLL — работают и на ванильном `mp.dll`. Три из них (отмечены отдельно) требуют ReHLDS.',
            'items': [
                event('player:use', 'Игрок нажал +use на чём-то с обработчиком Use — кнопке, рычаге, multi_manager.',
                      [PLAYER, ('e.entity', 'entity \\| nil', 'что использовали; `nil`, если оно уже пропало')],
                      slug='player_use',
                      extra='Не отменяемое: к моменту события движковый `DispatchUse` уже отработал.'),
                event('player:suicide', 'Игрок вот-вот покончит с собой командой `kill` из консоли.',
                      [PLAYER],
                      slug='player_suicide',
                      cancel='`e:cancel()` — команда не срабатывает вообще, даже таймер повторного `kill` не сдвигается.'),
                event('ents:should_collide', 'Движок решает, должны ли две сущности физически сталкиваться.',
                      [('e.entity', 'entity \\| nil', 'первая сущность'),
                       ('e.other', 'entity \\| nil', 'вторая сущность'),
                       ('e.collide', 'boolean', 'запись: сталкиваются ли; по умолчанию `true`')],
                      slug='should_collide',
                      extra='Не через `e:cancel()` — здесь нет «отмены», это гейт с записываемым результатом: последний обработчик, тронувший `e.collide`, и решает. Молчание любого обработчика оставляет `true` — обычную физику.\n\nЕдинственная реализация этого хука в принципе: ReGameDLL сам его не предоставляет, так что тут нечего «оборачивать» — какой ответ дашь, такой и будет.'),
                event('ents:free', 'Сущность вот-вот уничтожат — последняя точка, где её ещё можно прочитать.',
                      [('e.entity', 'entity \\| nil', 'сущность, ещё живая на момент события')],
                      slug='ents_free',
                      extra='Не отменяемое: держать сущность после этого события всё равно нельзя, движок продолжит её разрушать сразу после.'),
                event('player:cvar_value', 'Клиент ответил на запрос значения cvar — старый вариант API, без имени cvar и без id запроса.',
                      [PLAYER, ('e.value', 'string', 'значение, которое прислал клиент')],
                      slug='player_cvar_value',
                      extra='Плагин, который запрашивал значение, обязан сам помнить, о каком cvar шла речь — движок это не передаёт.',
                      see=[('player:cvar_value2', 'player_cvar_value2.md')]),
                event('player:cvar_value2', 'То же самое, но новый вариант API: с именем cvar и id запроса, чтобы сверить ответ с тем, что спрашивали.',
                      [PLAYER, ('e.request_id', 'number', 'id, который был передан при запросе'),
                       ('e.cvar', 'string', 'имя cvar, про который спросили'),
                       ('e.value', 'string', 'значение, которое прислал клиент')],
                      slug='player_cvar_value2',
                      see=[('player:cvar_value', 'player_cvar_value.md')]),
                event('server:cvar_change', 'Любой cvar вот-вот изменится — движковый, чужого плагина, свой.',
                      [('e.name', 'string', 'имя cvar'),
                       ('e.value', 'string', 'новое значение')],
                      slug='server_cvar_change',
                      cancel='`e:cancel()` — cvar остаётся со старым значением.',
                      extra='Обработчик, который сам меняет cvar изнутри этого события, войдёт в него снова — не меняй здесь тот же cvar безусловно, иначе рекурсия не остановится.',
                      notes=[REHLDS]),
                event('server:precache_generic', 'Прекеширован generic-ресурс (не модель и не звук) — движком, игрой, другим плагином.',
                      [('e.name', 'string', 'путь ресурса'),
                       ('e.index', 'number', 'слот прекеша')],
                      slug='server_precache_generic',
                      notes=[REHLDS]),
                event('client:bot_created', 'Подключился бот (fake client).',
                      [PLAYER],
                      slug='bot_created',
                      notes=[REHLDS]),
            ],
        },
    ],
}
