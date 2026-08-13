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
| [`hook.add`](add.md) | Подписывает функцию на событие |
| [`hook.remove`](remove.md) | Снимает подписку по имени события и id |
| [`hook.run`](run.md) | Запускает своё событие плагина |
| [`hook.list`](list.md) | Возвращает список подписок в порядке вызова |

## Подключение

Порядок: `client_connect` → `player_authorized` → `player_ready`.

|  |  |
|---|---|
| [`client_connect`](client_connect.md) | Игрок стучится на сервер; его ещё можно не пустить |
| [`client_disconnect`](client_disconnect.md) | Игрок отключился |
| [`player_authorized`](player_authorized.md) | Steam ответил, steamid наконец известен |
| [`player_ready`](player_ready.md) | Игрок в игре, сообщения до него доходят |
| [`player_chat`](player_chat.md) | Игрок написал в чат |
| [`menu_select`](menu_select.md) | Игрок нажал клавишу в меню, открытом из Lua |

## Жизнь сервера

|  |  |
|---|---|
| [`map_change`](map_change.md) | Карта заканчивается |
| [`plugin_unload`](plugin_unload.md) | Плагин или всё состояние уходит |

## Геймплей

Всё в этом разделе приходит из ReGameDLL.

|  |  |
|---|---|
| [`player_spawn`](player_spawn.md) | Игрок появился в раунде живым |
| [`player_hurt`](player_hurt.md) | Игроку наносят урон; урон можно изменить или погасить |
| [`player_hurt_post`](player_hurt_post.md) | Урон уже применён; только для наблюдателей |
| [`player_death`](player_death.md) | Игрок погиб |
| [`player_team_change`](player_team_change.md) | Игрок сменил сторону |
| [`weapon_fire`](weapon_fire.md) | Из ствола вышел выстрел |
| [`weapon_deploy`](weapon_deploy.md) | Оружие вот-вот покажет вьюмодель и модель в руках |
| [`grenade_explode`](grenade_explode.md) | Граната HE вот-вот взорвётся |
| [`grenade_thrown`](grenade_thrown.md) | HE-граната только что покинула руку |

## Раунд и бомба

|  |  |
|---|---|
| [`round_start`](round_start.md) | Раунд начался |
| [`round_end`](round_end.md) | Раунд закончился |
| [`round_freeze_end`](round_freeze_end.md) | Заморозка кончилась, игроки могут двигаться |
| [`bomb_planted`](bomb_planted.md) | Бомба заложена |
| [`bomb_defused`](bomb_defused.md) | Попытка разминирования завершилась |
| [`bomb_exploded`](bomb_exploded.md) | Бомба взорвалась |
