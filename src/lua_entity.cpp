#include "cslua.h"

#include "lua_entity.h"
#include "lua_natives.h"
#include "lua_sound.h"
#include "cslua_corpse.h"
#include "cslua_visibility.h"
#include "lua_pev.h"
#include "lua_player.h"
#include "players.h"

#include <string.h>
#include <vector>

// The shared metatable for every entity object, kept as a registry ref.
static int s_entity_mt_ref = LUA_NOREF;

// Plugins load during Meta_Attach, before any map exists. The engine's entity
// calls walk an edict table that is not there and take the server down; a clear
// Lua error beats that.
static void require_world(lua_State *L, const char *what)
{
	if (!cslua_world_ready())
		luaL_error(L, "%s: no map is loaded yet - call this from an event "
			"(player_spawn, round_start), not while the plugin loads", what);
}

// An object holds the edict index AND the serial number, so an object left over
// from a removed entity (or from before a map change) is caught.
static edict_t *self_edict(lua_State *L, bool required = true)
{
	luaL_checktype(L, 1, LUA_TTABLE);

	lua_getfield(L, 1, "index");
	if (!lua_isnumber(L, -1))
		luaL_error(L, "e: expected an entity object, use e:method() and not e.method()");
	int index = (int)lua_tointeger(L, -1);
	lua_pop(L, 1);

	lua_getfield(L, 1, "serial");
	int serial = (int)lua_tointeger(L, -1);
	lua_pop(L, 1);

	if (!cslua_world_ready()) {
		if (required)
			luaL_error(L, "e: entity #%d is gone (the map it belonged to has ended)", index);
		return NULL;
	}

	edict_t *e = g_engfuncs.pfnPEntityOfEntIndex(index);
	if (!e || e->free || e->serialnumber != serial) {
		if (required)
			luaL_error(L, "e: entity #%d is gone", index);
		return NULL;
	}

	return e;
}

// Wraps an edict in a fresh object. Anything not worth talking about comes back
// as nil.
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

