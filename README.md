# cs-lua

Плагины для CS 1.6 на Lua вместо SourcePawn. Metamod-модуль поверх
ReHLDS/ReGameDLL, внутри — LuaJIT 2.1.

```lua
plugin { name = "Heal", api_version = 1 }

permission { "heal.use", desc = "Лечить себя", default = "vip" }

command("heal", function(ctx)
	local p = ctx.player
	if not p or not p:alive() then return end

	p:health(p:health() + 25)
	p:chat("{green}[Server]{default} подлечили до " .. p:health())
end, { perm = "heal.use" })
```

Файл в `addons/lua/plugins/heal.lua`, в консоли `lua_reload` — работает.
Рестарт сервера не нужен.

Готовые плагины — в [cs-lua-plugins](https://github.com/KotyaraKryt/cs-lua-plugins).
Здесь только модуль и core-слой.

## Документация

| | |
|---|---|
| [Структура плагина](docs/plugins.md) | папки, `require`, манифест, core-слой, список глобалов |
| [События](docs/events.md) | таблица событий, `player_hurt`, ловушки `client_connect` |
| [Объект игрока](docs/player.md) | состояние, CS-состояние, кик и бан |
| [Команды](docs/commands.md) | `command()`, `ctx`, консольные команды модуля |
| [Права доступа](docs/access.md) | ноды, группы, иммунитет, `data/*.lua` |
| [Вывод](docs/ui.md) | чат, HUD, меню, кириллица, лимиты длины |
| [Таймеры, cvar'ы, звук](docs/runtime.md) | `after`, `cvar_register`, прекеш и лимит 512 |
| [Межплагинное общение](docs/exports.md) | `export`/`import`, `emit`/`on_export` |

## Возможности

- **Изоляция.** Плагин — папка со своим `require` и своим окружением. Глобалки
  не текут наружу, ошибка при загрузке снимает плагин целиком, а не половину.
- **Права как именованные ноды.** `shop.vip.buy`: группы, наследование,
  иммунитет по весу, выдача на срок, личные разрешения и запреты.
- **Windows и Linux.** Один код, `lua_mm.dll` и `lua_mm_i386.so`.
- **Без оффсетов.** Состояние игрока читается через entvars и ReGameDLL API, а
  не по сигнатурам — не ломается от обновления игры.
- **Прекеш не роняет сервер.** Модуль считает занятые слоты и отключает
  плагин-виновника до того, как движок упадёт с `Host_Error`.
- **Перезагрузка на лету.** `lua_reload` без рестарта сервера.

## Сборка

Зависимости — сабмодули, поэтому клонировать рекурсивно:

```
git clone --recursive https://github.com/KotyaraKryt/cs-lua.git
cd cs-lua
```

Только x86 — GoldSrc 32-битный. Конфигурация под x64 упадёт на этапе `cmake`.

### Windows

CMake и MSVC с x86-тулчейном. LuaJIT собирается один раз, в консоли
**x86 Native Tools Command Prompt**:

```
cd third_party\luajit\src
msvcbuild.bat static
```

Дальше сам модуль:

```
cmake -S . -B build -A Win32
cmake --build build --config Release
```

Результат — `lua_mm.dll`.

### Linux

Нужен 32-битный тулчейн: `gcc-multilib g++-multilib` (Debian/Ubuntu) или
`glibc-devel.i686 libstdc++-devel.i686 (Fedora)`. LuaJIT — статически и с PIC,
иначе не слинкуется в `.so`:

```
make -C third_party/luajit/src CC="gcc -m32" BUILDMODE=static TARGET_CFLAGS=-fPIC -j
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

Результат — `lua_mm_i386.so` (соглашение metamod для 32-битного модуля).

> Собирай на distro не новее целевого сервера. Модуль, слинкованный с
> современной glibc, потребует символы (`GLIBC_2.38` и подобные), которых на
> боксах под CS 1.6 обычно нет, и не загрузится. Debian 11 (glibc 2.31)
> покрывает всё, что ещё в строю.

### Установка на тестовый сервер

Сборка раскладывается сама: модуль, `core/` и `include/` копируются в
`<gamedir>/addons/lua/`.

```
cmake -S . -B build -DCSLUA_SERVER_DIR="/home/hlds/cstrike"
cmake -S . -B build -DCSLUA_SERVER_DIR=      # выключить копирование
```

`data/` и `plugins/` не трогаются: там живут права и чужой код.

## Установка

```
cstrike/addons/lua/lua_mm.dll         (Windows) или lua_mm_i386.so (Linux)
cstrike/addons/lua/core/              базовый слой, едет с модулем
cstrike/addons/lua/data/              groups.lua, users.lua
cstrike/addons/lua/include/           библиотеки для require
cstrike/addons/lua/plugins/           плагины
```

В `cstrike/addons/metamod/plugins.ini`:

```
win32 addons/lua/lua_mm.dll
linux addons/lua/lua_mm_i386.so
```

Модуль грузится при старте сервера и не выгружается — `meta unload` запрещён,
он отдаёт движку указатели на консольные команды. Итерировать по скриптам через
`lua_reload`.

### Тестовый сервер

В репозитории его нет: это игровой контент Valve. Поднимается руками —
HLDS с CS 1.6, поверх [ReHLDS](https://github.com/dreamstalker/rehlds) +
[ReGameDLL_CS](https://github.com/s1lentq/ReGameDLL_CS) и
[metamod-r](https://github.com/rehlds/Metamod-R). Без ReGameDLL модуль
заведётся, но события геймплея и CS-состояние работать не будут.

`data/groups.lua` и `data/users.lua` копируются из `scripts/data/` один раз
вручную: дальше их правит сервер, и затирать их сборкой нельзя.

## Безопасность

`luaL_openlibs` открывает в том числе `ffi` — плагин может делать с процессом
сервера что угодно. Это осознанно: плагины ставит админ сервера.

Вход по нику (`["name:Вася"]` в `users.lua`) годится только для LAN: ник
подделывается тривиально, а пароль летит по сети открытым текстом.

## Лицензия

[GPLv3](LICENSE) — как metamod-r и ReGameDLL, с которыми модуль собирается.

| зависимость | лицензия | как подключено |
|-------------|----------|----------------|
| [LuaJIT](https://luajit.org/) | MIT | сабмодуль, линкуется статически |
| [metamod-r](https://github.com/rehlds/Metamod-R) | GPLv3 | сабмодуль, только plugin-заголовки |
| cssdk из [ReAPI](https://github.com/s1lentq/reapi) | GPLv3 | копия в `third_party/regamedll/cssdk` |
