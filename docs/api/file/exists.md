---
title: "file.exists"
description: "Проверяет, есть ли файл."
---

# file.exists

Проверяет, есть ли файл в каталоге плагина.

```lua
file.exists(name)
```

### Аргументы
| # | Имя    | Тип      | Описание                                |
| - | ------ | -------- | ---------------------------------------- |
| 1 | `name` | `string` | Имя файла (см. [`file`](index.md))       |

### Возвращает
`true`/`false`. Никогда не ошибка: неверное имя или отсутствие каталога
плагина — тоже `false`, а не `nil, err`.

### Пример

```lua
if not file.exists("config.json") then
  file.write("config.json", "{}")
end
```
