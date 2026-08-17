import path from 'path';
import type {Config} from '@docusaurus/types';
import type * as Preset from '@docusaurus/preset-classic';
import {themes as prismThemes} from 'prism-react-renderer';
import githubAlerts from './src/remark/githubAlerts.mjs';

const organizationName = 'KotyaraKryt';
const projectName = 'cs-lua';

const config: Config = {
  title: 'cs-lua',
  tagline: 'Плагины для CS 1.6 на Lua вместо SourcePawn',
  favicon: 'img/favicon.svg',

  // GitHub Pages отдаёт сайт из подпапки, отсюда baseUrl. Превью на своей
  // машине живёт в корне порта, поэтому оба значения переопределяются:
  //   DOCS_URL=http://node1.example.com:8080 DOCS_BASE_URL=/ npm run build
  url: process.env.DOCS_URL || `https://${organizationName.toLowerCase()}.github.io`,
  baseUrl: process.env.DOCS_BASE_URL || `/${projectName}/`,
  organizationName,
  projectName,
  trailingSlash: false,

  // Битая ссылка в доках — это баг, а не предупреждение в логе сборки.
  onBrokenLinks: 'throw',
  onBrokenAnchors: 'throw',

  markdown: {
    hooks: {
      onBrokenMarkdownLinks: 'throw',
    },
  },

  i18n: {
    defaultLocale: 'ru',
    locales: ['ru'],
  },

  presets: [
    [
      'classic',
      {
        docs: {
          // Docs живут в docs/ у корня репозитория: их читают и с GitHub,
          // и с сайта, поэтому копии здесь нет.
          path: path.resolve(__dirname, '../docs'),
          routeBasePath: '/',
          sidebarPath: './sidebars.ts',
          // До штатных плагинов: он отдаёт containerDirective, который дальше
          // разбирает admonition-плагин Docusaurus.
          beforeDefaultRemarkPlugins: [githubAlerts],
          editUrl: `https://github.com/${organizationName}/${projectName}/edit/main/`,
          showLastUpdateTime: true,
        },
        blog: false,
        theme: {
          customCss: './src/css/custom.css',
        },
      } satisfies Preset.Options,
    ],
  ],

  // Поиск локальный, индекс собирается при сборке и лежит рядом с сайтом.
  // Algolia не берём: справочник по 190 функциям должен искаться и на форке, и
  // офлайн, без чужого ключа и без похода наружу за каждым запросом.
  themes: [
    [
      '@easyops-cn/docusaurus-search-local',
      {
        // Русский нужен со стеммингом: без него "события" не находит "событие".
        language: ['ru', 'en'],
        hashed: true,
        indexBlog: false,
        docsRouteBasePath: '/',
        // Имя вызова живёт в заголовке страницы, поэтому вес заголовков выше
        // обычного: по "p:health" должна находиться его страница, а не десяток
        // упоминаний в примерах.
        searchResultLimits: 12,
        searchResultContextMaxLength: 80,
        highlightSearchTermsOnTargetPage: true,
      },
    ],
  ],

  themeConfig: {
    colorMode: {
      defaultMode: 'dark',
      respectPrefersColorScheme: true,
    },
    // Страница - это несколько вызовов (H2), у каждого свои Аргументы/
    // Возвращает (H3). Без ограничения TOC справа показал бы и то, и другое -
    // список из полусотни строк вместо списка вызовов на странице.
    tableOfContents: {
      minHeadingLevel: 2,
      maxHeadingLevel: 2,
    },
    navbar: {
      title: 'cs-lua',
      items: [
        {
          type: 'docSidebar',
          sidebarId: 'docs',
          position: 'left',
          label: 'Документация',
        },
        {
          to: '/plugins',
          position: 'left',
          label: 'Быстрый старт',
        },
        {
          href: `https://github.com/${organizationName}/cs-lua-plugins`,
          label: 'Плагины',
          position: 'right',
        },
        {
          href: `https://github.com/${organizationName}/${projectName}`,
          label: 'GitHub',
          position: 'right',
        },
      ],
    },
    footer: {
      style: 'dark',
      links: [
        {
          title: 'Документация',
          items: [
            {label: 'Структура плагина', to: '/plugins'},
            {label: 'Справочник API', to: '/api'},
            {label: 'События', to: '/api/hook'},
            {label: 'Объект игрока', to: '/api/players'},
          ],
        },
        {
          title: 'Код',
          items: [
            {
              label: 'Модуль',
              href: `https://github.com/${organizationName}/${projectName}`,
            },
            {
              label: 'Готовые плагины',
              href: `https://github.com/${organizationName}/cs-lua-plugins`,
            },
            {
              label: 'Релизы',
              href: `https://github.com/${organizationName}/${projectName}/releases`,
            },
          ],
        },
        {
          title: 'Вокруг',
          items: [
            {label: 'ReHLDS', href: 'https://github.com/dreamstalker/rehlds'},
            {label: 'ReGameDLL_CS', href: 'https://github.com/s1lentq/ReGameDLL_CS'},
            {label: 'metamod-r', href: 'https://github.com/rehlds/Metamod-R'},
            {label: 'LuaJIT', href: 'https://luajit.org/'},
          ],
        },
      ],
      copyright: `cs-lua — GPLv3. Собрано на Docusaurus.`,
    },
    prism: {
      theme: prismThemes.github,
      darkTheme: prismThemes.vsDark,
      additionalLanguages: ['lua', 'bash', 'ini', 'cmake'],
    },
  } satisfies Preset.ThemeConfig,
};

export default config;
