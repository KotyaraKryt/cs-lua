---
title: hook — Движок и ReHLDS
description: "В отличие от «Геймплея» эти события не требуют ReGameDLL — работают и на ванильном `mp.dll`. Три из них (отмечены отдельно) требуют ReHLDS"
---

# Движок и ReHLDS

В отличие от «Геймплея» эти события не требуют ReGameDLL — работают и на ванильном `mp.dll`. Три из них (отмечены отдельно) требуют ReHLDS.

## player:use {#player_use}

Игрок нажал +use на чём-то с обработчиком Use — кнопке, рычаге, multi_manager.

```lua
hook.add("player:use", id, function(e)
	...
end)
```

### Поля события {#player_use-поля события}

| поле | тип |  |
|---|---|---|
| `e.player` | player | кого касается событие |
| `e.entity` | entity \| nil | что использовали; `nil`, если оно уже пропало |

Не отменяемое: к моменту события движковый `DispatchUse` уже отработал.

## player:suicide {#player_suicide}

Игрок вот-вот покончит с собой командой `kill` из консоли.

```lua
hook.add("player:suicide", id, function(e)
	...
end)
```

### Поля события {#player_suicide-поля события}

| поле | тип |  |
|---|---|---|
| `e.player` | player | кого касается событие |

**Отмена.** `e:cancel()` — команда не срабатывает вообще, даже таймер повторного `kill` не сдвигается.

## ents:should_collide {#should_collide}

Движок решает, должны ли две сущности физически сталкиваться.

```lua
hook.add("ents:should_collide", id, function(e)
	...
end)
```

### Поля события {#should_collide-поля события}

| поле | тип |  |
|---|---|---|
| `e.entity` | entity \| nil | первая сущность |
| `e.other` | entity \| nil | вторая сущность |
| `e.collide` | boolean | запись: сталкиваются ли; по умолчанию `true` |

Не через `e:cancel()` — здесь нет «отмены», это гейт с записываемым результатом: последний обработчик, тронувший `e.collide`, и решает. Молчание любого обработчика оставляет `true` — обычную физику.

Единственная реализация этого хука в принципе: ReGameDLL сам его не предоставляет, так что тут нечего «оборачивать» — какой ответ дашь, такой и будет.

## ents:free {#ents_free}

Сущность вот-вот уничтожат — последняя точка, где её ещё можно прочитать.

```lua
hook.add("ents:free", id, function(e)
	...
end)
```

### Поля события {#ents_free-поля события}

| поле | тип |  |
|---|---|---|
| `e.entity` | entity \| nil | сущность, ещё живая на момент события |

Не отменяемое: держать сущность после этого события всё равно нельзя, движок продолжит её разрушать сразу после.

## player:cvar_value {#player_cvar_value}

Клиент ответил на запрос значения cvar — старый вариант API, без имени cvar и без id запроса.

```lua
hook.add("player:cvar_value", id, function(e)
	...
end)
```

### Поля события {#player_cvar_value-поля события}

| поле | тип |  |
|---|---|---|
| `e.player` | player | кого касается событие |
| `e.value` | string | значение, которое прислал клиент |

Плагин, который запрашивал значение, обязан сам помнить, о каком cvar шла речь — движок это не передаёт.

### Смотри также

- [player:cvar_value2](engine.md#player_cvar_value2)

## player:cvar_value2 {#player_cvar_value2}

То же самое, но новый вариант API: с именем cvar и id запроса, чтобы сверить ответ с тем, что спрашивали.

```lua
hook.add("player:cvar_value2", id, function(e)
	...
end)
```

### Поля события {#player_cvar_value2-поля события}

| поле | тип |  |
|---|---|---|
| `e.player` | player | кого касается событие |
| `e.request_id` | number | id, который был передан при запросе |
| `e.cvar` | string | имя cvar, про который спросили |
| `e.value` | string | значение, которое прислал клиент |

### Смотри также

- [player:cvar_value](engine.md#player_cvar_value)

## server:cvar_change {#server_cvar_change}

Любой cvar вот-вот изменится — движковый, чужого плагина, свой.

```lua
hook.add("server:cvar_change", id, function(e)
	...
end)
```

### Поля события {#server_cvar_change-поля события}

| поле | тип |  |
|---|---|---|
| `e.name` | string | имя cvar |
| `e.value` | string | новое значение |

**Отмена.** `e:cancel()` — cvar остаётся со старым значением.

Обработчик, который сам меняет cvar изнутри этого события, войдёт в него снова — не меняй здесь тот же cvar безусловно, иначе рекурсия не остановится.

<Warning>
Событие требует ReHLDS. На движке без него — тихо никогда не
срабатывает, без ошибок и предупреждений.
</Warning>

## server:precache_generic {#server_precache_generic}

Прекеширован generic-ресурс (не модель и не звук) — движком, игрой, другим плагином.

```lua
hook.add("server:precache_generic", id, function(e)
	...
end)
```

### Поля события {#server_precache_generic-поля события}

| поле | тип |  |
|---|---|---|
| `e.name` | string | путь ресурса |
| `e.index` | number | слот прекеша |

<Warning>
Событие требует ReHLDS. На движке без него — тихо никогда не
срабатывает, без ошибок и предупреждений.
</Warning>

## client:bot_created {#bot_created}

Подключился бот (fake client).

```lua
hook.add("client:bot_created", id, function(e)
	...
end)
```

### Поля события {#bot_created-поля события}

| поле | тип |  |
|---|---|---|
| `e.player` | player | кого касается событие |

<Warning>
Событие требует ReHLDS. На движке без него — тихо никогда не
срабатывает, без ошибок и предупреждений.
</Warning>
