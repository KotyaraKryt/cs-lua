#include "cslua.h"

#include "lua_vector.h"
#include "lua_natives.h"

static Vector arg_vector(lua_State *L, int idx)
{
	return Vector(
		(float)luaL_checknumber(L, idx),
		(float)luaL_checknumber(L, idx + 1),
		(float)luaL_checknumber(L, idx + 2));
}

static int push_vector(lua_State *L, const Vector &v)
{
	lua_pushnumber(L, v.x);
	lua_pushnumber(L, v.y);
	lua_pushnumber(L, v.z);
	return 3;
}

// vec.distance(x1,y1,z1, x2,y2,z2) -> number
static int l_distance(lua_State *L)
{
	Vector a = arg_vector(L, 1);
	Vector b = arg_vector(L, 4);
	lua_pushnumber(L, (b - a).Length());
	return 1;
}

// vec.length(x,y,z) -> number
static int l_length(lua_State *L)
{
	lua_pushnumber(L, arg_vector(L, 1).Length());
	return 1;
}

// vec.normalize(x,y,z) -> unit vector. The zero vector normalizes to itself.
static int l_normalize(lua_State *L)
{
	Vector v = arg_vector(L, 1);
	if (v.LengthSquared() > 0.0f)
		v = v.Normalize();
	return push_vector(L, v);
}

// vec.to_angle(x,y,z) -> pitch, yaw, roll (pfnVecToAngles).
static int l_to_angle(lua_State *L)
{
	Vector v = arg_vector(L, 1);
	Vector angles;
	g_engfuncs.pfnVecToAngles(v, angles);
	return push_vector(L, angles);
}

// vec.angle_vector(pitch,yaw,roll) -> unit forward vector.
static int l_angle_vector(lua_State *L)
{
	Vector angles = arg_vector(L, 1);
	Vector forward, right, up;
	g_engfuncs.pfnAngleVectors(angles, forward, right, up);
	return push_vector(L, forward);
}

static const luaL_Reg s_vec[] =
{
	{ "distance",     l_distance },
	{ "length",       l_length },
	{ "normalize",    l_normalize },
	{ "to_angle",     l_to_angle },
	{ "angle_vector", l_angle_vector },
	{ NULL, NULL }
};

void cslua_register_vector(lua_State *L)
{
	cslua_register_namespace(L, "vec", s_vec);
}
