#include "cslua.h"
#include "lua_msg.h"
#include "lua_natives.h"
#include "lua_message.h"
#include "players.h"

#include <map>
#include <string>
#include <string.h>
#include <vector>

// A single WRITE_STRING inside a user message. There is no hard engine limit
// worth hard-coding here, but sharing a packet with other fields means it
// should stay well under the ~192-byte ceiling lua_message.cpp already uses
// for SayText - this is the same number for the same reason.
#define MSG_MAX_STRING 192

// msg.byte(1) etc build one of these {kind, value} pairs rather than writing
// anything themselves - msg.send is the only place that ever opens a
// message, so this is just a validated, inert description of one field.
static int push_number_field(lua_State *L, FieldKind kind, lua_Number value)
{
	lua_createtable(L, 2, 0);
	lua_pushinteger(L, kind);
	lua_rawseti(L, -2, 1);
	lua_pushnumber(L, value);
	lua_rawseti(L, -2, 2);
	return 1;
}

static int l_field_byte(lua_State *L)
{
	lua_Integer v = luaL_checkinteger(L, 1);
	if (v < 0 || v > 255)
		return luaL_error(L, "msg.byte: %d is out of range 0..255", (int)v);
	return push_number_field(L, MSGF_BYTE, (lua_Number)v);
}

static int l_field_char(lua_State *L)
{
	lua_Integer v = luaL_checkinteger(L, 1);
	if (v < -128 || v > 127)
		return luaL_error(L, "msg.char: %d is out of range -128..127", (int)v);
	return push_number_field(L, MSGF_CHAR, (lua_Number)v);
}

static int l_field_short(lua_State *L)
{
	lua_Integer v = luaL_checkinteger(L, 1);
	if (v < -32768 || v > 32767)
		return luaL_error(L, "msg.short: %d is out of range -32768..32767", (int)v);
	return push_number_field(L, MSGF_SHORT, (lua_Number)v);
}

static int l_field_long(lua_State *L)
{
	lua_Integer v = luaL_checkinteger(L, 1);
	return push_number_field(L, MSGF_LONG, (lua_Number)v);
}

static int l_field_angle(lua_State *L)
{
	return push_number_field(L, MSGF_ANGLE, luaL_checknumber(L, 1));
}

static int l_field_coord(lua_State *L)
{
	return push_number_field(L, MSGF_COORD, luaL_checknumber(L, 1));
}

static int l_field_string(lua_State *L)
{
	size_t len;
	const char *s = luaL_checklstring(L, 1, &len);
	if (len >= MSG_MAX_STRING)
		return luaL_error(L, "msg.string: %d bytes is too long (max %d)", (int)len, MSG_MAX_STRING - 1);

	lua_createtable(L, 2, 0);
	lua_pushinteger(L, MSGF_STRING);
	lua_rawseti(L, -2, 1);
	lua_pushvalue(L, 1);
	lua_rawseti(L, -2, 2);
	return 1;
}

static bool table_int_field(lua_State *L, int index, const char *key, int &out)
{
	lua_getfield(L, index, key);
	bool ok = lua_isnumber(L, -1) != 0;
	if (ok)
		out = (int)lua_tointeger(L, -1);
	lua_pop(L, 1);
	return ok;
}

// A player object carries "id", an ents entity carries "index" - either one
// names a live edict. -1 means neither, or the value named the world (0).
static int resolve_ent_index(lua_State *L, int index)
{
	if (!lua_istable(L, index))
		return -1;

	int v;
	if (table_int_field(L, index, "id", v) && v >= 1)
		return v;
	if (table_int_field(L, index, "index", v) && v >= 1)
		return v;
	return -1;
}

static int l_field_entity(lua_State *L)
{
	int index = resolve_ent_index(L, 1);
	if (index < 0)
		return luaL_error(L, "msg.entity: expected a player or entity object");
	return push_number_field(L, MSGF_ENTITY, (lua_Number)index);
}

// PVS/PAS need a point, not a recipient. Accepts a player, an ents entity
// (both read straight off their edict's origin) or a plain {x, y, z}.
static bool read_origin_arg(lua_State *L, int index, Vector &out)
{
	if (!lua_istable(L, index))
		return false;

	int ent = resolve_ent_index(L, index);
	if (ent >= 1) {
		edict_t *e = g_engfuncs.pfnPEntityOfEntIndex(ent);
		if (!e || e->free)
			return false;
		out = e->v.origin;
		return true;
	}

	lua_rawgeti(L, index, 1);
	lua_rawgeti(L, index, 2);
	lua_rawgeti(L, index, 3);
	bool ok = lua_isnumber(L, -3) && lua_isnumber(L, -2) && lua_isnumber(L, -1);
	if (ok) {
		out.x = (float)lua_tonumber(L, -3);
		out.y = (float)lua_tonumber(L, -2);
		out.z = (float)lua_tonumber(L, -1);
	}
	lua_pop(L, 3);
	return ok;
}

struct Field
{
	FieldKind kind;
	lua_Number num;
	std::string str;
};

