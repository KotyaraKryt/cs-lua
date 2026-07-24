# События

```lua
on("player_ready", function(p)
	p:chat("Welcome, " .. p:name())
end)
```

`client_*` — сетевой слой, игрока как такового может ещё не быть.
`player_*` — игрок уже в игре.

| событие             | аргументы                   | возврат          | ReGameDLL |
|---------------------|-----------------------------|------------------|:---------:|
| `client_connect`    | `p, name, ip`               | `false[, причина]` отклоняет | |
| `client_disconnect` | `p, name`                   | —                | |
| `player_authorized` | `p, steamid`                | —                | |
| `player_ready`      | `p`                         | —                | |
| `player_chat`       | `p, text, team`             | `false` проглотит | |
| `menu_select`       | `p, key`                    | —                | |
| `player_spawn`      | `p`                         | —                | да |
| `player_hurt`       | `p, attacker, damage, bits` | `false` \| число \| `nil` | да |
| `player_hurt_post`  | `p, attacker, damage, bits` | —                | да |
| `player_death`      | `p, killer, headshot`       | —                | да |
| `round_start`       | —                           | —                | да |
| `round_end`         | `winner` (1=CT, 2=T, 3=ничья) | —              | да |
| `round_freeze_end`  | —                           | —                | да |
| `bomb_planted`      | `p`                         | —                | да |
| `bomb_defused`      | `p, success`                | —                | да |
| `bomb_exploded`     | `x, y, z`                   | —                | да |

События с пометкой ReGameDLL на ванильном `mp.dll` просто не срабатывают.
Hookchain вешается только на те события, на которые есть подписка.

Ошибка в хендлере логируется с трейсбеком и не мешает остальным хендлерам.

## Три ловушки

**В `client_connect` бесполезен `p:steamid()`.** Steam отвечает на пару секунд
позже, до этого движок отдаёт `STEAM_ID_PENDING`. Всё, что завязано на steamid
(права, статистика, баны), вешай на `player_authorized` — оно срабатывает ровно
один раз, когда id стал настоящим. У ботов и на LAN id финальный сразу.

**В `client_connect` бесполезны и сообщения** — клиент ещё не получил таблицу
user messages. Для приветствий есть `player_ready`.

**В `player_death` `killer` может быть `nil`** (падение, world, урон окружения),
а суицид приходит как `killer.id == victim.id`. Проверять надо оба случая.

## player_hurt

Единственное событие, которое влияет на игру, а не наблюдает. Урон приходит по
ссылке, и то, что вернёт хендлер, движок и применит:

| возврат | что будет |
|---------|-----------|
| `false` | удар погашен полностью — ни звука боли, ни брони, ни смерти |
| число   | заменяет урон |
| `nil`   | оставить как есть |

```lua
on("player_hurt", function(victim, attacker, damage, bits)
	return damage * 2
end)
```

Если хендлеров несколько, каждый получает урон после предыдущего. Как только
кто-то обнулил — цепочка обрывается.

`bits` — маска типа урона (`DMG_FALL`, `DMG_BULLET`), приходит числом.

## Pre и Post

Там, где Pre и Post содержательно разные, есть отдельное имя с суффиксом
`_post`. Пока такое одно:

- `player_hurt` — урон ещё не нанесён, можно поменять или отменить;
- `player_hurt_post` — урон применён, `p:health()` актуальное.

`spawn`, `death`, `round_*` зовутся уже после игрового действия — отдельный Post
им не нужен.
