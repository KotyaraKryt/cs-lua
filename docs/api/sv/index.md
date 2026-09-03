---
title: sv
description: "Сервер: часы, карта, консоль, переменные движка"
---

# sv

Сервер: часы, карта, консоль, переменные движка.

```lua
print("Карта: " .. sv.map() .. ", время раунда: " .. sv.cvar("mp_roundtime"):int())
```

## Константы

| поле | тип | |
|---|---|---|
| `sv.version` | string | версия модуля, `"1.0.0"` |
| `sv.api` | number | версия Lua-API |
| `sv.dir` | string | абсолютный путь `addons/lua` |

## Сервер

|  |  |
|---|---|
| [`sv.time`](server.md#time) | Серверные часы в секундах |
| [`sv.map`](server.md#map) | Имя текущей карты |
| [`sv.maps`](server.md#maps) | Список карт, установленных на сервере |
| [`sv.map_exists`](server.md#map_exists) | Есть ли карта на диске |
| [`sv.cmd`](server.md#cmd) | Ставит команду в очередь серверной консоли |

## Cvar

|  |  |
|---|---|
| [`sv.cvar`](cvar.md#cvar) | Возвращает объект существующей переменной движка |
| [`sv.cvar_register`](cvar.md#cvar_register) | Заводит свою переменную движка |
| [`c:int`](cvar.md#int) | Значение переменной как целое |
| [`c:float`](cvar.md#float) | Значение переменной как число |
| [`c:str`](cvar.md#str) | Значение переменной как строка |
| [`c:set`](cvar.md#set) | Записывает значение переменной |
| [`c:name`](cvar.md#name) | Имя переменной |
