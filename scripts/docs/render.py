"""Рендер справочника: страница на группу функций (в духе Discord Docs), а не
на функцию - меньше кликов в дереве, TOC страницы = список вызовов на ней.

Данные лежат в scripts/docs/api/*.py, каждый модуль описывает одно
пространство имён. Здесь только шаблон страницы и сборка sidebar.

    python scripts/docs/gen.py
"""
import io
import os
import re


def _mdx(text):
    """Экранирует то, что MDX иначе съест.

    Фигурные скобки в MDX - это JSX-выражение, поэтому заголовок "plugin{}"
    отрисовывался как "plugin": пустое выражение подставляет пустоту. В
    таблицах и ссылках имена обёрнуты в обратные кавычки и попадают в code-span,
    там экранировать нечего - страдает только голый текст заголовка.
    """
    return text.replace('{', '\\{').replace('}', '\\}')


def _table(header, rows):
    if not rows:
        return []
    out = ['| ' + ' | '.join(header) + ' |',
           '|' + '---|' * len(header)]
    for row in rows:
        out.append('| ' + ' | '.join(str(c) for c in row) + ' |')
    out.append('')
    return out


_ADMONITION_TAG = {'warning': 'Warning', 'note': 'Note'}


def _notes(notes):
    out = []
    for kind, text in notes or []:
        tag = _ADMONITION_TAG[kind]
        out.append('<%s>' % tag)
        out.append(text.strip())
        out.append('</%s>' % tag)
        out.append('')
    return out


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


def _group_slug(group):
    """Явный slug группы - имя её файла.

    Тайтл по-русски, из него URL не вывести однозначно (и не нужно -
    опечатка в транслите хуже явного имени).
    """
    if not group.get('slug'):
        raise SystemExit(
            "группа '%s' используется как отдельная страница - нужен явный "
            "'slug'." % group.get('title'))
    return group['slug']


def render_function_block(fn):
    """Функция как раздел общей страницы.

    ## - её имя (плюс явный якорь, автогенерируемый id иначе делает из
    'p:health' нечитаемый 'phealth'), ### - Аргументы/Возвращает/Пример.
    """
    slug = _slug(fn)
    body = ['## %s {#%s}' % (_mdx(fn['name']), slug),
            '',
            _mdx(fn['brief']),
            '',
            '```lua',
            fn['sig'],
            '```',
            '']

    if fn.get('args'):
        body += ['### Аргументы', '']
        body += _table(['#', 'имя', 'тип', ''],
                       [(i + 1, '`%s`' % a[0], a[1], a[2])
                        for i, a in enumerate(fn['args'])])

    if fn.get('returns'):
        body += ['### Возвращает', '']
        body += _table(['тип', ''], [('`%s`' % r[0], r[1]) for r in fn['returns']])
    elif fn.get('args') is not None:
        body += ['### Возвращает', '', 'Ничего.', '']

    # 'Опции'/'Результат'/... самореференсны: описание аргумента ссылается на
    # свою же таблицу полей через голый '(#опции)' - работало, пока функция
    # была отдельной страницей. Теперь на одной странице таких заголовков
    # может быть несколько с одним и тем же текстом ('Опции' у mysql.connect
    # и у ui.color сразу не столкнутся, а вот у пары функций одной группы -
    # запросто), поэтому у заголовка - явный id с привязкой к функции, а её
    # же собственные "голые" ссылки на него переписываются следом.
    naive_anchor = None
    real_anchor = None
    if fn.get('fields'):
        title, rows = fn['fields']
        naive_anchor = title.lower()
        real_anchor = '%s-%s' % (slug, naive_anchor)
        body += ['### %s {#%s}' % (title, real_anchor), '']
        body += _table(['поле', 'тип', ''],
                       [('`%s`' % f[0], f[1], f[2]) for f in rows])

    if fn.get('example'):
        body += ['### Пример', '', '```lua', fn['example'].strip(), '```', '']

    if fn.get('extra'):
        body += [fn['extra'].strip(), '']

    if fn.get('notes'):
        body += _notes(fn['notes'])

    if fn.get('see'):
        body += ['### Смотри также', '']
        for label, link in fn['see']:
            body.append('- [%s](%s)' % (label, link))
        body.append('')

    if naive_anchor:
        text = '\n'.join(body).replace('(#%s)' % naive_anchor, '(#%s)' % real_anchor)
        body = text.split('\n')

    return body


