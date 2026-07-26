# -*- coding: utf-8 -*-
NS = {
    'dir': 'http',
    'label': 'http',
    'title': 'http',
    'brief': 'Исходящие HTTP-запросы, не блокирующие кадр.',
    'intro': """
```lua
local json = require("json")

http.get("https://api.example.com/players/" .. p:steamid(), function(res)
	if not res.ok then
		return print("не вышло: " .. res.error)
	end

	local data = json.decode(res.body)
	p:chat("ранг: " .. tostring(data.rank))
end)
```

## Как это устроено

Запрос уходит на рабочий поток, ответ возвращается в игровой — тем же проходом,
которым тикают таймеры. Ни одна строчка плагина не касается сокета, и ни один
рабочий поток не касается Lua: между ними ходят только байты.

Из этого следует главное свойство: **постановка запроса ничего не ждёт**. Вызов
возвращается сразу, кадр идёт дальше, коллбэк приходит когда придёт.

Потоков два. Один медленный адрес не мешает другому плагину, а плагин, обходящий
список игроков в цикле, не наплодит сотню потоков — лишнее встаёт в очередь.

## Транспорт

Тот, что уже есть в системе, поэтому модуль остаётся одним файлом рядом с
метамодом: ни OpenSSL, ни libcurl рядом класть не нужно.

| | |
|---|---|
| Windows | WinHTTP из состава системы, TLS через Schannel |
| Linux | libcurl, открывается при первом запросе |

На Linux libcurl не линкуется, а подгружается — поэтому для сборки не нужен
32-битный `-dev` пакет, а сервер без libcurl нормально загрузит модуль и скажет,
чего не хватает, при первом запросе.

## Объект ответа

Коллбэк получает **одну таблицу** — как и обработчик события.

| поле | тип | |
|---|---|---|
| `res.ok` | boolean | запрос дошёл и код в диапазоне 2xx–3xx |
| `res.status` | number | код ответа; `0`, если соединения не было |
| `res.body` | string | тело как есть |
| `res.error` | string \\| nil | причина: и сетевая, и `HTTP 404` |

`404` — не сбой транспорта, но и не успех: `ok` будет `false`, а `error`
заполнен, чтобы `if not res.ok then print(res.error) end` работал без разбора
частных случаев.

## Границы

| | |
|---|---|
| таймаут по умолчанию | 10 секунд |
| максимальный ответ | 4 МБ, дальше ошибка |
| редиректы | до 5, автоматически |

> [!WARNING]
> На Windows таймаут применяется к каждой фазе отдельно — разрешение имени,
> подключение, отправка, чтение. Запрос к висящему серверу вернётся примерно за
> удвоенное значение, а не ровно за него.

## Перезагрузка и выгрузка

Запросы, оставшиеся на проводе, переживают и `lua_reload`, и выгрузку своего
плагина — но их коллбэки не выполняются: они ссылаются на состояние, которого
больше нет. Ответ просто выбрасывается.

Перезагрузка при этом **не ждёт** незавершённые запросы: прервать чужой сокет
на середине нельзя, а держать сервер до конца самого медленного из них — хуже,
чем потерять его ответ.
""",
    'groups': [
        {
            'items': [
                {
                    'name': 'http.get',
                    'brief': 'Выполняет GET-запрос.',
                    'sig': 'http.get(url[, opts], fn)',
                    'args': [
                        ('url', 'string', 'адрес целиком, вместе со схемой'),
                        ('opts', 'table \\| nil', 'см. [Опции](#опции)'),
                        ('fn', 'function', 'получает объект ответа'),
                    ],
                    'returns': [('number', 'id для [`http.cancel`](cancel.md)')],
                    'fields': ('Опции', [
                        ('headers', 'table', '`{ ["Authorization"] = "Bearer ..." }`'),
                        ('timeout', 'number', 'секунд, по умолчанию `10`'),
                    ]),
                    'example': """
http.get("https://api.github.com/repos/rehlds/ReHLDS", function(res)
	if res.ok then
		print(require("json").decode(res.body).stargazers_count)
	end
end)
""",
                    'see': [('json', '../../plugins.md#include')],
                },
                {
                    'name': 'http.post',
                    'brief': 'Выполняет POST-запрос с телом.',
                    'sig': 'http.post(url, body[, opts], fn)',
                    'args': [
                        ('url', 'string', 'адрес'),
                        ('body', 'string', 'тело; для JSON — `json.encode(t)`'),
                        ('opts', 'table \\| nil', 'как у [`http.get`](get.md)'),
                        ('fn', 'function', 'получает объект ответа'),
                    ],
                    'returns': [('number', 'id для [`http.cancel`](cancel.md)')],
                    'example': """
local json = require("json")

http.post("https://discord.com/api/webhooks/...",
	json.encode({ content = p:name() .. " зашёл на сервер" }),
	{ headers = { ["Content-Type"] = "application/json" } },
	function(res)
		if not res.ok then print(res.error) end
	end)
""",
                    'notes': [('note', 'Заголовок `Content-Type` не подставляется сам: сервер на той\\nстороне обычно требует конкретный, и угадывать его — плохая\\nидея.')],
                },
                {
                    'name': 'http.request',
                    'brief': 'Выполняет запрос произвольным методом.',
                    'sig': 'http.request(opts, fn)',
                    'args': [('opts', 'table', 'см. [Опции](#опции)'),
                             ('fn', 'function', 'получает объект ответа')],
                    'returns': [('number', 'id для [`http.cancel`](cancel.md)')],
                    'fields': ('Опции', [
                        ('url', 'string', 'обязателен'),
                        ('method', 'string', '`GET` по умолчанию; `PUT`, `DELETE`, `PATCH`'),
                        ('body', 'string', 'тело запроса'),
                        ('headers', 'table', 'заголовки'),
                        ('timeout', 'number', 'секунд, по умолчанию `10`'),
                    ]),
                    'example': """
http.request({
	url     = "https://api.example.com/bans/42",
	method  = "DELETE",
	headers = { ["Authorization"] = "Bearer " .. token },
	timeout = 5,
}, function(res)
	print(res.status)
end)
""",
                },
                {
                    'name': 'http.cancel',
                    'brief': 'Отменяет запрос по id.',
                    'sig': 'http.cancel(id)',
                    'args': [('id', 'number', 'то, что вернул вызов')],
                    'returns': [('boolean', '`true`, если запрос нашёлся')],
                    'extra': 'Гарантия здесь одна: коллбэк не выполнится. Сам запрос, если он уже ушёл в сеть, доигрывается до конца — оборвать чужое соединение на середине нельзя.',
                },
            ],
        },
    ],
}
