# cs-lua

Плагины для CS 1.6 на Lua вместо SourcePawn. Metamod-модуль поверх
ReHLDS/ReGameDLL, внутри — LuaJIT 2.1.

**[Документация →](https://kotyarakryt.github.io/cs-lua/)**

```lua
plugin { name = "Heal", api_version = 2 }

access.declare("heal.use", { desc = "Лечить себя", default = "vip" })

cmd.add("heal", function(ctx)
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

Читается на [сайте](https://kotyarakryt.github.io/cs-lua/), исходники страниц
лежат в [docs/](docs/) и правятся обычным `.md`.

| | |
|---|---|
| [Что это](docs/intro.md) | обзор, отличия от AMXX, из чего состоит |
| [Установка](docs/install.md) | требования к серверу, раскладка файлов, `plugins.ini` |
| [Структура плагина](docs/plugins.md) | папки, `require`, окружение, core-слой |
| **[Справочник API](docs/api/index.md)** | все пространства имён |
| [Переход с v1 на v2](docs/migration.md) | таблица переименований, объект события |
| [Сборка модуля](docs/building.md) | Windows, Linux, деплой на тестовый сервер |

Сайт пересобирается сам при пуше в `main`; его исходники — в
[website/](website/).

## Возможности

- **Один стиль.** Всё регистрируется одинаково: пространство имён, глагол,
  обработчик на одну таблицу. `hook.add`, `cmd.add`, `timer.after`, `ents.create`.
- **Изоляция.** Плагин — папка со своим `require` и своим окружением. Глобалки
  не текут наружу, ошибка при загрузке снимает плагин целиком, а не половину.
- **Права как именованные ноды.** `shop.vip.buy`: группы, наследование,
  иммунитет по весу, выдача на срок, личные разрешения и запреты.
- **SQLite в комплекте.** Вкомпилирован в модуль, со сторожем против запроса,
  который заморозил бы сервер.
- **Windows и Linux.** Один код, `lua_mm.dll` и `lua_mm_i386.so`.
- **Без оффсетов.** Состояние игрока читается через entvars и ReGameDLL API, а
  не по сигнатурам — не ломается от обновления игры.
- **Прекеш не роняет сервер.** Модуль считает занятые слоты и отключает
  плагин-виновника до того, как движок упадёт с `Host_Error`.
- **Перезагрузка на лету.** `lua_reload` без рестарта сервера, либо
  `lua_reload <plugin>` — только один плагин, остальные продолжают работать.

## Установка

Бинарники — в [релизах](https://github.com/KotyaraKryt/cs-lua/releases).
`lua_mm.dll` (или `lua_mm_i386.so`) вместе с `core/` и `include/` кладутся в
`cstrike/addons/lua/`, модуль прописывается в `addons/metamod/plugins.ini`:

```
win32 addons/lua/lua_mm.dll
linux addons/lua/lua_mm_i386.so
```

Подробности и требования к серверу — в [Установке](docs/install.md), сборка из
исходников — в [Сборке модуля](docs/building.md).

## Права плагинов

Плагин — полноправный код на сервере, по правам ближе к модулю AMXX, чем к
`.amxx`-плагину: `ffi` открыт, файлы доступны. Отсюда же берутся прямой доступ к
entvars, SQLite и возможность дописать недостающее, не пересобирая модуль.
Игрок при этом загрузить или выполнить Lua-код не может — всё, что он присылает,
приходит в плагин как данные. Подробнее — в [Установке](docs/install.md).

Вход по нику (`["name:Вася"]` в `users.lua`) годится только для LAN: ник
подделывается тривиально, а пароль летит по сети открытым текстом.

## Лицензия

[GPLv3](LICENSE) — как metamod-r и ReGameDLL, с которыми модуль собирается.

| зависимость | лицензия | как подключено |
|-------------|----------|----------------|
| [LuaJIT](https://luajit.org/) | MIT | сабмодуль, линкуется статически |
| [metamod-r](https://github.com/rehlds/Metamod-R) | GPLv3 | сабмодуль, только plugin-заголовки |
| cssdk из [ReAPI](https://github.com/s1lentq/reapi) | GPLv3 | копия в `third_party/regamedll/cssdk` |
| [SQLite](https://sqlite.org/) | public domain | амальгама в `third_party/sqlite` |
