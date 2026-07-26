---
title: http.request
description: "Выполняет запрос произвольным методом"
---

# http.request

Выполняет запрос произвольным методом.

```lua
http.request(opts, fn)
```

## Аргументы

| # | имя | тип |  |
|---|---|---|---|
| 1 | `opts` | table | см. [Опции](#опции) |
| 2 | `fn` | function | получает объект ответа |

## Возвращает

| тип |  |
|---|---|
| `number` | id для [`http.cancel`](cancel.md) |

## Опции

| поле | тип |  |
|---|---|---|
| `url` | string | обязателен |
| `method` | string | `GET` по умолчанию; `PUT`, `DELETE`, `PATCH` |
| `body` | string | тело запроса |
| `headers` | table | заголовки |
| `timeout` | number | секунд, по умолчанию `10` |

## Пример

```lua
http.request({
	url     = "https://api.example.com/bans/42",
	method  = "DELETE",
	headers = { ["Authorization"] = "Bearer " .. token },
	timeout = 5,
}, function(res)
	print(res.status)
end)
```
