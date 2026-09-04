# cs-lua ↔ AmxModX: доступ к army_ranks_ultimate из Lua

Собрано из ветки `experiment/amxx-native-bridge` репозитория cs-lua
(коммиты `7cb690e`, `de08e36`). Экспериментальная штука: в `main` не
входит, поддерживается ровно до тех пор, пока нужна.

## Что здесь

```
common/addons/                         нужно в любом случае
  amxmodx/plugins/cslua_army_ranks.amxx    готовый плагин-обёртка
  amxmodx/scripting/cslua_army_ranks.sma   его исходник
  lua/plugins/army_ranks/                  Lua-обёртка с человеческим API

linux/addons/                          если прод на Linux
  amxmodx/modules/cslua_bridge_amxx_i386.so
  lua/lua_mm_i386.so

windows/addons/                        если прод на Windows
  amxmodx/modules/cslua_bridge_amxx.dll
  lua/lua_mm.dll
```

Копировать поверх `cstrike/`: сначала `common/addons/`, затем `addons/` из
своей платформы. Linux-бинарники собраны в ubuntu:18.04 (glibc 2.27), тем
же способом, что и релизные сборки cs-lua.

## Как это работает

Pawn резолвит натива на этапе компиляции, поэтому позвать чужой натив
«по имени» снаружи нельзя. Цепочка получается такая:

```
Lua: ar.level_name(p)
  -> amxx.call("ARB_LevelName", p.id, amxx.out())
  -> форвард AMXX по имени public
  -> cslua_army_ranks.amxx: public ARB_LevelName(id, name[])
  -> натив ar_get_user_level(id, name, len)   <- вот тут army_ranks
```

Три звена: модуль-мост (C++), плагин-обёртка (Pawn), Lua-обёртка. Если
любого нет — функции возвращают `nil` и один раз пишут причину в лог.

## Установка

**1. Модуль.** Положить `cslua_bridge_amxx.dll` в
`cstrike/addons/amxmodx/modules/` и дописать в
`cstrike/addons/amxmodx/configs/modules.ini` строку:

```
cslua_bridge
```

Имя именно такое. `_amxx` внутри имени модуля вешает AMXX 1.9.0.5249
намертво при старте (проверено), суффикс `_amxx` он дописывает сам.

**2. Плагин-обёртка.** `cslua_army_ranks.amxx` уже собран (amxxpc
1.9.0.5271 против настоящего `army_ranks_ultimate.inc`) — просто положить
в `plugins/`. Пересобрать при желании:

```
cd cstrike/addons/amxmodx/scripting
./amxxpc cslua_army_ranks.sma -iinclude
```

Строку в `configs/plugins.ini` — **после** `army_ranks_ultimate.amxx`:

```
cslua_army_ranks.amxx
```

**3. cs-lua.** Заменить `addons/lua/lua_mm.dll` (в этой сборке добавлен
namespace `amxx`) и скопировать папку `addons/lua/plugins/army_ranks/`.

**4. Проверка.** В консоли сервера:

```
amxx modules          ; cslua_bridge должен быть в списке
ar_check 1            ; уровень/звание/опыт игрока с индексом 1
```

## Использование из своих плагинов

```lua
local ar = import("army_ranks")

hook.add("player_spawn", "greet", function(e)
    local p = e.player
    local lvl, name = ar.level(p), ar.level_name(p)
    if lvl and name then
        p:chat(("Звание: %s (уровень %d, опыт %d)")
            :format(name, lvl, ar.all_xp(p) or 0))
    end
end)
```

Всё, что есть: `level`, `level_name`, `all_xp`, `real_xp`, `add_xp`,
`anew`, `bonus_hp`, `style`, `write`, `max_levels`, `level_xp`,
`name_of_level`, `csdm`, `map_locked`, `give_real_xp`, `give_add_xp`,
`give_anew`, `update`. Любая принимает объект игрока или его индекс.

## Ограничения

- **Только в одну сторону: Lua зовёт Pawn.** Форварды самого army_ranks
  (`ar_forward_newlevel`, `ar_forward_addxp`, `ar_forward_addanew`,
  `ar_forward_putinserver`) сюда не приходят — обратное направление в
  мосте не реализовано.
- **`ar_get_stats_data` не обёрнут** — там массивы `data[4]`/`stats[22]`,
  мост пока умеет только числа и строки.
- Аргументов у одного вызова не больше четырёх; строка наружу — только
  последним аргументом, строка внутрь — только первым.
