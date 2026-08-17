# -*- coding: utf-8 -*-
"""Собирает docs/api/ и website/sidebars.ts из scripts/docs/api/*.py.

    python scripts/docs/gen.py

Страницы под docs/api/ генерируются целиком: править их руками бесполезно,
правь описание в scripts/docs/api/.
"""
import importlib
import io
import json
import os
import shutil
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..', '..'))
sys.path.insert(0, HERE)

import render  # noqa: E402

# Порядок в меню: от того, с чего начинают, к тому, что ищут реже.
NAMESPACES = [
    'hook', 'cmd', 'players', 'timer', 'ents', 'fx', 'res',
    'sv', 'db', 'mysql', 'store', 'http', 'menu', 'ui', 'access', 'plugin', 'exports',
]

SIDEBAR = """import type {SidebarsConfig} from '@docusaurus/plugin-content-docs';

// Этот файл генерируется: python scripts/docs/gen.py
// Правь scripts/docs/api/*.py, а не его.
const sidebars: SidebarsConfig = {
  docs: [
    {type: 'doc', id: 'intro', label: 'Что это'},
    {type: 'doc', id: 'install', label: 'Установка'},
    {type: 'doc', id: 'plugins', label: 'Структура плагина'},
    {
      type: 'category',
      label: 'Справочник API',
      collapsed: false,
      link: {type: 'doc', id: 'api/index'},
      items: %(items)s,
    },
    {type: 'doc', id: 'migration', label: 'Переход с v1 на v2'},
    {type: 'doc', id: 'building', label: 'Сборка модуля'},
  ],
};

export default sidebars;
"""


def main():
    api_dir = os.path.join(ROOT, 'docs', 'api')

    # Каталог собирается с нуля: иначе удалённая функция осталась бы страницей.
    # index.md и console.md пишутся руками: они про API в целом, а не про
    # конкретную функцию.
    keep = {}
    for name in ('index.md', 'console.md'):
        path = os.path.join(api_dir, name)
        if os.path.exists(path):
            keep[name] = io.open(path, encoding='utf-8').read()

    if os.path.isdir(api_dir):
        shutil.rmtree(api_dir)
    os.makedirs(api_dir)

    for name, text in keep.items():
        io.open(os.path.join(api_dir, name), 'w', encoding='utf-8', newline='').write(text)

    loaded = [importlib.import_module('api.' + name).NS for name in NAMESPACES]
    entries, registry, pages = render.generate(loaded, api_dir)

    io.open(os.path.join(api_dir, 'all.md'), 'w', encoding='utf-8', newline='').write(
        render.render_all(loaded, registry))
    pages += 1

    entries.append({'type': 'doc', 'id': 'api/all', 'label': 'Все вызовы'})
    entries.append({'type': 'doc', 'id': 'api/console', 'label': 'Консольные команды'})

    io.open(os.path.join(ROOT, 'website', 'sidebars.ts'), 'w',
            encoding='utf-8', newline='').write(
        SIDEBAR % {'items': render.sidebar(entries)})

    # Параллельный вывод под Mintlify - живёт рядом с Docusaurus, пока переезд
    # не подтверждён (см. план миграции). Структура групп та же, что и в
    # sidebars.ts выше, просто в формате docs.json вместо TS-литерала.
    docs_json = {
        '$schema': 'https://mintlify.com/docs.json',
        'theme': 'palm',
        'name': 'cs-lua',
        'description': 'Плагины для CS 1.6 на Lua вместо SourcePawn',
        'colors': {'primary': '#b45309', 'light': '#f59e0b', 'dark': '#f59e0b'},
        'favicon': '/favicon.svg',
        'navigation': {
            'groups': [
                {'group': 'Документация',
                 'pages': ['intro', 'install', 'plugins', 'migration', 'building']},
                {'group': 'Справочник API',
                 'pages': ['api/index'] + render.mintlify_navigation(loaded) + ['api/all', 'api/console']},
            ],
        },
    }
    io.open(os.path.join(ROOT, 'docs', 'docs.json'), 'w', encoding='utf-8', newline='').write(
        json.dumps(docs_json, ensure_ascii=False, indent=2) + '\n')

    print('%d namespace(s), %d page(s)' % (len(NAMESPACES), pages))


if __name__ == '__main__':
    main()
