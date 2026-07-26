---
title: access.save
description: "Записывает `users.lua` на диск"
---

# access.save

Записывает `users.lua` на диск.

```lua
access.save()
```

## Возвращает

| тип |  |
|---|---|
| `boolean` | `true` при успехе |
| `string` | причина при ошибке |

Атомарно, с `.bak` — как [`datafile.save`](../store/save.md).
