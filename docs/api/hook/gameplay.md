---
title: hook — Геймплей
description: "Всё в этом разделе приходит из ReGameDLL"
---

# Геймплей

Всё в этом разделе приходит из ReGameDLL.

## player:spawn {#player_spawn}

Игрок появился в раунде живым.

```lua
hook.add("player:spawn", id, function(e)
	...
end)
```

### Поля события {#player_spawn-поля события}

| поле | тип |  |
|---|---|---|
| `e.player` | player | кого касается событие |

Спавн сбрасывает гравитацию, скорость, заморозку, godmode и noclip — ставь их здесь.

<Warning>
Событие приходит из ReGameDLL. На ванильном `mp.dll` оно не
срабатывает никогда.
</Warning>

## player:hurt {#player_hurt}

Игроку наносят урон; урон можно изменить или погасить.

```lua
hook.add("player:hurt", id, function(e)
	...
end)
```

### Поля события {#player_hurt-поля события}

| поле | тип |  |
|---|---|---|
| `e.victim` | player | кому прилетело |
| `e.attacker` | player \| nil | `nil` при падении и уроне мира |
| `e.damage` | number | запись: сколько урона применить |
| `e.bits` | number | маска типа урона: `DMG_FALL`, `DMG_BULLET` |
| `e.hitgroup` | number \| nil | куда попали: `1` голова, `2` грудь, `3` живот, `4`/`5` руки, `6`/`7` ноги; `nil`, если урон не от попадания |

### Пример

```lua
hook.add("player:hurt", "myplugin.double", function(e)
	e.damage = e.damage * 2
end)
```

**Отмена.** `e:cancel()` гасит урон полностью: ни звука боли, ни брони, ни смерти. Цепочка на этом обрывается.

Единственное событие, которое меняет игру. Каждый следующий обработчик видит
`e.damage` после предыдущего.

`e.hitgroup` заполнен только когда урон пришёл от попадания — пуля, нож. У
падения, взрыва и урона мира его нет: движок оставил бы там зону от прошлого
попадания, а это враньё.

<Warning>
Событие приходит из ReGameDLL. На ванильном `mp.dll` оно не
срабатывает никогда.
</Warning>

### Смотри также

