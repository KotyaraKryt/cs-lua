---
title: plugin.id
description: "Имя папки плагина"
---

# plugin.id

Имя папки плагина.

```lua
plugin.id()
```

## Возвращает

| тип |  |
|---|---|
| `string \| nil` | `nil` в core-слое |

Это и есть идентичность плагина — на неё ключуется реестр [`export`](../exports/index.md).
