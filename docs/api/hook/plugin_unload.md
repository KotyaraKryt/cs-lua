---
title: plugin_unload
description: "Плагин или всё состояние уходит"
---

# plugin_unload

Плагин или всё состояние уходит.

```lua
hook.add("plugin_unload", id, function(e)
	...
end)
```

## Поля события

| поле | тип |  |
|---|---|---|
| `e.plugin` | string \| nil | id уходящего плагина; `nil` — гасится всё состояние |

Подписываться стоит, если держишь ссылки на чужой плагин. Для снятия собственных эффектов есть [`plugin.on_unload`](../plugin/on_unload.md) — он про твой плагин.
