// Заглушка army_ranks для проверки Lua-обёртки без самого army_ranks:
// те же имена public, что и в cslua_army_ranks.sma, но с выдуманными
// данными. Только для тестового сервера.

#include <amxmodx>

#define ARB_STRLEN 127

public plugin_init()
{
    register_plugin("Army Ranks stub", "0.1", "cs-lua")
}

public ARB_Level(id)   return 7
public ARB_AllXP(id)   return 1234
public ARB_RealXP(id)  return 1000
public ARB_AddXP(id)   return 234
public ARB_Anew(id)    return 3
public ARB_BonusHP(id) return 15
public ARB_MaxLevels() return 20
public ARB_Csdm()      return 0

public ARB_LevelXP(level) return level * 100

public ARB_LevelName(id, name[])
{
    formatex(name, ARB_STRLEN, "Старший сержант")
    return 7
}

public ARB_NameOfLevel(level, name[])
{
    formatex(name, ARB_STRLEN, "звание #%d", level)
    return level
}

public ARB_Style(id, style[])
{
    formatex(style, ARB_STRLEN, "body { color: #fff; }")
    return 1
}

public ARB_Write(id, write[])
{
    formatex(write, ARB_STRLEN, "STEAM_0:1:12345")
    return 1
}

public ARB_LockMap(const mapname[])
{
    return equal(mapname, "de_dust2") ? 1 : 0
}

public ARB_SetRealXP(id, xp)  return xp
public ARB_SetAddXP(id, xp)   return xp
public ARB_UpdatePlayer(id)   return 1

public ARB_AddAnew(admin, player, anew) return anew
