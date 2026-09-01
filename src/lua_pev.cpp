#include "cslua.h"

#include "lua_pev.h"
#include "lua_entity.h"

#include <cstddef>
#include <string.h>

// Every field below is offsetof() against the real entvars_t in progdefs.h, so
// a rename/reorder upstream fails the build instead of reading wrong bytes.
enum PevType
{
	PEV_INT,
	PEV_FLOAT,
	PEV_VECTOR,
	PEV_STRING,
	PEV_EDICT,
};

struct PevField
{
	const char *name;
	PevType type;
	size_t offset;
};

#define F(name, type) { #name, type, offsetof(entvars_t, name) }

static const PevField s_pev_fields[] =
{
	F(classname,     PEV_STRING),
	F(globalname,    PEV_STRING),
	F(origin,         PEV_VECTOR),
	F(oldorigin,      PEV_VECTOR),
	F(velocity,       PEV_VECTOR),
	F(basevelocity,   PEV_VECTOR),
	F(clbasevelocity, PEV_VECTOR),
	F(movedir,        PEV_VECTOR),
	F(angles,         PEV_VECTOR),
	F(avelocity,      PEV_VECTOR),
	F(punchangle,     PEV_VECTOR),
	F(v_angle,        PEV_VECTOR),
	F(endpos,         PEV_VECTOR),
	F(startpos,       PEV_VECTOR),
	F(impacttime,    PEV_FLOAT),
	F(starttime,     PEV_FLOAT),
	F(fixangle,      PEV_INT),
	F(idealpitch,    PEV_FLOAT),
	F(pitch_speed,   PEV_FLOAT),
	F(ideal_yaw,     PEV_FLOAT),
	F(yaw_speed,     PEV_FLOAT),
	F(modelindex,    PEV_INT),
	F(model,         PEV_STRING),
	F(viewmodel,     PEV_INT),
	F(weaponmodel,   PEV_INT),
	F(absmin,         PEV_VECTOR),
	F(absmax,         PEV_VECTOR),
	F(mins,           PEV_VECTOR),
	F(maxs,           PEV_VECTOR),
	F(size,           PEV_VECTOR),
	F(ltime,         PEV_FLOAT),
	F(nextthink,     PEV_FLOAT),
	F(movetype,      PEV_INT),
	F(solid,         PEV_INT),
	F(skin,          PEV_INT),
	F(body,          PEV_INT),
	F(effects,       PEV_INT),
	F(gravity,       PEV_FLOAT),
	F(friction,      PEV_FLOAT),
	F(light_level,   PEV_INT),
	F(sequence,      PEV_INT),
	F(gaitsequence,  PEV_INT),
	F(frame,         PEV_FLOAT),
	F(animtime,      PEV_FLOAT),
	F(framerate,     PEV_FLOAT),
	// controller[4]/blending[2] are byte arrays, left out.
	F(scale,         PEV_FLOAT),
	F(rendermode,    PEV_INT),
	F(renderamt,     PEV_FLOAT),
	F(rendercolor,    PEV_VECTOR),
	F(renderfx,      PEV_INT),
	F(health,        PEV_FLOAT),
	F(frags,         PEV_FLOAT),
	F(weapons,       PEV_INT),
	F(takedamage,    PEV_FLOAT),
	F(deadflag,      PEV_INT),
	F(view_ofs,       PEV_VECTOR),
	F(button,        PEV_INT),
	F(impulse,       PEV_INT),
	F(chain,         PEV_EDICT),
	F(dmg_inflictor, PEV_EDICT),
	F(enemy,         PEV_EDICT),
	F(aiment,        PEV_EDICT),
	F(owner,         PEV_EDICT),
	F(groundentity,  PEV_EDICT),
	F(spawnflags,    PEV_INT),
	F(flags,         PEV_INT),
	F(colormap,      PEV_INT),
	F(team,          PEV_INT),
	F(max_health,    PEV_FLOAT),
	F(teleport_time, PEV_FLOAT),
	F(armortype,     PEV_FLOAT),
	F(armorvalue,    PEV_FLOAT),
	F(waterlevel,    PEV_INT),
	F(watertype,     PEV_INT),
	F(target,        PEV_STRING),
	F(targetname,    PEV_STRING),
	F(netname,       PEV_STRING),
	F(message,       PEV_STRING),
	F(dmg_take,      PEV_FLOAT),
	F(dmg_save,      PEV_FLOAT),
	F(dmg,           PEV_FLOAT),
	F(dmgtime,       PEV_FLOAT),
	F(noise,         PEV_STRING),
	F(noise1,        PEV_STRING),
	F(noise2,        PEV_STRING),
	F(noise3,        PEV_STRING),
	F(speed,             PEV_FLOAT),
	F(air_finished,      PEV_FLOAT),
	F(pain_finished,     PEV_FLOAT),
	F(radsuit_finished,  PEV_FLOAT),
	F(pContainingEntity, PEV_EDICT),
	F(playerclass,   PEV_INT),
	F(maxspeed,      PEV_FLOAT),
	F(fov,           PEV_FLOAT),
	F(weaponanim,    PEV_INT),
	F(pushmsec,      PEV_INT),
	F(bInDuck,          PEV_INT),
	F(flTimeStepSound,  PEV_INT),
	F(flSwimTime,       PEV_INT),
	F(flDuckTime,       PEV_INT),
	F(iStepLeft,        PEV_INT),
	F(flFallVelocity,   PEV_FLOAT),
	F(gamestate,     PEV_INT),
	F(oldbuttons,    PEV_INT),
	F(groupinfo,     PEV_INT),
	F(iuser1, PEV_INT), F(iuser2, PEV_INT), F(iuser3, PEV_INT), F(iuser4, PEV_INT),
	F(fuser1, PEV_FLOAT), F(fuser2, PEV_FLOAT), F(fuser3, PEV_FLOAT), F(fuser4, PEV_FLOAT),
	F(vuser1, PEV_VECTOR), F(vuser2, PEV_VECTOR), F(vuser3, PEV_VECTOR), F(vuser4, PEV_VECTOR),
	F(euser1, PEV_EDICT), F(euser2, PEV_EDICT), F(euser3, PEV_EDICT), F(euser4, PEV_EDICT),
};

