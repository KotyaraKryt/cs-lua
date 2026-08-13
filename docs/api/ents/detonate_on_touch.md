---
title: e:detonate_on_touch
description: "Следующее касание сразу запускает think сущности"
---

# e:detonate_on_touch

Следующее касание сразу запускает think сущности.

```lua
e:detonate_on_touch()
```

## Возвращает

Ничего.

## Пример

```lua
hook.add("grenade_thrown", "myplugin.instant", function(e)
	if e.entity then
		e.entity:detonate_on_touch()
	end
end)
```

Одноразово: при первом касании чего угодно — стены, пола, игрока — движок
на следующем кадре сам вызовет think сущности, вместо того чтобы ждать её
обычный таймер. Для брошенной гранаты это и есть «взрыв при попадании»:
`ExplodeHeGrenade`/`ExplodeSmokeGrenade` — это и есть её think, только
случившийся сразу, а не через несколько секунд.

Не колбэк: Lua не узнаёт, что было касание, и не может решить, стоило ли
его ловить — только форсирует то, что сущность и так должна была сделать
рано или поздно. Для гранаты это ровно то, что нужно; для чего-то с другим
think эффект будет другим.

> [!WARNING]
> Объект от удалённой сущности — или переживший `changelevel` —
> бросает `entity #N is gone`. Проверяй [`e:valid()`](valid.md).

## Смотри также

- [grenade_thrown](../hook/grenade_thrown.md)
- [grenade_explode](../hook/grenade_explode.md)
