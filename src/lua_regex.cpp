#include "cslua.h"
#include "lua_regex.h"
#include "lua_natives.h"

#include <chrono>
#include <condition_variable>
#include <map>
#include <memory>
#include <mutex>
#include <regex>
#include <string>
#include <thread>
#include <vector>

#include <string.h>

// Compiled std::regex cache keyed by source text; only the first call per
// pattern pays for compilation. Unbounded on purpose - assumes static patterns.
// std::map never invalidates references to existing elements on insert, and
// std::regex_search never mutates the regex, so the watchdog thread reading a
// cached entry while the game thread inserts another is safe.
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
		// unreachable - luaL_error longjmps out
		static const std::regex unreachable;
		return unreachable;
	}
}

// Watchdog
//
// std::regex has no abort hook; a pattern like "(a+)+$" can backtrack
// catastrophically and hang the game thread. The match runs on its own thread
// and the game thread gives up after cslua_regex_timeout_ms. The thread is
// detached, not killed; its result is dropped when nobody is waiting.

static char s_timeout_name[] = "cslua_regex_timeout_ms";
static char s_timeout_value[] = "100";
static cvar_t s_cvar_timeout = { s_timeout_name, s_timeout_value, 0, 100.0f, NULL };
static bool s_cvar_registered = false;

static void ensure_cvar_registered()
{
	if (s_cvar_registered)
		return;
	s_cvar_registered = true;

	if (!CVAR_GET_POINTER(s_cvar_timeout.name))
		CVAR_REGISTER(&s_cvar_timeout);
}

static int regex_timeout_ms()
{
	int ms = (int)CVAR_GET_FLOAT(s_cvar_timeout.name);
	return ms > 0 ? ms : 100;
}

// Base for every per-call job. Holds nothing Lua-facing and nothing pointing
// into a calling frame's stack: on timeout that frame is gone while the
// detached thread may still write here. shared_ptr keeps it alive for whichever
// side lets go last.
struct RegexWait
{
	std::mutex mtx;
	std::condition_variable cv;
	bool done = false;
};

// Runs `body` on its own thread, waits up to timeout_ms. Returns false on
// timeout; `body` keeps running and its later writes are never looked at.
template <typename J, typename F>
static bool run_with_deadline(const std::shared_ptr<J> &job, F body, int timeout_ms)
{
	std::thread([job, body]() {
		body();
		std::lock_guard<std::mutex> guard(job->mtx);
		job->done = true;
		job->cv.notify_one();
	}).detach();

	std::unique_lock<std::mutex> lock(job->mtx);
	return job->cv.wait_for(lock, std::chrono::milliseconds(timeout_ms),
		[&] { return job->done; });
}

// regex.match(str, pattern[, init]) -> capture, ... | whole_match | nil
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

	struct Result : RegexWait
	{
		bool matched = false;
		bool has_groups = false;
		std::string whole;
		std::vector<bool> present;
		std::vector<std::string> captures;
	};
	auto job = std::make_shared<Result>();
	std::string subject(str + (init - 1));

	int timeout_ms = regex_timeout_ms();
	bool finished = run_with_deadline(job, [job, subject, &re]() {
		std::smatch m;
		if (!std::regex_search(subject, m, re))
			return;
		job->matched = true;
		if (m.size() > 1) {
			job->has_groups = true;
			for (size_t i = 1; i < m.size(); i++) {
				job->present.push_back(m[i].matched);
				job->captures.push_back(m[i].matched ? m[i].str() : std::string());
			}
		} else {
			job->whole = m[0].str();
		}
	}, timeout_ms);

	if (!finished)
		return luaL_error(L, "regex: pattern took too long to match (>%dms), aborted", timeout_ms);

	if (!job->matched) {
		lua_pushnil(L);
		return 1;
	}

	if (job->has_groups) {
		for (size_t i = 0; i < job->captures.size(); i++) {
			if (job->present[i])
				lua_pushlstring(L, job->captures[i].data(), job->captures[i].size());
			else
				lua_pushnil(L);
		}
		return (int)job->captures.size();
	}

	lua_pushlstring(L, job->whole.data(), job->whole.size());
	return 1;
}

// regex.find(str, pattern[, init]) -> start, finish, capture, ... | nil
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

	struct Result : RegexWait
	{
		bool matched = false;
		long long position = 0;
		long long length = 0;
		std::vector<bool> present;
		std::vector<std::string> captures;
	};
	auto job = std::make_shared<Result>();
	std::string subject(str + (init - 1));

	int timeout_ms = regex_timeout_ms();
	bool finished = run_with_deadline(job, [job, subject, &re]() {
		std::smatch m;
		if (!std::regex_search(subject, m, re))
			return;
		job->matched = true;
		job->position = m.position(0);
		job->length = m.length(0);
		for (size_t i = 1; i < m.size(); i++) {
			job->present.push_back(m[i].matched);
			job->captures.push_back(m[i].matched ? m[i].str() : std::string());
		}
	}, timeout_ms);

	if (!finished)
		return luaL_error(L, "regex: pattern took too long to match (>%dms), aborted", timeout_ms);

	if (!job->matched) {
		lua_pushnil(L);
		return 1;
	}

	lua_Integer start = init + (lua_Integer)job->position;
	lua_Integer finish = start + (lua_Integer)job->length - 1;
	lua_pushinteger(L, start);
	lua_pushinteger(L, finish);

	for (size_t i = 0; i < job->captures.size(); i++) {
		if (job->present[i])
			lua_pushlstring(L, job->captures[i].data(), job->captures[i].size());
		else
			lua_pushnil(L);
	}

	return (int)(2 + job->captures.size());
}

// regex.replace(str, pattern, repl[, limit]) -> result, count
//
// `repl` may reference capture groups with $1, $2, ... ($& for the whole
// match). Replaces every match by default; `limit` caps how many.
static int l_replace(lua_State *L)
{
	const char *str = luaL_checkstring(L, 1);
	const char *pattern = luaL_checkstring(L, 2);
	const char *repl = luaL_checkstring(L, 3);
	lua_Integer limit = luaL_optinteger(L, 4, -1);

	const std::regex &re = compile(L, pattern);

	struct Result : RegexWait
	{
		std::string out;
		lua_Integer count = 0;
	};
	auto job = std::make_shared<Result>();
	std::string input(str);
	std::string repl_copy(repl);

	int timeout_ms = regex_timeout_ms();
	bool finished = run_with_deadline(job, [job, input, repl_copy, limit, &re]() {
		job->out.reserve(input.size());

		std::sregex_iterator it(input.begin(), input.end(), re);
		std::sregex_iterator end;

		size_t last = 0;
		for (; it != end; ++it) {
			if (limit >= 0 && job->count >= limit)
				break;

			const std::smatch &m = *it;
			size_t pos = (size_t)m.position(0);

			job->out.append(input, last, pos - last);
			job->out += m.format(repl_copy);
			last = pos + (size_t)m.length(0);
			job->count++;
		}

		job->out.append(input, last, input.size() - last);
	}, timeout_ms);

	if (!finished)
		return luaL_error(L, "regex: pattern took too long to match (>%dms), aborted", timeout_ms);

	lua_pushlstring(L, job->out.data(), job->out.size());
	lua_pushinteger(L, job->count);
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
	ensure_cvar_registered();
	cslua_register_namespace(L, "regex", s_regex);
}
