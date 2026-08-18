import type {SidebarsConfig} from '@docusaurus/plugin-content-docs';

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
      items: [
        {type: 'category', label: 'hook', collapsed: true, link: {type: 'doc', id: 'api/hook/index'}, items: [
          {type: 'doc', id: 'api/hook/namespace', label: 'Пространство имён'},
          {type: 'doc', id: 'api/hook/connection', label: 'Подключение'},
          {type: 'doc', id: 'api/hook/lifecycle', label: 'Жизнь сервера'},
          {type: 'doc', id: 'api/hook/gameplay', label: 'Геймплей'},
          {type: 'doc', id: 'api/hook/round', label: 'Раунд и бомба'},
          {type: 'doc', id: 'api/hook/shop', label: 'Магазин'},
        ]},
        {type: 'doc', id: 'api/cmd/index', label: 'cmd'},
        {type: 'category', label: 'players', collapsed: true, link: {type: 'doc', id: 'api/players/index'}, items: [
          {type: 'doc', id: 'api/players/namespace', label: 'Пространство имён'},
          {type: 'doc', id: 'api/players/identity', label: 'Идентификация'},
          {type: 'doc', id: 'api/players/state', label: 'Состояние'},
          {type: 'doc', id: 'api/players/cs-state', label: 'CS-состояние'},
          {type: 'doc', id: 'api/players/messages', label: 'Сообщения'},
          {type: 'doc', id: 'api/players/admin', label: 'Админские действия'},
          {type: 'doc', id: 'api/players/access', label: 'Права'},
        ]},
        {type: 'category', label: 'timer', collapsed: true, link: {type: 'doc', id: 'api/timer/index'}, items: [
          {type: 'doc', id: 'api/timer/anonymous', label: 'Анонимные'},
          {type: 'doc', id: 'api/timer/named', label: 'Именованные'},
        ]},
        {type: 'category', label: 'ents', collapsed: true, link: {type: 'doc', id: 'api/ents/index'}, items: [
          {type: 'doc', id: 'api/ents/namespace', label: 'Пространство имён'},
          {type: 'doc', id: 'api/ents/entity', label: 'Объект сущности'},
        ]},
        {type: 'doc', id: 'api/fx/index', label: 'fx'},
        {type: 'doc', id: 'api/res/index', label: 'res'},
        {type: 'category', label: 'sv', collapsed: true, link: {type: 'doc', id: 'api/sv/index'}, items: [
          {type: 'doc', id: 'api/sv/server', label: 'Сервер'},
          {type: 'doc', id: 'api/sv/cvar', label: 'Cvar'},
        ]},
        {type: 'category', label: 'db', collapsed: true, link: {type: 'doc', id: 'api/db/index'}, items: [
          {type: 'doc', id: 'api/db/open', label: 'Открытие'},
          {type: 'doc', id: 'api/db/database', label: 'Объект базы'},
          {type: 'doc', id: 'api/db/statement', label: 'Подготовленное выражение'},
        ]},
        {type: 'category', label: 'mysql', collapsed: true, link: {type: 'doc', id: 'api/mysql/index'}, items: [
          {type: 'doc', id: 'api/mysql/open', label: 'Открытие'},
          {type: 'doc', id: 'api/mysql/connection', label: 'Объект соединения'},
        ]},
        {type: 'category', label: 'store', collapsed: true, link: {type: 'doc', id: 'api/store/index'}, items: [
          {type: 'doc', id: 'api/store/kv', label: 'Ключ-значение'},
          {type: 'doc', id: 'api/store/datafile', label: 'datafile'},
        ]},
        {type: 'doc', id: 'api/http/index', label: 'http'},
        {type: 'doc', id: 'api/menu/index', label: 'menu'},
        {type: 'doc', id: 'api/ui/index', label: 'ui'},
        {type: 'category', label: 'access', collapsed: true, link: {type: 'doc', id: 'api/access/index'}, items: [
          {type: 'doc', id: 'api/access/declare', label: 'Объявление'},
          {type: 'doc', id: 'api/access/grant', label: 'Выдача'},
          {type: 'doc', id: 'api/access/read', label: 'Чтение'},
        ]},
        {type: 'doc', id: 'api/plugin/index', label: 'plugin'},
        {type: 'doc', id: 'api/exports/index', label: 'export / import'},
        {type: 'doc', id: 'api/all', label: 'Все вызовы'},
        {type: 'doc', id: 'api/console', label: 'Консольные команды'},
      ],
    },
    {type: 'doc', id: 'building', label: 'Сборка модуля'},
  ],
};

export default sidebars;
