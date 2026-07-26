---
title: access
description: "Права как именованные ноды, группы и иммунитет"
---

# access

Права как именованные ноды, группы и иммунитет.

Право — именованная нода: `admin.kick`, `shop.vip.buy`.

```lua
if p:can("shop.vip.buy") then ... end

cmd.add("slay", function(ctx)
	ctx.target:slay()
end, { perm = "admin.slay", target = 1 })
```

> [!NOTE]
> У консоли и rcon прав нет — им разрешено всё. `ctx.player` там `nil`, любая
> проверка проходит.

Методы игрока — [`p:can`](../players/can.md), [`p:groups`](../players/groups.md),
[`p:group`](../players/group.md), [`p:weight`](../players/weight.md),
[`p:outranks`](../players/outranks.md).

## Файлы

`data/groups.lua` правится руками, модуль его только читает.

```lua
return {
	default   = { weight = 0,   allow = { "chat.say" } },
	vip       = { weight = 10,  inherit = "default", allow = { "shop.vip.*" } },
	moderator = { weight = 30,  inherit = "vip", allow = { "admin.kick" } },
	admin     = { weight = 50,  inherit = "moderator", allow = { "admin.*" },
	              deny = { "admin.rcon" } },
	owner     = { weight = 100, inherit = "admin", allow = { "*" } },
}
```

`data/users.lua` — кто есть кто. Ключ записи: steamid или `name:<ник>`.

```lua
return {
	["STEAM_0:1:12345"] = { name = "kotyarakryt", groups = { "owner" } },
	["STEAM_0:0:99999"] = { groups = { "vip" }, until_ = 1785000000 },
	["name:Helper"]     = { password = "secret", groups = { "moderator" },
	                        where = { map = "de_dust2" } },
}
```

| поле | |
|---|---|
| `until_` | unix-время; при выдаче из кода — `"30d"`, `"12h"` или `"map"` |
| `where` | `{ map = "de_dust2" }` или `{ maps = {...} }`; есть и у групп |
| `password` | сверяется с `setinfo _pw` на клиенте |

Оба файла необязательны: без `groups.lua` работают встроенные `default`, `admin`
и `owner`. Битый файл даёт ошибку в консоль и пустую половину прав, а не
половинчатые права.

> [!WARNING]
> Вход по нику годится только для LAN: ник подделывается тривиально, а пароль
> передаётся открытым текстом. На публичном сервере выдавай права по steamid.

## Разрешение конфликтов

Нода в выдаче может быть с хвостовой звёздочкой (`shop.vip.*`, `*`) и с минусом
впереди — `-admin.ban` означает явный запрет.

Порядок в файле роли не играет. Побеждает запись, которая выигрывает по
признакам, в этом порядке:

1. **источник** — личная запись > своя группа > унаследованная > `default` ноды;
2. **вес** — среди групп одного уровня выигрывает более тяжёлая;
3. **точность** — `shop.vip.buy` > `shop.vip.*` > `shop.*` > `*`;
4. **запрет** — при полном равенстве выигрывает `deny`.

Поэтому `admin` с `allow = {"admin.*"}` и `deny = {"admin.rcon"}` получает всё
кроме rcon, а личный `allow = {"admin.rcon"}` в записи игрока это перебивает.

## Иммунитет

Иммунитет — это `weight` группы. Действие против другого игрока требует **строго
большего** веса, поэтому два админа одного ранга друг друга не тронут.

Вручную — [`p:outranks`](../players/outranks.md). Декларативно — `opts.target` в
[`cmd.add`](../cmd/add.md).

Права считаются лениво и кешируются на слот. Кеш сбрасывается сам: на
`player_authorized`, при смене карты, по истечении срока и при любом изменении
прав.

## Объявление

|  |  |
|---|---|
| [`access.declare`](declare.md) | Объявляет ноду прав |
| [`access.rule`](rule.md) | Задаёт динамическое правило для ноды |

## Выдача

|  |  |
|---|---|
| [`access.grant`](grant.md) | Выдаёт права по ключу |
| [`access.revoke`](revoke.md) | Забирает запись, группу или ноду |
| [`access.save`](save.md) | Записывает `users.lua` на диск |
| [`access.reload`](reload.md) | Перечитывает `groups.lua` и `users.lua` с диска |
| [`access.invalidate`](invalidate.md) | Сбрасывает кеш прав |

## Чтение

|  |  |
|---|---|
| [`access.can`](can.md) | Есть ли у игрока право |
| [`access.permissions`](permissions.md) | Все объявленные ноды |
| [`access.users`](users.md) | Все записи из `users.lua` |
| [`access.user`](user.md) | Одна запись из `users.lua` |
| [`access.all_groups`](all_groups.md) | Все группы |
| [`access.group`](group.md) | Одна группа по имени |
