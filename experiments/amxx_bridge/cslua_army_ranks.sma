// Glue between cs-lua's amxx.call() and army_ranks_ultimate.
//
// The bridge reaches `public` functions, and Pawn resolves natives at
// compile time, so every native worth exposing gets a one-line wrapper
// here. Compile with amxxpc against army_ranks_ultimate.inc and load it
// AFTER army_ranks_ultimate.amxx.
//
// From Lua:
//   amxx.call("ARB_Level", p.id)                  -> уровень
//   amxx.call("ARB_LevelName", p.id, amxx.out())  -> _, true, "Рядовой"
//
// Naming: out-strings must be the last argument, in-strings the first -
// those are the shapes the bridge knows how to build a forward for.

#include <amxmodx>
#include <army_ranks_ultimate>

// AMXX allots exactly STRINGEX_MAXLENGTH (128) cells for a forward's
// out-string, but a `name[]` parameter carries no size the compiler can
// see, so charsmax() on it is indeterminate - spell the size out.
#define ARB_STRLEN 127

public plugin_init()
{
    register_plugin("cs-lua <-> Army Ranks bridge", "0.1", "cs-lua")
}

// ---------------------------------------------------------------- ints --

public ARB_Level(id)        return ar_get_user_level(id)
public ARB_AllXP(id)        return ar_get_user_allxp(id)
public ARB_RealXP(id)       return ar_get_user_realxp(id)
public ARB_AddXP(id)        return ar_get_user_addxp(id)
public ARB_Anew(id)         return ar_get_user_anew(id)
public ARB_BonusHP(id)      return ar_get_bonus_hp(id)
public ARB_MaxLevels()      return ar_get_maxlevels()
public ARB_LevelXP(level)   return ar_get_levelxp(level)
public ARB_Csdm()           return ar_get_csdm()
public ARB_UpdatePlayer(id) return ar_update_player(id)

public ARB_SetRealXP(id, xp)  return ar_set_user_realxp(id, xp)
public ARB_SetAddXP(id, xp)   return ar_set_user_addxp(id, xp)

public ARB_AddAnew(admin, player, anew) return ar_add_user_anew(admin, player, anew)

// ------------------------------------------------------------- strings --
// The buffer AMXX hands a forward is always 128 cells, so no length
// argument needs to travel from Lua.

public ARB_LevelName(id, name[])
{
    return ar_get_user_level(id, name, ARB_STRLEN)
}

public ARB_NameOfLevel(level, name[])
{
    return ar_get_levelname(level, name, ARB_STRLEN)
}

public ARB_Style(id, style[])
{
    return ar_get_user_style(id, style, ARB_STRLEN)
}

public ARB_Write(id, write[])
{
    return ar_get_user_write(id, write, ARB_STRLEN)
}

// -------------------------------------------------------- string input --

public ARB_LockMap(const mapname[])
{
    return ar_get_lockmap(mapname)
}

// Без const: ar_get_write_addxp объявлен как write[], а не const write[] -
// с const компилятор ругается на несоответствие типа первого аргумента.
public ARB_WriteAddXP(write[])
{
    return ar_get_write_addxp(write)
}
