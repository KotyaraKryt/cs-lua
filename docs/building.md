---
description: "Сборка модуля из исходников под Windows и Linux, копирование на тестовый сервер, зависимости."
---

# Сборка модуля

Нужна, только если правишь C++ или собираешь текущий `main`. Готовые бинарники —
в [релизах](https://github.com/KotyaraKryt/cs-lua/releases).

Зависимости подключены сабмодулями:

```bash
git clone --recursive https://github.com/KotyaraKryt/cs-lua.git
cd cs-lua
```

Собирается только под x86 — GoldSrc 32-битный. Конфигурация под x64 останавливает
`cmake` с ошибкой.

## Windows

Нужны CMake и MSVC с x86-тулчейном. LuaJIT собирается отдельно и один раз, в
консоли **x86 Native Tools Command Prompt**:

```bat
cd third_party\luajit\src
msvcbuild.bat static
```

Дальше модуль:

```bash
cmake -S . -B build -A Win32
cmake --build build --config Release
```

Результат — `build/Release/lua_mm.dll`. LuaJIT слинкован статически, отдельной
библиотеки рядом нет.

## Linux

Нужен 32-битный тулчейн: `gcc-multilib g++-multilib` на Debian и Ubuntu,
`glibc-devel.i686 libstdc++-devel.i686` на Fedora. LuaJIT собирается статически и
с PIC, иначе не слинкуется в `.so`:

```bash
make -C third_party/luajit/src CC="gcc -m32" BUILDMODE=static TARGET_CFLAGS=-fPIC -j
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

Результат — `lua_mm_i386.so`: metamod ждёт для 32-битного модуля именно такое имя.

<Warning>
Собирай на дистрибутиве не новее целевого сервера. Модуль, слинкованный с
современной glibc, требует символы вроде `GLIBC_2.38`, которых на машинах под
CS 1.6 обычно нет, и metamod его не загрузит. CI собирает в Debian 11
(glibc 2.31).
</Warning>

## Копирование на тестовый сервер

`CSLUA_SERVER_DIR` кладёт модуль, `core/` и `include/` в `<gamedir>/addons/lua/`
на этапе сборки:

```bash
cmake -S . -B build -DCSLUA_SERVER_DIR="/home/hlds/cstrike"
cmake -S . -B build -DCSLUA_SERVER_DIR=      # выключить копирование
```

`data/` и `plugins/` не трогаются: там лежат права и код плагинов.

Самого тестового сервера в репозитории нет — это игровой контент Valve. Состав
описан в [Установке](install.md#требования).

## Тесты

```bash
luajit tests/run.lua              # всё
luajit tests/run.lua access       # один спек
```

Игра не нужна: `core/` и `include/` — обычный Lua, а пространства имён движка
подменяются стабами из `tests/support/stubs.lua`. Проверяется то, что ломается
молча: разрешение прав, роутер команд, разбор цвета, подсчёт длины строки и
запись данных на диск. Прогон занимает секунду и стоит первым в CI.

C++-поверхность так не проверить: регистрацию в пространствах имён, цепочки
хуков, объект события, таймеры, базу и жизнь сущностей видно только на живом
сервере. Для этого есть `tests/selftest/` — обычный плагин:

```bash
cp -r tests/selftest <сервер>/cstrike/addons/lua/plugins/
```

После `lua_reload` он прогоняет 104 проверки и печатает итог, а дальше `lua_test`
в консоли повторяет прогон без перезагрузки.

## Зависимости

| зависимость | лицензия | как подключено |
|---|---|---|
| [LuaJIT](https://luajit.org/) | MIT | сабмодуль, линкуется статически |
| [metamod-r](https://github.com/rehlds/Metamod-R) | GPLv3 | сабмодуль, только plugin-заголовки |
| cssdk из [ReAPI](https://github.com/s1lentq/reapi) | GPLv3 | копия в `third_party/regamedll/cssdk` |
| [SQLite](https://sqlite.org/) | public domain | амальгама в `third_party/sqlite` |

Модуль распространяется под
[GPLv3](https://github.com/KotyaraKryt/cs-lua/blob/main/LICENSE) — как metamod-r
и ReGameDLL, с которыми он собирается.
