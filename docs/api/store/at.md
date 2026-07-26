---
title: datafile.at
description: "Привязывает модуль к своему каталогу"
---

# datafile.at

Привязывает модуль к своему каталогу.

```lua
datafile.at(dir)
```

## Аргументы

| # | имя | тип |  |
|---|---|---|---|
| 1 | `dir` | string | существующий каталог |

## Возвращает

| тип |  |
|---|---|
| `table` | тот же набор функций, но в этом каталоге |

## Пример

```lua
local files = require("datafile").at(plugin.data_dir())
```

Без `at()` функции работают в общем `addons/lua/data/`, где лежит `users.lua` ядра.