// For callers outside this file that only have an index (a hookchain arg, say).
void cslua_push_entity_index(lua_State *L, int index)
{
	if (!cslua_world_ready() || index <= 0) {
		lua_pushnil(L);
		return;
	}

	push_entity(L, g_engfuncs.pfnPEntityOfEntIndex(index));
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

// Writing origin goes through the engine so the entity is relinked; assigning
// pev->origin leaves it colliding where it used to be.
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

// e:velocity([vx, vy, vz]) - a plain physics field, unlike origin.
static int l_velocity(lua_State *L)
{
	return vector_field(L, self_edict(L)->v.velocity);
}

// e:model("models/w_ak47.mdl") - must be precached already (res.model).
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

// e:valid() - the one method that answers instead of raising.
static int l_valid(lua_State *L)
{
	lua_pushboolean(L, self_edict(L, false) != NULL);
	return 1;
}

// e:spawn() - run the entity's own Spawn. Split from ents.create() on purpose:
// keyvalues and the model have to be in place first.
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

// ---------------------------------------------------------------------------
// Touch-triggered think
//
// No layout hacking: pfnTouch is a proper metamod hook point. We can't run an
// arbitrary Lua function on touch, but we can force an entity's own think to
// run right away - which for a grenade means "detonate now instead of on fuse".
namespace {
struct TouchWatch { int index; int serial; };
std::vector<TouchWatch> s_touch_watch;
}

void cslua_touch_detonate_watch(int index, int serial)
{
	s_touch_watch.push_back({ index, serial });
}

void cslua_touch_detonate_check(edict_t *touched)
{
	if (!touched || s_touch_watch.empty())
		return;

	int index = ENTINDEX(touched);
	for (size_t i = 0; i < s_touch_watch.size(); i++) {
		if (s_touch_watch[i].index == index && s_touch_watch[i].serial == touched->serialnumber) {
			s_touch_watch.erase(s_touch_watch.begin() + i);
			touched->v.nextthink = gpGlobals->time;
			return;
		}
	}
}

void cslua_touch_detonate_clear()
{
	s_touch_watch.clear();
}

// e:detonate_on_touch() - one-shot: the next touch forces nextthink to fire on
// the following frame.
static int l_detonate_on_touch(lua_State *L)
{
	edict_t *e = self_edict(L);
	cslua_touch_detonate_watch(ENTINDEX(e), e->serialnumber);
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
	return luaL_error(L, "e: entity objects are read-only, keep your own state in a table");
}

// ents.create("info_target") -> a bare entity at the world origin, or nil for
// an unknown classname. Set the model and origin, then call e:spawn().
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

// ents.in_sphere(x, y, z, radius) -> everything within radius of a point.
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

// Reads a {x, y, z} table.
static Vector read_point(lua_State *L, int idx)
{
	luaL_checktype(L, idx, LUA_TTABLE);

	Vector v;
	lua_rawgeti(L, idx, 1); v.x = (float)luaL_checknumber(L, -1); lua_pop(L, 1);
	lua_rawgeti(L, idx, 2); v.y = (float)luaL_checknumber(L, -1); lua_pop(L, 1);
	lua_rawgeti(L, idx, 3); v.z = (float)luaL_checknumber(L, -1); lua_pop(L, 1);
	return v;
}

// Shared by ents.trace_line/ents.trace_hull: opts.skip and opts.ignore_monsters.
static void read_trace_opts(lua_State *L, int idx, edict_t **skip, int *no_monsters)
{
	*skip = NULL;
	*no_monsters = dont_ignore_monsters;

	if (lua_isnoneornil(L, idx))
		return;
	luaL_checktype(L, idx, LUA_TTABLE);

	lua_getfield(L, idx, "skip");
	*skip = cslua_arg_to_edict(L, -1);
	lua_pop(L, 1);

	lua_getfield(L, idx, "ignore_monsters");
	if (lua_toboolean(L, -1))
		*no_monsters = ignore_monsters;
	lua_pop(L, 1);
}

// ents.trace_line(a, b[, opts]) - a ray between two arbitrary points.
static int l_trace_line(lua_State *L)
{
	require_world(L, "ents.trace_line");

	Vector start = read_point(L, 1);
	Vector end = read_point(L, 2);

	edict_t *skip;
	int ignore_monsters_val;
	read_trace_opts(L, 3, &skip, &ignore_monsters_val);

	TraceResult tr;
	TRACE_LINE(start, end, ignore_monsters_val, skip, &tr);

	cslua_push_trace_result(L, tr, start);
	return 1;
}

// ents.trace_hull(a, b[, opts]) - sweeps a box. opts.hull picks which
// (point/human/large/head_hull).
static int l_trace_hull(lua_State *L)
{
	require_world(L, "ents.trace_hull");

	Vector start = read_point(L, 1);
	Vector end = read_point(L, 2);

	edict_t *skip;
	int ignore_monsters_val;
	read_trace_opts(L, 3, &skip, &ignore_monsters_val);

	int hull = human_hull;
	if (!lua_isnoneornil(L, 3)) {
		lua_getfield(L, 3, "hull");
		if (lua_isnumber(L, -1)) {
			int h = (int)lua_tointeger(L, -1);
			if (h >= point_hull && h <= head_hull)
				hull = h;
		}
		lua_pop(L, 1);
	}

	TraceResult tr;
	TRACE_HULL(start, end, ignore_monsters_val, hull, skip, &tr);

	cslua_push_trace_result(L, tr, start);
	return 1;
}

// e:keyvalue("targetname", "door1") - the same channel the map file uses. Must
// happen before e:spawn().
static int l_keyvalue(lua_State *L)
{
	edict_t *e = self_edict(L);
	const char *key = luaL_checkstring(L, 2);
	const char *value = luaL_checkstring(L, 3);

	KeyValueData kvd;
	kvd.szClassName = (char *)STRING(e->v.classname);
	kvd.szKeyName = (char *)key;
	kvd.szValue = (char *)value;
	kvd.fHandled = FALSE;

	MDLL_KeyValue(e, &kvd);

	lua_pushboolean(L, kvd.fHandled != FALSE);
	return 1;
}

// e:solid() - 0 not solid, 1 trigger, 2 bbox, 3 slidebox, 4 brush model.
static int l_solid(lua_State *L)
{
	edict_t *e = self_edict(L);

	if (lua_isnoneornil(L, 2)) {
		lua_pushinteger(L, e->v.solid);
		return 1;
	}

	e->v.solid = (int)luaL_checkinteger(L, 2);
	return 0;
}

// e:movetype() - 0 none, 4 step, 5 fly, 6 toss, 7 push, 8 noclip, 10 bounce,
// 12 follow (const.h MOVETYPE_*).
static int l_movetype(lua_State *L)
{
	edict_t *e = self_edict(L);

	if (lua_isnoneornil(L, 2)) {
		lua_pushinteger(L, e->v.movetype);
		return 1;
	}

	e->v.movetype = (int)luaL_checkinteger(L, 2);
	return 0;
}

// e:attach(player, x, y, z) - rides along on player. The offset goes into
// pev->v_angle, which SV_Physics_Follow adds to the parent's origin every frame.
static int l_attach(lua_State *L)
{
	edict_t *e = self_edict(L);

	luaL_checktype(L, 2, LUA_TTABLE);
	lua_getfield(L, 2, "id");
	if (!lua_isnumber(L, -1))
		return luaL_error(L, "e:attach: expected a player object");
	int id = (int)lua_tointeger(L, -1);
	lua_pop(L, 1);

	if (!cslua_valid_player_id(id))
		return luaL_error(L, "e:attach: invalid player");

	edict_t *parent = g_engfuncs.pfnPEntityOfEntIndex(id);
	if (!parent || parent->free)
		return luaL_error(L, "e:attach: target player has no valid entity");

	Vector offset(
		(float)luaL_optnumber(L, 3, 0),
		(float)luaL_optnumber(L, 4, 0),
		(float)luaL_optnumber(L, 5, 0));

	e->v.aiment = parent;
	e->v.movetype = MOVETYPE_FOLLOW;
	e->v.solid = SOLID_NOT;
	e->v.v_angle = offset;
	return 0;
}

// e:detach() - stops following; the entity stays where it last was.
static int l_detach(lua_State *L)
{
	edict_t *e = self_edict(L);

	e->v.aiment = NULL;
	e->v.movetype = MOVETYPE_NONE;
	return 0;
}

// e:size(minx..maxz) - the bounding box. Goes through the engine so the entity
// is relinked.
static int l_size(lua_State *L)
{
	edict_t *e = self_edict(L);

	if (lua_isnoneornil(L, 2)) {
		lua_pushnumber(L, e->v.mins.x); lua_pushnumber(L, e->v.mins.y);
		lua_pushnumber(L, e->v.mins.z); lua_pushnumber(L, e->v.maxs.x);
		lua_pushnumber(L, e->v.maxs.y); lua_pushnumber(L, e->v.maxs.z);
		return 6;
	}

	Vector mins((float)luaL_checknumber(L, 2), (float)luaL_checknumber(L, 3),
		(float)luaL_checknumber(L, 4));
	Vector maxs((float)luaL_checknumber(L, 5), (float)luaL_checknumber(L, 6),
		(float)luaL_checknumber(L, 7));

	SET_SIZE(e, mins, maxs);
	return 0;
}

// e:sequence([n]) - which studio animation the model plays. Numeric only.
static int l_sequence(lua_State *L)
{
	edict_t *e = self_edict(L);

	if (lua_isnoneornil(L, 2)) {
		lua_pushinteger(L, e->v.sequence);
		return 1;
	}

	e->v.sequence = (int)luaL_checkinteger(L, 2);
	return 0;
}

// e:frame([n]) - position within the current sequence, 0-255 (normalized byte).
// 255 with framerate 0 freezes on the last pose - the "corpse" trick.
static int l_frame(lua_State *L)
{
	edict_t *e = self_edict(L);

	if (lua_isnoneornil(L, 2)) {
		lua_pushnumber(L, e->v.frame);
		return 1;
	}

	e->v.frame = (float)luaL_checknumber(L, 2);
	return 0;
}

// e:framerate([n]) - playback speed, 1.0 normal, 0 stops.
static int l_framerate(lua_State *L)
{
	edict_t *e = self_edict(L);

	if (lua_isnoneornil(L, 2)) {
		lua_pushnumber(L, e->v.framerate);
		return 1;
	}

	e->v.framerate = (float)luaL_checknumber(L, 2);
	return 0;
}

// e:body([n]) / e:skin([n]) - which sub-model and texture group a multi-body
// studio model shows. Cosmetic.
static int l_body(lua_State *L)
{
	edict_t *e = self_edict(L);

	if (lua_isnoneornil(L, 2)) {
		lua_pushinteger(L, e->v.body);
		return 1;
	}

	e->v.body = (int)luaL_checkinteger(L, 2);
	return 0;
}

static int l_skin(lua_State *L)
{
	edict_t *e = self_edict(L);

	if (lua_isnoneornil(L, 2)) {
		lua_pushinteger(L, e->v.skin);
		return 1;
	}

	e->v.skin = (int)luaL_checkinteger(L, 2);
	return 0;
}

// e:render{ mode =, amount =, color =, fx = } - transparency and glow.
static int l_render(lua_State *L)
{
	edict_t *e = self_edict(L);

	if (lua_isnoneornil(L, 2)) {
		lua_newtable(L);
		lua_pushinteger(L, e->v.rendermode);  lua_setfield(L, -2, "mode");
		lua_pushnumber(L, e->v.renderamt);    lua_setfield(L, -2, "amount");
		lua_pushinteger(L, e->v.renderfx);    lua_setfield(L, -2, "fx");

		lua_newtable(L);
		lua_pushnumber(L, e->v.rendercolor.x); lua_rawseti(L, -2, 1);
		lua_pushnumber(L, e->v.rendercolor.y); lua_rawseti(L, -2, 2);
		lua_pushnumber(L, e->v.rendercolor.z); lua_rawseti(L, -2, 3);
		lua_setfield(L, -2, "color");
		return 1;
	}

	luaL_checktype(L, 2, LUA_TTABLE);

	lua_getfield(L, 2, "mode");
	if (lua_isnumber(L, -1))
		e->v.rendermode = (int)lua_tointeger(L, -1);
	lua_pop(L, 1);

	lua_getfield(L, 2, "amount");
	if (lua_isnumber(L, -1))
		e->v.renderamt = (float)lua_tonumber(L, -1);
	lua_pop(L, 1);

	lua_getfield(L, 2, "fx");
	if (lua_isnumber(L, -1))
		e->v.renderfx = (int)lua_tointeger(L, -1);
	lua_pop(L, 1);

	lua_getfield(L, 2, "color");
	if (lua_istable(L, -1)) {
		float rgb[3] = { 0.0f, 0.0f, 0.0f };
		for (int i = 0; i < 3; i++) {
			lua_rawgeti(L, -1, i + 1);
			if (lua_isnumber(L, -1))
				rgb[i] = (float)lua_tonumber(L, -1);
			lua_pop(L, 1);
		}
		e->v.rendercolor = Vector(rgb[0], rgb[1], rgb[2]);
	}
	lua_pop(L, 1);

	return 0;
}

// Accepts a player object ({id=...}) or a bare id; the target must be a
// connected client.
static int arg_to_player_slot(lua_State *L, int idx)
{
	int id = 0;
	bool valid = false;

	if (lua_isnumber(L, idx)) {
		id = (int)lua_tointeger(L, idx);
		valid = true;
	} else if (lua_istable(L, idx)) {
		lua_getfield(L, idx, "id");
		if (lua_isnumber(L, -1)) {
			id = (int)lua_tointeger(L, -1);
			valid = true;
		}
		lua_pop(L, 1);
	}

	if (!valid)
		luaL_error(L, "expected a player object or a player id");

	if (!cslua_valid_player_id(id) || !g_players.is_connected(id))
		luaL_error(L, "no such connected player #%d", id);

	return id;
}

// e:render_for(player[, opts]) - like e:render(), but only for one client.
// Goes through pfnAddToFullPack, the engine's per-recipient snapshot build.
static int l_render_for(lua_State *L)
{
	edict_t *e = self_edict(L);
	int recipient = arg_to_player_slot(L, 2);

	CsluaRenderOverride cur;
	bool has_cur = cslua_visibility_get(e, recipient, cur);

	if (lua_isnoneornil(L, 3)) {
		if (!has_cur) {
			lua_pushnil(L);
			return 1;
		}

		lua_newtable(L);
		lua_pushinteger(L, cur.mode);   lua_setfield(L, -2, "mode");
		lua_pushnumber(L, cur.amount);  lua_setfield(L, -2, "amount");
		lua_pushinteger(L, cur.fx);     lua_setfield(L, -2, "fx");

		lua_newtable(L);
		lua_pushnumber(L, cur.r); lua_rawseti(L, -2, 1);
		lua_pushnumber(L, cur.g); lua_rawseti(L, -2, 2);
		lua_pushnumber(L, cur.b); lua_rawseti(L, -2, 3);
		lua_setfield(L, -2, "color");
		return 1;
	}

	luaL_checktype(L, 3, LUA_TTABLE);

	// A field opts leaves out keeps this override's current value, or (the
	// first time) the entity's own render.
	CsluaRenderOverride values = has_cur ? cur : CsluaRenderOverride{
		e->v.rendermode, e->v.renderamt,
		e->v.rendercolor.x, e->v.rendercolor.y, e->v.rendercolor.z,
		e->v.renderfx
	};

	lua_getfield(L, 3, "mode");
	if (lua_isnumber(L, -1))
		values.mode = (int)lua_tointeger(L, -1);
	lua_pop(L, 1);

	lua_getfield(L, 3, "amount");
	if (lua_isnumber(L, -1))
		values.amount = (float)lua_tonumber(L, -1);
	lua_pop(L, 1);

	lua_getfield(L, 3, "fx");
	if (lua_isnumber(L, -1))
		values.fx = (int)lua_tointeger(L, -1);
	lua_pop(L, 1);

	lua_getfield(L, 3, "color");
	if (lua_istable(L, -1)) {
		float rgb[3] = { values.r, values.g, values.b };
		for (int i = 0; i < 3; i++) {
			lua_rawgeti(L, -1, i + 1);
			if (lua_isnumber(L, -1))
				rgb[i] = (float)lua_tonumber(L, -1);
			lua_pop(L, 1);
		}
		values.r = rgb[0];
		values.g = rgb[1];
		values.b = rgb[2];
	}
	lua_pop(L, 1);

	cslua_visibility_set(e, recipient, values);
	return 0;
}

// e:clear_render_for(player) - removes an override.
static int l_clear_render_for(lua_State *L)
{
	edict_t *e = self_edict(L);
	int recipient = arg_to_player_slot(L, 2);
	cslua_visibility_clear(e, recipient);
	return 0;
}

// e:visible_to(player, visible) - the common case: make an entity invisible to
// one client (kRenderTransAlpha, amount 0). visible = true clears the override.
static int l_visible_to(lua_State *L)
{
	edict_t *e = self_edict(L);
	int recipient = arg_to_player_slot(L, 2);
	bool visible = lua_toboolean(L, 3) != 0;

	if (visible) {
		cslua_visibility_clear(e, recipient);
		return 0;
	}

	CsluaRenderOverride values = { kRenderTransAlpha, 0.0f, 0.0f, 0.0f, 0.0f, kRenderFxNone };
	cslua_visibility_set(e, recipient, values);
	return 0;
}

// e:pev(name[, v...]) - generic read/write for any entvars_t field by name.
static int l_pev(lua_State *L)
{
	edict_t *e = self_edict(L);
	return cslua_pev_call(L, &e->v, 2);
}

static const luaL_Reg s_methods[] =
{
	{ "origin",    l_origin },
	{ "pev",       l_pev },
	{ "angles",    l_angles },
	{ "velocity",  l_velocity },
	{ "model",     l_model },
	{ "classname", l_classname },
	{ "valid",     l_valid },
	{ "spawn",     l_spawn },
	{ "remove",    l_remove },
	{ "detonate_on_touch", l_detonate_on_touch },
	{ "keyvalue",  l_keyvalue },
	{ "solid",     l_solid },
	{ "movetype",  l_movetype },
	{ "attach",    l_attach },
	{ "detach",    l_detach },
	{ "size",      l_size },
	{ "render",    l_render },
	{ "render_for",       l_render_for },
	{ "clear_render_for", l_clear_render_for },
	{ "visible_to",       l_visible_to },
	{ "sequence",  l_sequence },
	{ "frame",     l_frame },
	{ "framerate", l_framerate },
	{ "body",      l_body },
	{ "skin",      l_skin },
	{ NULL, NULL }
};

// ents.hide_corpses(true) - swallow every player's native death ragdoll before
// it reaches any client. No per-player targeting (see cslua_corpse.cpp).
// Reference-counted: call in matching pairs.
static int l_hide_corpses(lua_State *L)
{
	luaL_checktype(L, 1, LUA_TBOOLEAN);
	cslua_corpse_hide_ref(lua_toboolean(L, 1) != 0);
	return 0;
}

static const luaL_Reg s_api[] =
{
	{ "create",       l_create_entity },
	{ "find",         l_entities },
	{ "in_sphere",    l_find_in_sphere },
	{ "trace_line",   l_trace_line },
	{ "trace_hull",   l_trace_hull },
	{ "hide_corpses", l_hide_corpses },
	{ NULL, NULL }
};

void cslua_register_entity(lua_State *L)
{
	cslua_push_metatable(L, s_methods);

	lua_pushcfunction(L, l_tostring);
	lua_setfield(L, -2, "__tostring");

	// Same reasoning as the player object: an entity handed to another plugin
	// must not carry one plugin's scratch fields.
	lua_pushcfunction(L, l_readonly);
	lua_setfield(L, -2, "__newindex");

	s_entity_mt_ref = luaL_ref(L, LUA_REGISTRYINDEX);

	cslua_register_namespace(L, "ents", s_api);
}

void cslua_entity_shutdown()
{
	s_entity_mt_ref = LUA_NOREF;
}
