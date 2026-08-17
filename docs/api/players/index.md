---
title: players
description: "Поиск игроков, рассылка и объект игрока"
---

# players

Поиск игроков, рассылка и объект игрока.

Объект игрока привязан к слоту и живёт всё время работы сервера — методы читают
живое состояние, а не снимок. Без аргумента метод читает, с аргументом пишет.

```lua
p:health(p:health() + 25)
```

Объект защищён от записи: `p.foo = 1` бросит ошибку. Своё состояние держи в
таблице плагина, ключом — `p.id`.

## Пространство имён

|  |  |
|---|---|
| [`players.get`](namespace.md#get) | Возвращает игрока по номеру слота |
| [`players.list`](namespace.md#list) | Возвращает массив подключённых игроков |
| [`players.find`](namespace.md#find) | Ищет игрока по слоту, userid или части ника |
| [`players.broadcast`](namespace.md#broadcast) | Приёмник «всем сразу»: только отправка сообщений |
| [`players.method`](namespace.md#method) | Добавляет свой метод всем объектам игроков |

## Идентификация

|  |  |
|---|---|
| [`p.id`](identity.md#id) | Номер слота игрока |
| [`p:name`](identity.md#name) | Ник игрока |
| [`p:ip`](identity.md#ip) | Адрес игрока вида `1.2.3.4:27005` |
| [`p:steamid`](identity.md#steamid) | SteamID игрока |
| [`p:userid`](identity.md#userid) | Userid движка |
| [`p:info`](identity.md#info) | Читает ключ инфобуфера клиента |
| [`p:connected`](identity.md#connected) | Занят ли слот |
| [`p:is_bot`](identity.md#is_bot) | Серверный бот (`FL_FAKECLIENT`) |
| [`p:is_hltv`](identity.md#is_hltv) | HLTV-прокси (`FL_PROXY`) |

## Состояние

Пишется в `entvars_t` напрямую, ReGameDLL не нужен.

|  |  |
|---|---|
| [`p:health`](state.md#health) | Здоровье игрока |
| [`p:armor`](state.md#armor) | Броня игрока |
| [`p:frags`](state.md#frags) | Счётчик фрагов |
| [`p:gravity`](state.md#gravity) | Множитель гравитации |
| [`p:maxspeed`](state.md#maxspeed) | Максимальная скорость движения |
| [`p:origin`](state.md#origin) | Позиция игрока в мире |
| [`p:angles`](state.md#angles) | Поворот модели игрока |
| [`p:velocity`](state.md#velocity) | Скорость игрока |
| [`p:aim`](state.md#aim) | Направление взгляда, только чтение |
| [`p:alive`](state.md#alive) | Жив ли игрок |
| [`p:on_ground`](state.md#on_ground) | Стоит ли игрок на земле |
| [`p:ducking`](state.md#ducking) | Сидит ли игрок |
| [`p:freeze`](state.md#freeze) | Заморозка движения (`FL_FROZEN`) |
| [`p:godmode`](state.md#godmode) | Неуязвимость |
| [`p:noclip`](state.md#noclip) | Полёт сквозь стены |
| [`p:trace`](state.md#trace) | Пускает луч из глаз игрока туда, куда он смотрит |
| [`p:trace_to`](state.md#trace_to) | Пускает луч из глаз игрока прямо к другому игроку |

## CS-состояние

Читает и меняет `CBasePlayer` игры.

|  |  |
|---|---|
| [`p:team`](cs-state.md#team) | Читает или меняет сторону игрока |
| [`p:spawn`](cs-state.md#spawn) | Респаун игрока без потери денег и счёта |
| [`p:money`](cs-state.md#money) | Читает или задаёт деньги игрока |
| [`p:deaths`](cs-state.md#deaths) | Счётчик смертей |
| [`p:give`](cs-state.md#give) | Выдаёт игроку предмет по classname |
| [`p:strip`](cs-state.md#strip) | Забирает у игрока всё оружие |
| [`p:weapon`](cs-state.md#weapon) | Classname оружия в руках |
| [`p:weapons`](cs-state.md#weapons) | Массив classname всего, что несёт игрок |
| [`p:ammo`](cs-state.md#ammo) | Читает или задаёт патроны в запасе |
| [`p:clip`](cs-state.md#clip) | Читает или задаёт патроны в магазине |
| [`p:drop`](cs-state.md#drop) | Выбрасывает оружие на пол |

## Сообщения

Те же методы есть у [`players.broadcast`](namespace.md#broadcast).

|  |  |
|---|---|
| [`p:chat`](messages.md#chat) | Отправляет строку в чат |
| [`p:console`](messages.md#console) | Пишет строку в консоль игрока |
| [`p:center`](messages.md#center) | Показывает текст по центру экрана |
| [`p:hud`](messages.md#hud) | Рисует текст на HUD с позицией, цветом и таймингами |
| [`p:dhud`](messages.md#dhud) | То же через `SVC_DIRECTOR` — directed HUD |
| [`p:motd`](messages.md#motd) | Открывает игроку панель MOTD |
| [`p:play_sound`](messages.md#play_sound) | Проигрывает игроку звук |

## Админские действия

|  |  |
|---|---|
| [`p:kick`](admin.md#kick) | Отключает игрока от сервера |
| [`p:ban`](admin.md#ban) | Банит игрока |
| [`p:slay`](admin.md#slay) | Убивает игрока |
| [`p:slap`](admin.md#slap) | Наносит урон и толкает игрока в случайную сторону |
| [`p:exec`](admin.md#exec) | Выполняет консольную команду на машине игрока |

## Права

Добавлены core-слоем, подробности — в [`access`](../access/index.md).

|  |  |
|---|---|
| [`p:can`](access.md#can) | Есть ли у игрока право |
| [`p:groups`](access.md#groups) | Массив групп игрока |
| [`p:group`](access.md#group) | Состоит ли игрок в группе, с учётом наследования |
| [`p:weight`](access.md#weight) | Вес игрока для иммунитета |
| [`p:outranks`](access.md#outranks) | Старше ли игрок другого по весу |
