---
title: plugin
description: "Манифест, пути плагина и его выгрузка"
---

# plugin

Манифест, пути плагина и его выгрузка.

`plugin` — одновременно вызов манифеста и пространство имён.

## plugin\{\} {#manifest}

Объявляет метаданные и требования плагина.

```lua
plugin { name = , version = , author = , api_version = , min_engine_version = , max_engine_version = , requires = }
```

### Поля {#manifest-поля}

| поле | тип |  |
|---|---|---|
| `name` | string | имя в `lua_list` |
| `version` | string | версия плагина |
| `author` | string | автор |
| `api_version` | number | версия Lua-API, под которую написан плагин |
| `min_engine_version` | string | минимальная сборка cs-lua, `"X.Y.Z"` |
| `max_engine_version` | string | максимальная проверенная сборка cs-lua, `"X.Y.Z"` |
| `requires` | table | модули, которые должны резолвиться через `require` |

### Пример

```lua
plugin {
	name               = "Shop",
	version            = "1.0",
	author             = "kotyarakryt",
	api_version        = 2,
	min_engine_version = "2.1.0",
	requires           = { "class" },
}
```

Вызывается один раз, в `manifest.lua` — это единственная обязанность этого
файла. Движок грузит `manifest.lua` раньше `init.lua` и требует, чтобы `plugin{}`
там прозвучал: если нет, плагин не запускается и `init.lua` не выполняется вовсе.
См. [структуру плагина](../../plugins.md).

`requires` проверяется при загрузке: отсутствующий модуль останавливает плагин на
первой строке `manifest.lua` с указанием, чего не хватает.

`api_version` меняется только на ломающем изменении API — новый натив
(например `http_server`) может появиться в сборке, которая всё ещё говорит
`api_version = 2`. Для этого `min_engine_version`: точнее, чем `api_version`, и не
требует ждать следующего v3. `max_engine_version` — обратный случай, «плагин
проверен только по эту сборку», отказ загрузки на более новой. Сравниваются
как `X.Y.Z`, отсутствующая часть читается как `0`. Текущая сборка — `sv.version`.

<Warning>
`api_version` ниже текущего — отказ загрузки: v2 переименовал
весь API, см. [Переход с v1 на v2](../../migration.md). Выше
текущего — тоже отказ.
</Warning>

<Note>
Лёгкая опечатка: `plugin = { ... }` вместо `plugin { ... }`
присваивает таблицу и затирает пространство имён — движок решит, что
манифест ничего не объявил, и остановит загрузку с ошибкой в консоли.
</Note>

## plugin.id {#id}

Имя папки плагина.

```lua
plugin.id()
```

### Возвращает

| тип |  |
|---|---|
| `string \| nil` | `nil` в core-слое |

Это и есть идентичность плагина — на неё ключуется реестр [`export`](../exports/index.md).

## plugin.dir {#dir}

Абсолютный путь к папке плагина.

```lua
plugin.dir()
```

### Возвращает

| тип |  |
|---|---|
| `string \| nil` | `nil` в core-слое |

<Warning>
Здесь лежит код, и установщик перезаписывает его при обновлении.
Данные клади в [`plugin.data_dir()`](index.md#data_dir).
</Warning>

## plugin.data_dir {#data_dir}

Каталог плагина под данные, переживающий обновление.

```lua
plugin.data_dir()
```

### Возвращает

| тип |  |
|---|---|
| `string` | `addons/lua/data/<plugin_id>/` |

Создаётся при первом вызове. Туда же кладут файлы [`store`](../store/kv.md#open) и [`db.open`](../db/open.md#open).

## plugin.on_unload {#on_unload}

Регистрирует обработчик выгрузки этого плагина.

```lua
plugin.on_unload(fn)
```

### Аргументы

| # | имя | тип |  |
|---|---|---|---|
| 1 | `fn` | function | что выполнить перед выгрузкой |

### Возвращает

Ничего.

### Пример

```lua
plugin.on_unload(function()
	for _, p in ipairs(players.list()) do
		p:gravity(1.0)
	end
end)
```

Срабатывает и при `lua_reload`, и при `lua_reload <plugin>`. Обработчики
выполняются в обратном порядке регистрации — плагин разбирает себя так же, как
собирал.

Выполняются **до** того, как снимаются хендлеры, таймеры и базы плагина, поэтому
внутри всё ещё работает.

### Смотри также

- [plugin_unload](../hook/lifecycle.md#plugin_unload)
