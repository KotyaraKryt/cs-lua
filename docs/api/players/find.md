---
title: players.find
description: "Ищет игрока по слоту, userid или части ника"
---

# players.find

Ищет игрока по слоту, userid или части ника.

```lua
players.find(token)
```

## Аргументы

| # | имя | тип |  |
|---|---|---|---|
| 1 | `token` | string | см. [Форматы](#форматы) |

## Возвращает

| тип |  |
|---|---|
| `player \| nil` | найденный игрок |
| `string` | причина, если не найден |

## Форматы

| поле | тип |  |
|---|---|---|
| `"3"` | string | номер слота |
| `"#12"` | string | userid |
| `"kotya"` | string | часть ника, регистр не важен; совпадение должно быть одно |

## Пример

```lua
local target, why = players.find(ctx.args[1])
if not target then
	return ctx.reply(why)
end
```

Тот же поиск делает `opts.target` в [`cmd.add`](../cmd/add.md).
