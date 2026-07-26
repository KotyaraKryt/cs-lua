---
title: http.get
description: "Выполняет GET-запрос"
---

# http.get

Выполняет GET-запрос.

```lua
http.get(url[, opts], fn)
```

## Аргументы

| # | имя | тип |  |
|---|---|---|---|
| 1 | `url` | string | адрес целиком, вместе со схемой |
| 2 | `opts` | table \| nil | см. [Опции](#опции) |
| 3 | `fn` | function | получает объект ответа |

## Возвращает

| тип |  |
|---|---|
| `number` | id для [`http.cancel`](cancel.md) |

## Опции

| поле | тип |  |
|---|---|---|
| `headers` | table | `{ ["Authorization"] = "Bearer ..." }` |
| `timeout` | number | секунд, по умолчанию `10` |

## Пример

```lua
http.get("https://api.github.com/repos/rehlds/ReHLDS", function(res)
	if res.ok then
		print(require("json").decode(res.body).stargazers_count)
	end
end)
```

## Смотри также

- [json](../../plugins.md#include)
