---
title: hook
description: "Одна система на движковые события и на свои"
---

# hook

Одна система на движковые события и на свои.

Обработчик получает **одну таблицу** — объект события. Возврат не читается:
чтобы что-то изменить, пиши в поле, чтобы отменить — вызови `e:cancel()`.

```lua
hook.add("player:hurt", "myplugin.double", function(e)
	e.damage = e.damage * 2
end)
```

Одна и та же таблица достаётся всей цепочке, поэтому второй обработчик видит
правки первого. У каждого события есть `e.name` и `e.cancelled`.

Имя события — `subject:verb` через двоеточие (`player:spawn`, `weapon:buy`).
Своё событие плагина — через точку (`shop.bought`): так модуль отличает
опечатку в движковом имени от чужого события, которое просто ещё не
зарегистрировано.

## Пространство имён

|  |  |
|---|---|
| [`hook.add`](namespace.md#add) | Подписывает функцию на событие |
| [`hook.remove`](namespace.md#remove) | Снимает подписку по имени события и id |
| [`hook.run`](namespace.md#run) | Запускает своё событие плагина |
| [`hook.list`](namespace.md#list) | Возвращает список подписок в порядке вызова |

## Подключение

Порядок: `client:connect` → `client:authorized` → `client:connected`.

|  |  |
|---|---|
| [`client:connect`](connection.md#connect) | Игрок стучится на сервер; его ещё можно не пустить |
| [`client:disconnect`](connection.md#disconnect) | Игрок отключился |
| [`client:authorized`](connection.md#authorized) | Steam ответил, steamid наконец известен |
| [`client:connected`](connection.md#connected) | Игрок в игре, сообщения до него доходят |
| [`player:chat`](connection.md#chat) | Игрок написал в чат |
| [`menu:select`](connection.md#select) | Игрок нажал клавишу в меню, открытом из Lua |

## Жизнь сервера

|  |  |
|---|---|
| [`server:map_change`](lifecycle.md#map_change) | Карта заканчивается |
| [`module:plugin_unload`](lifecycle.md#plugin_unload) | Плагин или всё состояние уходит |

## Геймплей

Всё в этом разделе приходит из ReGameDLL.

|  |  |
|---|---|
| [`player:spawn`](gameplay.md#player_spawn) | Игрок появился в раунде живым |
| [`player:hurt`](gameplay.md#player_hurt) | Игроку наносят урон; урон можно изменить или погасить |
| [`player:hurt_post`](gameplay.md#player_hurt_post) | Урон уже применён; только для наблюдателей |
| [`player:trace_attack`](gameplay.md#player_trace_attack) | Один хитбокс получил попадание — раньше и точнее, чем `player:hurt`: до брони, до множителей, до суммирования дроби дробовика в одно число |
| [`player:heal`](gameplay.md#player_heal) | Игроку вот-вот дадут здоровье — аптечка, админ-команда; своей регенерации в CS нет |
| [`player:death`](gameplay.md#player_death) | Игрок погиб |
| [`player:team_change`](gameplay.md#player_team_change) | Игрок сменил сторону |
| [`player:jump`](gameplay.md#player_jump) | Игрок прыгнул |
| [`player:duck`](gameplay.md#player_duck) | Игрок присел |
| [`player:spectate`](gameplay.md#player_spectate) | Игрок перешёл в режим наблюдателя |
| [`player:radio`](gameplay.md#player_radio) | Игрок использовал радиокоманду |
| [`player:respawn_check`](gameplay.md#player_can_respawn) | Игра вот-вот решит, можно ли игроку возродиться |
| [`player:money_change`](gameplay.md#money_change) | У игрока вот-вот изменятся деньги — раундовый бонус, награда за фраг, покупка, действие с заложником и т.д |
| [`weapon:fire`](gameplay.md#weapon_fire) | Из ствола вышел выстрел |
| [`weapon:deploy`](gameplay.md#weapon_deploy) | Оружие вот-вот покажет вьюмодель и модель в руках |
| [`weapon:reload`](gameplay.md#weapon_reload) | Началась настоящая перезарядка |
| [`item:give`](gameplay.md#item_give) | Движок вот-вот выдаст игроку предмет по имени classname |
| [`player:strip`](gameplay.md#player_strip) | У игрока только что забрали весь инвентарь |
| [`player:can_have_item`](gameplay.md#player_can_have_item) | Игра вот-вот решит, может ли игрок вообще получить этот предмет |
| [`weapon:pickup`](gameplay.md#weapon_pickup) | Игрок подобрал оружие с земли |
| [`weapon:throw`](gameplay.md#weapon_throw) | Любой гранатный слот вот-вот покинёт руку, до того как движок решил, HE это, дымовая или флешка |
| [`weapon:secondary_attack`](gameplay.md#weapon_secondary_attack) | Игрок нажал правую кнопку мыши, держа это оружие |
| [`grenade:throw`](gameplay.md#grenade_throw) | HE, дымовая или флешка вот-вот покинёт руку |
| [`grenade:thrown`](gameplay.md#grenade_thrown) | HE, дымовая или флешка только что покинула руку |
| [`grenade:explode`](gameplay.md#grenade_explode) | HE, дымовая или флешка вот-вот взорвётся |

## Раунд и бомба

|  |  |
|---|---|
| [`round:start`](round.md#round_start) | Раунд начался |
| [`round:end`](round.md#round_end) | Раунд закончился |
| [`round:freeze_end`](round.md#round_freeze_end) | Заморозка кончилась, игроки могут двигаться |
| [`round:balance_teams`](round.md#round_balance_teams) | Автобаланс только что раскидал игроков по командам |
| [`round:intermission`](round.md#round_intermission) | Карта закончилась, сервер вот-вот покажет табло перед сменой карты |
| [`bomb:planted`](round.md#bomb_planted) | Бомба заложена |
| [`bomb:defusing`](round.md#bomb_defuse_start) | Игрок начал разминирование |
| [`bomb:defused`](round.md#bomb_defused) | Попытка разминирования завершилась |
| [`bomb:exploded`](round.md#bomb_exploded) | Бомба взорвалась |

## Магазин

|  |  |
|---|---|
| [`weapon:buy`](shop.md#weapon_buy) | Игрок покупает оружие в магазине |
| [`ammo:buy`](shop.md#ammo_buy) | Игрок покупает патроны для оружия в руках |
| [`item:buy`](shop.md#item_buy) | Игрок покупает не-оружие в магазине: броню, прибор ночного видения, набор для разминирования, щит или гранату |
| [`ammo:pickup`](shop.md#ammo_pickup) | Игроку вот-вот добавят патронов в запас — подбор с земли или дозарядка через магазин (отдельно от `ammo:buy`, которое про сам факт покупки) |
| [`weapon:drop`](shop.md#weapon_drop) | Игрок вот-вот выбросит оружие на пол — командой `drop`, из `p:drop()` или при потере слота |