- [player:hurt_post](gameplay.md#player_hurt_post)
- [player:trace_attack](gameplay.md#player_trace_attack)

## player:hurt_post {#player_hurt_post}

Урон уже применён; только для наблюдателей.

```lua
hook.add("player:hurt_post", id, function(e)
	...
end)
```

### Поля события {#player_hurt_post-поля события}

| поле | тип |  |
|---|---|---|
| `e.victim` | player | кому прилетело |
| `e.attacker` | player \| nil | `nil` при падении и уроне мира |
| `e.damage` | number | сколько урона применилось |
| `e.bits` | number | маска типа урона |
| `e.hitgroup` | number \| nil | куда попали: `1` голова, `2` грудь, `3` живот, `4`/`5` руки, `6`/`7` ноги; `nil`, если урон не от попадания |

`e.victim:health()` здесь уже актуальное. Менять нечего — событие неотменяемое.

<Warning>
Событие приходит из ReGameDLL. На ванильном `mp.dll` оно не
срабатывает никогда.
</Warning>

## player:trace_attack {#player_trace_attack}

Один хитбокс получил попадание — раньше и точнее, чем `player:hurt`: до брони, до множителей, до суммирования дроби дробовика в одно число.

```lua
hook.add("player:trace_attack", id, function(e)
	...
end)
```

### Поля события {#player_trace_attack-поля события}

| поле | тип |  |
|---|---|---|
| `e.victim` | player | кому прилетело |
| `e.attacker` | player \| nil | `nil` при уроне мира |
| `e.damage` | number | запись: сырой урон этого попадания, до брони и множителей |
| `e.bits` | number | маска типа урона |
| `e.hitgroup` | number \| nil | куда попали: `1` голова, `2` грудь, `3` живот, `4`/`5` руки, `6`/`7` ноги |
| `e.x, e.y, e.z` | number | точка попадания |

**Отмена.** `e:cancel()` (или `e.damage = 0`) убирает это попадание целиком: ни крови, ни вклада в итоговый урон, который дальше увидит `player:hurt`.

Название — по движковому хуку `CBasePlayer::TraceAttack` (тот же, что `RG_CBasePlayer_TraceAttack` в ReAPI). Дробовик даёт одно `player:trace_attack` на каждую долетевшую дробину и одно `player:hurt`/`weapon:fire` в сумме — здесь урон ещё не сложен.

<Warning>
Событие приходит из ReGameDLL. На ванильном `mp.dll` оно не
срабатывает никогда.
</Warning>

### Смотри также

- [player:hurt](gameplay.md#player_hurt)

## player:heal {#player_heal}

Игроку вот-вот дадут здоровье — аптечка, админ-команда; своей регенерации в CS нет.

```lua
hook.add("player:heal", id, function(e)
	...
end)
```

### Поля события {#player_heal-поля события}

| поле | тип |  |
|---|---|---|
| `e.player` | player | кого касается событие |
| `e.amount` | number | запись: сколько здоровья дать |
| `e.bits` | number | маска, с которой движок передал лечение |

**Отмена.** `e:cancel()` (или `e.amount = 0`) — здоровье не даётся вообще.

<Warning>
Событие приходит из ReGameDLL. На ванильном `mp.dll` оно не
срабатывает никогда.
</Warning>

## player:death {#player_death}

Игрок погиб.

```lua
hook.add("player:death", id, function(e)
	...
end)
```

### Поля события {#player_death-поля события}

| поле | тип |  |
|---|---|---|
| `e.victim` | player | погибший |
| `e.killer` | player \| nil | `nil` при падении, уроне мира и окружения |
| `e.headshot` | boolean | попадание в голову |
| `e.weapon` | string \| nil | classname того, что было у убийцы в руках |
| `e.distance` | number \| nil | юнитов между убийцей и жертвой |

### Пример

```lua
hook.add("player:death", "myplugin.bounty", function(e)
	if not e.killer or e.killer.id == e.victim.id then return end
	e.killer:money(e.killer:money() + (e.headshot and 600 or 300), { hud = true })
end)
```

Суицид приходит как `e.killer.id == e.victim.id`. Вместе с `e.killer` в `nil` уходят `e.weapon` и `e.distance`.

<Note>
Граната и взрыв C4 приходят как оружие в руках убийцы, а не как
`hegrenade`. Отличить снаряд можно по `e.bits` в `player:hurt`.
</Note>

<Warning>
Событие приходит из ReGameDLL. На ванильном `mp.dll` оно не
срабатывает никогда.
</Warning>

## player:team_change {#player_team_change}

Игрок сменил сторону.

```lua
hook.add("player:team_change", id, function(e)
	...
end)
```

### Поля события {#player_team_change-поля события}

| поле | тип |  |
|---|---|---|
| `e.player` | player | кого касается событие |
| `e.old_team` | string | `CT`, `T`, `SPEC`, `NONE` |
| `e.new_team` | string | куда перешёл |

Команда опрашивается каждый кадр, поэтому ловится любая смена: меню, автобаланс, `p:team()`, сторонний мод. Событие приходит на следующем кадре. Первый заход игрока событием не считается.

<Warning>
Событие приходит из ReGameDLL. На ванильном `mp.dll` оно не
срабатывает никогда.
</Warning>

## player:jump {#player_jump}

Игрок прыгнул.

```lua
hook.add("player:jump", id, function(e)
	...
end)
```

### Поля события {#player_jump-поля события}

| поле | тип |  |
|---|---|---|
| `e.player` | player | кого касается событие |

Не отменяемое: к моменту события движок уже применил прыжок к движению игрока, отменять нечего.

<Warning>
Событие приходит из ReGameDLL. На ванильном `mp.dll` оно не
срабатывает никогда.
</Warning>

## player:duck {#player_duck}

Игрок присел.

```lua
hook.add("player:duck", id, function(e)
	...
end)
```

### Поля события {#player_duck-поля события}

| поле | тип |  |
|---|---|---|
| `e.player` | player | кого касается событие |

Не отменяемое, по той же причине, что и `player:jump`.

<Warning>
Событие приходит из ReGameDLL. На ванильном `mp.dll` оно не
срабатывает никогда.
</Warning>

## player:spectate {#player_spectate}

Игрок перешёл в режим наблюдателя.

```lua
hook.add("player:spectate", id, function(e)
	...
end)
```

### Поля события {#player_spectate-поля события}

| поле | тип |  |
|---|---|---|
| `e.player` | player | кого касается событие |

<Warning>
Событие приходит из ReGameDLL. На ванильном `mp.dll` оно не
срабатывает никогда.
</Warning>

## player:radio {#player_radio}

Игрок использовал радиокоманду.

```lua
hook.add("player:radio", id, function(e)
	...
end)
```

### Поля события {#player_radio-поля события}

| поле | тип |  |
|---|---|---|
| `e.player` | player | кого касается событие |
| `e.sentence` | string | имя произносимой фразы движка |
| `e.sample` | string | звуковой файл |

**Отмена.** `e:cancel()` — ни звука, ни строки «Radio: ...» в консоли у остальных.

<Warning>
Событие приходит из ReGameDLL. На ванильном `mp.dll` оно не
срабатывает никогда.
</Warning>

## player:respawn_check {#player_can_respawn}

Игра вот-вот решит, можно ли игроку возродиться.

```lua
hook.add("player:respawn_check", id, function(e)
	...
end)
```

### Поля события {#player_can_respawn-поля события}

| поле | тип |  |
|---|---|---|
| `e.player` | player | кого касается событие |

**Отмена.** `e:cancel()` принудительно запрещает респаун — независимо от того, что решили бы правила игры сами. Разрешить респаун, который правила и так запретили бы, этим событием нельзя: только забрать разрешение, не выдать его.

<Warning>
Событие приходит из ReGameDLL. На ванильном `mp.dll` оно не
срабатывает никогда.
</Warning>

## player:money_change {#money_change}

У игрока вот-вот изменятся деньги — раундовый бонус, награда за фраг, покупка, действие с заложником и т.д.

```lua
hook.add("player:money_change", id, function(e)
	...
end)
```

### Поля события {#money_change-поля события}

| поле | тип |  |
|---|---|---|
| `e.player` | player | кого касается событие |
| `e.amount` | number | на сколько меняется баланс; отрицательное — трата |
| `e.reason` | string | `round_bonus`, `enemy_killed`, `bought_something`, `hostage_rescued` и т.д. |

**Отмена.** `e:cancel()` — баланс не меняется вообще.

`weapon:buy`/`ammo:buy`/`item:buy` уже покрывают саму покупку; это событие — про деньги в отрыве от того, что их вызвало, включая раундовый бонус и награды за фраги, которые через магазинные события не видны.

<Warning>
Событие приходит из ReGameDLL. На ванильном `mp.dll` оно не
срабатывает никогда.
</Warning>

## weapon:fire {#weapon_fire}

Из ствола вышел выстрел.

```lua
hook.add("weapon:fire", id, function(e)
	...
end)
```

### Поля события {#weapon_fire-поля события}

| поле | тип |  |
|---|---|---|
| `e.player` | player | кого касается событие |
| `e.weapon` | string | classname оружия |
| `e.clip` | number | патронов в магазине после выстрела |

Только огнестрел: нож бьёт через `TraceAttack`, гранаты идут своими цепочками. Дробовик даёт одно событие на выстрел, а не на дробину.

<Warning>
Событие приходит из ReGameDLL. На ванильном `mp.dll` оно не
срабатывает никогда.
</Warning>

## weapon:deploy {#weapon_deploy}

Оружие вот-вот покажет вьюмодель и модель в руках.

```lua
hook.add("weapon:deploy", id, function(e)
	...
end)
```

### Поля события {#weapon_deploy-поля события}

| поле | тип |  |
|---|---|---|
| `e.player` | player | кого касается событие |
| `e.weapon` | string | classname оружия |
| `e.view_model` | string | путь вьюмодели; запись подменяет её |
| `e.world_model` | string | путь модели в руках; запись подменяет её |

### Пример

```lua
hook.add("weapon:deploy", "healnade.reskin", function(e)
	if e.weapon == "weapon_hegrenade" then
		e.view_model = "models/reapi_healthnade/v_hegrenade_v1.mdl"
		e.world_model = "models/reapi_healthnade/p_hegrenade_v1.mdl"
	end
end)
```

Оба поля приходят уже заполненными тем, что показал бы движок — меняет их только тот, кто хочет реснуть оружие. Не заменяет модель на земле/в полёте: это `grenade:thrown` про `weapon_hegrenade`.

<Warning>
Событие приходит из ReGameDLL. На ванильном `mp.dll` оно не
срабатывает никогда.
</Warning>

### Смотри также

- [grenade:thrown](gameplay.md#grenade_thrown)

## weapon:reload {#weapon_reload}

Началась настоящая перезарядка.

```lua
hook.add("weapon:reload", id, function(e)
	...
end)
```

### Поля события {#weapon_reload-поля события}

| поле | тип |  |
|---|---|---|
| `e.player` | player | кого касается событие |
| `e.weapon` | string | classname оружия |
| `e.clip` | number | патронов в магазине на момент начала перезарядки |
| `e.delay` | number | секунд до конца анимации перезарядки |
| `e.max_clip` | number \| nil | вместимость магазина; `nil`, если движок её не отдал |

### Пример

```lua
hook.add("weapon:reload", "myplugin.reload_log", function(e)
	e.player:hud(("перезаряжаешь %s (%d/%s патронов), готово через %.1fс")
		:format(e.weapon, e.clip, e.max_clip or "?", e.delay))
end)
```

Приходит из `DefaultReload`/`DefaultShotgunReload` — но только когда те реально начинают перезарядку. Сами хелперы вызываются при каждой попытке (в том числе от зажатой или забинженной клавиши), а внутри молча ничего не делают, если магазин уже полон или патронов в запасе нет — так что это не то же самое, что чтение кнопки `reload` на клиенте: событие приходит, только когда перезарядка правда стартовала.

`e.clip` — это ещё старое значение, до заполнения: сколько патронов оставалось в магазине в момент, когда игрок начал перезарядку.

`e.delay` — время, которое движок передал самой анимации, а не пересчитанный остаток; для дробовиков (перезарядка патрон за патроном) это время именно текущего патрона, а не всей перезарядки целиком.

`e.max_clip` — вместимость магазина этого оружия. Для обычного оружия это тот же `iClipSize`, что движок передаёт в `DefaultReload`; для дробовиков (у `DefaultShotgunReload` такого параметра нет) читается через `GetItemInfo()`. Пригождается, когда нужно посчитать, сколько патронов реально уйдёт из запаса — например, для механики, где недострелянные патроны в стволе не досыпаются к новому магазину, а теряются.

<Warning>
Событие приходит из ReGameDLL. На ванильном `mp.dll` оно не
срабатывает никогда.
</Warning>

### Смотри также

- [weapon:deploy](gameplay.md#weapon_deploy)

## item:give {#item_give}

Движок вот-вот выдаст игроку предмет по имени classname.

```lua
hook.add("item:give", id, function(e)
	...
end)
```

### Поля события {#item_give-поля события}

| поле | тип |  |
|---|---|---|
| `e.player` | player | кого касается событие |
| `e.item` | string | classname выдаваемого предмета |

**Отмена.** `e:cancel()` — ничего не создаётся и не выдаётся.

Не тот же путь, что `p:give()` (тот идёт через `GiveNamedItemEx`, отдельный вызов ReGameDLL, который SDK не документирует как обёртку над этим). Это всё остальное, что раздаёт предметы по имени: стартовое снаряжение раунда, `give` из rcon, сторонний мод.

<Warning>
Событие приходит из ReGameDLL. На ванильном `mp.dll` оно не
срабатывает никогда.
</Warning>

## player:strip {#player_strip}

У игрока только что забрали весь инвентарь.

```lua
hook.add("player:strip", id, function(e)
	...
end)
```

### Поля события {#player_strip-поля события}

| поле | тип |  |
|---|---|---|
| `e.player` | player | кого касается событие |
| `e.remove_suit` | boolean | забрали и костюм тоже |

Не отменяемое: к моменту события инвентарь уже пуст. Срабатывает и на `p:strip()` из любого плагина — хук общий для всех путей, которые ведут к `RemoveAllItems`.

<Warning>
Событие приходит из ReGameDLL. На ванильном `mp.dll` оно не
срабатывает никогда.
</Warning>

## player:can_have_item {#player_can_have_item}

Игра вот-вот решит, может ли игрок вообще получить этот предмет.

```lua
hook.add("player:can_have_item", id, function(e)
	...
end)
```

### Поля события {#player_can_have_item-поля события}

| поле | тип |  |
|---|---|---|
| `e.player` | player | кого касается событие |
| `e.item` | string | classname предмета |

**Отмена.** `e:cancel()` принудительно запрещает — независимо от того, что решили бы правила игры сами. Разрешить то, что правила и так запретили бы, этим событием нельзя: только забрать разрешение, не выдать его.

Комментарий SDK для этого хука дословно: «the player is touching an item, do I give it to him» — шире, чем просто подбор с земли: сюда же попадает и `item:give`, и всё остальное, что спрашивает разрешения выдать предмет.

<Warning>
Событие приходит из ReGameDLL. На ванильном `mp.dll` оно не
срабатывает никогда.
</Warning>

### Смотри также

- [weapon:pickup](gameplay.md#weapon_pickup)

## weapon:pickup {#weapon_pickup}

Игрок подобрал оружие с земли.

```lua
hook.add("weapon:pickup", id, function(e)
	...
end)
```

### Поля события {#weapon_pickup-поля события}

| поле | тип |  |
|---|---|---|
| `e.player` | player | кого касается событие |
| `e.weapon` | string | classname подобранного оружия |

Комментарий SDK для этого хука дословно: «called each time a player picks up a weapon from the ground» — именно подбор с земли, не `item:give` и не покупка.

<Warning>
Событие приходит из ReGameDLL. На ванильном `mp.dll` оно не
срабатывает никогда.
</Warning>

### Смотри также

- [ammo:pickup](shop.md#ammo_pickup)
- [weapon:drop](shop.md#weapon_drop)

## weapon:throw {#weapon_throw}

Любой гранатный слот вот-вот покинёт руку, до того как движок решил, HE это, дымовая или флешка.

```lua
hook.add("weapon:throw", id, function(e)
	...
end)
```

### Поля события {#weapon_throw-поля события}

| поле | тип |  |
|---|---|---|
| `e.player` | player | кого касается событие |
| `e.weapon` | string | classname брошенного предмета |
| `e.ammo_type` | number | `m_iPrimaryAmmoType` предмета — отличает переодетый предмет от настоящей гранаты того же classname |
| `e.x, e.y, e.z` | number | точка броска |
| `e.vx, e.vy, e.vz` | number | вектор броска |
| `e.time` | number | таймер, который движок передаст дальше |

**Отмена.** `e:cancel()` забирает бросок целиком себе: движковый `CGrenade` не создаётся вообще. Обработчик, который отменяет, обычно уже успел построить свой снаряд сам (`ents.create` + `e:movetype(8)`) — иначе ничего не полетит.

Общий перехватчик перед тем, как движок решит, какая это граната — `grenade:throw` про конкретно HE/дымовую/флешку идёт дальше по цепочке отдельно.

<Warning>
Событие приходит из ReGameDLL. На ванильном `mp.dll` оно не
срабатывает никогда.
</Warning>

### Смотри также

- [grenade:throw](gameplay.md#grenade_throw)

## weapon:secondary_attack {#weapon_secondary_attack}

Игрок нажал правую кнопку мыши, держа это оружие.

```lua
hook.add("weapon:secondary_attack", id, function(e)
	...
end)
```

### Поля события {#weapon_secondary_attack-поля события}

| поле | тип |  |
|---|---|---|
| `e.player` | player | кого касается событие |
| `e.weapon` | string | classname оружия |
| `e.ammo_type` | number | `m_iPrimaryAmmoType` оружия |

Срабатывает на каждое нажатие, не на каждый кадр зажатой кнопки. Нет отдельного движкового хука на secondary attack (в отличие от primary, за которым стоит `weapon:fire`) — событие собрано из детектора фронта кнопки в `PlayerPreThink`.

<Warning>
Событие приходит из ReGameDLL. На ванильном `mp.dll` оно не
срабатывает никогда.
</Warning>

## grenade:throw {#grenade_throw}

HE, дымовая или флешка вот-вот покинёт руку.

```lua
hook.add("grenade:throw", id, function(e)
	...
end)
```

### Поля события {#grenade_throw-поля события}

| поле | тип |  |
|---|---|---|
| `e.player` | player \| nil | кто бросает |
| `e.weapon` | string | `weapon_hegrenade`, `weapon_smokegrenade` или `weapon_flashbang` |
| `e.fuse` | number | секунд до взрыва; запись подменяет таймер |

### Пример

```lua
hook.add("grenade:throw", "healnade.instant", function(e)
	if e.weapon == "weapon_smokegrenade" then
		e.fuse = 0.05
	end
end)
```

Приходит до того, как движок создаст сам снаряд — на этом этапе ещё нет сущности для `e:detonate_on_touch()`, только число. Взрыв по касанию, а не по таймеру — это [`e:detonate_on_touch()`](../ents/entity.md#detonate_on_touch) в `grenade:thrown`, а не подмена `fuse` здесь.

<Warning>
Событие приходит из ReGameDLL. На ванильном `mp.dll` оно не
срабатывает никогда.
</Warning>

### Смотри также

- [grenade:thrown](gameplay.md#grenade_thrown)

## grenade:thrown {#grenade_thrown}

HE, дымовая или флешка только что покинула руку.

```lua
hook.add("grenade:thrown", id, function(e)
	...
end)
```

### Поля события {#grenade_thrown-поля события}

| поле | тип |  |
|---|---|---|
| `e.player` | player \| nil | кто бросил |
| `e.weapon` | string | `weapon_hegrenade`, `weapon_smokegrenade` или `weapon_flashbang` |
| `e.entity` | entity \| nil | сама граната; `nil`, если бросок не удался |

### Пример

```lua
hook.add("grenade:thrown", "healnade.reskin", function(e)
	if e.weapon == "weapon_smokegrenade" and e.entity then
		e.entity:model("models/reapi_healthnade/w_hegrenade_v1.mdl")
		e.entity:detonate_on_touch()
	end
end)
```

`e.entity` — обычный объект сущности, тот же, что даёт `ents.create`: `e:model()`, `e:origin()`, [`e:detonate_on_touch()`](../ents/entity.md#detonate_on_touch) и весь остальной [`ents`](../ents/entity.md#index) работают на нём как на любой другой.

<Warning>
Событие приходит из ReGameDLL. На ванильном `mp.dll` оно не
срабатывает никогда.
</Warning>

### Смотри также

- [weapon:deploy](gameplay.md#weapon_deploy)
- [ents](../ents/entity.md#index)

## player:disappear {#player_disappear}

Сущность игрока временно убирается из мира.

```lua
hook.add("player:disappear", id, function(e)
	...
end)
```

### Поля события {#player_disappear-поля события}

| поле | тип |  |
|---|---|---|
| `e.player` | player | кого касается событие |

Название и место — по движковому хуку `CBasePlayer::Disappear`. Не отменяемое: к моменту события сущность уже убрана.

<Warning>
Событие приходит из ReGameDLL. На ванильном `mp.dll` оно не
срабатывает никогда.
</Warning>

## player:can_switch_team {#player_can_switch_team}

Игра вот-вот решит, может ли игрок перейти в команду team.

```lua
hook.add("player:can_switch_team", id, function(e)
	...
end)
```

### Поля события {#player_can_switch_team-поля события}

| поле | тип |  |
|---|---|---|
| `e.player` | player | кого касается событие |
| `e.team` | string | `CT`, `T`, `SPEC` или `NONE` — куда переходит |

**Отмена.** `e:cancel()` принудительно запрещает переход — независимо от того, что решили бы правила игры сами. Разрешить переход, который правила и так запретили бы, этим событием нельзя: только забрать разрешение, не выдать его.

<Warning>
Событие приходит из ReGameDLL. На ванильном `mp.dll` оно не
срабатывает никогда.
</Warning>

### Смотри также

- [player:team_change](gameplay.md#player_team_change)

## player:shield_give {#player_shield_give}

Игроку вот-вот выдадут щит.

```lua
hook.add("player:shield_give", id, function(e)
	...
end)
```

### Поля события {#player_shield_give-поля события}

| поле | тип |  |
|---|---|---|
| `e.player` | player | кого касается событие |
| `e.deploy` | boolean | щит достаётся из рук сразу же |

<Warning>
Событие приходит из ReGameDLL. На ванильном `mp.dll` оно не
срабатывает никогда.
</Warning>

### Смотри также

- [player:shield_drop](gameplay.md#player_shield_drop)

## player:shield_drop {#player_shield_drop}

У игрока только что забрали щит.

```lua
hook.add("player:shield_drop", id, function(e)
	...
end)
```

### Поля события {#player_shield_drop-поля события}

| поле | тип |  |
|---|---|---|
| `e.player` | player | кого касается событие |
| `e.deploy` | boolean | то же значение deploy, что было при выдаче |

Не отменяемое: к моменту события щит уже убран.

<Warning>
Событие приходит из ReGameDLL. На ванильном `mp.dll` оно не
срабатывает никогда.
</Warning>

### Смотри также

- [player:shield_give](gameplay.md#player_shield_give)

## player:observer_next {#player_observer_next}

Наблюдатель вот-вот переключится на другую цель.

```lua
hook.add("player:observer_next", id, function(e)
	...
end)
```

### Поля события {#player_observer_next-поля события}

| поле | тип |  |
|---|---|---|
| `e.player` | player | кого касается событие |
| `e.reverse` | boolean | направление переключения |
| `e.target` | string \| nil | конкретное имя цели, если оно было задано |

Не отменяемое: к моменту события движок уже выбрал следующую цель.

<Warning>
Событие приходит из ReGameDLL. На ванильном `mp.dll` оно не
срабатывает никогда.
</Warning>

### Смотри также

- [player:observer_mode](gameplay.md#player_observer_mode)

## player:observer_mode {#player_observer_mode}

У наблюдателя только что сменился режим камеры.

```lua
hook.add("player:observer_mode", id, function(e)
	...
end)
```

### Поля события {#player_observer_mode-поля события}

| поле | тип |  |
|---|---|---|
| `e.player` | player | кого касается событие |
| `e.mode` | number | `0` свободный полёт выключен, `1` слежение без поворота камеры, `2` слежение со свободной камерой, `3` роуминг, `4` вид от первого лица, `5` карта свободно, `6` карта со слежением — см. `OBS_*` в SDK |

Не отменяемое.

<Warning>
Событие приходит из ReGameDLL. На ванильном `mp.dll` оно не
срабатывает никогда.
</Warning>

### Смотри также

- [player:observer_next](gameplay.md#player_observer_next)

## player:score_add {#player_score_add}

Игроку вот-вот изменят личный счёт очков.

```lua
hook.add("player:score_add", id, function(e)
	...
end)
```

### Поля события {#player_score_add-поля события}

| поле | тип |  |
|---|---|---|
| `e.player` | player | кого касается событие |
| `e.score` | number | запись: на сколько изменить счёт |
| `e.allow_negative` | boolean | запись: разрешить ли счёту уйти в минус |

**Отмена.** `e:cancel()` — счёт не меняется вообще.

<Warning>
Событие приходит из ReGameDLL. На ванильном `mp.dll` оно не
срабатывает никогда.
</Warning>

### Смотри также

- [team:score_add](gameplay.md#team_score_add)

## team:score_add {#team_score_add}

Команде вот-вот изменят счёт очков.

```lua
hook.add("team:score_add", id, function(e)
	...
end)
```

### Поля события {#team_score_add-поля события}

| поле | тип |  |
|---|---|---|
| `e.player` | player | кого касается событие |
| `e.score` | number | запись: на сколько изменить счёт |
| `e.allow_negative` | boolean | запись: разрешить ли счёту уйти в минус |

**Отмена.** `e:cancel()` — счёт не меняется вообще.

`e.player` — не «кому идут очки», а тот, через кого движок дёрнул вызов; какой команде считать очки, смотри через `e.player:team()`.

<Warning>
Событие приходит из ReGameDLL. На ванильном `mp.dll` оно не
срабатывает никогда.
</Warning>

### Смотри также

- [player:score_add](gameplay.md#player_score_add)

## player:userinfo_change {#player_userinfo_change}

У игрока изменился один из userinfo-ключей (модель, имя и т.д.).

```lua
hook.add("player:userinfo_change", id, function(e)
	...
end)
```

### Поля события {#player_userinfo_change-поля события}

| поле | тип |  |
|---|---|---|
| `e.player` | player | кого касается событие |

Сырой буфер движка в событие не попадает — сразу читай нужный ключ через [`p:info(key)`](../players/identity.md#info).

<Warning>
Событие приходит из ReGameDLL. На ванильном `mp.dll` оно не
срабатывает никогда.
</Warning>

## player:can_hear {#player_can_hear}

Игра вот-вот решит, слышит ли listener голос speaker.

```lua
hook.add("player:can_hear", id, function(e)
	...
end)
```

### Поля события {#player_can_hear-поля события}

| поле | тип |  |
|---|---|---|
| `e.listener` | player | кто слушает |
| `e.speaker` | player | кто говорит |

**Отмена.** `e:cancel()` принудительно запрещает — независимо от того, что решили бы правила игры сами. Разрешить то, что правила и так запретили бы, этим событием нельзя: только забрать разрешение, не выдать его.

<Warning>
Событие приходит из ReGameDLL. На ванильном `mp.dll` оно не
срабатывает никогда.
</Warning>

## player:choose_model {#player_choose_model}

Игрок выбрал пункт в меню внешности.

```lua
hook.add("player:choose_model", id, function(e)
	...
end)
```

### Поля события {#player_choose_model-поля события}

| поле | тип |  |
|---|---|---|
| `e.player` | player | кого касается событие |
| `e.slot` | number | номер пункта меню, не имя модели |

Не отменяемое: к моменту события выбор уже применён.

<Warning>
Событие приходит из ReGameDLL. На ванильном `mp.dll` оно не
срабатывает никогда.
</Warning>

### Смотри также

- [player:choose_team](gameplay.md#player_choose_team)

## player:choose_team {#player_choose_team}

Игрок выбрал пункт в меню команды.

```lua
hook.add("player:choose_team", id, function(e)
	...
end)
```

### Поля события {#player_choose_team-поля события}

| поле | тип |  |
|---|---|---|
| `e.player` | player | кого касается событие |
| `e.slot` | number | номер пункта меню, не имя команды |

**Отмена.** `e:cancel()` блокирует сам переход в команду: выбор из этого меню применяется внутри того же вызова, так что отмена до него срабатывает как полный запрет, а не откат задним числом.

<Warning>
Событие приходит из ReGameDLL. На ванильном `mp.dll` оно не
срабатывает никогда.
</Warning>

### Смотри также

- [player:choose_model](gameplay.md#player_choose_model)
- [player:can_switch_team](gameplay.md#player_can_switch_team)

## grenade:explode {#grenade_explode}

HE, дымовая или флешка вот-вот взорвётся.

```lua
hook.add("grenade:explode", id, function(e)
	...
end)
```

### Поля события {#grenade_explode-поля события}

| поле | тип |  |
|---|---|---|
| `e.player` | player \| nil | кто бросил; `nil`, если бросавшего не осталось |
| `e.weapon` | string | `weapon_hegrenade`, `weapon_smokegrenade` или `weapon_flashbang` |
| `e.entity` | entity \| nil | сама граната; `nil`, если уже пропала |
| `e.x, e.y, e.z` | number | точка взрыва |

### Пример

```lua
hook.add("grenade:explode", "healnade.explode", function(e)
	if e.weapon ~= "weapon_smokegrenade" or not e.player then return end
	e:cancel()
	for _, p in ipairs(players.list{ alive = true, team = e.player:team() }) do
		p:health(math.min(100, p:health() + 25))
	end
	if e.entity then e.entity:remove() end
end)
```

**Отмена.** `e:cancel()` забирает взрыв целиком себе: ни урона (у HE), ни дыма (у smoke), ни ослепления/звона в ушах (у флешки), ни звука от игры — дальше плагин сам решает, что происходит в этой точке. Сущность при этом не убирается сама: если она больше не нужна, убирай через `e.entity:remove()`.

Бомбу не задевает: у неё отдельная цепочка, `bomb:exploded`.

<Warning>
Событие приходит из ReGameDLL. На ванильном `mp.dll` оно не
срабатывает никогда.
</Warning>