// Reads back one {kind, value} table built by msg.byte/.../.entity above.
// Anything else at this argument position - wrong shape, a value swapped for
// the wrong type - fails here, before msg.send has touched the engine.
static bool read_field(lua_State *L, int index, Field &out)
{
	if (!lua_istable(L, index))
		return false;

	lua_rawgeti(L, index, 1);
	if (!lua_isnumber(L, -1)) {
		lua_pop(L, 1);
		return false;
	}
	int kind = (int)lua_tointeger(L, -1);
	lua_pop(L, 1);

	lua_rawgeti(L, index, 2);
	bool ok;
	switch (kind) {
	case MSGF_STRING:
		ok = lua_isstring(L, -1) != 0;
		if (ok)
			out.str = lua_tostring(L, -1);
		break;
	case MSGF_BYTE:
	case MSGF_CHAR:
	case MSGF_SHORT:
	case MSGF_LONG:
	case MSGF_ANGLE:
	case MSGF_COORD:
	case MSGF_ENTITY:
		ok = lua_isnumber(L, -1) != 0;
		if (ok)
			out.num = lua_tonumber(L, -1);
		break;
	default:
		ok = false;
		break;
	}
	lua_pop(L, 1);

	if (ok)
		out.kind = (FieldKind)kind;
	return ok;
}

// Message ids are assigned once by the engine and never change for the rest
// of the process - REG_USER_MSG is idempotent, so this just avoids paying
// the lookup again on every send. Same name for something the game DLL
// already registered (e.g. "TextMsg") returns its existing id; a name
// nothing has claimed yet registers a brand new message for a custom client.
static int resolve_message_id(const char *name)
{
	static std::map<std::string, int> s_cache;

	auto it = s_cache.find(name);
	if (it != s_cache.end())
		return it->second;

	int id = REG_USER_MSG(name, -1);
	if (id > 0)
		s_cache[name] = id;
	return id;
}

// msg.send(dest, name, target, ...fields)
//
//   dest   "one" | "one_unreliable" | "all" | "pvs" | "pas"
//   name   message name; resolved through the engine, existing or new
//   target player for one/one_unreliable; player, entity or {x,y,z} for
//          pvs/pas; ignored (pass nil) for all
//   fields msg.byte/char/short/long/angle/coord/string/entity(...) values,
//          in wire order
//
// Everything is read and validated up front. MESSAGE_BEGIN only runs once
// there is nothing left that could fail - see lua_msg.h for why.
static int l_msg_send(lua_State *L)
{
	const char *dest_name = luaL_checkstring(L, 1);
	const char *name = luaL_checkstring(L, 2);

	int dest;
	bool need_edict = false, need_origin = false;

	if (!strcmp(dest_name, "one")) { dest = MSG_ONE; need_edict = true; }
	else if (!strcmp(dest_name, "one_unreliable")) { dest = MSG_ONE_UNRELIABLE; need_edict = true; }
	else if (!strcmp(dest_name, "all")) { dest = MSG_ALL; }
	else if (!strcmp(dest_name, "pvs")) { dest = MSG_PVS; need_origin = true; }
	else if (!strcmp(dest_name, "pas")) { dest = MSG_PAS; need_origin = true; }
	else
		return luaL_error(L, "msg.send: unknown destination '%s' (want one, one_unreliable, all, pvs or pas)", dest_name);

	edict_t *target_edict = NULL;
	Vector origin;

	if (need_edict) {
		int index = resolve_ent_index(L, 3);
		if (index < 1 || index >= CSLUA_MAXPLAYERS || !g_players.is_connected(index))
			return luaL_error(L, "msg.send: '%s' needs a connected player as the target", dest_name);
		target_edict = g_engfuncs.pfnPEntityOfEntIndex(index);
		if (!target_edict || target_edict->free)
			return luaL_error(L, "msg.send: target player has no valid entity");
	} else if (need_origin) {
		if (!read_origin_arg(L, 3, origin))
			return luaL_error(L, "msg.send: '%s' needs a player, entity or {x, y, z} target for the origin", dest_name);
	}

	int msg_id = resolve_message_id(name);
	if (msg_id <= 0)
		return luaL_error(L, "msg.send: engine refused to register message '%s'", name);

	int top = lua_gettop(L);
	std::vector<Field> fields;
	fields.reserve(top > 3 ? top - 3 : 0);
	for (int i = 4; i <= top; i++) {
		Field f;
		if (!read_field(L, i, f))
			return luaL_error(L, "msg.send: argument #%d is not a msg.byte/char/short/long/angle/coord/string/entity value", i);
		fields.push_back(f);
	}

	if (need_edict)
		MESSAGE_BEGIN(dest, msg_id, NULL, target_edict);
	else if (need_origin)
		MESSAGE_BEGIN(dest, msg_id, origin, (edict_t *)NULL);
	else
		MESSAGE_BEGIN(dest, msg_id, NULL, (edict_t *)NULL);

	for (const Field &f : fields) {
		switch (f.kind) {
		case MSGF_BYTE:   WRITE_BYTE((int)f.num); break;
		case MSGF_CHAR:   WRITE_CHAR((int)f.num); break;
		case MSGF_SHORT:  WRITE_SHORT((int)f.num); break;
		case MSGF_LONG:   WRITE_LONG((int)f.num); break;
		case MSGF_ANGLE:  WRITE_ANGLE((float)f.num); break;
		case MSGF_COORD:  WRITE_COORD((float)f.num); break;
		case MSGF_ENTITY: WRITE_ENTITY((int)f.num); break;
		case MSGF_STRING: {
			std::string encoded = cslua_text_for_client(f.str.c_str());
			WRITE_STRING(encoded.c_str());
			break;
		}
		}
	}

	MESSAGE_END();

	lua_pushboolean(L, 1);
	return 1;
}

static const luaL_Reg s_msg[] =
{
	{ "send", l_msg_send },
	{ "byte", l_field_byte },
	{ "char", l_field_char },
	{ "short", l_field_short },
	{ "long", l_field_long },
	{ "angle", l_field_angle },
	{ "coord", l_field_coord },
	{ "string", l_field_string },
	{ "entity", l_field_entity },
	{ NULL, NULL }
};

void cslua_register_msg(lua_State *L)
{
	cslua_register_namespace(L, "msg", s_msg);
}
