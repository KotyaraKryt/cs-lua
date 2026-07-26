---
title: players.method
description: "Добавляет свой метод всем объектам игроков"
---

# players.method

Добавляет свой метод всем объектам игроков.

```lua
players.method(name, fn)
```

## Аргументы

| # | имя | тип |  |
|---|---|---|---|
| 1 | `name` | string | имя метода |
| 2 | `fn` | function | получает `(self, ...)` |

## Возвращает

Ничего.

## Пример

```lua
players.method("playtime", function(self)
	return sessions[self.id] and sessions[self.id]:seconds() or 0
end)
```

> [!WARNING]
> Занятое имя переопределить нельзя — это то, что не даёт одному
> плагину подменить `p:health()` для всех остальных.
