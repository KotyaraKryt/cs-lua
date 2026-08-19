#include "cslua.h"
#include "lua_regex.h"
#include "lua_natives.h"

#include <map>
#include <regex>
#include <string>
#include <string.h>

// Plugins tend to compile the same handful of patterns (a chat command
// shape, a log line format) and run them against many strings - a chat
// filter checked on every message, say. Caching the compiled std::regex
// keyed by its source text means only the first call per pattern pays for
// compilation. Unbounded on purpose: this assumes patterns are static
// strings plugin authors wrote, not built dynamically per call. A plugin
// that generates truly unique patterns in a loop would grow this
// unboundedly, same trust-the-author tradeoff the rest of this module
// already makes elsewhere (nothing here validates plugin input for taste,
// only for safety).
static std::map<std::string, std::regex> s_cache;

static const std::regex &compile(lua_State *L, const char *pattern)
{
	std::map<std::string, std::regex>::iterator it = s_cache.find(pattern);
	if (it != s_cache.end())
		return it->second;

	try {
		std::regex re(pattern, std::regex::ECMAScript);
		return s_cache.emplace(pattern, std::move(re)).first->second;
	} catch (const std::regex_error &e) {
		luaL_error(L, "regex: invalid pattern '%s': %s", pattern, e.what());
		// unreachable - luaL_error longjmps out - but every path needs a
		// return to satisfy the compiler.
		static const std::regex unreachable;
		return unreachable;
	}
}

// regex.match(str, pattern[, init]) -> capture, ... | whole_match | nil
//
// Same shape as Lua's own string.match: with groups in the pattern, returns
// one value per group (nil for one that did not participate); with none,
// returns the whole match. No match at all is a bare nil, not an error -
// "does this match" is a query, not a mistake.
static int l_match(lua_State *L)
{
	const char *str = luaL_checkstring(L, 1);
	const char *pattern = luaL_checkstring(L, 2);
	size_t len = strlen(str);
	lua_Integer init = luaL_optinteger(L, 3, 1);
	if (init < 1) init = 1;
	if ((size_t)init > len + 1) {
		lua_pushnil(L);
		return 1;
	}

	const std::regex &re = compile(L, pattern);

	std::cmatch m;
	if (!std::regex_search(str + (init - 1), m, re)) {
		lua_pushnil(L);
		return 1;
	}

	if (m.size() > 1) {
		for (size_t i = 1; i < m.size(); i++) {
			if (m[i].matched)
				lua_pushlstring(L, m[i].first, m[i].length());
			else
				lua_pushnil(L);
		}
		return (int)(m.size() - 1);
	}

	lua_pushlstring(L, m[0].first, m[0].length());
	return 1;
}

// regex.find(str, pattern[, init]) -> start, finish, capture, ... | nil
//
// Same shape as Lua's own string.find: 1-based, inclusive start/finish
// (so str:sub(start, finish) reproduces the match), plus one value per
// capture group if the pattern has any.
static int l_find(lua_State *L)
{
	const char *str = luaL_checkstring(L, 1);
	const char *pattern = luaL_checkstring(L, 2);
	size_t len = strlen(str);
	lua_Integer init = luaL_optinteger(L, 3, 1);
	if (init < 1) init = 1;
	if ((size_t)init > len + 1) {
		lua_pushnil(L);
		return 1;
	}

	const std::regex &re = compile(L, pattern);

	std::cmatch m;
	if (!std::regex_search(str + (init - 1), m, re)) {
		lua_pushnil(L);
		return 1;
	}

	lua_Integer start = init + m.position(0);
	lua_Integer finish = start + (lua_Integer)m.length(0) - 1;
	lua_pushinteger(L, start);
	lua_pushinteger(L, finish);

	for (size_t i = 1; i < m.size(); i++) {
		if (m[i].matched)
			lua_pushlstring(L, m[i].first, m[i].length());
		else
			lua_pushnil(L);
	}

	return (int)(2 + (m.size() > 1 ? m.size() - 1 : 0));
}

// regex.replace(str, pattern, repl[, limit]) -> result, count
//
// `repl` may reference capture groups with $1, $2, ... ($& for the whole
// match), the same substitution syntax std::regex already uses. Replaces
// every match by default; `limit` caps how many (leftmost first), same idea
// as Lua's own string.gsub taking an optional count. Named replace rather
// than gsub - this is not a Lua pattern, and "gsub" already means something
// specific to anyone who knows Lua.
static int l_replace(lua_State *L)
{
	const char *str = luaL_checkstring(L, 1);
	const char *pattern = luaL_checkstring(L, 2);
	const char *repl = luaL_checkstring(L, 3);
	lua_Integer limit = luaL_optinteger(L, 4, -1);

	const std::regex &re = compile(L, pattern);

	std::string input(str);
	std::string out;
	out.reserve(input.size());

	std::sregex_iterator it(input.begin(), input.end(), re);
	std::sregex_iterator end;

	size_t last = 0;
	lua_Integer count = 0;

	for (; it != end; ++it) {
		if (limit >= 0 && count >= limit)
			break;

		const std::smatch &m = *it;
		size_t pos = (size_t)m.position(0);

		out.append(input, last, pos - last);
		out += m.format(repl);
		last = pos + (size_t)m.length(0);
		count++;
	}

	out.append(input, last, input.size() - last);

	lua_pushlstring(L, out.data(), out.size());
	lua_pushinteger(L, count);
	return 2;
}

static const luaL_Reg s_regex[] =
{
	{ "match", l_match },
	{ "find", l_find },
	{ "replace", l_replace },
	{ NULL, NULL }
};

void cslua_register_regex(lua_State *L)
{
	cslua_register_namespace(L, "regex", s_regex);
}
