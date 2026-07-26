# -*- coding: utf-8 -*-

NO_MAP = ('warning', 'Пока карта не загружена, сущностей не существует. Тело плагина\nвыполняется задолго до неё, и здесь это ошибка. Создавай сущности\nиз события: `round_start`, `player_spawn`, таймер, команда.')
STALE = ('warning', 'Объект от удалённой сущности — или переживший `changelevel` —\nбросает `entity #N is gone`. Проверяй [`e:valid()`](valid.md).')

NS = {
    'dir': 'ents',
    'label': 'ents',
    'title': 'ents',
    'brief': 'Сущности: дропы, точки интереса, зоны, маркеры.',
    'intro': """
ReGameDLL не нужен — слой работает и на ванильном `mp.dll`.

Объект хранит индекс edict'а и его serial number. Движок переиспользует индексы,
поэтому каждый метод сверяет serial и при несовпадении бросает ошибку, а не
пишет в чужую сущность.

Объект защищён от записи: `e.foo = 1` бросит ошибку. Своё состояние держи в
своей таблице, ключом — `e.index`.
""",
    'groups': [
        {
            'title': 'Пространство имён',
            'items': [
                {
                    'name': 'ents.create',
                    'brief': 'Создаёт сущность по classname.',
                    'sig': 'ents.create(classname)',
                    'args': [('classname', 'string', '`info_target`, `func_door`, …')],
                    'returns': [('entity \\| nil', 'новая сущность'),
                                ('string', 'причина, если игра не знает classname')],
                    'example': """
hook.add("round_start", "drops.spawn", function()
	local e = ents.create("info_target")
	e:model("models/w_ak47.mdl")
	e:origin(x, y, z)
	e:spawn()
end)
""",
                    'extra': """
Возвращает голый edict: игра ещё не превратила его ни во что. Сначала модель и
позиция, потом [`e:spawn()`](spawn.md) — `Spawn` читает уже выставленные поля. В
обратном порядке получится сущность, которую игра игнорирует.

Модель обязана быть предкэширована через [`res.model`](../res/model.md), а это
делается в теле плагина.
""",
                    'notes': [NO_MAP],
                },
                {
                    'name': 'ents.find',
                    'brief': 'Находит все сущности с заданным classname.',
                    'sig': 'ents.find(classname)',
                    'args': [('classname', 'string', 'что искать')],
                    'returns': [('table', 'массив сущностей, пустой если ничего нет')],
                    'notes': [NO_MAP],
                },
                {
                    'name': 'ents.in_sphere',
                    'brief': 'Находит все сущности в радиусе от точки.',
                    'sig': 'ents.in_sphere(x, y, z, radius)',
                    'args': [('x, y, z', 'number', 'центр'),
                             ('radius', 'number', 'радиус в юнитах')],
                    'returns': [('table', 'массив сущностей, порядок произвольный')],
                    'extra': 'Из этого делается зона: объём без собственной сущности-триггера.',
                    'notes': [NO_MAP],
                },
            ],
        },
        {
            'title': 'Объект сущности',
            'items': [
                {
                    'name': 'e.index',
                    'brief': 'Индекс edict\'а.',
                    'sig': 'e.index',
                    'args': None,
                    'returns': [('number', 'индекс в таблице edict\'ов')],
                    'extra': 'Поле, а не метод. Годится ключом в своих таблицах.',
                },
                {
                    'name': 'e:origin',
                    'brief': 'Читает или задаёт позицию сущности.',
                    'sig': 'e:origin([x, y, z])',
                    'args': [('x, y, z', 'number', 'новая позиция; без них метод читает')],
                    'returns': [('number, number, number', 'координаты')],
                    'extra': 'Запись идёт через движок и перелинковывает сущность в мире; присваивание `pev->origin` напрямую оставило бы её сталкиваться там, где она была.',
                    'notes': [STALE],
                },
                {
                    'name': 'e:angles',
                    'brief': 'Читает или задаёт поворот сущности.',
                    'sig': 'e:angles([x, y, z])',
                    'args': [('x, y, z', 'number', 'новые углы; без них метод читает')],
                    'returns': [('number, number, number', 'углы')],
                    'notes': [STALE],
                },
                {
                    'name': 'e:model',
                    'brief': 'Читает или задаёт модель сущности.',
                    'sig': 'e:model([path])',
                    'args': [('path', 'string \\| nil', 'путь к модели; без него метод читает')],
                    'returns': [('string', 'текущая модель')],
                    'notes': [('warning', 'Модель обязана быть предкэширована через\n[`res.model`](../res/model.md). Не предкэшированная стоит серверу\nошибки, а сущность рисуется ничем.'), STALE],
                },
                {
                    'name': 'e:classname',
                    'brief': 'Classname сущности.',
                    'sig': 'e:classname()',
                    'args': [],
                    'returns': [('string', 'classname')],
                    'notes': [STALE],
                },
                {
                    'name': 'e:valid',
                    'brief': 'Жива ли сущность.',
                    'sig': 'e:valid()',
                    'args': [],
                    'returns': [('boolean', '`false`, если сущность удалена или осталась с прошлой карты')],
                    'extra': 'Единственный метод, который отвечает, а не бросает ошибку. Всё, что переживает раунд или смену карты, проверяй им.',
                    'example': """
if e:valid() then
	e:remove()
end
""",
                },
                {
                    'name': 'e:spawn',
                    'brief': 'Запускает `Spawn` сущности.',
                    'sig': 'e:spawn()',
                    'args': [],
                    'extra': 'Именно это превращает голый edict в объект с хитбоксом и think-функцией. Вызывать после того, как выставлены модель и позиция.',
                    'notes': [STALE],
                },
                {
                    'name': 'e:remove',
                    'brief': 'Убирает сущность из мира.',
                    'sig': 'e:remove()',
                    'args': [],
                    'extra': 'На уже удалённой сущности ошибкой не является и ничего не делает.',
                },
            ],
        },
        {
            'title': 'Границы слоя',
            'note': """
Слой не даёт `solid`, `movetype`, рендер-флаги и keyvalue. Сущность-маркер и
сущность-триггер он создаёт, физику предмета — нет.
""",
            'items': [],
        },
    ],
}