def render_group_page(ns, group):
    """Отдельная страница на группу: несколько функций одним документом с

    якорями - страница ресурса, как у Discord, а не страница на вызов.
    """
    description = (group.get('note') or ns['brief']).strip().split('\n')[0]
    body = ['---',
            'title: %s — %s' % (ns['title'], group['title']),
            'description: "%s"' % description.replace('"', "'").rstrip('.'),
            '---',
            '',
            '# %s' % _mdx(group['title']),
            '']

    if group.get('note'):
        body += [group['note'].strip(), '']

    for fn in group['items']:
        body += render_function_block(fn)

    return '\n'.join(body).rstrip() + '\n'


def render_index(ns):
    """Обзорная страница пространства имён.

    Если в нём реально одна группа функций - весь справочник целиком уходит
    сюда (никого не заставляем открывать вторую страницу ради трёх функций).
    Если групп несколько - здесь только интро и таблица-переход в каждую
    группу; сама группа - на своей странице (render_group_page).
    """
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

    non_empty = [g for g in ns['groups'] if g['items']]

    if len(non_empty) <= 1:
        for g in ns['groups']:
            if g.get('title'):
                body += ['## %s' % g['title'], '']
            if g.get('note'):
                body += [g['note'].strip(), '']
            for fn in g['items']:
                body += render_function_block(fn)
    else:
        for group in ns['groups']:
            # Раздел без функций - это чистая проза (например, про константы
            # или границы слоя), но title/note всё равно нужны на обзорной
            # странице: у него просто нет своей страницы и нечего сводить в
            # таблицу.
            if group.get('title'):
                body += ['## %s' % group['title'], '']
            if group.get('note'):
                body += [group['note'].strip(), '']
            if group['items']:
                page_slug = _group_slug(group)
                body += _table(['', ''],
                               [('[`%s`](%s.md#%s)' % (f['name'], page_slug, _slug(f)),
                                 f['brief'].rstrip('.'))
                                for f in group['items']])

    return '\n'.join(body).rstrip() + '\n'


def render_all(namespaces, registry):
    """Плоский список всего API на одной странице.

    Справочник разложен по пространствам имён, и это правильно для чтения, но
    мешает, когда помнишь только имя вызова. Здесь всё подряд, отсортировано:
    поиск находит эту страницу целиком, а Ctrl+F по ней работает без поиска
    вообще.
    """
    rows = []
    for ns in namespaces:
        for group in ns['groups']:
            for fn in group['items']:
                slug = _slug(fn)
                page_slug, anchor = registry[(ns['dir'], slug)]
                rows.append((
                    fn['name'],
                    '%s/%s.md#%s' % (ns['dir'], page_slug, anchor),
                    ns['label'],
                    fn['brief'].rstrip('.'),
                ))

    # По имени без префикса: p:health и players.get стоят под h и g, то есть
    # там, где их будут искать глазами.
    rows.sort(key=lambda r: (r[0].split('.')[-1].split(':')[-1].lower(), r[0]))

    body = ['---',
            'title: Все вызовы',
            'description: "Плоский список всех функций и методов cs-lua с ссылками на их страницы."',
            '---',
            '',
            '# Все вызовы',
            '',
            'Каждая функция и метод API одной таблицей — когда помнишь имя, но не',
            'помнишь, к какому пространству оно относится. Разложенный по темам',
            'справочник — в [обзоре](index.md).',
            '']

    body += _table(['вызов', 'namespace', ''],
                   [('[`%s`](%s)' % (name, link), label, brief)
                    for name, link, label, brief in rows])

    body.append('Всего: %d.' % len(rows))
    return '\n'.join(body).rstrip() + '\n'


# Ссылка внутри своей группы файлов - "slug.md", на чужое пространство имён -
# "../ns/slug.md", и то, и другое иногда с якорем на конкретный подраздел
# ("trace.md#результат" - ссылка не на саму функцию, а на её "Результат").
# Все три вида после переезда на страницы-группы должны обзавестись именем
# страницы группы, на которой функция оказалась: "slug.md" -> "group.md#slug",
# "slug.md#anchor" -> "group.md#anchor" (свой якорь не трогаем).
_LINK_RE = re.compile(r'\]\((?:\.\./([a-z0-9_]+)/)?([a-zA-Z_][a-zA-Z0-9_]*)\.md(#[^)]*)?\)')


