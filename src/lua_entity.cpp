#include "cslua.h"

#include "lua_entity.h"
#include "lua_natives.h"
#include "lua_sound.h"

#include <string.h>

// The shared metatable for every entity object; kept as a registry ref so a
// pushed entity costs one table plus a lookup, not a rebuilt method set.
static int s_entity_mt_ref = LUA_NOREF;

// Plugins load during Meta_Attach, before any map exists. The engine's entity
// calls do not check for that - they walk an edict table that has not been
// allocated and take the whole server down without a word. A clear Lua error
// beats a silent crash, and it points at the fix: do this from an event.
static void require_world(lua_State *L, const char *what)
{
	if (!cslua_world_ready())
		luaL_error(L, "%s: no map is loaded yet - call this from an event "
			"(player_spawn, round_start), not while the plugin loads", what);
}

// Entities die and their slots come back as something else. An object holds
// the edict index AND the serial number the engine stamped on it, so an object
// left over from a removed entity - or from before a map change - is caught
// rather than quietly writing into whatever moved in.
static edict_t *self_edict(lua_State *L, bool required = true)
{
	luaL_checktype(L, 1, LUA_TTABLE);

	lua_getfield(L, 1, "index");
	if (!lua_isnumber(L, -1))
		luaL_error(L, "expected an entity object, use e:method() and not e.method()");
	int index = (int)lua_tointeger(L, -1);
	lua_pop(L, 1);

	lua_getfield(L, 1, "serial");
	int serial = (int)lua_tointeger(L, -1);
	lua_pop(L, 1);

	// An object that outlived its map: the edicts are gone, and asking the
	// engine about them would be the same crash as touching one too early.
	if (!cslua_world_ready()) {
		if (required)
			luaL_error(L, "entity #%d is gone (the map it belonged to has ended)", index);
		return NULL;
	}

	edict_t *e = g_engfuncs.pfnPEntityOfEntIndex(index);
	if (!e || e->free || e->serialnumber != serial) {
		if (required)
			luaL_error(L, "entity #%d is gone", index);
		return NULL;
	}

	return e;
}

// Wraps an edict in a fresh object. Anything not worth talking about - a free
// slot, the null edict - comes back as nil, so scripts test it plainly.
static void push_entity(lua_State *L, edict_t *e)
{
	if (!e || e->free || s_entity_mt_ref == LUA_NOREF) {
		lua_pushnil(L);
		return;
	}

	lua_newtable(L);

	lua_pushinteger(L, ENTINDEX(e));
	lua_setfield(L, -2, "index");

	lua_pushinteger(L, e->serialnumber);
	lua_setfield(L, -2, "serial");

	lua_rawgeti(L, LUA_REGISTRYINDEX, s_entity_mt_ref);
	lua_setmetatable(L, -2);
}

// Same read/write-in-one-method idiom the player object uses.
static int vector_field(lua_State *L, Vector &field)
{
	if (lua_isnoneornil(L, 2)) {
		lua_pushnumber(L, field.x);
		lua_pushnumber(L, field.y);
		lua_pushnumber(L, field.z);
		return 3;
	}

	field.x = (float)luaL_checknumber(L, 2);
	field.y = (float)luaL_checknumber(L, 3);
	field.z = (float)luaL_checknumber(L, 4);
	return 0;
}

// Writing origin goes through the engine so the entity is relinked into the
// world; assigning pev->origin leaves it colliding where it used to be.
static int l_origin(lua_State *L)
{
	edict_t *e = self_edict(L);

	if (lua_isnoneornil(L, 2)) {
		lua_pushnumber(L, e->v.origin.x);
		lua_pushnumber(L, e->v.origin.y);
		lua_pushnumber(L, e->v.origin.z);
		return 3;
	}

	Vector where(
		(float)luaL_checknumber(L, 2),
		(float)luaL_checknumber(L, 3),
		(float)luaL_checknumber(L, 4));

	SET_ORIGIN(e, where);
	return 0;
}

static int l_angles(lua_State *L)
{
	return vector_field(L, self_edict(L)->v.angles);
}

// e:model("models/w_ak47.mdl") - the model has to be precached already, so put
// res.model() at the top of the plugin. Setting one that is not costs the
// server a "no precache" error and the entity draws as nothing.
static int l_model(lua_State *L)
{
	edict_t *e = self_edict(L);

	if (lua_isnoneornil(L, 2)) {
		lua_pushstring(L, STRING(e->v.model));
		return 1;
	}

	SET_MODEL(e, luaL_checkstring(L, 2));
	return 0;
}

static int l_classname(lua_State *L)
{
	lua_pushstring(L, STRING(self_edict(L)->v.classname));
	return 1;
}

// e:valid() - the one method that answers instead of raising. Anything holding
// on to an entity across a round or a map change wants this first.
static int l_valid(lua_State *L)
{
	lua_pushboolean(L, self_edict(L, false) != NULL);
	return 1;
}

