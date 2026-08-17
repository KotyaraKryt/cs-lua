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

<Note>
У консоли и rcon прав нет — им разрешено всё. `ctx.player` там `nil`, любая
проверка проходит.
</Note>

Методы игрока — [`p:can`](../players/access.md#can), [`p:groups`](../players/access.md#groups),
[`p:group`](../players/access.md#group), [`p:weight`](../players/access.md#weight),
[`p:outranks`](../players/access.md#outranks).

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

<Warning>
Вход по нику годится только для LAN: ник подделывается тривиально, а пароль
передаётся открытым текстом. На публичном сервере выдавай права по steamid.
</Warning>

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

Вручную — [`p:outranks`](../players/access.md#outranks). Декларативно — `opts.target` в
[`cmd.add`](../cmd/index.md#add).

Права считаются лениво и кешируются на слот. Кеш сбрасывается сам: на
`player_authorized`, при смене карты, по истечении срока и при любом изменении
прав.

## Объявление

|  |  |
|---|---|
| [`access.declare`](declare.md#declare) | Объявляет ноду прав |
| [`access.rule`](declare.md#rule) | Задаёт динамическое правило для ноды |

## Выдача

|  |  |
|---|---|
| [`access.grant`](grant.md#grant) | Выдаёт права по ключу |
| [`access.revoke`](grant.md#revoke) | Забирает запись, группу или ноду |
| [`access.save`](grant.md#save) | Записывает `users.lua` на диск |
| [`access.reload`](grant.md#reload) | Перечитывает `groups.lua` и `users.lua` с диска |
| [`access.invalidate`](grant.md#invalidate) | Сбрасывает кеш прав |

## Чтение

|  |  |
|---|---|
| [`access.can`](read.md#can) | Есть ли у игрока право |
| [`access.permissions`](read.md#permissions) | Все объявленные ноды |
| [`access.users`](read.md#users) | Все записи из `users.lua` |
| [`access.user`](read.md#user) | Одна запись из `users.lua` |
| [`access.all_groups`](read.md#all_groups) | Все группы |
| [`access.group`](read.md#group) | Одна группа по имени |
