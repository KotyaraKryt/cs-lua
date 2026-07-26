---
title: http.post
description: "Выполняет POST-запрос с телом"
---

# http.post

Выполняет POST-запрос с телом.

```lua
http.post(url, body[, opts], fn)
```

## Аргументы

| # | имя | тип |  |
|---|---|---|---|
| 1 | `url` | string | адрес |
| 2 | `body` | string | тело; для JSON — `json.encode(t)` |
| 3 | `opts` | table \| nil | как у [`http.get`](get.md) |
| 4 | `fn` | function | получает объект ответа |

## Возвращает

| тип |  |
|---|---|
| `number` | id для [`http.cancel`](cancel.md) |

## Пример

```lua
local json = require("json")

http.post("https://discord.com/api/webhooks/...",
	json.encode({ content = p:name() .. " зашёл на сервер" }),
	{ headers = { ["Content-Type"] = "application/json" } },
	function(res)
		if not res.ok then print(res.error) end
	end)
```

> [!NOTE]
> Заголовок `Content-Type` не подставляется сам: сервер на той\nстороне обычно требует конкретный, и угадывать его — плохая\nидея.
