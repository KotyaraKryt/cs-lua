---
title: Все вызовы
description: "Плоский список всех функций и методов cs-lua с ссылками на их страницы."
---

# Все вызовы

Каждая функция и метод API одной таблицей — когда помнишь имя, но не
помнишь, к какому пространству оно относится. Разложенный по темам
справочник — в [обзоре](index.md).

| вызов | namespace |  |
|---|---|---|
| [`cmd.add`](cmd/add.md) | cmd | Регистрирует команду |
| [`hook.add`](hook/add.md) | hook | Подписывает функцию на событие |
| [`m:add`](menu/add.md) | menu | Добавляет пункт |
| [`timer.after`](timer/after.md) | timer | Вызывает функцию один раз через заданное время |
| [`p:aim`](players/aim.md) | players | Направление взгляда, только чтение |
| [`p:alive`](players/alive.md) | players | Жив ли игрок |
| [`s:all`](store/all.md) | store | Всё содержимое хранилища |
| [`access.all_groups`](access/all_groups.md) | access | Все группы |
| [`p:ammo`](players/ammo.md) | players | Читает или задаёт патроны в запасе |
| [`e:angles`](ents/angles.md) | ents | Читает или задаёт поворот сущности |
| [`p:angles`](players/angles.md) | players | Поворот модели игрока |
| [`p:armor`](players/armor.md) | players | Броня игрока |
| [`datafile.at`](store/at.md) | store | Привязывает модуль к своему каталогу |
| [`p:ban`](players/ban.md) | players | Банит игрока |
| [`fx.beam_cylinder`](fx/beam_cylinder.md) | fx | Кольцо луча, расширяющееся от точки |
| [`bomb_defused`](hook/bomb_defused.md) | hook | Попытка разминирования завершилась |
| [`bomb_exploded`](hook/bomb_exploded.md) | hook | Бомба взорвалась |
| [`bomb_planted`](hook/bomb_planted.md) | hook | Бомба заложена |
| [`players.broadcast`](players/broadcast.md) | players | Приёмник «всем сразу»: только отправка сообщений |
| [`access.can`](access/can.md) | access | Есть ли у игрока право |
| [`p:can`](players/can.md) | players | Есть ли у игрока право |
| [`http.cancel`](http/cancel.md) | http | Отменяет запрос по id |
| [`timer.cancel`](timer/cancel.md) | timer | Снимает таймер по id |
| [`p:center`](players/center.md) | players | Показывает текст по центру экрана |
| [`db:changes`](db/changes.md) | db | Сколько строк изменил последний запрос |
| [`p:chat`](players/chat.md) | players | Отправляет строку в чат |
| [`e:classname`](ents/classname.md) | ents | Classname сущности |
| [`client_connect`](hook/client_connect.md) | hook | Игрок стучится на сервер; его ещё можно не пустить |
| [`client_disconnect`](hook/client_disconnect.md) | hook | Игрок отключился |
| [`p:clip`](players/clip.md) | players | Читает или задаёт патроны в магазине |
| [`db:close`](db/close.md) | db | Закрывает базу |
| [`m:close`](menu/close.md) | menu | Убирает меню с экрана игрока |
| [`st:close`](db/stmt_close.md) | db | Освобождает выражение |
| [`sv.cmd`](sv/cmd.md) | sv | Ставит команду в очередь серверной консоли |
| [`m:color`](menu/color.md) | menu | Перекрашивает меню целиком |
| [`ui.color`](ui/color.md) | ui | Разбирает цвет в `{ r, g, b }` |
| [`p:connected`](players/connected.md) | players | Занят ли слот |
| [`p:console`](players/console.md) | players | Пишет строку в консоль игрока |
| [`m:count`](menu/count.md) | menu | Количество пунктов |
| [`ents.create`](ents/create.md) | ents | Создаёт сущность по classname |
| [`timer.create`](timer/create.md) | timer | Заводит именованный таймер; повторный вызов заменяет старый |
| [`sv.cvar`](sv/cvar.md) | sv | Возвращает объект существующей переменной движка |
| [`sv.cvar_register`](sv/cvar_register.md) | sv | Заводит свою переменную движка |
| [`plugin.data_dir`](plugin/data_dir.md) | plugin | Каталог плагина под данные, переживающий обновление |
| [`p:deaths`](players/deaths.md) | players | Счётчик смертей |
| [`access.declare`](access/declare.md) | access | Объявляет ноду прав |
| [`s:delete`](store/delete.md) | store | Удаляет ключ |
| [`timer.destroy`](timer/destroy.md) | timer | Снимает именованный таймер этого плагина |
| [`p:dhud`](players/dhud.md) | players | То же через `SVC_DIRECTOR` — directed HUD |
| [`datafile.dir`](store/dir.md) | store | Каталог, к которому привязан модуль |
| [`plugin.dir`](plugin/dir.md) | plugin | Абсолютный путь к папке плагина |
| [`p:drop`](players/drop.md) | players | Выбрасывает оружие на пол |
| [`p:ducking`](players/ducking.md) | players | Сидит ли игрок |
| [`timer.every`](timer/every.md) | timer | Вызывает функцию каждые N секунд, пока её не снимут |
| [`db:exec`](db/exec.md) | db | Выполняет запрос, ничего не возвращающий |
| [`p:exec`](players/exec.md) | players | Выполняет консольную команду на машине игрока |
| [`fx.explosion`](fx/explosion.md) | fx | Спрайт взрыва в точке |
| [`export`](exports/export.md) | export / import | Публикует функцию плагина наружу |
| [`ents.find`](ents/find.md) | ents | Находит все сущности с заданным classname |
| [`players.find`](players/find.md) | players | Ищет игрока по слоту, userid или части ника |
| [`db:first`](db/first.md) | db | Выполняет запрос и возвращает первую строку |
| [`st:first`](db/stmt_first.md) | db | Выполняет выражение и возвращает первую строку |
| [`c:float`](sv/float.md) | sv | Значение переменной как число |
| [`s:flush`](store/flush.md) | store | Записывает очередь на диск одной транзакцией |
| [`p:frags`](players/frags.md) | players | Счётчик фрагов |
| [`p:freeze`](players/freeze.md) | players | Заморозка движения (`FL_FROZEN`) |
| [`http.get`](http/get.md) | http | Выполняет GET-запрос |
| [`players.get`](players/get.md) | players | Возвращает игрока по номеру слота |
| [`s:get`](store/get.md) | store | Читает значение по ключу |
| [`p:give`](players/give.md) | players | Выдаёт игроку предмет по classname |
| [`p:godmode`](players/godmode.md) | players | Неуязвимость |
| [`access.grant`](access/grant.md) | access | Выдаёт права по ключу |
| [`p:gravity`](players/gravity.md) | players | Множитель гравитации |
| [`grenade_explode`](hook/grenade_explode.md) | hook | Граната HE вот-вот взорвётся |
| [`grenade_thrown`](hook/grenade_thrown.md) | hook | HE-граната только что покинула руку |
| [`access.group`](access/group.md) | access | Одна группа по имени |
| [`p:group`](players/group.md) | players | Состоит ли игрок в группе, с учётом наследования |
| [`p:groups`](players/groups.md) | players | Массив групп игрока |
| [`p:health`](players/health.md) | players | Здоровье игрока |
| [`p:hud`](players/hud.md) | players | Рисует текст на HUD с позицией, цветом и таймингами |
| [`p.id`](players/id.md) | players | Номер слота игрока |
| [`plugin.id`](plugin/id.md) | plugin | Имя папки плагина |
| [`import`](exports/import.md) | export / import | Возвращает прокси к экспортам плагина; жёсткая зависимость |
| [`ents.in_sphere`](ents/in_sphere.md) | ents | Находит все сущности в радиусе от точки |
| [`e.index`](ents/index.md) | ents | Индекс edict'а |
| [`p:info`](players/info.md) | players | Читает ключ инфобуфера клиента |
| [`c:int`](sv/int.md) | sv | Значение переменной как целое |
| [`access.invalidate`](access/invalidate.md) | access | Сбрасывает кеш прав |
| [`p:ip`](players/ip.md) | players | Адрес игрока вида `1.2.3.4:27005` |
| [`p:is_bot`](players/is_bot.md) | players | Серверный бот (`FL_FAKECLIENT`) |
| [`p:is_hltv`](players/is_hltv.md) | players | HLTV-прокси (`FL_PROXY`) |
| [`m:item_color`](menu/item_color.md) | menu | Перекрашивает один пункт |
| [`s:keys`](store/keys.md) | store | Все ключи хранилища |
| [`e:keyvalue`](ents/keyvalue.md) | ents | Задаёт keyvalue — то же, что делает карта |
| [`p:kick`](players/kick.md) | players | Отключает игрока от сервера |
| [`db:last_id`](db/last_id.md) | db | Rowid последней вставки |
| [`cmd.list`](cmd/list.md) | cmd | Возвращает имена зарегистрированных команд |
| [`hook.list`](hook/list.md) | hook | Возвращает список подписок в порядке вызова |
| [`players.list`](players/list.md) | players | Возвращает массив подключённых игроков |
| [`datafile.load`](store/load.md) | store | Читает таблицу из `<dir>/<name>.lua` |
| [`sv.map`](sv/map.md) | sv | Имя текущей карты |
| [`map_change`](hook/map_change.md) | hook | Карта заканчивается |
| [`p:maxspeed`](players/maxspeed.md) | players | Максимальная скорость движения |
| [`menu_select`](hook/menu_select.md) | hook | Игрок нажал клавишу в меню, открытом из Lua |
| [`players.method`](players/method.md) | players | Добавляет свой метод всем объектам игроков |
| [`e:model`](ents/model.md) | ents | Читает или задаёт модель сущности |
| [`res.model`](res/model.md) | res | Регистрирует модель для прекеша |
| [`p:money`](players/money.md) | players | Читает или задаёт деньги игрока |
| [`p:motd`](players/motd.md) | players | Открывает игроку панель MOTD |
| [`e:movetype`](ents/movetype.md) | ents | Читает или задаёт, как движок двигает сущность |
| [`c:name`](sv/name.md) | sv | Имя переменной |
| [`p:name`](players/name.md) | players | Ник игрока |
| [`menu.new`](menu/new.md) | menu | Создаёт меню |
| [`p:noclip`](players/noclip.md) | players | Полёт сквозь стены |
| [`p:on_ground`](players/on_ground.md) | players | Стоит ли игрок на земле |
| [`plugin.on_unload`](plugin/on_unload.md) | plugin | Регистрирует обработчик выгрузки этого плагина |
| [`db.open`](db/open.md) | db | Открывает базу в каталоге плагина |
| [`store.open`](store/open.md) | store | Открывает хранилище |
| [`optional`](exports/optional.md) | export / import | То же, что import, но `nil` вместо ошибки; мягкая зависимость |
| [`e:origin`](ents/origin.md) | ents | Читает или задаёт позицию сущности |
| [`p:origin`](players/origin.md) | players | Позиция игрока в мире |
| [`p:outranks`](players/outranks.md) | players | Старше ли игрок другого по весу |
| [`db:path`](db/path.md) | db | Путь к файлу базы |
| [`s:pending`](store/pending.md) | store | Сколько записей ждёт в очереди |
| [`access.permissions`](access/permissions.md) | access | Все объявленные ноды |
| [`p:play_sound`](players/play_sound.md) | players | Проигрывает игроку звук |
| [`player_authorized`](hook/player_authorized.md) | hook | Steam ответил, steamid наконец известен |
| [`player_chat`](hook/player_chat.md) | hook | Игрок написал в чат |
| [`player_death`](hook/player_death.md) | hook | Игрок погиб |
| [`player_hurt`](hook/player_hurt.md) | hook | Игроку наносят урон; урон можно изменить или погасить |
| [`player_hurt_post`](hook/player_hurt_post.md) | hook | Урон уже применён; только для наблюдателей |
| [`player_ready`](hook/player_ready.md) | hook | Игрок в игре, сообщения до него доходят |
| [`player_spawn`](hook/player_spawn.md) | hook | Игрок появился в раунде живым |
| [`player_team_change`](hook/player_team_change.md) | hook | Игрок сменил сторону |
| [`plugin_unload`](hook/plugin_unload.md) | hook | Плагин или всё состояние уходит |
| [`plugin{}`](plugin/manifest.md) | plugin | Объявляет метаданные и требования плагина |
| [`http.post`](http/post.md) | http | Выполняет POST-запрос с телом |
| [`db:prepare`](db/prepare.md) | db | Разбирает SQL один раз для многократного выполнения |
| [`db:query`](db/query.md) | db | Выполняет запрос и возвращает все строки |
| [`st:query`](db/stmt_query.md) | db | Выполняет выражение и возвращает все строки |
| [`access.reload`](access/reload.md) | access | Перечитывает `groups.lua` и `users.lua` с диска |
| [`cmd.remove`](cmd/remove.md) | cmd | Снимает команду |
| [`e:remove`](ents/remove.md) | ents | Убирает сущность из мира |
| [`hook.remove`](hook/remove.md) | hook | Снимает подписку по имени события и id |
| [`e:render`](ents/render.md) | ents | Читает или задаёт прозрачность и свечение |
| [`http.request`](http/request.md) | http | Выполняет запрос произвольным методом |
| [`access.revoke`](access/revoke.md) | access | Забирает запись, группу или ноду |
| [`round_end`](hook/round_end.md) | hook | Раунд закончился |
| [`round_freeze_end`](hook/round_freeze_end.md) | hook | Заморозка кончилась, игроки могут двигаться |
| [`round_start`](hook/round_start.md) | hook | Раунд начался |
| [`access.rule`](access/rule.md) | access | Задаёт динамическое правило для ноды |
| [`hook.run`](hook/run.md) | hook | Запускает своё событие плагина |
| [`st:run`](db/stmt_run.md) | db | Выполняет выражение, ничего не возвращающее |
| [`access.save`](access/save.md) | access | Записывает `users.lua` на диск |
| [`datafile.save`](store/save.md) | store | Пишет таблицу в `<dir>/<name>.lua` |
| [`datafile.serialize`](store/serialize.md) | store | Превращает таблицу в текст, без записи |
| [`c:set`](sv/set.md) | sv | Записывает значение переменной |
| [`s:set`](store/set.md) | store | Кладёт значение в очередь записи |
| [`m:show`](menu/show.md) | menu | Показывает меню игроку |
| [`e:size`](ents/size.md) | ents | Читает или задаёт ограничивающий объём |
| [`p:slap`](players/slap.md) | players | Наносит урон и толкает игрока в случайную сторону |
| [`p:slay`](players/slay.md) | players | Убивает игрока |
| [`e:solid`](ents/solid.md) | ents | Читает или задаёт тип столкновений |
| [`res.sound`](res/sound.md) | res | Регистрирует звук для прекеша |
| [`e:spawn`](ents/spawn.md) | ents | Запускает `Spawn` сущности |
| [`p:spawn`](players/spawn.md) | players | Респаун игрока без потери денег и счёта |
| [`fx.sprite_trail`](fx/sprite_trail.md) | fx | Поток светящихся спрайтов между двумя точками |
| [`st:sql`](db/stmt_sql.md) | db | Исходный текст запроса |
| [`p:steamid`](players/steamid.md) | players | SteamID игрока |
| [`c:str`](sv/str.md) | sv | Значение переменной как строка |
| [`p:strip`](players/strip.md) | players | Забирает у игрока всё оружие |
| [`p:team`](players/team.md) | players | Читает или меняет сторону игрока |
| [`sv.time`](sv/time.md) | sv | Серверные часы в секундах |
| [`p:trace`](players/trace.md) | players | Пускает луч из глаз игрока туда, куда он смотрит |
| [`db:transaction`](db/transaction.md) | db | Выполняет блок одной транзакцией |
| [`access.user`](access/user.md) | access | Одна запись из `users.lua` |
| [`p:userid`](players/userid.md) | players | Userid движка |
| [`access.users`](access/users.md) | access | Все записи из `users.lua` |
| [`e:valid`](ents/valid.md) | ents | Жива ли сущность |
| [`p:velocity`](players/velocity.md) | players | Скорость игрока |
| [`p:weapon`](players/weapon.md) | players | Classname оружия в руках |
| [`weapon_deploy`](hook/weapon_deploy.md) | hook | Оружие вот-вот покажет вьюмодель и модель в руках |
| [`weapon_fire`](hook/weapon_fire.md) | hook | Из ствола вышел выстрел |
| [`p:weapons`](players/weapons.md) | players | Массив classname всего, что несёт игрок |
| [`p:weight`](players/weight.md) | players | Вес игрока для иммунитета |

Всего: 185.
