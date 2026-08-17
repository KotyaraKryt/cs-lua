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
| [`cmd.add`](cmd/index.md#add) | cmd | Регистрирует команду |
| [`hook.add`](hook/namespace.md#add) | hook | Подписывает функцию на событие |
| [`m:add`](menu/index.md#add) | menu | Добавляет пункт |
| [`timer.after`](timer/anonymous.md#after) | timer | Вызывает функцию один раз через заданное время |
| [`p:aim`](players/state.md#aim) | players | Направление взгляда, только чтение |
| [`p:alive`](players/state.md#alive) | players | Жив ли игрок |
| [`s:all`](store/kv.md#all) | store | Всё содержимое хранилища |
| [`access.all_groups`](access/read.md#all_groups) | access | Все группы |
| [`p:ammo`](players/cs-state.md#ammo) | players | Читает или задаёт патроны в запасе |
| [`e:angles`](ents/entity.md#angles) | ents | Читает или задаёт поворот сущности |
| [`p:angles`](players/state.md#angles) | players | Поворот модели игрока |
| [`p:armor`](players/state.md#armor) | players | Броня игрока |
| [`datafile.at`](store/datafile.md#at) | store | Привязывает модуль к своему каталогу |
| [`p:ban`](players/admin.md#ban) | players | Банит игрока |
| [`fx.beam_cylinder`](fx/index.md#beam_cylinder) | fx | Кольцо луча, расширяющееся от точки |
| [`bomb_defused`](hook/round.md#bomb_defused) | hook | Попытка разминирования завершилась |
| [`bomb_exploded`](hook/round.md#bomb_exploded) | hook | Бомба взорвалась |
| [`bomb_planted`](hook/round.md#bomb_planted) | hook | Бомба заложена |
| [`players.broadcast`](players/namespace.md#broadcast) | players | Приёмник «всем сразу»: только отправка сообщений |
| [`access.can`](access/read.md#can) | access | Есть ли у игрока право |
| [`p:can`](players/access.md#can) | players | Есть ли у игрока право |
| [`http.cancel`](http/index.md#cancel) | http | Отменяет запрос по id |
| [`timer.cancel`](timer/anonymous.md#cancel) | timer | Снимает таймер по id |
| [`p:center`](players/messages.md#center) | players | Показывает текст по центру экрана |
| [`db:changes`](db/database.md#changes) | db | Сколько строк изменил последний запрос |
| [`p:chat`](players/messages.md#chat) | players | Отправляет строку в чат |
| [`e:classname`](ents/entity.md#classname) | ents | Classname сущности |
| [`client_connect`](hook/connection.md#client_connect) | hook | Игрок стучится на сервер; его ещё можно не пустить |
| [`client_disconnect`](hook/connection.md#client_disconnect) | hook | Игрок отключился |
| [`p:clip`](players/cs-state.md#clip) | players | Читает или задаёт патроны в магазине |
| [`conn:close`](mysql/connection.md#close) | mysql | Закрывает соединение |
| [`db:close`](db/database.md#close) | db | Закрывает базу |
| [`m:close`](menu/index.md#close) | menu | Убирает меню с экрана игрока |
| [`st:close`](db/statement.md#stmt_close) | db | Освобождает выражение |
| [`sv.cmd`](sv/server.md#cmd) | sv | Ставит команду в очередь серверной консоли |
| [`m:color`](menu/index.md#color) | menu | Перекрашивает меню целиком |
| [`ui.color`](ui/index.md#color) | ui | Разбирает цвет в `{ r, g, b }` |
| [`mysql.connect`](mysql/open.md#connect) | mysql | Открывает соединение с сайтовой базой |
| [`p:connected`](players/identity.md#connected) | players | Занят ли слот |
| [`p:console`](players/messages.md#console) | players | Пишет строку в консоль игрока |
| [`m:count`](menu/index.md#count) | menu | Количество пунктов |
| [`ents.create`](ents/namespace.md#create) | ents | Создаёт сущность по classname |
| [`timer.create`](timer/named.md#create) | timer | Заводит именованный таймер; повторный вызов заменяет старый |
| [`sv.cvar`](sv/cvar.md#cvar) | sv | Возвращает объект существующей переменной движка |
| [`sv.cvar_register`](sv/cvar.md#cvar_register) | sv | Заводит свою переменную движка |
| [`plugin.data_dir`](plugin/index.md#data_dir) | plugin | Каталог плагина под данные, переживающий обновление |
| [`p:deaths`](players/cs-state.md#deaths) | players | Счётчик смертей |
| [`access.declare`](access/declare.md#declare) | access | Объявляет ноду прав |
| [`s:delete`](store/kv.md#delete) | store | Удаляет ключ |
| [`timer.destroy`](timer/named.md#destroy) | timer | Снимает именованный таймер этого плагина |
| [`e:detonate_on_touch`](ents/entity.md#detonate_on_touch) | ents | Следующее касание сразу запускает think сущности |
| [`p:dhud`](players/messages.md#dhud) | players | То же через `SVC_DIRECTOR` — directed HUD |
| [`datafile.dir`](store/datafile.md#dir) | store | Каталог, к которому привязан модуль |
| [`plugin.dir`](plugin/index.md#dir) | plugin | Абсолютный путь к папке плагина |
| [`p:drop`](players/cs-state.md#drop) | players | Выбрасывает оружие на пол |
| [`p:ducking`](players/state.md#ducking) | players | Сидит ли игрок |
| [`timer.every`](timer/anonymous.md#every) | timer | Вызывает функцию каждые N секунд, пока её не снимут |
| [`conn:exec`](mysql/connection.md#exec) | mysql | То же самое, что conn:query — имя удобнее для запросов без строк |
| [`db:exec`](db/database.md#exec) | db | Выполняет запрос, ничего не возвращающий |
| [`p:exec`](players/admin.md#exec) | players | Выполняет консольную команду на машине игрока |
| [`fx.explosion`](fx/index.md#explosion) | fx | Спрайт взрыва в точке |
| [`export`](exports/index.md#export) | export / import | Публикует функцию плагина наружу |
| [`ents.find`](ents/namespace.md#find) | ents | Находит все сущности с заданным classname |
| [`players.find`](players/namespace.md#find) | players | Ищет игрока по слоту, userid или части ника |
| [`db:first`](db/database.md#first) | db | Выполняет запрос и возвращает первую строку |
| [`st:first`](db/statement.md#stmt_first) | db | Выполняет выражение и возвращает первую строку |
| [`c:float`](sv/cvar.md#float) | sv | Значение переменной как число |
| [`s:flush`](store/kv.md#flush) | store | Записывает очередь на диск одной транзакцией |
| [`p:frags`](players/state.md#frags) | players | Счётчик фрагов |
| [`p:freeze`](players/state.md#freeze) | players | Заморозка движения (`FL_FROZEN`) |
| [`http.get`](http/index.md#get) | http | Выполняет GET-запрос |
| [`players.get`](players/namespace.md#get) | players | Возвращает игрока по номеру слота |
| [`s:get`](store/kv.md#get) | store | Читает значение по ключу |
| [`p:give`](players/cs-state.md#give) | players | Выдаёт игроку предмет по classname |
| [`p:godmode`](players/state.md#godmode) | players | Неуязвимость |
| [`access.grant`](access/grant.md#grant) | access | Выдаёт права по ключу |
| [`p:gravity`](players/state.md#gravity) | players | Множитель гравитации |
| [`grenade_explode`](hook/gameplay.md#grenade_explode) | hook | HE- или дымовая граната вот-вот взорвётся |
| [`grenade_throw`](hook/gameplay.md#grenade_throw) | hook | HE- или дымовая граната вот-вот покинёт руку |
| [`grenade_thrown`](hook/gameplay.md#grenade_thrown) | hook | HE- или дымовая граната только что покинула руку |
| [`access.group`](access/read.md#group) | access | Одна группа по имени |
| [`p:group`](players/access.md#group) | players | Состоит ли игрок в группе, с учётом наследования |
| [`p:groups`](players/access.md#groups) | players | Массив групп игрока |
| [`p:health`](players/state.md#health) | players | Здоровье игрока |
| [`p:hud`](players/messages.md#hud) | players | Рисует текст на HUD с позицией, цветом и таймингами |
| [`p.id`](players/identity.md#id) | players | Номер слота игрока |
| [`plugin.id`](plugin/index.md#id) | plugin | Имя папки плагина |
| [`import`](exports/index.md#import) | export / import | Возвращает прокси к экспортам плагина; жёсткая зависимость |
| [`ents.in_sphere`](ents/namespace.md#in_sphere) | ents | Находит все сущности в радиусе от точки |
| [`e.index`](ents/entity.md#index) | ents | Индекс edict'а |
| [`p:info`](players/identity.md#info) | players | Читает ключ инфобуфера клиента |
| [`c:int`](sv/cvar.md#int) | sv | Значение переменной как целое |
| [`access.invalidate`](access/grant.md#invalidate) | access | Сбрасывает кеш прав |
| [`p:ip`](players/identity.md#ip) | players | Адрес игрока вида `1.2.3.4:27005` |
| [`p:is_bot`](players/identity.md#is_bot) | players | Серверный бот (`FL_FAKECLIENT`) |
| [`p:is_hltv`](players/identity.md#is_hltv) | players | HLTV-прокси (`FL_PROXY`) |
| [`m:item_color`](menu/index.md#item_color) | menu | Перекрашивает один пункт |
| [`s:keys`](store/kv.md#keys) | store | Все ключи хранилища |
| [`e:keyvalue`](ents/entity.md#keyvalue) | ents | Задаёт keyvalue — то же, что делает карта |
| [`p:kick`](players/admin.md#kick) | players | Отключает игрока от сервера |
| [`db:last_id`](db/database.md#last_id) | db | Rowid последней вставки |
| [`cmd.list`](cmd/index.md#list) | cmd | Возвращает имена зарегистрированных команд |
| [`hook.list`](hook/namespace.md#list) | hook | Возвращает список подписок в порядке вызова |
| [`players.list`](players/namespace.md#list) | players | Возвращает массив подключённых игроков |
| [`datafile.load`](store/datafile.md#load) | store | Читает таблицу из `<dir>/<name>.lua` |
| [`sv.map`](sv/server.md#map) | sv | Имя текущей карты |
| [`map_change`](hook/lifecycle.md#map_change) | hook | Карта заканчивается |
| [`p:maxspeed`](players/state.md#maxspeed) | players | Максимальная скорость движения |
| [`menu_select`](hook/connection.md#menu_select) | hook | Игрок нажал клавишу в меню, открытом из Lua |
| [`players.method`](players/namespace.md#method) | players | Добавляет свой метод всем объектам игроков |
| [`e:model`](ents/entity.md#model) | ents | Читает или задаёт модель сущности |
| [`res.model`](res/index.md#model) | res | Регистрирует модель для прекеша |
| [`p:money`](players/cs-state.md#money) | players | Читает или задаёт деньги игрока |
| [`p:motd`](players/messages.md#motd) | players | Открывает игроку панель MOTD |
| [`e:movetype`](ents/entity.md#movetype) | ents | Читает или задаёт, как движок двигает сущность |
| [`c:name`](sv/cvar.md#name) | sv | Имя переменной |
| [`p:name`](players/identity.md#name) | players | Ник игрока |
| [`menu.new`](menu/index.md#new) | menu | Создаёт меню |
| [`p:noclip`](players/state.md#noclip) | players | Полёт сквозь стены |
| [`p:on_ground`](players/state.md#on_ground) | players | Стоит ли игрок на земле |
| [`plugin.on_unload`](plugin/index.md#on_unload) | plugin | Регистрирует обработчик выгрузки этого плагина |
| [`db.open`](db/open.md#open) | db | Открывает базу в каталоге плагина |
| [`store.open`](store/kv.md#open) | store | Открывает хранилище |
| [`optional`](exports/index.md#optional) | export / import | То же, что import, но `nil` вместо ошибки; мягкая зависимость |
| [`e:origin`](ents/entity.md#origin) | ents | Читает или задаёт позицию сущности |
| [`p:origin`](players/state.md#origin) | players | Позиция игрока в мире |
| [`p:outranks`](players/access.md#outranks) | players | Старше ли игрок другого по весу |
| [`db:path`](db/database.md#path) | db | Путь к файлу базы |
| [`s:pending`](store/kv.md#pending) | store | Сколько записей ждёт в очереди |
| [`access.permissions`](access/read.md#permissions) | access | Все объявленные ноды |
| [`p:play_sound`](players/messages.md#play_sound) | players | Проигрывает игроку звук |
| [`player_authorized`](hook/connection.md#player_authorized) | hook | Steam ответил, steamid наконец известен |
| [`player_chat`](hook/connection.md#player_chat) | hook | Игрок написал в чат |
| [`player_death`](hook/gameplay.md#player_death) | hook | Игрок погиб |
| [`player_hurt`](hook/gameplay.md#player_hurt) | hook | Игроку наносят урон; урон можно изменить или погасить |
| [`player_hurt_post`](hook/gameplay.md#player_hurt_post) | hook | Урон уже применён; только для наблюдателей |
| [`player_ready`](hook/connection.md#player_ready) | hook | Игрок в игре, сообщения до него доходят |
| [`player_spawn`](hook/gameplay.md#player_spawn) | hook | Игрок появился в раунде живым |
| [`player_team_change`](hook/gameplay.md#player_team_change) | hook | Игрок сменил сторону |
| [`plugin_unload`](hook/lifecycle.md#plugin_unload) | hook | Плагин или всё состояние уходит |
| [`plugin{}`](plugin/index.md#manifest) | plugin | Объявляет метаданные и требования плагина |
| [`http.post`](http/index.md#post) | http | Выполняет POST-запрос с телом |
| [`db:prepare`](db/database.md#prepare) | db | Разбирает SQL один раз для многократного выполнения |
| [`conn:query`](mysql/connection.md#query) | mysql | Выполняет запрос и отдаёт строки в коллбэк |
| [`db:query`](db/database.md#query) | db | Выполняет запрос и возвращает все строки |
| [`st:query`](db/statement.md#stmt_query) | db | Выполняет выражение и возвращает все строки |
| [`access.reload`](access/grant.md#reload) | access | Перечитывает `groups.lua` и `users.lua` с диска |
| [`cmd.remove`](cmd/index.md#remove) | cmd | Снимает команду |
| [`e:remove`](ents/entity.md#remove) | ents | Убирает сущность из мира |
| [`hook.remove`](hook/namespace.md#remove) | hook | Снимает подписку по имени события и id |
| [`e:render`](ents/entity.md#render) | ents | Читает или задаёт прозрачность и свечение |
| [`http.request`](http/index.md#request) | http | Выполняет запрос произвольным методом |
| [`access.revoke`](access/grant.md#revoke) | access | Забирает запись, группу или ноду |
| [`round_end`](hook/round.md#round_end) | hook | Раунд закончился |
| [`round_freeze_end`](hook/round.md#round_freeze_end) | hook | Заморозка кончилась, игроки могут двигаться |
| [`round_start`](hook/round.md#round_start) | hook | Раунд начался |
| [`access.rule`](access/declare.md#rule) | access | Задаёт динамическое правило для ноды |
| [`hook.run`](hook/namespace.md#run) | hook | Запускает своё событие плагина |
| [`st:run`](db/statement.md#stmt_run) | db | Выполняет выражение, ничего не возвращающее |
| [`access.save`](access/grant.md#save) | access | Записывает `users.lua` на диск |
| [`datafile.save`](store/datafile.md#save) | store | Пишет таблицу в `<dir>/<name>.lua` |
| [`datafile.serialize`](store/datafile.md#serialize) | store | Превращает таблицу в текст, без записи |
| [`c:set`](sv/cvar.md#set) | sv | Записывает значение переменной |
| [`s:set`](store/kv.md#set) | store | Кладёт значение в очередь записи |
| [`m:show`](menu/index.md#show) | menu | Показывает меню игроку |
| [`e:size`](ents/entity.md#size) | ents | Читает или задаёт ограничивающий объём |
| [`p:slap`](players/admin.md#slap) | players | Наносит урон и толкает игрока в случайную сторону |
| [`p:slay`](players/admin.md#slay) | players | Убивает игрока |
| [`e:solid`](ents/entity.md#solid) | ents | Читает или задаёт тип столкновений |
| [`res.sound`](res/index.md#sound) | res | Регистрирует звук для прекеша |
| [`e:spawn`](ents/entity.md#spawn) | ents | Запускает `Spawn` сущности |
| [`p:spawn`](players/cs-state.md#spawn) | players | Респаун игрока без потери денег и счёта |
| [`fx.sprite_trail`](fx/index.md#sprite_trail) | fx | Поток светящихся спрайтов между двумя точками |
| [`st:sql`](db/statement.md#stmt_sql) | db | Исходный текст запроса |
| [`p:steamid`](players/identity.md#steamid) | players | SteamID игрока |
| [`c:str`](sv/cvar.md#str) | sv | Значение переменной как строка |
| [`p:strip`](players/cs-state.md#strip) | players | Забирает у игрока всё оружие |
| [`p:team`](players/cs-state.md#team) | players | Читает или меняет сторону игрока |
| [`sv.time`](sv/server.md#time) | sv | Серверные часы в секундах |
| [`p:trace`](players/state.md#trace) | players | Пускает луч из глаз игрока туда, куда он смотрит |
| [`p:trace_to`](players/state.md#trace_to) | players | Пускает луч из глаз игрока прямо к другому игроку |
| [`db:transaction`](db/database.md#transaction) | db | Выполняет блок одной транзакцией |
| [`access.user`](access/read.md#user) | access | Одна запись из `users.lua` |
| [`p:userid`](players/identity.md#userid) | players | Userid движка |
| [`access.users`](access/read.md#users) | access | Все записи из `users.lua` |
| [`e:valid`](ents/entity.md#valid) | ents | Жива ли сущность |
| [`p:velocity`](players/state.md#velocity) | players | Скорость игрока |
| [`p:weapon`](players/cs-state.md#weapon) | players | Classname оружия в руках |
| [`weapon_deploy`](hook/gameplay.md#weapon_deploy) | hook | Оружие вот-вот покажет вьюмодель и модель в руках |
| [`weapon_fire`](hook/gameplay.md#weapon_fire) | hook | Из ствола вышел выстрел |
| [`p:weapons`](players/cs-state.md#weapons) | players | Массив classname всего, что несёт игрок |
| [`p:weight`](players/access.md#weight) | players | Вес игрока для иммунитета |

Всего: 192.