def _resolve_links(text, ns_dir, registry):
    def repl(m):
        other_dir, slug, existing_anchor = m.group(1), m.group(2), m.group(3)
        entry = registry.get((other_dir or ns_dir, slug))
        if not entry:
            # Не функция этого пространства имён (или её и не было) - обзорная
            # страница, консоль и т.п. остаются как есть.
            return m.group(0)
        page_slug, anchor = entry
        frag = existing_anchor or ('#' + anchor)
        if other_dir:
            return '](../%s/%s.md%s)' % (other_dir, page_slug, frag)
        return '](%s.md%s)' % (page_slug, frag)

    return _LINK_RE.sub(repl, text)


def _build_registry(namespaces):
    """(ns_dir, slug функции) -> (slug страницы, якорь) по всем пространствам

    имён разом - нужен целиком до записи файлов, чтобы ссылки между
    пространствами резолвились правильно независимо от порядка генерации.
    """
    registry = {}
    for ns in namespaces:
        non_empty = [g for g in ns['groups'] if g['items']]
        multi = len(non_empty) > 1
        for g in non_empty:
            page_slug = _group_slug(g) if multi else 'index'
            for fn in g['items']:
                registry[(ns['dir'], _slug(fn))] = (page_slug, _slug(fn))
    return registry


def write(root, ns, registry):
    """Пишет docs/api/<dir>/ и возвращает описание для sidebar."""
    d = os.path.join(root, ns['dir'])
    os.makedirs(d, exist_ok=True)

    non_empty = [g for g in ns['groups'] if g['items']]
    multi = len(non_empty) > 1

    if multi:
        seen_pages = {'index': ns['label']}
        for g in non_empty:
            slug = _group_slug(g)
            if slug in seen_pages:
                raise SystemExit(
                    "%s: группа '%s' и '%s' дают одну страницу '%s'"
                    % (ns['dir'], g['title'], seen_pages[slug], slug))
            seen_pages[slug] = g['title']

    # Два вызова с одинаковым slug на одной странице молча схлопнули бы якоря:
    # ссылка вела бы на первый из двух. Ловим здесь, а не глазами на сайте.
    for g in non_empty:
        seen = {}
        for fn in g['items']:
            slug = _slug(fn)
            if slug in seen:
                raise SystemExit("%s и %s дают один якорь '%s' на одной странице"
                                  % (seen[slug], fn['name'], slug))
            seen[slug] = fn['name']

    index_text = _resolve_links(render_index(ns), ns['dir'], registry)
    io.open(os.path.join(d, 'index.md'), 'w', encoding='utf-8', newline='').write(index_text)

    items = []
    if multi:
        for g in non_empty:
            slug = _group_slug(g)
            text = _resolve_links(render_group_page(ns, g), ns['dir'], registry)
            io.open(os.path.join(d, slug + '.md'), 'w', encoding='utf-8', newline='').write(text)
            items.append({'type': 'doc', 'id': 'api/%s/%s' % (ns['dir'], slug), 'label': g['title']})

        return {'type': 'category',
                'label': ns['label'],
                'collapsed': True,
                'link': {'type': 'doc', 'id': 'api/%s/index' % ns['dir']},
                'items': items}

    # Одна страница на всё пространство имён - категория без пунктов внутри
    # была бы лишним кликом ради пустоты: сразу простая ссылка.
    return {'type': 'doc', 'id': 'api/%s/index' % ns['dir'], 'label': ns['label']}


def generate(namespaces, api_dir):
    """Строит реестр ссылок и пишет страницы всех пространств имён.

    Возвращает (записи sidebar, реестр ссылок, число написанных страниц).
    """
    registry = _build_registry(namespaces)
    entries = []
    pages = 0
    for ns in namespaces:
        entries.append(write(api_dir, ns, registry))
        non_empty = [g for g in ns['groups'] if g['items']]
        pages += 1 + (len(non_empty) if len(non_empty) > 1 else 0)
    return entries, registry, pages


def mintlify_navigation(namespaces):
    """Список {'group': ns_label, 'pages': [...]} для docs.json. Пути - id
    страниц без расширения, то же разбиение на группы, что write() уже
    даёт по файлам под docs/api/.
    """
    groups = []
    for ns in namespaces:
        non_empty = [g for g in ns['groups'] if g['items']]
        pages = ['api/%s/index' % ns['dir']]
        if len(non_empty) > 1:
            for g in non_empty:
                pages.append('api/%s/%s' % (ns['dir'], _group_slug(g)))
        groups.append({'group': ns['label'], 'pages': pages})
    return groups


