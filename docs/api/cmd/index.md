---
title: cmd
description: "Команды чата, серверной консоли и rcon"
---

# cmd

Команды чата, серверной консоли и rcon.

Одна регистрация покрывает чат (`!name`, `/name`), консоль и rcon.

## cmd.add {#add}

Регистрирует команду.

```lua
cmd.add(name, fn[, opts])
```

### Аргументы

| # | имя | тип |  |
|---|---|---|---|
| 1 | `name` | string | имя без префикса |
| 2 | `fn` | function | получает [`ctx`](#ctx) |
| 3 | `opts` | table \| nil | см. [Опции](#add-опции) |

### Возвращает

Ничего.

### Опции {#add-опции}

| поле | тип |  |
|---|---|---|
| `source` | string \| table | `console`, `chat`, `chat_team`; список или ничего — везде |
| `perm` | string | нода прав, проверяется до вызова обработчика |
| `target` | number | номер аргумента с именем игрока → `ctx.target` |
| `immunity` | boolean | `false` отключает проверку иммунитета при `target` |

### Пример

```lua
cmd.add("heal", function(ctx)
	local p = ctx.player
	if p then p:health(p:health() + 25) end
	ctx.reply("healed")
end)

cmd.add("kick",  fn, { source = "console" })
cmd.add("plant", fn, { source = "chat_team" })
cmd.add("slay",  fn, { perm = "admin.slay", target = 1 })
```

### ctx

| поле | тип | |
|---|---|---|
| `ctx.player` | player \| nil | `nil` для консоли и rcon |
| `ctx.args` | table | слова после имени команды |
| `ctx.name` | string | имя команды |
| `ctx.source` | string | `console`, `chat`, `chat_team` |
| `ctx.reply(text)` | function | ответ туда, откуда пришла команда |
| `ctx.target` | player | игрок из аргумента при `opts.target` |
| `ctx.can(node)` | function | проверить право вызвавшего |

Распознанная чат-команда в общий чат не попадает.

С `target` роутер сам находит игрока по слоту, `#userid` или части ника и
отказывает, если у цели иммунитет не ниже.

<Note>
У консоли и rcon прав нет — им разрешено всё. `ctx.player` там
`nil`, и `perm` с `immunity` не проверяются.
</Note>

<Warning>
Имя команды регистрируется в движке навсегда. `lua_reload`
меняет только Lua-обработчик: если плагин не объявил команду
заново, она становится пустышкой.
</Warning>

### Смотри также

- [players.find](../players/namespace.md#find)
- [access](../access/index.md)

## cmd.remove {#remove}

Снимает команду.

```lua
cmd.remove(name)
```

### Аргументы

| # | имя | тип |  |
|---|---|---|---|
| 1 | `name` | string | имя команды |

### Возвращает

| тип |  |
|---|---|
| `boolean` | `true`, если команда была |

## cmd.list {#list}

Возвращает имена зарегистрированных команд.

```lua
cmd.list()
```

### Возвращает

| тип |  |
|---|---|
| `table` | отсортированный массив строк |
