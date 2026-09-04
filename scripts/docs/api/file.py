# -*- coding: utf-8 -*-

NAME_ARG = ('name', 'string', 'имя файла в каталоге плагина (см. песочницу выше)')

NS = {
    'dir': 'file',
    'label': 'file',
    'title': 'file',
    'brief': 'Сырое чтение и запись файлов в собственном каталоге плагина.',
    'intro': """
`file` читает и пишет обычные файлы — байтовую строку целиком, без разбора
формата. Для структурированных данных (таблиц) есть `datafile` и `db`; `file`
для всего остального: csv-экспорт, готовый json, любой текстовый или бинарный
формат, который плагин собирает сам.

```lua
file.write("report.csv", "name,score\\nplayer1,10\\n")
local content = file.read("report.csv")
```

## Песочница

Все имена — только внутри `plugin.data_dir()`, той же папки, куда пишут
`db.open()` и `datafile`. Подпапок нет — `name` не может содержать `/`, `\\`
или что-то похожее на путь.

Имя — это `stem` или `stem.ext`:
- `stem`: буквы, цифры, `_`, `-`
- `.ext` — необязательно, ровно одна точка на всё имя, только буквы и цифры
- максимум 64 символа

Валидно: `report.csv`, `export_2026.json`, `state`.
Невалидно: `../evil.txt`, `sub/dir.txt`, `a.b.c`, `.hidden`, `hidden.`.

## Предел размера

Одно чтение или одна запись/дозапись — не больше 4 МиБ. Больше — `nil` и
причина, ещё до того, как файл открыт: частично записанного файла при отказе
не остаётся.
""",
    'groups': [
        {
            'items': [
                {
                    'name': 'file.read',
                    'brief': 'Читает содержимое файла целиком.',
                    'sig': 'file.read(name)',
                    'args': [NAME_ARG],
                    'returns': [('string \\| nil', 'содержимое файла'),
                                ('string', 'причина, если файл не открылся, его нет или он больше 4 МиБ')],
                    'example': """
local content, err = file.read("report.csv")
if not content then
  print("не смог прочитать: " .. err)
  return
end
""",
                    'notes': [('warning', 'Отсутствие файла — тоже ошибка здесь (`nil, "cannot open ..."`), в отличие от\n`file.size`, где отсутствие файла — просто `nil` без причины.')],
                    'see': [('file.size', 'size.md')],
                },
                {
                    'name': 'file.write',
                    'brief': 'Перезаписывает файл целиком (или создаёт его).',
                    'sig': 'file.write(name, content)',
                    'args': [NAME_ARG, ('content', 'string', 'что записать, до 4 МиБ')],
                    'returns': [('boolean \\| nil', '`true` при успехе'),
                                ('string', 'причина при ошибке')],
                    'example': """
local ok, err = file.write("report.csv", "name,score\\n")
if not ok then
  print("не смог записать: " .. err)
end
""",
                    'extra': 'Для дозаписи в конец существующего файла — `file.append`.',
                    'see': [('file.append', 'append.md')],
                },
                {
                    'name': 'file.append',
                    'brief': 'Дописывает в конец файла, не трогая то, что уже было (или создаёт его).',
                    'sig': 'file.append(name, content)',
                    'args': [NAME_ARG, ('content', 'string', 'что дописать, до 4 МиБ за один вызов')],
                    'returns': [('boolean \\| nil', '`true` при успехе'),
                                ('string', 'причина при ошибке')],
                    'example': """
hook.add("player:death", "myplugin.kills_log", function(e)
  file.append("kills.csv", ("%s,%s\\n"):format(e.attacker and e.attacker:name() or "world", e.player:name()))
end)
""",
                    'notes': [('warning', 'Предел 4 МиБ — на один вызов, не на итоговый размер файла: у `file.append`\nнет ограничения на то, насколько большим станет файл со временем. Для\nсамоограничивающегося лога с ротацией по дням — [`log.write`](../log/index.md).')],
                },
                {
                    'name': 'file.exists',
                    'brief': 'Проверяет, есть ли файл в каталоге плагина.',
                    'sig': 'file.exists(name)',
                    'args': [NAME_ARG],
                    'returns': [('boolean', '`true`/`false`; неверное имя или отсутствие каталога плагина — тоже `false`, не ошибка')],
                    'example': """
if not file.exists("config.json") then
  file.write("config.json", "{}")
end
""",
                },
                {
                    'name': 'file.remove',
                    'brief': 'Удаляет файл из каталога плагина.',
                    'sig': 'file.remove(name)',
                    'args': [NAME_ARG],
                    'returns': [('boolean \\| nil', '`true` при успехе'),
                                ('string', 'причина при ошибке, в том числе если файла и не было')],
                    'example': 'local ok, err = file.remove("tmp.csv")',
                },
                {
                    'name': 'file.size',
                    'brief': 'Возвращает размер файла в байтах.',
                    'sig': 'file.size(name)',
                    'args': [NAME_ARG],
                    'returns': [('number \\| nil', 'размер в байтах; `nil` без причины, если файла нет, имя не годится или нет каталога плагина')],
                    'example': """
local size = file.size("report.csv")
if size and size > 1024 * 1024 then
  file.remove("report.csv")
end
""",
                    'see': [('file.read', 'read.md')],
                },
                {
                    'name': 'file.list',
                    'brief': 'Возвращает имена всех файлов верхнего уровня в каталоге плагина.',
                    'sig': 'file.list()',
                    'args': [],
                    'returns': [('table', 'массив имён файлов; пустой, если каталога плагина ещё нет')],
                    'example': """
for _, name in ipairs(file.list()) do
  print(name)
end
""",
                    'notes': [('warning', 'Подпапка `logs/`, которую создаёт [`log.write`](../log/index.md), в список не\nпопадает — это отдельное хранилище, не файлы плагина.')],
                },
            ],
        },
    ],
}
