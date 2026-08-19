---
title: hook — Магазин
description: "Одна система на движковые события и на свои"
---

# Магазин

## weapon_buy {#weapon_buy}

Игрок покупает оружие в магазине.

```lua
hook.add("weapon:buy", id, function(e)
	...
end)
```

### Поля события {#weapon_buy-поля события}

| поле | тип |  |
|---|---|---|
| `e.player` | player | кого касается событие |
| `e.weapon` | string | classname покупаемого оружия |

### Пример

```lua
hook.add("weapon:buy", "myplugin.no_awp", function(e)
	if e.weapon == "weapon_awp" then
		e.player:hud("AWP запрещён на этой карте")
		e:cancel()
	end
end)
```

**Отмена.** `e:cancel()` отменяет покупку целиком: деньги не списываются, оружие не выдаётся — как будто игрок ничего не покупал.

Приходит на любую покупку оружия — ручную, ребай и автобай, все идут через один и тот же вызов движка. Сама сущность оружия на этот момент ещё не создана, поэтому в событии нет `e.entity`.

<Warning>
Событие приходит из ReGameDLL. На ванильном `mp.dll` оно не
срабатывает никогда.
</Warning>

### Смотри также

- [ammo_buy](shop.md#ammo_buy)
- [item_buy](shop.md#item_buy)

## ammo_buy {#ammo_buy}

Игрок покупает патроны для оружия в руках.

```lua
hook.add("ammo:buy", id, function(e)
	...
end)
```

### Поля события {#ammo_buy-поля события}

| поле | тип |  |
|---|---|---|
| `e.player` | player | кого касается событие |
| `e.weapon` | string | classname оружия, для которого докупают патроны |

**Отмена.** `e:cancel()` отменяет покупку: деньги не списываются, патроны не добавляются.

<Warning>
Событие приходит из ReGameDLL. На ванильном `mp.dll` оно не
срабатывает никогда.
</Warning>

### Смотри также

- [weapon_buy](shop.md#weapon_buy)

## item_buy {#item_buy}

Игрок покупает не-оружие в магазине: броню, прибор ночного видения, набор для разминирования, щит или гранату.

```lua
hook.add("item:buy", id, function(e)
	...
end)
```

### Поля события {#item_buy-поля события}

| поле | тип |  |
|---|---|---|
| `e.player` | player | кого касается событие |
| `e.item` | string | `vest`, `vesthelm`, `flashbang`, `hegrenade`, `smokegrenade`, `nvg`, `defusekit` или `shield` |

### Пример

```lua
hook.add("item:buy", "myplugin.no_defuse_for_t", function(e)
	if e.item == "defusekit" and e.player:team() == "t" then
		e:cancel()
	end
end)
```

**Отмена.** `e:cancel()` отменяет покупку: деньги не списываются, предмет не выдаётся.

Гранаты (`hegrenade`, `flashbang`, `smokegrenade`) идут через тот же пункт меню, что и броня/щит — это не то же самое, что бросок: `weapon_buy` про оружие, `item_buy` про всё остальное в магазине.

<Warning>
Событие приходит из ReGameDLL. На ванильном `mp.dll` оно не
срабатывает никогда.
</Warning>

### Смотри также

- [weapon_buy](shop.md#weapon_buy)
