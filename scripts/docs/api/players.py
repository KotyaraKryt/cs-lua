# -*- coding: utf-8 -*-

CONNECTED = ('warning', 'Обращение к состоянию отключённого игрока бросает ошибку.\nВ таймерах и отложенных коллбэках проверяй `p:connected()`.')
REGAMEDLL = ('warning', 'Требует ReGameDLL. На ванильном `mp.dll` метод бросает ошибку.')


def rw(name, brief, typ, extra=None, notes=None, example=None, args_extra=None):
    """Метод «без аргумента читает, с аргументом пишет»."""
    return {
        'name': 'p:' + name,
        'brief': brief,
        'sig': 'p:%s([value])' % name,
        'args': [('value', typ + ' \\| nil', 'новое значение; без него метод читает')] + (args_extra or []),
        'returns': [(typ, 'текущее значение при чтении; при записи ничего')],
        'example': example,
        'extra': extra,
        'notes': notes,
    }


def ro(name, brief, typ, sig=None, args=None, notes=None, example=None, extra=None):
    return {
        'name': 'p:' + name,
        'brief': brief,
        'sig': sig or 'p:%s()' % name,
        'args': args or [],
        'returns': [(typ, brief.rstrip('.').lower())],
        'example': example,
        'extra': extra,
        'notes': notes,
    }


def vec(name, brief, writable=True, notes=None):
    return {
        'name': 'p:' + name,
        'brief': brief,
        'sig': 'p:%s([x, y, z])' % name if writable else 'p:%s()' % name,
        'args': [('x, y, z', 'number', 'новые координаты; без них метод читает')] if writable else [],
        'returns': [('number, number, number', 'три компоненты вектора')],
        'notes': notes,
    }


