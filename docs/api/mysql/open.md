---
title: mysql — Открытие
description: "MySQL/MariaDB: удалённая база, не блокирующая кадр"
---

# Открытие

## mysql.connect {#connect}

Открывает соединение с сайтовой базой.

```lua
mysql.connect(opts)
```

### Аргументы

| # | имя | тип |  |
|---|---|---|---|
| 1 | `opts` | table | см. [Опции](#connect-опции) |

### Возвращает

| тип |  |
|---|---|
| `conn` | объект соединения — не ждёт настоящего TCP-подключения |

### Опции {#connect-опции}

| поле | тип |  |
|---|---|---|
| `host` | string | по умолчанию `127.0.0.1` |
| `port` | number | по умолчанию `3306` |
| `user` | string |  |
| `password` | string |  |
| `database` | string |  |
| `charset` | string | по умолчанию `utf8` |

### Пример

```lua
local site = mysql.connect{
    host = "127.0.0.1", user = "root", password = "", database = "gamecms",
}
```

<Note>
Открывается лениво: ошибка в адресе или пароле проявится не здесь,
а в `res.error` первого запроса.
</Note>
