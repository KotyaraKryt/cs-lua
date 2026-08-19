---
title: "file.append"
description: "Дописывает в конец файла."
---

# file.append

Дописывает содержимое в конец файла, не трогая то, что уже было записано
(создаёт файл, если его не было).

```lua
file.append(name, content)
```

### Аргументы
| # | Имя       | Тип      | Описание                                |
| - | --------- | -------- | ---------------------------------------- |
| 1 | `name`    | `string` | Имя файла в каталоге плагина (см. [`file`](index.md)) |
| 2 | `content` | `string` | Что дописать, до 4 МиБ за один вызов     |

### Возвращает
`true` при успехе. При ошибке — `nil` и вторым значением причина.

### Пример

```lua
hook.add("player:death", "myplugin.kills_log", function(e)
  file.append("kills.csv", ("%s,%s\n"):format(e.attacker and e.attacker:name() or "world", e.player:name()))
end)
```

<Warning>
Предел 4 МиБ — на один вызов, не на итоговый размер файла: у `file.append`
нет ограничения на то, насколько большим станет файл со временем. Для
самоограничивающегося лога с ротацией по дням — [`log.write`](../log/index.md).
</Warning>
