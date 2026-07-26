---
title: p:dhud
description: "То же через `SVC_DIRECTOR` — directed HUD"
---

# p:dhud

То же через `SVC_DIRECTOR` — directed HUD.

```lua
p:dhud(text[, opts])
```

## Аргументы

| # | имя | тип |  |
|---|---|---|---|
| 1 | `text` | string | до 127 байт |
| 2 | `opts` | table \| nil | как у [`p:hud`](hud.md), кроме `channel` и `color2` |

## Возвращает

Ничего.

Держит до 8 сообщений одновременно.
