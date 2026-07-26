---
title: plugin.data_dir
description: "Каталог плагина под данные, переживающий обновление"
---

# plugin.data_dir

Каталог плагина под данные, переживающий обновление.

```lua
plugin.data_dir()
```

## Возвращает

| тип |  |
|---|---|
| `string` | `addons/lua/data/<plugin_id>/` |

Создаётся при первом вызове. Туда же кладут файлы [`store`](../store/open.md) и [`db.open`](../db/open.md).
