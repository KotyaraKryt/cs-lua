#include "cslua.h"
#include "lua_file.h"
#include "lua_engine.h"
#include "lua_natives.h"
#include "platform.h"

#include <string>
#include <vector>

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <ctime>

// One read/write/append is capped so a plugin cannot block the game thread on
// a huge file - this is for small config/export/log-sized data.
static const long kMaxFileSize = 4 * 1024 * 1024;

// A file name is a name, not a path: stem[.ext], exactly one dot, both halves
// restricted. Same reason as valid_db_name() in lua_db.cpp.
static bool valid_file_name(const char *name)
{
	if (!name || !*name || strlen(name) > 64)
		return false;

	if (!strcmp(name, ".") || !strcmp(name, ".."))
		return false;

	const char *dot = strrchr(name, '.');
	if (dot) {
		if (dot == name)			// leading dot: empty stem
			return false;
		if (strchr(name, '.') != dot)		// more than one dot
			return false;
		if (!*(dot + 1))			// trailing dot: empty extension
			return false;
	}

	for (const char *p = name; *p; p++) {
		if (p == dot)
			continue;

		bool alnum = (*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z')
			|| (*p >= '0' && *p <= '9');

		if (dot && p > dot) {
			if (!alnum)
				return false;		// extension: letters/digits only
		} else {
			if (!alnum && *p != '_' && *p != '-')
				return false;		// stem: letters/digits/_/-
		}
	}

	return true;
}

// Resolves name to a path inside the calling plugin's data directory. On
// failure pushes (nil, reason) and returns false.
static bool file_path(lua_State *L, const char *name, std::string &out)
{
	if (!valid_file_name(name)) {
		lua_pushnil(L);
		lua_pushfstring(L, "'%s' is not a usable file name: letters, digits, "
			"'_', '-', and at most one '.' extension, no path separators", name);
		return false;
	}

	std::string dir = cslua_plugin_data_dir(g_lua.current_index());
	if (dir.empty()) {
		lua_pushnil(L);
		lua_pushstring(L, "no data directory (file functions have to be called from a plugin)");
		return false;
	}

	out = dir + "/" + name;
	return true;
}

// file.read(name) -> content:string | nil, reason:string
static int l_file_read(lua_State *L)
{
	const char *name = luaL_checkstring(L, 1);

	std::string path;
	if (!file_path(L, name, path))
		return 2;

	FILE *fh = fopen(path.c_str(), "rb");
	if (!fh) {
		lua_pushnil(L);
		lua_pushfstring(L, "cannot open '%s': %s", name, strerror(errno));
		return 2;
	}

	fseek(fh, 0, SEEK_END);
	long size = ftell(fh);
	fseek(fh, 0, SEEK_SET);

	if (size < 0 || size > kMaxFileSize) {
		fclose(fh);
		lua_pushnil(L);
		lua_pushfstring(L, "'%s' is larger than the %d MiB limit", name, (int)(kMaxFileSize / (1024 * 1024)));
		return 2;
	}

	std::vector<char> buf((size_t)size);
	size_t got = size > 0 ? fread(&buf[0], 1, (size_t)size, fh) : 0;
	fclose(fh);

	lua_pushlstring(L, size > 0 ? &buf[0] : "", got);
	return 1;
}

// file.write(name, content) -> true | nil, reason:string
static int l_file_write(lua_State *L)
{
	const char *name = luaL_checkstring(L, 1);
	size_t len;
	const char *content = luaL_checklstring(L, 2, &len);

	if (len > (size_t)kMaxFileSize) {
		lua_pushnil(L);
		lua_pushfstring(L, "content is larger than the %d MiB limit", (int)(kMaxFileSize / (1024 * 1024)));
		return 2;
	}

	std::string path;
	if (!file_path(L, name, path))
		return 2;

	FILE *fh = fopen(path.c_str(), "wb");
	if (!fh) {
		lua_pushnil(L);
		lua_pushfstring(L, "cannot open '%s' for writing: %s", name, strerror(errno));
		return 2;
	}

	fwrite(content, 1, len, fh);
	fclose(fh);

	lua_pushboolean(L, 1);
	return 1;
}

// file.append(name, content) -> true | nil, reason:string
static int l_file_append(lua_State *L)
{
	const char *name = luaL_checkstring(L, 1);
	size_t len;
	const char *content = luaL_checklstring(L, 2, &len);

	if (len > (size_t)kMaxFileSize) {
		lua_pushnil(L);
		lua_pushfstring(L, "content is larger than the %d MiB limit", (int)(kMaxFileSize / (1024 * 1024)));
		return 2;
	}

	std::string path;
	if (!file_path(L, name, path))
		return 2;

	FILE *fh = fopen(path.c_str(), "ab");
	if (!fh) {
		lua_pushnil(L);
		lua_pushfstring(L, "cannot open '%s' for writing: %s", name, strerror(errno));
		return 2;
	}

	fwrite(content, 1, len, fh);
	fclose(fh);

	lua_pushboolean(L, 1);
	return 1;
}

// file.exists(name) -> boolean. A bad name or missing plugin context is "no".
static int l_file_exists(lua_State *L)
{
	const char *name = luaL_checkstring(L, 1);

	if (!valid_file_name(name)) {
		lua_pushboolean(L, 0);
		return 1;
	}

	std::string dir = cslua_plugin_data_dir(g_lua.current_index());
	if (dir.empty()) {
		lua_pushboolean(L, 0);
		return 1;
	}

	lua_pushboolean(L, cslua_file_exists(dir + "/" + name));
	return 1;
}

// file.remove(name) -> true | nil, reason:string
static int l_file_remove(lua_State *L)
{
	const char *name = luaL_checkstring(L, 1);

	std::string path;
	if (!file_path(L, name, path))
		return 2;

	if (remove(path.c_str()) != 0) {
		lua_pushnil(L);
		lua_pushfstring(L, "cannot remove '%s': %s", name, strerror(errno));
		return 2;
	}

	lua_pushboolean(L, 1);
	return 1;
}

// file.size(name) -> number | nil. Missing file is a plain nil.
static int l_file_size(lua_State *L)
{
	const char *name = luaL_checkstring(L, 1);

	if (!valid_file_name(name)) {
		lua_pushnil(L);
		return 1;
	}

	std::string dir = cslua_plugin_data_dir(g_lua.current_index());
	if (dir.empty()) {
		lua_pushnil(L);
		return 1;
	}

	std::string path = dir + "/" + name;
	if (!cslua_file_exists(path)) {
		lua_pushnil(L);
		return 1;
	}

	FILE *fh = fopen(path.c_str(), "rb");
	if (!fh) {
		lua_pushnil(L);
		return 1;
	}
	fseek(fh, 0, SEEK_END);
	long size = ftell(fh);
	fclose(fh);

	lua_pushnumber(L, (lua_Number)size);
	return 1;
}

// file.list() -> { name, ... }. Top-level regular files only; the "logs"
// subdirectory is filtered out.
static int l_file_list(lua_State *L)
{
	std::string dir = cslua_plugin_data_dir(g_lua.current_index());

	lua_newtable(L);
	if (dir.empty())
		return 1;

	std::vector<CsLuaDirEntry> entries;
	cslua_list_dir(dir, entries);

	int n = 0;
	for (size_t i = 0; i < entries.size(); i++) {
		if (entries[i].is_dir)
			continue;
		lua_pushstring(L, entries[i].name.c_str());
		lua_rawseti(L, -2, ++n);
	}

	return 1;
}

static const luaL_Reg s_file[] =
{
	{ "read", l_file_read },
	{ "write", l_file_write },
	{ "append", l_file_append },
	{ "exists", l_file_exists },
	{ "remove", l_file_remove },
	{ "size", l_file_size },
	{ "list", l_file_list },
	{ NULL, NULL }
};

void cslua_register_file(lua_State *L)
{
	cslua_register_namespace(L, "file", s_file);
}

// Where log.write() writes. Plugin-owned (default) is the calling plugin's own
// logs/; `global = true` is addons/lua/logs/, filed under the plugin's id.
static bool log_target_dir(bool global, std::string &out, std::string &prefix)
{
	if (!global) {
		std::string dir = cslua_plugin_data_dir(g_lua.current_index());
		if (dir.empty())
			return false;
		out = dir + "/logs";
		prefix = "";
		return true;
	}

	const std::vector<LuaPlugin> &plugins = g_lua.plugins();
	int index = g_lua.current_index();
	if (index < 0 || index >= (int)plugins.size())
		return false;
	out = cslua_base_dir() + "/logs";
	prefix = plugins[index].id + "-";
	return true;
}

enum LogLevel { CSLOG_INFO, CSLOG_WARNING, CSLOG_ERROR, CSLOG_DEBUG, CSLOG_SUCCESS };

static bool log_level_from_string(const char *s, LogLevel &out)
{
	if (!strcmp(s, "info"))    { out = CSLOG_INFO;    return true; }
	if (!strcmp(s, "warning")) { out = CSLOG_WARNING; return true; }
	if (!strcmp(s, "error"))   { out = CSLOG_ERROR;   return true; }
	if (!strcmp(s, "debug"))   { out = CSLOG_DEBUG;   return true; }
	if (!strcmp(s, "success")) { out = CSLOG_SUCCESS; return true; }
	return false;
}

static const char *log_level_tag(LogLevel level)
{
	switch (level) {
	case CSLOG_WARNING: return "WARNING";
	case CSLOG_ERROR:   return "ERROR";
	case CSLOG_DEBUG:   return "DEBUG";
	case CSLOG_SUCCESS: return "SUCCESS";
	default:          return "INFO";
	}
}

// log.write(msg[, opts]) -> boolean. Best-effort; does not also go to the
// console. opts.global picks the directory, opts.level is stamped on the line.
static int l_log_write(lua_State *L)
{
	const char *msg = luaL_checkstring(L, 1);

	bool global = false;
	LogLevel level = CSLOG_INFO;

	if (!lua_isnoneornil(L, 2)) {
		luaL_checktype(L, 2, LUA_TTABLE);

		lua_getfield(L, 2, "global");
		if (!lua_isnil(L, -1))
			global = lua_toboolean(L, -1) != 0;
		lua_pop(L, 1);

		lua_getfield(L, 2, "level");
		if (!lua_isnil(L, -1)) {
			const char *level_str = luaL_checkstring(L, -1);
			if (!log_level_from_string(level_str, level)) {
				lua_pop(L, 1);
				return luaL_error(L, "log.write: level must be one of "
					"\"info\", \"warning\", \"error\", \"debug\", \"success\", got '%s'", level_str);
			}
		}
		lua_pop(L, 1);
	}

	std::string logs_dir, prefix;
	if (!log_target_dir(global, logs_dir, prefix)) {
		lua_pushboolean(L, 0);
		return 1;
	}

	if (!cslua_make_dir(logs_dir)) {
		lua_pushboolean(L, 0);
		return 1;
	}

	time_t now = time(NULL);
	struct tm *lt = localtime(&now);

	char datebuf[16];
	strftime(datebuf, sizeof datebuf, "%Y-%m-%d", lt);

	FILE *fh = fopen((logs_dir + "/" + prefix + datebuf + ".log").c_str(), "a");
	if (!fh) {
		lua_pushboolean(L, 0);
		return 1;
	}

	char stamp[32];
	strftime(stamp, sizeof stamp, "%Y-%m-%d %H:%M:%S", lt);
	fprintf(fh, "[%s] [%s] %s\n", stamp, log_level_tag(level), msg);
	fclose(fh);

	lua_pushboolean(L, 1);
	return 1;
}

static const luaL_Reg s_log[] =
{
	{ "write", l_log_write },
	{ NULL, NULL }
};

void cslua_register_log(lua_State *L)
{
	cslua_register_namespace(L, "log", s_log);
}
