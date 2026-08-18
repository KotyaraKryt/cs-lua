---
description: "Справочник API cs-lua для разработки серверных Lua-плагинов."
---

# Справочник API

API `cs-lua` предоставляет Lua-плагинам доступ к игрокам, событиям,
командам, таймерам, сущностям, серверу и другим возможностям движка.

## Основные возможности

<CardGroup cols={2}>
  <Card title="Игроки" icon="user" href="/api/players">
    Работа с игроками: здоровье, броня, оружие, поиск и рассылка сообщений.
  </Card>

  <Card title="События" icon="zap" href="/api/hook">
    Обработка игровых событий и создание собственных событий.
  </Card>

  <Card title="Команды" icon="terminal" href="/api/cmd">
    Регистрация команд чата, серверной консоли и RCON.
  </Card>

  <Card title="Таймеры" icon="clock" href="/api/timer">
    Выполнение кода с задержкой или через заданные интервалы.
  </Card>

  <Card title="Сущности" icon="box" href="/api/ents">
    Поиск и работа с сущностями игрового мира.
  </Card>

  <Card title="Сервер" icon="server" href="/api/sv">
    Управление сервером, картой, CVar и выполнение консольных команд.
  </Card>
</CardGroup>

## Дополнительные возможности

<CardGroup cols={2}>
  <Card title="Эффекты" icon="sparkles" href="/api/fx">
    Взрывы, лучи, спрайты и другие визуальные эффекты.
  </Card>

  <Card title="Ресурсы" icon="database" href="/api/res">
    Прекеш звуков, моделей и других ресурсов.
  </Card>

  <Card title="База данных" icon="database" href="/api/db">
    Работа с SQLite и MySQL.
  </Card>

  <Card title="Хранилище" icon="hard-drive" href="/api/store">
    Key-value хранилище на SQLite.
  </Card>

  <Card title="HTTP" icon="globe" href="/api/http">
    Выполнение исходящих HTTP-запросов без блокировки игрового цикла.
  </Card>

  <Card title="Меню" icon="list" href="/api/menu">
    Создание интерактивных меню для игроков.
  </Card>

  <Card title="UI" icon="palette" href="/api/ui">
    Цвета, текст и другие возможности для работы с интерфейсом.
  </Card>
</CardGroup>

## Плагины и доступ

<CardGroup cols={2}>
  <Card title="Права доступа" icon="shield" href="/api/access">
    Права, группы и иммунитет игроков.
  </Card>

  <Card title="Плагины" icon="puzzle-piece" href="/api/plugin">
    Метаданные, жизненный цикл и управление плагином.
  </Card>

  <Card title="Экспорты" icon="share" href="/api/exports">
    Вызов функций и взаимодействие между плагинами.
  </Card>
</CardGroup>

## Модули

Помимо API, `cs-lua` предоставляет несколько модулей, которые можно
подключить через стандартный `require()`.

| Модуль | Описание |
|---|---|
| `store` | Key-value хранилище на SQLite |
| `datafile` | Чтение и запись файлов `data/*.lua` |
| `json` | Разбор и сборка JSON |
| `color` | Разбор цветов |
| `text` | Работа с длиной строк в формате клиента |
| `class` | Классы с наследованием |

Подробнее о подключении модулей — в разделе
[Структура плагина](/guides/plugin-structure).

## Консольные команды

`cs-lua` также предоставляет несколько консольных команд для управления
модулем и плагинами.

Полный список доступен в [консольных командах](/api/console).