---
title: hook
description: "Одна система на движковые события и на свои"
---

# hook

Одна система на движковые события и на свои.

Обработчик получает **одну таблицу** — объект события. Возврат не читается:
чтобы что-то изменить, пиши в поле, чтобы отменить — вызови `e:cancel()`.

```lua
hook.add("player_hurt", "myplugin.double", function(e)
	e.damage = e.damage * 2
end)
```

Одна и та же таблица достаётся всей цепочке, поэтому второй обработчик видит
правки первого. У каждого события есть `e.name` и `e.cancelled`.

## Пространство имён

|  |  |
|---|---|
| [`hook.add`](namespace.md#add) | Подписывает функцию на событие |
| [`hook.remove`](namespace.md#remove) | Снимает подписку по имени события и id |
| [`hook.run`](namespace.md#run) | Запускает своё событие плагина |
| [`hook.list`](namespace.md#list) | Возвращает список подписок в порядке вызова |

## Подключение

Порядок: `client_connect` → `player_authorized` → `player_ready`.

|  |  |
|---|---|
| [`client_connect`](connection.md#client_connect) | Игрок стучится на сервер; его ещё можно не пустить |
| [`client_disconnect`](connection.md#client_disconnect) | Игрок отключился |
| [`player_authorized`](connection.md#player_authorized) | Steam ответил, steamid наконец известен |
| [`player_ready`](connection.md#player_ready) | Игрок в игре, сообщения до него доходят |
| [`player_chat`](connection.md#player_chat) | Игрок написал в чат |
| [`menu_select`](connection.md#menu_select) | Игрок нажал клавишу в меню, открытом из Lua |

## Жизнь сервера

|  |  |
|---|---|
| [`map_change`](lifecycle.md#map_change) | Карта заканчивается |
| [`plugin_unload`](lifecycle.md#plugin_unload) | Плагин или всё состояние уходит |

## Геймплей

Всё в этом разделе приходит из ReGameDLL.

|  |  |
|---|---|
| [`player_spawn`](gameplay.md#player_spawn) | Игрок появился в раунде живым |
| [`player_hurt`](gameplay.md#player_hurt) | Игроку наносят урон; урон можно изменить или погасить |
| [`player_hurt_post`](gameplay.md#player_hurt_post) | Урон уже применён; только для наблюдателей |
| [`player_death`](gameplay.md#player_death) | Игрок погиб |
| [`player_team_change`](gameplay.md#player_team_change) | Игрок сменил сторону |
| [`weapon_fire`](gameplay.md#weapon_fire) | Из ствола вышел выстрел |
| [`weapon_deploy`](gameplay.md#weapon_deploy) | Оружие вот-вот покажет вьюмодель и модель в руках |
| [`grenade_throw`](gameplay.md#grenade_throw) | HE- или дымовая граната вот-вот покинёт руку |
| [`grenade_thrown`](gameplay.md#grenade_thrown) | HE- или дымовая граната только что покинула руку |
| [`grenade_explode`](gameplay.md#grenade_explode) | HE- или дымовая граната вот-вот взорвётся |

## Раунд и бомба

|  |  |
|---|---|
| [`round_start`](round.md#round_start) | Раунд начался |
| [`round_end`](round.md#round_end) | Раунд закончился |
| [`round_freeze_end`](round.md#round_freeze_end) | Заморозка кончилась, игроки могут двигаться |
| [`bomb_planted`](round.md#bomb_planted) | Бомба заложена |
| [`bomb_defused`](round.md#bomb_defused) | Попытка разминирования завершилась |
| [`bomb_exploded`](round.md#bomb_exploded) | Бомба взорвалась |