NS = {
    'dir': 'players',
    'label': 'players',
    'title': 'players',
    'brief': 'Поиск игроков, рассылка и объект игрока.',
    'intro': """
Объект игрока привязан к слоту и живёт всё время работы сервера — методы читают
живое состояние, а не снимок. Без аргумента метод читает, с аргументом пишет.

```lua
p:health(p:health() + 25)
```

Объект защищён от записи: `p.foo = 1` бросит ошибку. Своё состояние держи в
таблице плагина, ключом — `p.id`.
""",
    'groups': [
        {
            'title': 'Пространство имён',
            'slug': 'namespace',
            'items': [
                {
                    'name': 'players.get',
                    'brief': 'Возвращает игрока по номеру слота.',
                    'sig': 'players.get(id)',
                    'args': [('id', 'number', 'номер слота, 1..32')],
                    'returns': [('player \\| nil', '`nil`, если слот пуст')],
                },
                {
                    'name': 'players.list',
                    'brief': 'Возвращает массив подключённых игроков.',
                    'sig': 'players.list([filter])',
                    'args': [('filter', 'table \\| nil', 'см. [Фильтр](#фильтр)')],
                    'returns': [('table', 'массив объектов игроков')],
                    'fields': ('Фильтр', [
                        ('alive', 'boolean', 'только живые или только мёртвые'),
                        ('team', 'string', '`CT`, `T`, `SPEC`'),
                    ]),
                    'example': """
for _, p in ipairs(players.list{ alive = true, team = "CT" }) do
	p:chat("живой контр")
end
""",
                    'notes': [('warning', 'Фильтры читают живое CS-состояние и требуют ReGameDLL.\nБез фильтра метод работает и на ванильном `mp.dll`.')],
                },
                {
                    'name': 'players.find',
                    'brief': 'Ищет игрока по слоту, userid или части ника.',
                    'sig': 'players.find(token)',
                    'args': [('token', 'string', 'см. [Форматы](#форматы)')],
                    'returns': [('player \\| nil', 'найденный игрок'),
                                ('string', 'причина, если не найден')],
                    'fields': ('Форматы', [
                        ('"3"', 'string', 'номер слота'),
                        ('"#12"', 'string', 'userid'),
                        ('"kotya"', 'string', 'часть ника, регистр не важен; совпадение должно быть одно'),
                    ]),
                    'example': """
local target, why = players.find(ctx.args[1])
if not target then
	return ctx.reply(why)
end
""",
                    'extra': 'Тот же поиск делает `opts.target` в [`cmd.add`](../cmd/add.md).',
                },
                {
                    'name': 'players.broadcast',
                    'brief': 'Приёмник «всем сразу»: только отправка сообщений.',
                    'sig': 'players.broadcast:chat(text[, opts])',
                    'args': None,
                    'example': """
players.broadcast:chat("{green}[Server]{default} раунд начался")
players.broadcast:play_sound("items/9mmclip1.wav")
""",
                    'extra': """
Понимает те же методы отправки, что и обычный игрок:
[`chat`](chat.md), [`console`](console.md), [`center`](center.md),
[`hud`](hud.md), [`dhud`](dhud.md), [`play_sound`](play_sound.md).

`play_sound` тут — один `EMIT_SOUND` от первого подключённого игрока, а не
цикл по всем: `EMIT_SOUND` и так слышен всем, у кого PAS накрывает точку
излучения, повторять его на каждого — значит дать части слушателей услышать
один и тот же клип по два-три раза подряд.
""",
                    'notes': [('warning', 'Состояния у него нет: `players.broadcast:alive()` бросает ошибку\nс объяснением. Для чтения и записи состояния перебирай\n`players.list()`.')],
                },
                {
                    'name': 'players.method',
                    'brief': 'Добавляет свой метод всем объектам игроков.',
                    'sig': 'players.method(name, fn)',
                    'args': [('name', 'string', 'имя метода'),
                             ('fn', 'function', 'получает `(self, ...)`')],
                    'example': """
players.method("playtime", function(self)
	return sessions[self.id] and sessions[self.id]:seconds() or 0
end)
""",
                    'notes': [('warning', 'Занятое имя переопределить нельзя — это то, что не даёт одному\nплагину подменить `p:health()` для всех остальных.')],
                },
            ],
        },
        {
            'title': 'Идентификация',
            'slug': 'identity',
            'items': [
                {
                    'name': 'p.id',
                    'brief': 'Номер слота игрока.',
                    'sig': 'p.id',
                    'args': None,
                    'returns': [('number', '1..32, постоянен пока игрок на сервере')],
                    'extra': 'Поле, а не метод. Годится ключом в своих таблицах.',
                },
                ro('name', 'Ник игрока.', 'string'),
                ro('ip', 'Адрес игрока вида `1.2.3.4:27005`.', 'string'),
                ro('steamid', 'SteamID игрока.', 'string',
                   extra='`STEAM_0:...` у Steam-клиента, `HLTV` у прокси, `STEAM_ID_LAN` без Steam.',
                   notes=[('warning', 'В событии `client_connect` возвращает `STEAM_ID_PENDING` —\nSteam отвечает позже. Всё, что завязано на steamid, вешай на\n[`player_authorized`](../hook/player_authorized.md).')]),
                ro('userid', 'Userid движка.', 'number', extra='`-1`, если игрока в слоте нет.'),
                ro('info', 'Читает ключ инфобуфера клиента.', 'string',
                   sig='p:info(key)',
                   args=[('key', 'string', 'имя ключа, например `_pw`')],
                   example='local password = p:info("_pw")'),
                ro('connected', 'Занят ли слот.', 'boolean',
                   extra='Единственная безопасная проверка перед обращением к состоянию из отложенного кода.'),
                ro('is_bot', 'Серверный бот (`FL_FAKECLIENT`).', 'boolean'),
                ro('is_hltv', 'HLTV-прокси (`FL_PROXY`).', 'boolean'),
            ],
        },
        {
            'title': 'Состояние',
            'slug': 'state',
            'note': 'Пишется в `entvars_t` напрямую, ReGameDLL не нужен.',
            'items': [
                rw('health', 'Здоровье игрока.', 'number',
                   example='p:health(p:health() + 25)', notes=[CONNECTED]),
                rw('armor', 'Броня игрока.', 'number'),
                rw('frags', 'Счётчик фрагов.', 'number'),
                rw('gravity', 'Множитель гравитации.', 'number',
                   extra='`1.0` — обычная, `0.5` — вдвое слабее.',
                   notes=[('note', 'Спавн сбрасывает значение. Ставь его в\n[`player_spawn`](../hook/player_spawn.md), снимай в\n[`plugin.on_unload`](../plugin/on_unload.md).')]),
                rw('maxspeed', 'Максимальная скорость движения.', 'number'),
                vec('origin', 'Позиция игрока в мире.',
                    notes=[('note', 'Запись телепортирует: проверки столкновений нет, игрока можно\nзасунуть в стену.')]),
                vec('angles', 'Поворот модели игрока.'),
                vec('velocity', 'Скорость игрока.'),
                vec('aim', 'Направление взгляда, только чтение.', writable=False),
                ro('alive', 'Жив ли игрок.', 'boolean'),
                ro('on_ground', 'Стоит ли игрок на земле.', 'boolean'),
                ro('ducking', 'Сидит ли игрок.', 'boolean'),
                rw('freeze', 'Заморозка движения (`FL_FROZEN`).', 'boolean',
                   extra='Движковая заморозка. Обычный обходной путь `p:maxspeed(1)` оставляет клиент предсказывать движение, которого сервер не даёт — отсюда рывки.'),
                rw('godmode', 'Неуязвимость.', 'boolean'),
                rw('noclip', 'Полёт сквозь стены.', 'boolean'),
                {
                    'name': 'p:trace',
                    'brief': 'Пускает луч из глаз игрока туда, куда он смотрит.',
                    'sig': 'p:trace([distance])',
                    'args': [('distance', 'number \\| nil', 'длина луча в юнитах, по умолчанию `8192`')],
                    'returns': [('table', 'что попалось, см. [Результат](#результат)')],
                    'fields': ('Результат', [
                        ('kind', 'string', '`player`, `entity`, `world`, `none`'),
                        ('player', 'player', 'только при `kind == "player"`'),
                        ('classname', 'string', 'classname цели; нет при `none`'),
                        ('x, y, z', 'number', 'точка попадания; при `none` — конец луча'),
                        ('distance', 'number', 'юнитов от глаз до точки'),
                        ('hitgroup', 'number', '`1` — голова, `0` — тело'),
                    ]),
                    'example': """
local t = p:trace()
if t.kind == "player" then
	p:chat(("%s, %d hp, %d юнитов"):format(t.player:name(), t.player:health(), t.distance))
end
""",
                },
                {
                    'name': 'p:trace_to',
                    'brief': 'Пускает луч из глаз игрока прямо к другому игроку.',
                    'sig': 'p:trace_to(other)',
                    'args': [('other', 'player', 'до кого трассировать')],
                    'returns': [('table', 'что попалось, см. [Результат](trace.md#trace-результат) — тот же формат, что у `p:trace`')],
                    'example': """
local t = p:trace_to(victim)
if t.kind == "player" and t.player.id == victim.id then
	-- прямая видимость есть, между ними ничего твёрдого
end
""",
                    'extra': '`p:trace()` отвечает «куда сейчас смотрит `p`»; этот — «есть ли что-то твёрдое между `p` и `other`». Разница важна там, где важна не текущая наводка, а сам факт видимости: разброс дроби или отдача уводят прицел в сторону от точки, куда реально попала пуля, так что трассировка по прицелу в момент попадания может соврать — трассировка прямо на цель нет.',
                    'see': [('p:trace', 'trace.md')],
                },
            ],
        },
        {
            'title': 'CS-состояние',
            'slug': 'cs-state',
            'note': 'Читает и меняет `CBasePlayer` игры.',
            'items': [
                {
                    'name': 'p:team',
                    'brief': 'Читает или меняет сторону игрока.',
                    'sig': 'p:team([team[, opts]])',
                    'args': [('team', 'string \\| nil', '`CT`, `T`, `SPEC`; без него метод читает'),
                             ('opts', 'table \\| nil', 'см. [Опции](#опции)')],
                    'returns': [('string', '`CT`, `T`, `SPEC` или `NONE`')],
                    'fields': ('Опции', [
                        ('force', 'boolean', 'игрок умирает и заходит заново, как это делает игра'),
                    ]),
                    'example': """
p:team("CT")                      -- сторона меняется, игрок остаётся жив
p:team("T", { force = true })     -- через смерть и повторный вход
""",
                    'extra': 'Вход из спектатора всегда идёт как force: мягкий путь не умеет завести зрителя в идущий раунд.',
                    'notes': [REGAMEDLL],
                    'see': [('p:spawn', 'spawn.md')],
                },
                {
                    'name': 'p:spawn',
                    'brief': 'Респаун игрока без потери денег и счёта.',
                    'sig': 'p:spawn()',
                    'args': [],
                    'extra': 'Тот же `RoundRespawn`, что игра делает между раундами. Работает и на живом, и на мёртвом. У игрока в `SPEC` или `NONE` не делает ничего.',
                    'notes': [
                        REGAMEDLL,
                        ('note', '`p:team(t, { force = true })` — не респаун: он убивает живого\nигрока, экран мигает, счёт сбрасывается.'),
                    ],
                },
                {
                    'name': 'p:money',
                    'brief': 'Читает или задаёт деньги игрока.',
                    'sig': 'p:money([amount[, opts]])',
                    'args': [('amount', 'number \\| nil', 'новая сумма; без неё метод читает'),
                             ('opts', 'table \\| nil', 'см. [Опции](#опции)')],
                    'returns': [('number', 'текущая сумма при чтении')],
                    'fields': ('Опции', [
                        ('hud', 'boolean', 'мигнуть цифрой на экране, как при подборе денег'),
                    ]),
                    'example': 'p:money(p:money() + 1000, { hud = true })',
                    'notes': [REGAMEDLL],
                },
                rw('deaths', 'Счётчик смертей.', 'number',
                   extra='Запись обновляет строку в таблице счёта.', notes=[REGAMEDLL]),
                {
                    'name': 'p:give',
                    'brief': 'Выдаёт игроку предмет по classname.',
                    'sig': 'p:give(classname)',
                    'args': [('classname', 'string', '`weapon_ak47`, `item_assaultsuit`, `weapon_hegrenade`')],
                    'example': 'p:give("weapon_ak47")',
                    'notes': [REGAMEDLL],
                },
                {
                    'name': 'p:strip',
                    'brief': 'Забирает у игрока всё оружие.',
                    'sig': 'p:strip([suit])',
                    'args': [('suit', 'boolean \\| nil', '`true` — вместе с бронежилетом')],
                    'notes': [REGAMEDLL],
                },
                ro('weapon', 'Classname оружия в руках.', 'string \\| nil', notes=[REGAMEDLL]),
                ro('weapons', 'Массив classname всего, что несёт игрок.', 'table', notes=[REGAMEDLL]),
                {
                    'name': 'p:ammo',
                    'brief': 'Читает или задаёт патроны в запасе.',
                    'sig': 'p:ammo([weapon][, value])',
                    'args': [('weapon', 'string \\| nil', 'classname; без него — оружие в руках'),
                             ('value', 'number \\| nil', 'новое количество')],
                    'returns': [('number', 'патронов в запасе')],
                    'example': """
p:ammo()                     -- запас для оружия в руках
p:ammo("weapon_ak47", 90)
""",
                    'extra': 'Запас ключуется оружием, а не типом патрона: оружие одного калибра делит запас. Запись ограничена потолком игры.',
                    'notes': [REGAMEDLL],
                },
                {
                    'name': 'p:clip',
                    'brief': 'Читает или задаёт патроны в магазине.',
                    'sig': 'p:clip([weapon][, value])',
                    'args': [('weapon', 'string \\| nil', 'classname; без него — оружие в руках'),
                             ('value', 'number \\| nil', 'новое количество')],
                    'returns': [('number \\| nil', '`nil` у ножа и гранат — магазина у них нет')],
                    'extra': 'Цифра доходит до HUD только для оружия в руках; при переключении она встаёт правильная.',
                    'notes': [REGAMEDLL],
                },
                {
                    'name': 'p:drop',
                    'brief': 'Выбрасывает оружие на пол.',
                    'sig': 'p:drop([weapon])',
                    'args': [('weapon', 'string \\| nil', 'classname; без него — то, что в руках')],
                    'notes': [REGAMEDLL],
                },
            ],
        },
        {
            'title': 'Сообщения',
            'slug': 'messages',
            'note': 'Те же методы есть у [`players.broadcast`](broadcast.md).',
            'items': [
                {
                    'name': 'p:chat',
                    'brief': 'Отправляет строку в чат.',
                    'sig': 'p:chat(text[, opts])',
                    'args': [('text', 'string', 'текст; понимает теги цвета'),
                             ('opts', 'table \\| nil', 'см. [Опции](#опции)')],
                    'fields': ('Опции', [
                        ('from', 'player', 'чей цвет команды подставит `{team}`'),
                    ]),
                    'example': 'p:chat("{green}[Server]{default} Привет, {team}" .. p:name())',
                    'extra': 'Теги цвета — в [`ui`](../ui/color.md). Лимит строки — 188 байт.',
                },
                {
                    'name': 'p:console',
                    'brief': 'Пишет строку в консоль игрока.',
                    'sig': 'p:console(text)',
                    'args': [('text', 'string', 'до 254 байт')],
                },
                {
                    'name': 'p:center',
                    'brief': 'Показывает текст по центру экрана.',
                    'sig': 'p:center(text)',
                    'args': [('text', 'string', 'до 254 байт')],
                },
                {
                    'name': 'p:hud',
                    'brief': 'Рисует текст на HUD с позицией, цветом и таймингами.',
                    'sig': 'p:hud(text[, opts])',
                    'args': [('text', 'string', 'до 511 байт'),
                             ('opts', 'table \\| nil', 'см. [Опции](#опции)')],
                    'fields': ('Опции', [
                        ('x, y', 'number', 'позиция 0..1; `-1` — по центру оси. По умолчанию `-1`, `0.35`'),
                        ('color', 'string \\| table', 'имя, `"#rrggbb"` или `{ r, g, b[, a] }`'),
                        ('color2', 'string \\| table', 'вторая точка градиента для `effect = 2`'),
                        ('effect', 'number', '`0` fade, `1` flicker, `2` typewriter'),
                        ('fadein', 'number', 'по умолчанию `0.1`'),
                        ('fadeout', 'number', 'по умолчанию `0.2`'),
                        ('hold', 'number', 'секунд на экране, по умолчанию `5`'),
                        ('fxtime', 'number', 'по умолчанию `0.25`'),
                        ('channel', 'number', '`0..3`, разные каналы не затирают друг друга'),
                    ]),
                    'example': """
p:hud("Раунд начался", { y = 0.3, color = "green", hold = 3 })
""",
                    'see': [('ui.color', '../ui/color.md')],
                },
                {
                    'name': 'p:dhud',
                    'brief': 'То же через `SVC_DIRECTOR` — directed HUD.',
                    'sig': 'p:dhud(text[, opts])',
                    'args': [('text', 'string', 'до 127 байт'),
                             ('opts', 'table \\| nil', 'как у [`p:hud`](hud.md), кроме `channel` и `color2`')],
                    'extra': 'Держит до 8 сообщений одновременно.',
                },
                {
                    'name': 'p:motd',
                    'brief': 'Открывает игроку панель MOTD.',
                    'sig': 'p:motd(text[, opts])',
                    'args': [('text', 'string', 'до 1536 байт после обёртки'),
                             ('opts', 'table \\| nil', 'см. [Опции](#опции)')],
                    'fields': ('Опции', [
                        ('raw', 'boolean', 'слать как есть, разметку пишет плагин'),
                    ]),
                    'example': """
p:motd(table.concat(lines, "\\n"))

p:motd([[<meta charset="utf-8">
<body style="background:#111;color:#ddd">
  <h3>Магазин</h3>
  <table><tr><td>AK-47</td><td>2500</td></tr></table>
</body>]], { raw = true })
""",
                    'extra': """
Единственная многострочная поверхность в игре: меню — это девять клавиш, а HUD —
одна строка. Каталог магазина, топ-10 и правила сервера показывают здесь.

### Панель — это HTML

У клиентов, которые ещё запускают, MOTD рисует HTML-вьюха, а не текстовое поле.
Из этого следуют две вещи, знать которые вызывающему незачем, поэтому по
умолчанию модуль разбирается с ними сам:

| | |
|---|---|
| перевод строки | в HTML это пробел, поэтому `\\n` заменяется на `<br>` |
| кодировка | без объявления движок читает UTF-8 как системную кодовую страницу и показывает мусор, поэтому в начало идёт `<meta charset>` |
| `<`, `>`, `&` | экранируются: ник с угловой скобкой не должен становиться разметкой |
| шрифт | моноширинный — в панели показывают таблицы цен и топы, пропорциональный их разъезжает |

`raw = true` отключает всю обёртку: текст уходит байт в байт, `<meta charset>`
пишешь сам. Это режим для оформления — фон, цвета, таблицы.

Текст уходит кусками по 60 байт, клиент собирает их обратно; снаружи это один
вызов.
""",
                    'notes': [
                        ('note', 'Заголовок самого окна задаёт клиент: в протоколе поля для него\nнет.'),
                        ('warning', 'Длиннее 1536 байт обрезается, и считается это уже вместе с\nобёрткой. При `cslua_cp1251 1` кириллица занимает вдвое меньше,\nчем в UTF-8.'),
                    ],
                    'see': [('p:center', 'center.md'), ('menu.new', '../menu/new.md')],
                },
                {
                    'name': 'p:play_sound',
                    'brief': 'Проигрывает игроку звук.',
                    'sig': 'p:play_sound(path[, opts])',
                    'args': [('path', 'string', 'путь от `cstrike/sound/`, обязан быть предкэширован'),
                             ('opts', 'table \\| nil', 'см. [Опции](#опции)')],
                    'fields': ('Опции', [
                        ('volume', 'number', '0..1, по умолчанию `1.0`'),
                        ('attenuation', 'number', 'затухание с расстоянием; `0` — одинаково слышно везде. По умолчанию `0.8`'),
                        ('channel', 'number', 'канал звука, по умолчанию `0`'),
                        ('pitch', 'number', 'тон, по умолчанию `100`'),
                    ]),
                    'example': 'p:play_sound("items/9mmclip1.wav", { attenuation = 0 })',
                    'extra': 'Это `EMIT_SOUND`, движковый вызов уровня всего сервера: он эмитится из этого игрока как из точки в мире, и слышит его любой, чей PAS накрывает эту точку — не обязательно только `p`. `attenuation = 0` не делает звук приватным, только одинаково громким всем, кто его услышал. Для звука, который должен услышать ровно этот игрок и никто больше, используй [`p:exec`](exec.md) с консольной командой `spk`: `p:exec(\'spk "путь/без/точки/wav"\')` — это настоящая адресная команда клиенту, а не излучение в мир.',
                    'see': [('res.sound', '../res/sound.md'), ('p:exec', 'exec.md')],
                },
            ],
        },
        {
            'title': 'Админские действия',
            'slug': 'admin',
            'items': [
                {
                    'name': 'p:kick',
                    'brief': 'Отключает игрока от сервера.',
                    'sig': 'p:kick([reason])',
                    'args': [('reason', 'string \\| nil', 'показывается игроку в консоли перед отключением')],
                    'extra': 'Идёт через консольную команду движка и адресуется по userid: ник может содержать пробелы и кавычки.',
                },
                {
                    'name': 'p:ban',
                    'brief': 'Банит игрока.',
                    'sig': 'p:ban(minutes[, reason])',
                    'args': [('minutes', 'number', 'срок в минутах; `0` — навсегда'),
                             ('reason', 'string \\| nil', 'показывается игроку')],
                    'extra': 'Бан ложится в тот же список, которым управляет админ сервера, и сохраняется через `writeid`.',
                },
                {
                    'name': 'p:slay',
                    'brief': 'Убивает игрока.',
                    'sig': 'p:slay()',
                    'args': [],
                    'extra': 'Через `TakeDamage` от мира, поэтому отрабатывает всё как при обычной смерти: дроп оружия, сообщение, проверка конца раунда.',
                },
                {
                    'name': 'p:slap',
                    'brief': 'Наносит урон и толкает игрока в случайную сторону.',
                    'sig': 'p:slap([damage])',
                    'args': [('damage', 'number \\| nil', 'по умолчанию `1`; `0` — только толчок')],
                },
                {
                    'name': 'p:exec',
                    'brief': 'Выполняет консольную команду на машине игрока.',
                    'sig': 'p:exec(command)',
                    'args': [('command', 'string', 'до 120 символов')],
                },
            ],
        },
        {
            'title': 'Права',
            'slug': 'access',
            'note': 'Добавлены core-слоем, подробности — в [`access`](../access/index.md).',
            'items': [
                ro('can', 'Есть ли у игрока право.', 'boolean',
                   sig='p:can(node)', args=[('node', 'string', 'нода вида `admin.kick`')],
                   example='if p:can("shop.vip.buy") then ... end'),
                ro('groups', 'Массив групп игрока.', 'table'),
                ro('group', 'Состоит ли игрок в группе, с учётом наследования.', 'boolean',
                   sig='p:group(name)', args=[('name', 'string', 'имя группы')]),
                ro('weight', 'Вес игрока для иммунитета.', 'number'),
                ro('outranks', 'Старше ли игрок другого по весу.', 'boolean',
                   sig='p:outranks(other)', args=[('other', 'player', 'кого сравниваем')],
                   extra='Требует **строго большего** веса, поэтому два админа одного ранга друг друга не тронут.'),
            ],
        },
    ],
}
