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

`play_sound` тут — один `EMIT_SOUND` от первого подключённого игрока, а не
цикл по всем: `EMIT_SOUND` и так слышен всем, у кого PAS накрывает точку
излучения, повторять его на каждого — значит дать части слушателей услышать
один и тот же клип по два-три раза подряд.

> [!WARNING]
> Состояния у него нет: `players.broadcast:alive()` бросает ошибку
> с объяснением. Для чтения и записи состояния перебирай
> `players.list()`.
