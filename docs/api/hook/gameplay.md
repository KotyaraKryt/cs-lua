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
