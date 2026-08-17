#include "cslua.h"
#include "lua_fx.h"
#include "lua_natives.h"

// Not in common/const.h - it lives in the dlls/hltv headers we don't want to
// drag in for one integer. Same call lua_message.cpp already made.
#define SVC_TEMPENTITY 23

static int model_index(const char *path)
{
	// Idempotent: the engine looks the name up if it is already precached
	// rather than adding a new slot, so this never risks the 512-slot table -
	// as long as the path went through res.model first, same contract as
	// p:play_sound and res.sound.
	return PRECACHE_MODEL((char *)path);
}

static void read_origin(lua_State *L, int base, Vector &out)
{
	out.x = (float)luaL_checknumber(L, base);
	out.y = (float)luaL_checknumber(L, base + 1);
	out.z = (float)luaL_checknumber(L, base + 2);
}

static int opt_int(lua_State *L, int table, const char *field, int def)
{
	lua_getfield(L, table, field);
	int v = lua_isnil(L, -1) ? def : (int)luaL_checkinteger(L, -1);
	lua_pop(L, 1);
	return v;
}

static float opt_float(lua_State *L, int table, const char *field, float def)
{
	lua_getfield(L, table, field);
	float v = lua_isnil(L, -1) ? def : (float)luaL_checknumber(L, -1);
	lua_pop(L, 1);
	return v;
}

static const char *require_sprite(lua_State *L, int table)
{
	lua_getfield(L, table, "sprite");
	if (!lua_isstring(L, -1))
		luaL_error(L, "opts.sprite is required and must be a path already precached with res.model");
	const char *s = lua_tostring(L, -1);
	lua_pop(L, 1);
	return s;
}

static void opt_color(lua_State *L, int table, int &r, int &g, int &b)
{
	r = g = b = 255;
	lua_getfield(L, table, "color");
	if (lua_istable(L, -1)) {
		lua_rawgeti(L, -1, 1); r = (int)luaL_optinteger(L, -1, 255); lua_pop(L, 1);
		lua_rawgeti(L, -1, 2); g = (int)luaL_optinteger(L, -1, 255); lua_pop(L, 1);
		lua_rawgeti(L, -1, 3); b = (int)luaL_optinteger(L, -1, 255); lua_pop(L, 1);
	}
	lua_pop(L, 1);
}

// Reads opts.to = { x, y, z } if present, otherwise start + (0, 0, height).
static void opt_endpoint(lua_State *L, int table, const Vector &start, float default_height, Vector &out)
{
	lua_getfield(L, table, "to");
	if (lua_istable(L, -1)) {
		lua_rawgeti(L, -1, 1); out.x = (float)luaL_checknumber(L, -1); lua_pop(L, 1);
		lua_rawgeti(L, -1, 2); out.y = (float)luaL_checknumber(L, -1); lua_pop(L, 1);
		lua_rawgeti(L, -1, 3); out.z = (float)luaL_checknumber(L, -1); lua_pop(L, 1);
		lua_pop(L, 1);
		return;
	}
	lua_pop(L, 1);

	out = start;
	out.z += opt_float(L, table, "height", default_height);
}

// fx.explosion(x, y, z, opts)
static int l_explosion(lua_State *L)
{
	Vector origin;
	read_origin(L, 1, origin);
	luaL_checktype(L, 4, LUA_TTABLE);

	int index = model_index(require_sprite(L, 4));
	int scale = opt_int(L, 4, "scale", 30);
	int framerate = opt_int(L, 4, "framerate", 20);
	int flags = opt_int(L, 4, "flags", TE_EXPLFLAG_NONE);

	MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, origin, (edict_t *)NULL);
	WRITE_BYTE(TE_EXPLOSION);
	WRITE_COORD(origin.x);
	WRITE_COORD(origin.y);
	WRITE_COORD(origin.z);
	WRITE_SHORT(index);
	WRITE_BYTE(scale);
	WRITE_BYTE(framerate);
	WRITE_BYTE(flags);
	MESSAGE_END();

	return 0;
}

