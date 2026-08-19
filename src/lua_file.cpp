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

// A single read/write/append is capped here so a plugin cannot block the
// game thread on a huge file or exhaust memory on a bad `content` string -
// same reasoning as the read/write happening synchronously at all: this is
// meant for small config/export/log-sized data, not bulk transfer.
static const long kMaxFileSize = 4 * 1024 * 1024;

// A file name is a name, not a path - same rule as valid_db_name() in
// lua_db.cpp and the same reason: anything with a separator in it could put
// a file outside the plugin's own directory, which is exactly what
// plugin_data_dir() exists to prevent. Unlike db.open() (which appends ".db"
// itself and so forbids dots outright), this lets the plugin pick its own
// extension - so the rule is stem[.ext], exactly one dot, both halves
// restricted to a safe character set.
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
		if (strchr(name, '.') != dot)		// more than one dot anywhere
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
				return false;		// extension half: letters/digits only
		} else {
			if (!alnum && *p != '_' && *p != '-')
				return false;		// stem half: letters/digits/_/-
		}
	}

	return true;
}

// Resolves name to a full path inside the calling plugin's data directory.
// On failure pushes (nil, reason) and returns false - the caller's native
// function should `return 2` immediately. This is a situation (bad name, no
// plugin context), not a programmer mistake, so nil+reason rather than
// luaL_error - same convention l_sqlite() uses in lua_db.cpp.
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

	// Caps the single write, not the file's total size - the same limit
	// db/store implicitly rely on the OS for, applied here because a plugin
	// handing this a multi-megabyte string in one call is far more likely
	// to be a mistake than a deliberate huge log line.
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

// file.exists(name) -> boolean
//
// A bad name or missing plugin context is "not there" here, not an error -
// existence checks do not fail, they answer no.
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

// file.size(name) -> number | nil
//
// Missing file is a plain nil, no second value - same "absence is not an
// error" stance as file.exists().
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

// file.list() -> { name, ... }
//
// Top-level regular files only - the "logs" subdirectory log.write() creates
// is filtered out so a plugin's own file list never mixes in the logging
// implementation's storage. An unreadable/missing directory is an empty
// list, not an error, matching cslua_list_dir()'s own "absent looks like
// empty" stance.
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

// Where log.write() writes, and under what file name. Plugin-owned (the
// default) is the calling plugin's own logs/, invisible to everyone else -
// the usual case. `global = true` is addons/lua/logs/, the same directory
// the module's own internal logging (cslua_netwatch.cpp's log_line())
// already writes into, for a plugin that specifically wants its record
// sitting alongside the module's own rather than buried in its private data
// dir - a shared moderation log several plugins contribute to, say. Filed
// under the plugin's own id there too, so ten plugins logging global do not
// interleave into one unreadable file, and none of them can collide with
// the module's own unprefixed netwatch.log.
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

// log.write(msg[, opts]) -> boolean
//
// opts.global (boolean, default false) picks the destination directory - see
// log_target_dir() above. opts.level (string, default "info"; one of
// "info"/"warning"/"error"/"debug"/"success") is stamped on the line so a
// human (or grep) can filter without parsing the message text.
//
// Best-effort by design: meant to be sprinkled anywhere without every call
// site checking two return values, the way file.write() has to be. false is
// the rare path (no plugin context, disk full, directory not creatable) -
// most callers will never look at the result. Deliberately does not also go
// to the console: print()/cslua_print already own "the operator should see
// this now"; this is the other case; a durable record without spamming the
// live console. Mirroring it to both would take away the choice.
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
