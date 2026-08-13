---
title: p:button
description: "Держит ли игрок кнопку прямо сейчас"
---

# p:button

Держит ли игрок кнопку прямо сейчас.

```lua
p:button(name)
```

## Аргументы

| # | имя | тип |  |
|---|---|---|---|
| 1 | `name` | string | `attack`, `attack2`, `jump`, `duck`, `use`, `reload`, `forward`, `back`, `moveleft`, `moveright` |

## Возвращает

| тип |  |
|---|---|
| `boolean` | зажата ли кнопка в этом кадре |

## Пример

```lua
timer.every(0.1, function()
	for _, p in ipairs(players.list()) do
		if p:alive() and p:button("use") then
			p:chat("держишь E")
		end
	end
end)
```

Только чтение: кнопку выставляет клиент в своей usercmd, писать в неё
бессмысленно — следующий кадр перезапишет. Читается напрямую из живых
entvars, без задержки на кадр.

Список кнопок сознательно короткий: остальное в usercmd (мышь, impulse)
не сводится к одному биту.