// fx.beam_cylinder(x, y, z, opts) - a ring that expands outward from the
// point up to opts.height, same shape the old plugin used for its shockwave.
static int l_beam_cylinder(lua_State *L)
{
	Vector origin, endpoint;
	read_origin(L, 1, origin);
	luaL_checktype(L, 4, LUA_TTABLE);

	int index = model_index(require_sprite(L, 4));
	opt_endpoint(L, 4, origin, 128.0f, endpoint);

	int framerate = opt_int(L, 4, "framerate", 10);
	int life = (int)(opt_float(L, 4, "life", 1.0f) * 10.0f);
	int width = opt_int(L, 4, "width", 10);
	int amplitude = opt_int(L, 4, "amplitude", 0);
	int r, g, b;
	opt_color(L, 4, r, g, b);
	int brightness = opt_int(L, 4, "brightness", 255);
	int speed = opt_int(L, 4, "speed", 10);

	MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, origin, (edict_t *)NULL);
	WRITE_BYTE(TE_BEAMCYLINDER);
	WRITE_COORD(origin.x);
	WRITE_COORD(origin.y);
	WRITE_COORD(origin.z);
	WRITE_COORD(endpoint.x);
	WRITE_COORD(endpoint.y);
	WRITE_COORD(endpoint.z);
	WRITE_SHORT(index);
	WRITE_BYTE(0);				// start frame
	WRITE_BYTE(framerate);
	WRITE_BYTE(life);
	WRITE_BYTE(width);
	WRITE_BYTE(amplitude);
	WRITE_BYTE(r);
	WRITE_BYTE(g);
	WRITE_BYTE(b);
	WRITE_BYTE(brightness);
	WRITE_BYTE(speed);
	MESSAGE_END();

	return 0;
}

// fx.sprite_trail(x, y, z, opts) - a stream of glowing sprites drifting from
// the point to opts.to (or straight up opts.height units).
static int l_sprite_trail(lua_State *L)
{
	Vector origin, endpoint;
	read_origin(L, 1, origin);
	luaL_checktype(L, 4, LUA_TTABLE);

	int index = model_index(require_sprite(L, 4));
	opt_endpoint(L, 4, origin, 64.0f, endpoint);

	int count = opt_int(L, 4, "count", 20);
	int life = (int)(opt_float(L, 4, "life", 1.0f) * 10.0f);
	int scale = (int)(opt_float(L, 4, "scale", 1.0f) * 10.0f);
	int vel = (int)(opt_float(L, 4, "velocity", 10.0f) * 10.0f);
	int rand = (int)(opt_float(L, 4, "spread", 20.0f) * 10.0f);

	MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, origin, (edict_t *)NULL);
	WRITE_BYTE(TE_SPRITETRAIL);
	WRITE_COORD(origin.x);
	WRITE_COORD(origin.y);
	WRITE_COORD(origin.z);
	WRITE_COORD(endpoint.x);
	WRITE_COORD(endpoint.y);
	WRITE_COORD(endpoint.z);
	WRITE_SHORT(index);
	WRITE_BYTE(count);
	WRITE_BYTE(life);
	WRITE_BYTE(scale);
	WRITE_BYTE(vel);
	WRITE_BYTE(rand);
	MESSAGE_END();

	return 0;
}

