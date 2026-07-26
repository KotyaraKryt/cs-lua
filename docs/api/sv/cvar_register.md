---
title: sv.cvar_register
description: "Заводит свою переменную движка"
---

# sv.cvar_register

Заводит свою переменную движка.

```lua
sv.cvar_register(name, default[, flags])
```

## Аргументы

| # | имя | тип |  |
|---|---|---|---|
| 1 | `name` | string | имя переменной |
| 2 | `default` | string | значение по умолчанию, строкой |
| 3 | `flags` | table \| nil | см. [Флаги](#флаги) |

## Возвращает

| тип |  |
|---|---|
| `cvar` | объект переменной |

## Флаги

| поле | тип |  |
|---|---|---|
| `archive` | boolean | значение сохраняется в конфиг |
| `server` | boolean | игроки получают уведомление об изменении |
| `protected` | boolean | значение не отдаётся клиентам, для паролей |

## Пример

```lua
local enabled = sv.cvar_register("greet_enabled", "1", { archive = true })
if enabled:int() ~= 0 then ... end
```

Повторный вызов для существующего имени возвращает объект, не сбрасывая значение, поэтому `lua_reload` cvar'ы не трогает.
