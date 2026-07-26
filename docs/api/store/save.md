---
title: datafile.save
description: "Пишет таблицу в `<dir>/<name>.lua`"
---

# datafile.save

Пишет таблицу в `<dir>/<name>.lua`.

```lua
datafile.save(name, t[, header])
```

## Аргументы

| # | имя | тип |  |
|---|---|---|---|
| 1 | `name` | string | имя файла без расширения |
| 2 | `t` | table | что записать |
| 3 | `header` | string \| nil | комментарий первой строкой |

## Возвращает

| тип |  |
|---|---|
| `boolean` | `true` при успехе |
| `string` | причина при ошибке |