// fx.firefield(x, y, z, opts) - a field of N sprites scattered in a square
// around the point, one network message for the whole batch. TE_EXPLOSION
// (fx.explosion) plays a sprite's own animation frames additively and once;
// this is the other shape entirely - alpha-blended by default, an explicit
// duration independent of the sprite's frame count, and count sprites for
// the cost of a single message instead of one message each. What the
// reference AmxModX smoke plugin actually built its cloud from.
static int l_firefield(lua_State *L)
{
	Vector origin;
	read_origin(L, 1, origin);
	luaL_checktype(L, 4, LUA_TTABLE);

	int index = model_index(require_sprite(L, 4));
	int radius = opt_int(L, 4, "radius", 100);
	int count = opt_int(L, 4, "count", 20);
	int duration = (int)(opt_float(L, 4, "duration", 2.0f) * 10.0f);

	int flags = 0;
	lua_getfield(L, 4, "all_float");  if (lua_toboolean(L, -1)) flags |= TEFIRE_FLAG_ALLFLOAT;  lua_pop(L, 1);
	lua_getfield(L, 4, "some_float"); if (lua_toboolean(L, -1)) flags |= TEFIRE_FLAG_SOMEFLOAT; lua_pop(L, 1);
	lua_getfield(L, 4, "loop");       if (lua_toboolean(L, -1)) flags |= TEFIRE_FLAG_LOOP;      lua_pop(L, 1);
	lua_getfield(L, 4, "planar");     if (lua_toboolean(L, -1)) flags |= TEFIRE_FLAG_PLANAR;    lua_pop(L, 1);
	lua_getfield(L, 4, "additive");   if (lua_toboolean(L, -1)) flags |= TEFIRE_FLAG_ADDITIVE;  lua_pop(L, 1);
	// alpha-blended (not additive-glow) is the sane default for smoke/gas -
	// opt in to a solid look with alpha = false rather than the reverse.
	lua_getfield(L, 4, "alpha");
	bool alpha = lua_isnil(L, -1) ? true : lua_toboolean(L, -1);
	lua_pop(L, 1);
	if (alpha) flags |= TEFIRE_FLAG_ALPHA;

	MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, origin, (edict_t *)NULL);
	WRITE_BYTE(TE_FIREFIELD);
	WRITE_COORD(origin.x);
	WRITE_COORD(origin.y);
	WRITE_COORD(origin.z);
	WRITE_SHORT(radius);
	WRITE_SHORT(index);
	WRITE_BYTE(count);
	WRITE_BYTE(flags);
	WRITE_BYTE(duration);
	MESSAGE_END();

	return 0;
}

// fx.beam(x1, y1, z1, x2, y2, z2, opts) - a straight line between two exact
// points, unlike beam_cylinder/sprite_trail which only take one point plus a
// height. For drawing a line between two known things (a shot's path, a
// visibility check) rather than an effect radiating from a single origin.
static int l_beam(lua_State *L)
{
	Vector start, endpoint;
	read_origin(L, 1, start);
	read_origin(L, 4, endpoint);
	luaL_checktype(L, 7, LUA_TTABLE);

	int index = model_index(require_sprite(L, 7));
	int framerate = opt_int(L, 7, "framerate", 0);
	int life = (int)(opt_float(L, 7, "life", 0.3f) * 10.0f);
	int width = opt_int(L, 7, "width", 5);
	int noise = opt_int(L, 7, "noise", 0);
	int r, g, b;
	opt_color(L, 7, r, g, b);
	int brightness = opt_int(L, 7, "brightness", 255);
	int speed = opt_int(L, 7, "speed", 0);

	MESSAGE_BEGIN(MSG_PVS, SVC_TEMPENTITY, start, (edict_t *)NULL);
	WRITE_BYTE(TE_BEAMPOINTS);
	WRITE_COORD(start.x);
	WRITE_COORD(start.y);
	WRITE_COORD(start.z);
	WRITE_COORD(endpoint.x);
	WRITE_COORD(endpoint.y);
	WRITE_COORD(endpoint.z);
	WRITE_SHORT(index);
	WRITE_BYTE(0);				// start frame
	WRITE_BYTE(framerate);
	WRITE_BYTE(life);
	WRITE_BYTE(width);
	WRITE_BYTE(noise);
	WRITE_BYTE(r);
	WRITE_BYTE(g);
	WRITE_BYTE(b);
	WRITE_BYTE(brightness);
	WRITE_BYTE(speed);
	MESSAGE_END();

	return 0;
}

static const luaL_Reg s_fx[] =
{
	{ "explosion", l_explosion },
	{ "beam_cylinder", l_beam_cylinder },
	{ "sprite_trail", l_sprite_trail },
	{ "firefield", l_firefield },
	{ "beam", l_beam },
	{ NULL, NULL }
};

void cslua_register_fx(lua_State *L)
{
	cslua_register_namespace(L, "fx", s_fx);
}
