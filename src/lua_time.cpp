#include "cslua.h"
#include "lua_time.h"
#include "lua_natives.h"

#include <stdio.h>
#include <time.h>

struct TimeUnit
{
	char suffix;
	long long seconds;
};

// Order matters for l_format (largest to smallest). 'y' here is a fixed
// 365-day approximation, duration-only - add_calendar has its own calendar-
// aware year/month handling below.
static const TimeUnit s_units[] =
{
	{ 'y', 31536000 },
	{ 'w', 604800 },
	{ 'd', 86400 },
	{ 'h', 3600 },
	{ 'm', 60 },
	{ 's', 1 },
};

static long long unit_seconds(char suffix)
{
	for (size_t i = 0; i < sizeof(s_units) / sizeof(s_units[0]); i++)
		if (s_units[i].suffix == suffix)
			return s_units[i].seconds;
	return -1;
}

// time.parse("60s60m24h32d") -> 2854860
//
// Digits accumulate until a unit letter is hit, then get multiplied and
// added to the total - so "1d" and "24h" both parse, and so does mixing
// them in one string. A malformed string is a config typo, not a runtime
// condition, so it throws like regex's invalid-pattern case does.
static int l_parse(lua_State *L)
{
	size_t len = 0;
	const char *str = luaL_checklstring(L, 1, &len);

	long long total = 0;
	long long number = 0;
	bool has_digits = false;

	for (size_t i = 0; i < len; i++) {
		char c = str[i];

		if (c >= '0' && c <= '9') {
			number = number * 10 + (c - '0');
			has_digits = true;
			continue;
		}

		if (!has_digits)
			return luaL_error(L, "time.parse: '%c' in \"%s\" has no number before it", c, str);

		long long secs = unit_seconds(c);
		if (secs < 0)
			return luaL_error(L, "time.parse: unknown unit '%c' in \"%s\" (expected s/m/h/d/w/y)", c, str);

		total += number * secs;
		number = 0;
		has_digits = false;
	}

	if (has_digits || len == 0)
		return luaL_error(L, "time.parse: \"%s\" is not a valid duration", str);

	lua_pushinteger(L, (lua_Integer)total);
	return 1;
}

// time.format(2854860) -> "32d24h60m60s"
static int l_format(lua_State *L)
{
	lua_Integer seconds = luaL_checkinteger(L, 1);
	if (seconds < 0)
		seconds = 0;

	luaL_Buffer b;
	luaL_buffinit(L, &b);

	bool wrote = false;
	for (size_t i = 0; i < sizeof(s_units) / sizeof(s_units[0]); i++) {
		const TimeUnit &u = s_units[i];
		if (u.suffix == 'y')
			continue; // "1y" vs "365d" is ambiguous on the way back out

		lua_Integer value = seconds / u.seconds;
		if (value <= 0)
			continue;

		char chunk[32];
		snprintf(chunk, sizeof(chunk), "%lld%c", (long long)value, u.suffix);
		luaL_addstring(&b, chunk);

		seconds %= u.seconds;
		wrote = true;
	}

	if (!wrote)
		luaL_addstring(&b, "0s");

	luaL_pushresult(&b);
	return 1;
}

// time.add_calendar(unixtime, "1y5mo1d") -> unixtime shifted by real
// calendar months/years (leap-aware), not a fixed-seconds approximation.
// "mo" is a month; a lone "m" stays a minute, same as in time.parse.
static int l_add_calendar(lua_State *L)
{
	lua_Integer base = luaL_checkinteger(L, 1);
	size_t len = 0;
	const char *str = luaL_checklstring(L, 2, &len);

	long long number = 0;
	bool has_digits = false;

	long long plain_seconds = 0;
	long long months = 0;
	long long years = 0;

	for (size_t i = 0; i < len; i++) {
		char c = str[i];

		if (c >= '0' && c <= '9') {
			number = number * 10 + (c - '0');
			has_digits = true;
			continue;
		}

		if (!has_digits)
			return luaL_error(L, "time.add_calendar: '%c' in \"%s\" has no number before it", c, str);

		if (c == 'y') {
			years += number;
		} else if (c == 'm' && i + 1 < len && str[i + 1] == 'o') {
			months += number;
			i++; // consume the 'o'
		} else {
			long long secs = unit_seconds(c);
			if (secs < 0 || c == 'y')
				return luaL_error(L, "time.add_calendar: unknown unit near '%c' in \"%s\" (expected s/m/h/d/w/mo/y)", c, str);
			plain_seconds += number * secs;
		}

		number = 0;
		has_digits = false;
	}

	if (has_digits)
		return luaL_error(L, "time.add_calendar: \"%s\" is not a valid duration", str);

	time_t t = (time_t)base;
	struct tm tm_val;
#if defined(_WIN32)
	gmtime_s(&tm_val, &t);
#else
	gmtime_r(&t, &tm_val);
#endif
	tm_val.tm_year += (int)years;

	long long total_months = tm_val.tm_mon + months;
	tm_val.tm_year += (int)(total_months / 12);
	tm_val.tm_mon = (int)(total_months % 12);
	if (tm_val.tm_mon < 0) {
		tm_val.tm_mon += 12;
		tm_val.tm_year--;
	}

#if defined(_WIN32)
	time_t shifted = _mkgmtime(&tm_val);
#else
	time_t shifted = timegm(&tm_val);
#endif

	lua_pushinteger(L, (lua_Integer)shifted + plain_seconds);
	return 1;
}

// time.now() -> current unixtime (UTC)
static int l_now(lua_State *L)
{
	lua_pushinteger(L, (lua_Integer)time(NULL));
	return 1;
}

// time.until_(unixtime) -> seconds remaining, negative if already past
static int l_until(lua_State *L)
{
	lua_Integer target = luaL_checkinteger(L, 1);
	lua_pushinteger(L, target - (lua_Integer)time(NULL));
	return 1;
}

// time.is_expired(unixtime) -> bool
static int l_is_expired(lua_State *L)
{
	lua_Integer target = luaL_checkinteger(L, 1);
	lua_pushboolean(L, target <= (lua_Integer)time(NULL));
	return 1;
}

static const luaL_Reg s_time[] =
{
	{ "parse", l_parse },
	{ "format", l_format },
	{ "add_calendar", l_add_calendar },
	{ "now", l_now },
	{ "until_", l_until },
	{ "is_expired", l_is_expired },
	{ NULL, NULL }
};

void cslua_register_time(lua_State *L)
{
	cslua_register_namespace(L, "time", s_time);
}