// e:spawn() - run the entity's own Spawn, which is what turns a bare edict into
// something the game treats as real: collision box, think function, the lot.
// Split from ents.create() on purpose - keyvalues and the model have to be in
// place before Spawn reads them.
static int l_spawn(lua_State *L)
{
	MDLL_Spawn(self_edict(L));
	return 0;
}

static int l_remove(lua_State *L)
{
	edict_t *e = self_edict(L, false);
	if (e)
		REMOVE_ENTITY(e);
	return 0;
}

static int l_tostring(lua_State *L)
{
	edict_t *e = self_edict(L, false);
	if (!e)
		lua_pushstring(L, "entity (gone)");
	else
		lua_pushfstring(L, "entity #%d %s", ENTINDEX(e), STRING(e->v.classname));
	return 1;
}

static int l_readonly(lua_State *L)
{
	return luaL_error(L, "entity objects are read-only, keep your own state in a table");
}

// ents.create("info_target") -> a bare entity at the world origin, or nil if
// the game DLL does not know that classname.
//
// It is not in the world yet in any useful sense: set the model and the origin,
// then call e:spawn(). Doing it in that order is the difference between a prop
// that works and one the game ignores.
static int l_create_entity(lua_State *L)
{
	const char *classname = luaL_checkstring(L, 1);
	require_world(L, "ents.create");

	edict_t *e = CREATE_NAMED_ENTITY(ALLOC_STRING(classname));
	if (!e) {
		lua_pushnil(L);
		lua_pushfstring(L, "unknown classname '%s'", classname);
		return 2;
	}

	push_entity(L, e);
	return 1;
}

// ents.find("func_door") -> array of everything with that classname.
static int l_entities(lua_State *L)
{
	const char *classname = luaL_checkstring(L, 1);
	require_world(L, "ents.find");

	lua_newtable(L);

	int n = 0;
	edict_t *e = NULL;
	while ((e = FIND_ENTITY_BY_STRING(e, "classname", classname)) != NULL) {
		if (FNullEnt(e))
			break;

		push_entity(L, e);
		if (lua_isnil(L, -1)) {
			lua_pop(L, 1);
			continue;
		}
		lua_rawseti(L, -2, ++n);
	}

	return 1;
}

// ents.in_sphere(x, y, z, radius) -> everything within radius of a point, in no
// particular order. This is what a zone is built out of: a trigger volume with
// no entity of its own.
static int l_find_in_sphere(lua_State *L)
{
	Vector center(
		(float)luaL_checknumber(L, 1),
		(float)luaL_checknumber(L, 2),
		(float)luaL_checknumber(L, 3));
	float radius = (float)luaL_checknumber(L, 4);
	require_world(L, "ents.in_sphere");

	lua_newtable(L);

	int n = 0;
	edict_t *e = NULL;
	while ((e = FIND_ENTITY_IN_SPHERE(e, center, radius)) != NULL) {
		if (FNullEnt(e))
			break;

		push_entity(L, e);
		if (lua_isnil(L, -1)) {
			lua_pop(L, 1);
			continue;
		}
		lua_rawseti(L, -2, ++n);
	}

	return 1;
}

static const luaL_Reg s_methods[] =
{
	{ "origin",    l_origin },
	{ "angles",    l_angles },
	{ "model",     l_model },
	{ "classname", l_classname },
	{ "valid",     l_valid },
	{ "spawn",     l_spawn },
	{ "remove",    l_remove },
	{ NULL, NULL }
};

static const luaL_Reg s_api[] =
{
	{ "create",    l_create_entity },
	{ "find",      l_entities },
	{ "in_sphere", l_find_in_sphere },
	{ NULL, NULL }
};

void cslua_register_entity(lua_State *L)
{
	lua_newtable(L);				// metatable

	lua_newtable(L);				// methods
	for (const luaL_Reg *r = s_methods; r->name; r++) {
		lua_pushcfunction(L, r->func);
		lua_setfield(L, -2, r->name);
	}
	lua_setfield(L, -2, "__index");

	lua_pushcfunction(L, l_tostring);
	lua_setfield(L, -2, "__tostring");

	// Same reasoning as the player object: an entity handed to another plugin
	// through an export must not carry one plugin's scratch fields.
	lua_pushcfunction(L, l_readonly);
	lua_setfield(L, -2, "__newindex");

	lua_pushboolean(L, 0);
	lua_setfield(L, -2, "__metatable");

	s_entity_mt_ref = luaL_ref(L, LUA_REGISTRYINDEX);

	cslua_register_namespace(L, "ents", s_api);
}

void cslua_entity_shutdown()
{
	// The ref dies with the state; just forget it.
	s_entity_mt_ref = LUA_NOREF;
}
