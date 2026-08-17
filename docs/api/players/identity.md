---
title: players — Идентификация
description: "Поиск игроков, рассылка и объект игрока"
---

# Идентификация

## p.id {#id}

Номер слота игрока.

```lua
p.id
```

### Возвращает

| тип |  |
|---|---|
| `number` | 1..32, постоянен пока игрок на сервере |

Поле, а не метод. Годится ключом в своих таблицах.

## p:name {#name}

Ник игрока.

```lua
p:name()
```

### Возвращает

| тип |  |
|---|---|
| `string` | ник игрока |

## p:ip {#ip}

Адрес игрока вида `1.2.3.4:27005`.

```lua
p:ip()
```

### Возвращает

| тип |  |
|---|---|
| `string` | адрес игрока вида `1.2.3.4:27005` |

## p:steamid {#steamid}

SteamID игрока.

```lua
p:steamid()
```

### Возвращает

| тип |  |
|---|---|
| `string` | steamid игрока |

`STEAM_0:...` у Steam-клиента, `HLTV` у прокси, `STEAM_ID_LAN` без Steam.

<Warning>
В событии `client_connect` возвращает `STEAM_ID_PENDING` —
Steam отвечает позже. Всё, что завязано на steamid, вешай на
[`player_authorized`](../hook/connection.md#player_authorized).
</Warning>

## p:userid {#userid}

Userid движка.

```lua
p:userid()
```

### Возвращает

| тип |  |
|---|---|
| `number` | userid движка |

`-1`, если игрока в слоте нет.

## p:info {#info}

Читает ключ инфобуфера клиента.

```lua
p:info(key)
```

### Аргументы

| # | имя | тип |  |
|---|---|---|---|
| 1 | `key` | string | имя ключа, например `_pw` |

### Возвращает

| тип |  |
|---|---|
| `string` | читает ключ инфобуфера клиента |

### Пример

```lua
local password = p:info("_pw")
```

## p:connected {#connected}

Занят ли слот.

```lua
p:connected()
```

### Возвращает

| тип |  |
|---|---|
| `boolean` | занят ли слот |

Единственная безопасная проверка перед обращением к состоянию из отложенного кода.

## p:is_bot {#is_bot}

Серверный бот (`FL_FAKECLIENT`).

```lua
p:is_bot()
```

### Возвращает

| тип |  |
|---|---|
| `boolean` | серверный бот (`fl_fakeclient`) |

## p:is_hltv {#is_hltv}

HLTV-прокси (`FL_PROXY`).

```lua
p:is_hltv()
```

### Возвращает

| тип |  |
|---|---|
| `boolean` | hltv-прокси (`fl_proxy`) |
