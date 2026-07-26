---
title: p:steamid
description: "SteamID игрока"
---

# p:steamid

SteamID игрока.

```lua
p:steamid()
```

## Возвращает

| тип |  |
|---|---|
| `string` | steamid игрока |

`STEAM_0:...` у Steam-клиента, `HLTV` у прокси, `STEAM_ID_LAN` без Steam.

> [!WARNING]
> В событии `client_connect` возвращает `STEAM_ID_PENDING` —
> Steam отвечает позже. Всё, что завязано на steamid, вешай на
> [`player_authorized`](../hook/player_authorized.md).
