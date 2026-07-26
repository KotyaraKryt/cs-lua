---
description: "Что такое cs-lua, отличия от AMX Mod X, состав модуля и требования к серверу."
---

# Что это

Metamod-модуль для CS 1.6, выполняющий серверные плагины на Lua. Внутри
LuaJIT 2.1, состояние игроков и события геймплея — из ReHLDS и ReGameDLL.

Плагин — `.lua` файл или папка с `init.lua` в `addons/lua/plugins/`. После
`lua_reload` он работает: без компиляции и рестарта сервера.

```lua title="addons/lua/plugins/heal.lua"
plugin { name = "Heal", api_version = 2 }

access.declare("heal.use", { desc = "Лечить себя", default = "vip" })

cmd.add("heal", function(ctx)
	local p = ctx.player
	if not p or not p:alive() then return end

	p:health(p:health() + 25)
	p:chat("{green}[Server]{default} подлечили до " .. p:health())
end, { perm = "heal.use" })
```

Команда доступна в чате (`!heal`, `/heal`), в консоли и по rcon. Право
проверяется до вызова хендлера.

## Отличия от AMX Mod X

| | AMX Mod X | cs-lua |
|---|---|---|
| цикл разработки | `amxxpc` → смена карты или рестарт | `lua_reload` |
| язык | Pawn: статические массивы, строки фиксированной длины | Lua: таблицы, замыкания, GC |
| доступ к движку | модули и оффсеты под версию игры | entvars и ReGameDLL API |
| изоляция | общее пространство нативов | своё окружение и свой `require` на плагин |
| права плагина | `.amxx` ограничен нативами | полный доступ, как у модуля AMXX |
| готовые плагины | двадцать лет наработок | [cs-lua-plugins](https://github.com/KotyaraKryt/cs-lua-plugins) |

## Состав

```
cstrike/addons/lua/
  lua_mm.dll          модуль, грузится metamod'ом при старте сервера
  core/               базовый Lua-слой: команды, права, меню, экспорты
  include/            библиотеки под require: store, datafile, color, text, class
  data/               groups.lua, users.lua
  plugins/            плагины
```

Модуль отдаёт в Lua игроков, события, сущности, таймеры, cvar'ы и SQLite. Роутер
команд, права и меню написаны на Lua, лежат в `core/` и грузятся до плагинов.

Модуль не выгружается: `meta unload` запрещён, движок держит указатели на его
консольные команды. Итерации по скриптам — `lua_reload`.

## Требования

| | |
|---|---|
| [metamod-r](https://github.com/rehlds/Metamod-R) | обязательно |
| [ReGameDLL_CS](https://github.com/s1lentq/ReGameDLL_CS) | обязательно для событий геймплея и CS-состояния |
| [ReHLDS](https://github.com/dreamstalker/rehlds) | желательно, даёт точный учёт прекеша |

Без ReGameDLL модуль загрузится, но раунды, деньги, команды игроков и покупки
работать не будут.

## Дальше

| | |
|---|---|
| [Установка](install.md) | раскладка файлов, `plugins.ini`, проверка |
| [Структура плагина](plugins.md) | папки, `require`, окружение, core-слой |
| [Справочник API](api/index.md) | все пространства имён |
| [Переход с v1 на v2](migration.md) | таблица переименований, объект события |
