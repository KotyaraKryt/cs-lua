# -*- coding: utf-8 -*-

NS = {
    'dir': 'vec',
    'label': 'vec',
    'title': 'vec',
    'brief': 'Векторная математика: расстояние, длина, нормализация, углы.',
    'intro': """
Чистая математика без обращения к движку или сущностям — векторы теми же
тройками чисел, что и везде в модуле (`p:origin()` возвращает `x, y, z`,
а не таблицу, и `vec.*` принимает их так же).
""",
    'groups': [
        {
            'items': [
                {
                    'name': 'vec.distance',
                    'brief': 'Расстояние между двумя точками.',
                    'sig': 'vec.distance(x1, y1, z1, x2, y2, z2)',
                    'args': [('x1, y1, z1', 'number', 'первая точка'), ('x2, y2, z2', 'number', 'вторая точка')],
                    'returns': [('number', 'расстояние в юнитах')],
                    'example': """
local x1, y1, z1 = p:origin()
local x2, y2, z2 = other:origin()
local d = vec.distance(x1, y1, z1, x2, y2, z2)
""",
                    'notes': [('note', '`p:origin()` — не последний аргумент вызова, поэтому Lua обрежет его до\nодного значения, если передать его напрямую вторым в списке аргументов:\nразложи обе точки в переменные, как в примере выше.')],
                },
                {
                    'name': 'vec.length',
                    'brief': 'Длина вектора.',
                    'sig': 'vec.length(x, y, z)',
                    'args': [('x, y, z', 'number', 'вектор')],
                    'returns': [('number', 'длина')],
                },
                {
                    'name': 'vec.normalize',
                    'brief': 'Вектор единичной длины в том же направлении.',
                    'sig': 'vec.normalize(x, y, z)',
                    'args': [('x, y, z', 'number', 'вектор')],
                    'returns': [('number, number, number', 'единичный вектор')],
                    'extra': 'Нулевой вектор нормализуется сам в себя — деления на ноль не будет.',
                },
                {
                    'name': 'vec.to_angle',
                    'brief': 'Углы, которые смотрят вдоль вектора.',
                    'sig': 'vec.to_angle(x, y, z)',
                    'args': [('x, y, z', 'number', 'вектор направления')],
                    'returns': [('number, number, number', '`pitch, yaw, roll`')],
                    'extra': 'Обратная операция для [vec.angle_vector](angle_vector.md).',
                    'example': """
local px, py, pz = p:origin()
local pitch, yaw, roll = vec.to_angle(target.x - px, target.y - py, target.z - pz)
p:angles(pitch, yaw, roll)   -- модель игрока разворачивается на точку
""",
                },
                {
                    'name': 'vec.angle_vector',
                    'brief': 'Единичный вектор направления взгляда для заданных углов.',
                    'sig': 'vec.angle_vector(pitch, yaw, roll)',
                    'args': [('pitch, yaw, roll', 'number', 'углы')],
                    'returns': [('number, number, number', 'единичный вектор вперёд')],
                    'extra': 'Тот же движковый вызов, что `p:trace()` использует для направления взгляда\nигрока (`MAKE_VECTORS`), только без привязки к конкретному игроку — годится\nдля сущностей и вычисленных углов.',
                    'example': """
local fx, fy, fz = vec.angle_vector(p:pev("v_angle"))
""",
                    'see': [('vec.to_angle', 'to_angle.md')],
                },
            ],
        },
    ],
}
