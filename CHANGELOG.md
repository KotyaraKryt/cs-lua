# Changelog

Формат — [Keep a Changelog](https://keepachangelog.com/ru/1.1.0/),
версии по [SemVer](https://semver.org/lang/ru/).

Отдельно от версии проекта живёт `api_version` — версия Lua-API, которую плагин
объявляет в `plugin{}`. Она растёт только при ломающем изменении API, и каждая
такая правка обязана попасть в **Изменено** с пометкой, что бампнуто.

## [1.0.0] — не выпущено

Первый релиз. `api_version` — 1.

Модуль умеет: плагины-папки с изолированным `require`, единую систему событий
(движковых и своих) через объект-таблицу, права доступа на именованных нодах с
группами и иммунитетом, роутер команд (консоль + чат + rcon), меню, таймеры,
cvar'ы, звук с безопасным прекешем, вывод в чат/HUD/DHUD с цветом, сущности,
SQLite, исходящий и входящий HTTP, перезагрузку плагинов без рестарта сервера —
как всех сразу, так и по одному.

Собирается под Windows (`lua_mm.dll`) и Linux (`lua_mm_i386.so`) из общего кода;
CI собирает обе платформы и прикладывает бинарники к релизу. Подробности — в
[README](README.md).

### Добавлено

**Игрок**

- `p:spawn()` — респаун без потери денег и счёта, через движковый `RoundRespawn()`.
- `p:trace([distance])` — луч из глаз игрока: `kind`, `player`, `classname`,
  координаты, дистанция, `hitgroup`.
- `p:freeze()`, `p:godmode()`, `p:noclip()` — флаги состояния.
- `p:exec(cmd)` — консольная команда на машине игрока.
- Оружие: `p:weapons()`, `p:ammo()`, `p:clip()`, `p:drop()`.

**События**

- `player_death` несёт `weapon` и `distance`.
- `player_team_change`, `weapon_fire` — опрашиваются каждый кадр, ловят любой
  источник смены, включая сторонние моды.
- `map_change`, `plugin_unload` — точки, где плагин снимает свои эффекты.
- `grenade_throw`/`grenade_thrown`/`grenade_explode` теперь покрывают и
  флешку (`weapon_flashbang`), не только HE и дымовую.

**Сущности**

- `ents.create`, `ents.find`, `ents.in_sphere` и объект сущности: `origin`,
  `angles`, `model`, `classname`, `valid`, `spawn`, `remove`. Работает без
  ReGameDLL.
- `e:attach(player[, x, y, z])` / `e:detach()` — движковый MOVETYPE_FOLLOW
  вместо ручного перетаскивания по таймеру.

**Данные**

- SQLite, вкомпилированный в модуль: `db.open(name)`, `exec`/`query`/`first`/
  `prepare`/`transaction`, подготовленные запросы, значения только через `?`.
  Синхронно, прямо в кадре; сторож `cslua_db_timeout_ms` не даёт запросу
  подвесить сервер.
- `require("store")` — key-value на SQLite с отложенной записью.
  `require("datafile")` — читаемые `.lua`-файлы для ручной правки.
- `file.read`/`write`/`append`/`exists`/`remove`/`size`/`list` — сырые
  байтовые файлы в каталоге плагина, песочница как у `db.open()`.
- `log.write(msg)` — один лог-файл на плагин с ротацией по дням
  (`logs/YYYY-MM-DD.log`), без ручной настройки каналов и без автоочистки.

**Ресурсы**

- `res.declare(t)` — объявляет и прекеширует дерево звуков/моделей одним
  вызовом вместо ручного `for` по таблице; понимает любую вложенность и
  возвращает тот же `t` как готовый набор хендлов.
- `res.sound_exists(path)` / `res.model_exists(path)` — уже ли этот путь в
  прекеше, чужой плагин или свой.

**Сеть**

- `http_server` — входящий HTTP в процессе сервера: `listen`, `route` с
  `:param`-сегментами, `stream`/`push` для Server-Sent Events.
- `msg.send(dest, name, target, ...)` — произвольное сетевое user-сообщение
  (стоковое или своё) одним атомарным вызовом, без открытого состояния между
  `MESSAGE_BEGIN` и `MESSAGE_END`.

**Плагины и инструменты**

- `lua_reload <plugin>` — перезагрузка одного плагина без остановки остальных.
- `plugin.on_unload(fn)`, `plugin.data_dir()`, `plugin.list()`,
  `plugin.reload(id)` / `plugin.reload_all()`.
- `lua_check <plugin>` — проверяет `manifest.lua` в одноразовом слоте, не
  трогая рабочие плагины.
- `lua_hooks [событие]` — кто на что подписан, в порядке вызова.
- `timer.create`/`timer.destroy` — именованные таймеры; `persist` управляет
  тем, переживает ли таймер смену карты.
- `cmd.remove`, `cmd.list`, `hook.list([событие])`.
- `access.declare` подставляет владельца ноды через `plugin.id()` сам;
  `lua_perms list` показывает, какой плагин объявил каждую ноду.
- `plugin{}` — `min_engine_version` / `max_engine_version` для точной привязки
  к сборке, отдельно от `api_version`.

### Изменено

- Плагин — только папка с `manifest.lua` + `init.lua`, не одиночный `.lua`.
  Подробности — [docs/plugins.md](docs/plugins.md).
- `players.broadcast` — приёмник «всем сразу», только рассылка сообщений.
- Опции вместо безымянных булевых флагов: `p:money(500, { hud = true })`.
- Один тип цвета на все каналы: имя из палитры, `"#rrggbb"` или `{ r, g, b }`.
