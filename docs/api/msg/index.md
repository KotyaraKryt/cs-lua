---
title: msg
description: "Отправка сырых сетевых сообщений движка — своих и стоковых"
---

# msg

Отправка сырых сетевых сообщений движка — своих и стоковых.

`msg` отправляет сетевое user-сообщение напрямую — то же самое, что
`MESSAGE_BEGIN`/`WRITE_*`/`MESSAGE_END` в SDK, но одним вызовом. Подходит и
для стоковых сообщений игры (`TextMsg`, `ScreenFade`, `Damage` и другие,
которые уже умеет читать клиент), и для полностью своих — под клиентский мод,
который сам их разбирает.

```lua
msg.send("one", "TextMsg", e.player,
  msg.byte(4), msg.string("#Game_dead"))
```

## Почему не begin/write/end

В SDK `MESSAGE_BEGIN` открывает сообщение, `WRITE_*` пишут в него поля,
`MESSAGE_END` закрывает. Если дать Lua самому вызывать эти шаги по очереди,
любая ошибка между `begin` и `end` — не то поле, `nil`, что угодно —
оставит сообщение недописанным. Это не падение одного плагина: битый или
недооткрытый пакет уходит в общий сетевой поток движка и задевает всех
на сервере.

Поэтому `msg.send` — один вызов. Все аргументы и все поля собираются и
проверяются в Lua заранее; `MESSAGE_BEGIN` вызывается только тогда, когда
упасть уже физически нечему.

Приём чужих сообщений — не здесь, а в [`hook.add("msg:Name", ...)`](../hook/namespace.md#add-msg).

## Отправка

|  |  |
|---|---|
| [`msg.send`](send.md#send) | Собирает и отправляет одно сетевое сообщение |

## Поля сообщения

Каждая функция описывает одно поле в порядке провода — сама ничего не
отправляет, только проверяет диапазон и возвращает значение, которое
`msg.send` пишет по очереди.

```lua
msg.send("one", "TextMsg", e.player,
  msg.byte(4), msg.string("hello"))
```

|  |  |
|---|---|
| [`msg.byte`](fields.md#byte) | n — `0..255` (`WRITE_BYTE` в SDK) |
| [`msg.char`](fields.md#char) | n — `-128..127` (`WRITE_CHAR` в SDK) |
| [`msg.short`](fields.md#short) | n — `-32768..32767` (`WRITE_SHORT` в SDK) |
| [`msg.long`](fields.md#long) | n — любое 32-битное целое (`WRITE_LONG` в SDK) |
| [`msg.angle`](fields.md#angle) | f — число, градусы (`WRITE_ANGLE` в SDK) |
| [`msg.coord`](fields.md#coord) | f — число, игровые единицы (`WRITE_COORD` в SDK) |
| [`msg.string`](fields.md#string) | s — до 191 байта (`WRITE_STRING` в SDK) |
| [`msg.entity`](fields.md#entity) | v — игрок или объект `ents`, индекс `>= 1` (`WRITE_ENTITY` в SDK) |
