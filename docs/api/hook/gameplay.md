---
title: hook — Геймплей
description: "Всё в этом разделе приходит из ReGameDLL"
---

# Геймплей

Всё в этом разделе приходит из ReGameDLL.

## player_spawn {#player_spawn}

Игрок появился в раунде живым.

```lua
hook.add("player_spawn", id, function(e)
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

## player_hurt {#player_hurt}

Игроку наносят урон; урон можно изменить или погасить.

```lua
hook.add("player_hurt", id, function(e)
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
hook.add("player_hurt", "myplugin.double", function(e)
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

- [player_hurt_post](gameplay.md#player_hurt_post)

## player_hurt_post {#player_hurt_post}

Урон уже применён; только для наблюдателей.

```lua
hook.add("player_hurt_post", id, function(e)
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

## player_death {#player_death}

Игрок погиб.

```lua
hook.add("player_death", id, function(e)
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
hook.add("player_death", "myplugin.bounty", function(e)
	if not e.killer or e.killer.id == e.victim.id then return end
	e.killer:money(e.killer:money() + (e.headshot and 600 or 300), { hud = true })
end)
```

Суицид приходит как `e.killer.id == e.victim.id`. Вместе с `e.killer` в `nil` уходят `e.weapon` и `e.distance`.

<Note>
Граната и взрыв C4 приходят как оружие в руках убийцы, а не как
`hegrenade`. Отличить снаряд можно по `e.bits` в `player_hurt`.
</Note>

<Warning>
Событие приходит из ReGameDLL. На ванильном `mp.dll` оно не
срабатывает никогда.
</Warning>

## player_team_change {#player_team_change}

Игрок сменил сторону.

```lua
hook.add("player_team_change", id, function(e)
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

## weapon_fire {#weapon_fire}

Из ствола вышел выстрел.

```lua
hook.add("weapon_fire", id, function(e)
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

## weapon_deploy {#weapon_deploy}

Оружие вот-вот покажет вьюмодель и модель в руках.

```lua
hook.add("weapon_deploy", id, function(e)
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
hook.add("weapon_deploy", "healnade.reskin", function(e)
	if e.weapon == "weapon_hegrenade" then
		e.view_model = "models/reapi_healthnade/v_hegrenade_v1.mdl"
		e.world_model = "models/reapi_healthnade/p_hegrenade_v1.mdl"
	end
end)
```

Оба поля приходят уже заполненными тем, что показал бы движок — меняет их только тот, кто хочет реснуть оружие. Не заменяет модель на земле/в полёте: это `grenade_thrown` про `weapon_hegrenade`.

<Warning>
Событие приходит из ReGameDLL. На ванильном `mp.dll` оно не
срабатывает никогда.
</Warning>

### Смотри также

- [grenade_thrown](gameplay.md#grenade_thrown)

## weapon_reload {#weapon_reload}

Началась настоящая перезарядка.

```lua
hook.add("weapon_reload", id, function(e)
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
hook.add("weapon_reload", "myplugin.reload_log", function(e)
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

- [weapon_deploy](gameplay.md#weapon_deploy)

## grenade_throw {#grenade_throw}

HE- или дымовая граната вот-вот покинёт руку.

```lua
hook.add("grenade_throw", id, function(e)
	...
end)
```

### Поля события {#grenade_throw-поля события}

| поле | тип |  |
|---|---|---|
| `e.player` | player \| nil | кто бросает |
| `e.weapon` | string | `weapon_hegrenade` или `weapon_smokegrenade` |
| `e.fuse` | number | секунд до взрыва; запись подменяет таймер |

### Пример

```lua
hook.add("grenade_throw", "healnade.instant", function(e)
	if e.weapon == "weapon_smokegrenade" then
		e.fuse = 0.05
	end
end)
```

Приходит до того, как движок создаст сам снаряд — на этом этапе ещё нет сущности для `e:detonate_on_touch()`, только число. Взрыв по касанию, а не по таймеру — это [`e:detonate_on_touch()`](../ents/entity.md#detonate_on_touch) в `grenade_thrown`, а не подмена `fuse` здесь.

<Warning>
Событие приходит из ReGameDLL. На ванильном `mp.dll` оно не
срабатывает никогда.
</Warning>

### Смотри также

- [grenade_thrown](gameplay.md#grenade_thrown)

## grenade_thrown {#grenade_thrown}

HE- или дымовая граната только что покинула руку.

```lua
hook.add("grenade_thrown", id, function(e)
	...
end)
```

### Поля события {#grenade_thrown-поля события}

| поле | тип |  |
|---|---|---|
| `e.player` | player \| nil | кто бросил |
| `e.weapon` | string | `weapon_hegrenade` или `weapon_smokegrenade` |
| `e.entity` | entity \| nil | сама граната; `nil`, если бросок не удался |

### Пример

```lua
hook.add("grenade_thrown", "healnade.reskin", function(e)
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

- [weapon_deploy](gameplay.md#weapon_deploy)
- [ents](../ents/entity.md#index)

## grenade_explode {#grenade_explode}

HE- или дымовая граната вот-вот взорвётся.

```lua
hook.add("grenade_explode", id, function(e)
	...
end)
```

### Поля события {#grenade_explode-поля события}

| поле | тип |  |
|---|---|---|
| `e.player` | player \| nil | кто бросил; `nil`, если бросавшего не осталось |
| `e.weapon` | string | `weapon_hegrenade` или `weapon_smokegrenade` |
| `e.entity` | entity \| nil | сама граната; `nil`, если уже пропала |
| `e.x, e.y, e.z` | number | точка взрыва |

### Пример

```lua
hook.add("grenade_explode", "healnade.explode", function(e)
	if e.weapon ~= "weapon_smokegrenade" or not e.player then return end
	e:cancel()
	for _, p in ipairs(players.list{ alive = true, team = e.player:team() }) do
		p:health(math.min(100, p:health() + 25))
	end
	if e.entity then e.entity:remove() end
end)
```

**Отмена.** `e:cancel()` забирает взрыв целиком себе: ни урона (у HE), ни дыма (у smoke), ни звука от игры — дальше плагин сам решает, что происходит в этой точке. Сущность при этом не убирается сама: если она больше не нужна, убирай через `e.entity:remove()`.

Бомбу не задевает: у неё отдельная цепочка, `bomb_exploded`. Флешку тоже — у неё свой взрыв без урона и дыма, событие его не трогает.

<Warning>
Событие приходит из ReGameDLL. На ванильном `mp.dll` оно не
срабатывает никогда.
</Warning>
