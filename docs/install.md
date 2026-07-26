---
description: "Требования к серверу, раскладка файлов, регистрация в metamod, проверка, права плагинов."
---

# Установка

Ставится как обычный metamod-модуль: файлы в `addons/lua/`, строка в
`plugins.ini`, рестарт сервера.

## Требования

| | |
|---|---|
| [metamod-r](https://github.com/rehlds/Metamod-R) | обязательно |
| [ReGameDLL_CS](https://github.com/s1lentq/ReGameDLL_CS) | обязательно для событий геймплея и CS-состояния |
| [ReHLDS](https://github.com/dreamstalker/rehlds) | желательно, даёт точный учёт прекеша |

Бинарники — в [релизах](https://github.com/KotyaraKryt/cs-lua/releases):
`lua_mm.dll` под Windows, `lua_mm_i386.so` под Linux. Сборка из исходников — в
[Сборке модуля](building.md).

## Файлы

```
cstrike/addons/lua/lua_mm.dll         (Windows) или lua_mm_i386.so (Linux)
cstrike/addons/lua/core/              базовый слой, едет вместе с модулем
cstrike/addons/lua/include/           библиотеки для require
cstrike/addons/lua/data/              groups.lua, users.lua
cstrike/addons/lua/plugins/           плагины
```

| каталог | кому принадлежит |
|---|---|
| `core/`, `include/` | модулю: обновляй вместе с бинарником, версии должны совпадать |
| `data/`, `plugins/` | серверу: обновление их не трогает |

Стартовые `groups.lua` и `users.lua` лежат в репозитории в `scripts/data/`.
Скопируй один раз вручную.

## plugins.ini

В `cstrike/addons/metamod/plugins.ini`:

```ini
win32 addons/lua/lua_mm.dll
linux addons/lua/lua_mm_i386.so
```

Модуль грузится при старте сервера и не выгружается: `meta unload` запрещён.
Перезагрузка скриптов — `lua_reload`.

## Проверка

| команда | что показывает |
|---|---|
| `lua_list` | загруженные плагины; `[FAILED]` у упавших |
| `lua_precache` | занятые слоты прекеша |
| `lua_reload` | перечитывает `core/`, `include/`, `plugins/` |

Пустой `lua_list` — в `plugins/` ничего нет или все плагины упали при загрузке.
Причина печатается в консоль с именем файла.

## Первый плагин

`cstrike/addons/lua/plugins/hello.lua`:

```lua
plugin { name = "Hello", api_version = 1 }

hook.add("player_spawn", "test.spawn", function(e)
	p:chat("{green}Привет, {default}" .. p:name())
end)
```

`lua_reload` — и плагин работает. Дальше — [Структура плагина](plugins.md).

## Права плагинов

Плагин выполняется с правами серверного процесса: `luaL_openlibs` открывает в том
числе `ffi`, поэтому доступны память процесса, файловая система и системные
вызовы. По правам это модуль AMXX (`.dll`/`.so`), а не `.amxx`-плагин.

Игрок загрузить или выполнить Lua-код не может: ник, текст в чате и аргументы
команды попадают в плагин как данные.

> [!WARNING]
> Доступ к `plugins/` равен доступу к серверу. Lua не компилируется — код лежит
> рядом текстом, читай его перед установкой.

Вход по нику (`["name:Вася"]` в `users.lua`) годится только для LAN: ник
подделывается, пароль передаётся открытым текстом. На публичном сервере права
выдавай по SteamID — [access](api/access/index.md).