#undef F

static const PevField *find_pev_field(const char *name)
{
	for (size_t i = 0; i < sizeof(s_pev_fields) / sizeof(s_pev_fields[0]); i++) {
		if (strcmp(s_pev_fields[i].name, name) == 0)
			return &s_pev_fields[i];
	}
	return NULL;
}

// Accepts an entity object ({index=...}), a player object ({id=...}), or nil.
edict_t *cslua_arg_to_edict(lua_State *L, int idx)
{
	if (lua_isnoneornil(L, idx))
		return NULL;

	luaL_checktype(L, idx, LUA_TTABLE);

	lua_getfield(L, idx, "index");
	bool has_index = lua_isnumber(L, -1) != 0;
	int n = has_index ? (int)lua_tointeger(L, -1) : 0;
	lua_pop(L, 1);

	// Only an entity object carries a serial - a stale one must not resolve to
	// whatever lives in the slot now. Player slots are not reused that way.
	int serial = 0;
	if (has_index) {
		lua_getfield(L, idx, "serial");
		serial = (int)lua_tointeger(L, -1);
		lua_pop(L, 1);
	}

	bool has_field = has_index;
	if (!has_field) {
		lua_getfield(L, idx, "id");
		has_field = lua_isnumber(L, -1) != 0;
		n = has_field ? (int)lua_tointeger(L, -1) : 0;
		lua_pop(L, 1);
	}

	if (!has_field)
		luaL_error(L, "e:pev: expected an entity or player object, or nil");

	edict_t *e = g_engfuncs.pfnPEntityOfEntIndex(n);
	if (!e || e->free || (has_index && e->serialnumber != serial))
		luaL_error(L, "e:pev: target has no valid entity");
	return e;
}

int cslua_pev_call(lua_State *L, entvars_t *pev, int name_arg)
{
	const char *name = luaL_checkstring(L, name_arg);
	const PevField *field = find_pev_field(name);
	if (!field)
		return luaL_error(L, "e:pev: unknown field '%s'", name);

	int value_arg = name_arg + 1;

	// lua_isnoneornil can't tell "no value" from "nil on purpose", and
	// e:pev("owner", nil) clearing an edict field is a real write. Whether an
	// argument slot exists at all is what matters.
	bool write = lua_gettop(L) >= value_arg;
	char *base = (char *)pev + field->offset;

	switch (field->type) {
	case PEV_INT: {
		int *p = (int *)base;
		if (!write) {
			lua_pushinteger(L, *p);
			return 1;
		}
		*p = (int)luaL_checkinteger(L, value_arg);
		return 0;
	}
	case PEV_FLOAT: {
		float *p = (float *)base;
		if (!write) {
			lua_pushnumber(L, *p);
			return 1;
		}
		*p = (float)luaL_checknumber(L, value_arg);
		return 0;
	}
	case PEV_VECTOR: {
		Vector *p = (Vector *)base;
		if (!write) {
			lua_pushnumber(L, p->x);
			lua_pushnumber(L, p->y);
			lua_pushnumber(L, p->z);
			return 3;
		}
		p->x = (float)luaL_checknumber(L, value_arg);
		p->y = (float)luaL_checknumber(L, value_arg + 1);
		p->z = (float)luaL_checknumber(L, value_arg + 2);
		return 0;
	}
	case PEV_STRING: {
		string_t *p = (string_t *)base;
		if (!write) {
			lua_pushstring(L, STRING(*p));
			return 1;
		}
		*p = ALLOC_STRING(luaL_checkstring(L, value_arg));
		return 0;
	}
	case PEV_EDICT: {
		edict_t **p = (edict_t **)base;
		if (!write) {
			cslua_push_entity_index(L, (*p && !(*p)->free) ? ENTINDEX(*p) : 0);
			return 1;
		}
		*p = cslua_arg_to_edict(L, value_arg);
		return 0;
	}
	}

	return luaL_error(L, "e:pev: unhandled field type for '%s'", name);
}
