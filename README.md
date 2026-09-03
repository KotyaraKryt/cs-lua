# cs-lua

Плагины для CS 1.6 на Lua вместо SourcePawn. Metamod-модуль поверх
ReHLDS/ReGameDLL, внутри — LuaJIT 2.1.

**[Документация →](https://cs-lua.mintlify.site/)**

```lua title="addons/lua/plugins/heal/manifest.lua"
plugin { name = "Heal", api_version = 1 }

access.declare("heal.use", { desc = "Лечить себя", default = "vip" })
```

```lua title="addons/lua/plugins/heal/init.lua"
cmd.add("heal", function(ctx)
	local p = ctx.player
	if not p or not p:alive() then return end

	p:health(p:health() + 25)
	p:chat("{green}[Server]{default} подлечили до " .. p:health())
end, { perm = "heal.use" })
```

`lua_reload` в консоли — и `!heal` уже работает. Рестарт сервера не нужен.

Готовые плагины — в [cs-lua-plugins](https://github.com/KotyaraKryt/cs-lua-plugins).
Здесь только модуль и core-слой.

## Возможности

Единый стиль регистрации (`hook.add`, `cmd.add`, `timer.after`, `ents.create`
— пространство имён, глагол, один обработчик), своё окружение и `require` у
каждого плагина, права как именованные ноды с наследованием, SQLite в
комплекте, Windows и Linux из одного кода, `lua_reload` без рестарта сервера
(целиком или один плагин). Подробнее — в [документации](https://cs-lua.mintlify.site/).

## Установка

Бинарники — в [релизах](https://github.com/KotyaraKryt/cs-lua/releases).
`lua_mm.dll` (или `lua_mm_i386.so`) вместе с `core/` и `lib/` — в
`cstrike/addons/lua/`, модуль — в `addons/metamod/plugins.ini`:

```
win32 addons/lua/lua_mm.dll
linux addons/lua/lua_mm_i386.so
```

Требования к серверу и сборка из исходников — в [документации](https://cs-lua.mintlify.site/install).

## Права плагинов

Плагин — полноправный код на сервере: `ffi` открыт, файлы доступны, прямой
доступ к entvars и SQLite. Игрок Lua-код не выполняет — только шлёт данные. Но
и ограничений по CPU и памяти нет — плагин может повесить сервер обычной
ошибкой, своей или чужой. Подробнее — в [документации](https://cs-lua.mintlify.site/install).

## Лицензия

[GPLv3](LICENSE) — как metamod-r и ReGameDLL, с которыми модуль собирается.

| зависимость | лицензия | как подключено |
|-------------|----------|----------------|
| [LuaJIT](https://luajit.org/) | MIT | сабмодуль, линкуется статически |
| [metamod-r](https://github.com/rehlds/Metamod-R) | GPLv3 | сабмодуль, только plugin-заголовки |
| cssdk из [ReAPI](https://github.com/s1lentq/reapi) | GPLv3 | копия в `third_party/regamedll/cssdk` |
| [SQLite](https://sqlite.org/) | public domain | амальгама в `third_party/sqlite` |
