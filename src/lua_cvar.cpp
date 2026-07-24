#include "cslua.h"
#include "lua_cvar.h"

#include <string.h>
#include <string>
#include <vector>

// A cvar registered from Lua. The engine keeps a pointer to this cvar_t and to
// its name/string buffers forever, so they must never move or be freed while
// the server runs. We hand out stable storage and never release it.
struct OwnedCvar
{
	std::string name;
	std::string default_string;
	cvar_t var;
};

// Pointers are handed to the engine, so the list must never reallocate its
// elements out from under it - hence a deque-like stable container. A vector of
// pointers gives the same guarantee and is simplest here.
static std::vector<OwnedCvar *> s_owned;

static OwnedCvar *find_owned(const char *name)
{
	for (size_t i = 0; i < s_owned.size(); i++)
		if (s_owned[i]->name == name)
			return s_owned[i];
	return nullptr;
}

// A cvar object is just { name = "..." }; every access resolves the live
// cvar_t by name, exactly like a player object resolves its edict. That keeps
// the value current and never dangles.
static const char *cvar_object_name(lua_State *L)
{
	luaL_checktype(L, 1, LUA_TTABLE);
	lua_getfield(L, 1, "name");
	const char *name = lua_tostring(L, -1);
	if (!name)
		luaL_error(L, "expected a cvar object, use c:method() not c.method()");
	lua_pop(L, 1);
	return name;
}

static int l_cvar_int(lua_State *L)
{
	lua_pushinteger(L, (int)CVAR_GET_FLOAT(cvar_object_name(L)));
	return 1;
}

static int l_cvar_float(lua_State *L)
{
	lua_pushnumber(L, CVAR_GET_FLOAT(cvar_object_name(L)));
	return 1;
}

static int l_cvar_str(lua_State *L)
{
	const char *s = CVAR_GET_STRING(cvar_object_name(L));
	lua_pushstring(L, s ? s : "");
	return 1;
}

static int l_cvar_set(lua_State *L)
{
	const char *name = cvar_object_name(L);

	// Set from a number or a string, whichever the script passed.
	if (lua_isnumber(L, 2))
		CVAR_SET_FLOAT(name, (float)lua_tonumber(L, 2));
	else
		CVAR_SET_STRING(name, luaL_checkstring(L, 2));
	return 0;
}

static int l_cvar_name(lua_State *L)
{
	lua_pushstring(L, cvar_object_name(L));
	return 1;
}

static const luaL_Reg s_cvar_methods[] =
{
	{ "int",   l_cvar_int },
	{ "float", l_cvar_float },
	{ "str",   l_cvar_str },
	{ "set",   l_cvar_set },
	{ "name",  l_cvar_name },
	{ NULL, NULL }
};

// The shared metatable, kept in the registry so every object reuses it.
static int s_cvar_mt_ref = LUA_NOREF;

// Builds and returns a fresh cvar object bound to `name`.
static void push_cvar_object(lua_State *L, const char *name)
{
	lua_newtable(L);
	lua_pushstring(L, name);
	lua_setfield(L, -2, "name");

	lua_rawgeti(L, LUA_REGISTRYINDEX, s_cvar_mt_ref);
	lua_setmetatable(L, -2);
}

// cvar("mp_freezetime") -> object, or nil if no such cvar exists.
static int l_cvar(lua_State *L)
{
	const char *name = luaL_checkstring(L, 1);

	if (!CVAR_GET_POINTER(name)) {
		lua_pushnil(L);
		return 1;
	}

	push_cvar_object(L, name);
	return 1;
}

// cvar_register("greet_prefix", "[Server]", { archive = true }) -> object.
// Calling it again for an existing name just returns the object, so a plugin
// can register the same cvar on every load without stacking duplicates.
static int l_cvar_register(lua_State *L)
{
	const char *name = luaL_checkstring(L, 1);
	const char *value = luaL_optstring(L, 2, "");

	if (find_owned(name) || CVAR_GET_POINTER(name)) {
		// Already exists (ours or someone else's): don't register twice.
		push_cvar_object(L, name);
		return 1;
	}

	int flags = 0;
	if (lua_istable(L, 3)) {
		lua_getfield(L, 3, "archive");
		if (lua_toboolean(L, -1)) flags |= FCVAR_ARCHIVE;
		lua_pop(L, 1);
		lua_getfield(L, 3, "protected");
		if (lua_toboolean(L, -1)) flags |= FCVAR_PROTECTED;
		lua_pop(L, 1);
		lua_getfield(L, 3, "server");
		if (lua_toboolean(L, -1)) flags |= FCVAR_SERVER;
		lua_pop(L, 1);
	}

	OwnedCvar *owned = new OwnedCvar();
	owned->name = name;
	owned->default_string = value;
	// cvar_t.string is a char*, and the engine may write to it; give it its own
	// buffer that lives as long as the cvar does.
	owned->var.name = owned->name.c_str();
	owned->var.string = const_cast<char *>(owned->default_string.c_str());
	owned->var.flags = flags;
	owned->var.value = (float)atof(value);
	owned->var.next = nullptr;

	s_owned.push_back(owned);
	CVAR_REGISTER(&owned->var);

	push_cvar_object(L, name);
	return 1;
}

void cslua_register_cvar(lua_State *L)
{
	// One metatable for every cvar object: __index holds the methods.
	lua_newtable(L);
	lua_newtable(L);
	for (const luaL_Reg *r = s_cvar_methods; r->name; r++) {
		lua_pushcfunction(L, r->func);
		lua_setfield(L, -2, r->name);
	}
	lua_setfield(L, -2, "__index");
	lua_pushboolean(L, 0);
	lua_setfield(L, -2, "__metatable");
	s_cvar_mt_ref = luaL_ref(L, LUA_REGISTRYINDEX);

	lua_pushcfunction(L, l_cvar);
	lua_setglobal(L, "cvar");

	lua_pushcfunction(L, l_cvar_register);
	lua_setglobal(L, "cvar_register");
}

void cslua_cvar_shutdown()
{
	// The metatable ref dies with the state; forget it so the next init makes
	// a fresh one. s_owned is deliberately kept: the engine still points at
	// those cvar_t's, freeing them would dangle. They live until the process
	// exits, which for a game server is fine.
	s_cvar_mt_ref = LUA_NOREF;
}
