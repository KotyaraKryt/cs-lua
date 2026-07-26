---
title: access.grant
description: "Выдаёт права по ключу"
---

# access.grant

Выдаёт права по ключу.

```lua
access.grant(key, spec)
```

## Аргументы

| # | имя | тип |  |
|---|---|---|---|
| 1 | `key` | string | steamid или `name:<ник>` |
| 2 | `spec` | table | см. [Поля](#поля) |

## Возвращает

| тип |  |
|---|---|
| `boolean` | `true` при успехе |
| `string` | причина при ошибке |

## Поля

| поле | тип |  |
|---|---|---|
| `groups` | string \| table | группа или список групп |
| `allow` | string \| table | ноды, которые выдать |
| `deny` | string \| table | ноды, которые запретить |
| `until_` | string \| number | `"30d"`, `"12h"`, `"map"` или unix-время |
| `where` | table | `{ map = }` или `{ maps = {} }` |

> [!NOTE]
> Действует сразу; в файл попадает только после
> [`access.save`](save.md).
