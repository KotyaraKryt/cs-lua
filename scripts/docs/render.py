"""Рендер справочника: одна страница на функцию.

Данные лежат в scripts/docs/api/*.py, каждый модуль описывает одно
пространство имён. Здесь только шаблон страницы и сборка sidebar.

    python scripts/docs/gen.py
"""
import io
import os


def _table(header, rows):
    if not rows:
        return []
    out = ['| ' + ' | '.join(header) + ' |',
           '|' + '---|' * len(header)]
    for row in rows:
        out.append('| ' + ' | '.join(str(c) for c in row) + ' |')
    out.append('')
    return out


def _notes(notes):
    out = []
    for kind, text in notes or []:
        out.append('> [!%s]' % kind.upper())
        for line in text.strip().split('\n'):
            out.append('> ' + line if line else '>')
        out.append('')
    return out


def render_function(fn):
    """fn: dict(name, brief, sig, args, returns, example, notes, extra)"""
    body = ['---',
            'title: %s' % fn['name'],
            'description: "%s"' % fn['brief'].replace('"', "'").rstrip('.'),
            '---',
            '',
            '# %s' % fn['name'],
            '',
            fn['brief'],
            '',
            '```lua',
            fn['sig'],
            '```',
            '']

    if fn.get('args'):
        body += ['## Аргументы', '']
        body += _table(['#', 'имя', 'тип', ''],
                       [(i + 1, '`%s`' % a[0], a[1], a[2])
                        for i, a in enumerate(fn['args'])])

    if fn.get('returns'):
        body += ['## Возвращает', '']
        body += _table(['тип', ''], [('`%s`' % r[0], r[1]) for r in fn['returns']])
    elif fn.get('args') is not None:
        body += ['## Возвращает', '', 'Ничего.', '']

    if fn.get('fields'):
        title, rows = fn['fields']
        body += ['## %s' % title, '']
        body += _table(['поле', 'тип', ''],
                       [('`%s`' % f[0], f[1], f[2]) for f in rows])

    if fn.get('example'):
        body += ['## Пример', '', '```lua', fn['example'].strip(), '```', '']

    if fn.get('extra'):
        body += [fn['extra'].strip(), '']

    if fn.get('notes'):
        body += _notes(fn['notes'])

    if fn.get('see'):
        body += ['## Смотри также', '']
        for label, link in fn['see']:
            body.append('- [%s](%s)' % (label, link))
        body.append('')

    return '\n'.join(body).rstrip() + '\n'


def render_index(ns):
    """Обзорная страница пространства имён: список того, что внутри."""
    body = ['---',
            'title: %s' % ns['title'],
            'description: "%s"' % ns['brief'].replace('"', "'").rstrip('.'),
            '---',
            '',
            '# %s' % ns['title'],
            '',
            ns['brief'],
            '']

    if ns.get('intro'):
        body += [ns['intro'].strip(), '']

    for group in ns['groups']:
        if group.get('title'):
            body += ['## %s' % group['title'], '']
        if group.get('note'):
            body += [group['note'].strip(), '']
        body += _table(['', ''],
                       [('[`%s`](%s.md)' % (f['name'], _slug(f)), f['brief'].rstrip('.'))
                        for f in group['items']])

    return '\n'.join(body).rstrip() + '\n'


def _slug(fn):
    """timer.after -> after; p:health -> health.

    Явный slug в описании перебивает: имя не всегда даёт уникальный хвост -
    db:query и st:query оба кончаются на query.
    """
    if fn.get('slug'):
        return fn['slug']

    name = fn['name']
    tail = name.split('.')[-1].split(':')[-1]
    return tail.replace('{}', '').strip() or 'index'


def write(root, ns):
    """Пишет docs/api/<dir>/ и возвращает описание для sidebar."""
    d = os.path.join(root, ns['dir'])
    os.makedirs(d, exist_ok=True)

    io.open(os.path.join(d, 'index.md'), 'w', encoding='utf-8', newline='').write(
        render_index(ns))

    # Два одинаковых slug молча затирают друг друга: одна страница пропадает,
    # а обе ссылки в меню ведут на оставшуюся. Docusaurus вдобавок считает
    # <папка>/<папка>.md вторым индексом папки, так что такой slug тоже
    # схлопывается с index. Ловим здесь, а не глазами на сайте.
    seen = {}
    for group in ns['groups']:
        for fn in group['items']:
            slug = _slug(fn)

            if slug == ns['dir']:
                raise SystemExit(
                    "%s: slug '%s' совпадает с именем каталога - Docusaurus "
                    "сделает из страницы второй index. Задай slug явно."
                    % (fn['name'], slug))

            if slug in seen:
                raise SystemExit(
                    "%s и %s дают один slug '%s'. Задай slug явно."
                    % (seen[slug], fn['name'], slug))

            seen[slug] = fn['name']

    items = []
    for group in ns['groups']:
        # Раздел без функций существует только ради текста на обзорной
        # странице - в меню ему соответствовать нечему.
        if not group['items']:
            continue

        entries = []
        for fn in group['items']:
            slug = _slug(fn)
            io.open(os.path.join(d, slug + '.md'), 'w', encoding='utf-8', newline='').write(
                render_function(fn))
            entries.append({'type': 'doc',
                            'id': 'api/%s/%s' % (ns['dir'], slug),
                            'label': fn['name']})

        if group.get('title') and len(ns['groups']) > 1:
            items.append({'type': 'category', 'label': group['title'],
                          'collapsed': True, 'items': entries})
        else:
            items += entries

    return {'type': 'category',
            'label': ns['label'],
            'collapsed': True,
            'link': {'type': 'doc', 'id': 'api/%s/index' % ns['dir']},
            'items': items}


def _ts(value, indent=8):
    """Питоновские dict/list -> литерал TypeScript."""
    pad = ' ' * indent
    if isinstance(value, dict):
        inner = ', '.join('%s: %s' % (k, _ts(v, indent)) for k, v in value.items())
        return '{%s}' % inner
    if isinstance(value, list):
        rows = ',\n'.join(pad + _ts(v, indent + 2) for v in value)
        return '[\n%s,\n%s]' % (rows, ' ' * (indent - 2))
    if isinstance(value, bool):
        return 'true' if value else 'false'
    return "'%s'" % value


def sidebar(entries):
    return _ts(entries, 8)
