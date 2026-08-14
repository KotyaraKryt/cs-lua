---
title: e:detach
description: "Отменяет e:attach — сущность перестаёт следовать"
---

# e:detach

Отменяет [`e:attach`](attach.md) — сущность перестаёт следовать и остаётся
там, где была на последнем кадре.

```lua
e:detach()
```

## Возвращает

Ничего.

Ставит `movetype` обратно в `0` (`MOVETYPE_NONE`) и сбрасывает `aiment`.
Саму сущность не убирает — для этого [`e:remove`](remove.md).

## Смотри также

- [e:attach](attach.md)
