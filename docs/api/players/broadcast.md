---
title: players.broadcast
description: "Приёмник «всем сразу»: только отправка сообщений"
---

# players.broadcast

Приёмник «всем сразу»: только отправка сообщений.

```lua
players.broadcast:chat(text[, opts])
```

## Пример

```lua
players.broadcast:chat("{green}[Server]{default} раунд начался")
players.broadcast:play_sound("items/9mmclip1.wav")
```

Понимает те же методы отправки, что и обычный игрок:
[`chat`](chat.md), [`console`](console.md), [`center`](center.md),
[`hud`](hud.md), [`dhud`](dhud.md), [`play_sound`](play_sound.md).

> [!WARNING]
> Состояния у него нет: `players.broadcast:alive()` бросает ошибку
> с объяснением. Для чтения и записи состояния перебирай
> `players.list()`.
