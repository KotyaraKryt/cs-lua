---
description: "Как установить модуль cs-lua на сервер."
---

## Требования

Перед установкой убедитесь, что на сервере установлены:

- [ReHLDS](https://github.com/dreamstalker/rehlds) — `v3.15.0.896`
- [ReGameDLL_CS+](https://github.com/s1lentq/ReGameDLL_CS) — `v5.30.0.814`
- [Metamod-R](https://github.com/rehlds/Metamod-R) — `v1.3.0.149`

<Danger>Более старые версии зависимостей также могут работать, однако часть функций может быть недоступна или работать некорректно.</Danger>

<Warning>Без ReGameDLL_CS модуль загрузится, но часть API, требующая ReGameDLL_CS, будет недоступна.</Warning>


## Установка модуля

Скачайте последнюю версию модуля со [страницы релизов](https://github.com/KotyaraKryt/cs-lua/releases).

Выберите файл в зависимости от операционной системы сервера:

- Windows — `lua_mm.dll`
- Linux — `lua_mm_i386.so`

После распаковки поместите файлы модуля в:
```text
cstrike/addons/lua/
```

В результате структура каталогов должна выглядеть следующим образом:
<FileTree>
- cstrike/
  - addons/
    - lua/
      - core/
      - include/
      - lua_mm.dll

</FileTree>
```
cstrike/
└── addons/
    └── lua/
        ├── core/
        ├── include/
        └── lua_mm.dll
```
<Tip>Для Linux вместо `lua_mm.dll` используется `lua_mm_i386.so`.</Tip>

## Подключение модуля

<Tabs>
  <Tab title="Windows">
  Добавьте в `cstrike/addons/metamod/plugins.ini`:

  ```ini
  win32 addons/lua/lua_mm.dll
  ```
  </Tab>
  <Tab title="Linux">
  Добавьте в `cstrike/addons/metamod/plugins.ini`:

  ```ini
  linux addons/lua/lua_mm_i386.so
  ```
  </Tab>
</Tabs>

## Проверка установки

Перезапустите сервер, чтобы Metamod загрузил модуль.
После запуска выполните в консоли сервера:

```text
lua_list
```

Если модуль успешно загружен, команда выполнится без ошибок и выведет список загруженных Lua-скриптов.
<Tip>
Если `lua_list` выполняется без ошибок, `cs-lua` успешно установлен и подключён к серверу.
</Tip>

## Дальше

<CardGroup cols={2}>
  <Card title="Первый плагин" icon="sparkles" href="/guides/first-plugin">
    Создайте свой первый Lua-плагин и добавьте команду.
  </Card>
  <Card title="Структура плагина" icon="folder-tree" href="/guides/plugin-structure">
    Как устроен плагин, `require`, окружение и порядок загрузки.
  </Card>
  <Card title="Справочник API" icon="book" href="/api">
    Все пространства имён: игроки, сущности, события, таймеры, команды и т.д.
  </Card>
  <Card title="Готовые плагины" icon="puzzle-piece" href="https://github.com/KotyaraKryt/cs-lua-plugins">
    Примеры и готовые решения, которые можно установить сразу.
  </Card>
</CardGroup>