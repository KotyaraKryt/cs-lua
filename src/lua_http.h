#pragma once

#include <lua.hpp>

#include <string>
#include <utility>
#include <vector>

// Outbound HTTP for plugins.
//
//   http.get("https://example.com/api", function(res)
//       if res.ok then print(res.status, res.body) end
//   end)
//
// The request runs on a worker; the callback on the game thread, from the drain
// that ticks timers. Transport is whatever the platform ships: WinHTTP on
// Windows, libcurl opened at first use on Linux.

void cslua_register_http(lua_State *L);

// Runs finished requests' callbacks. Once per frame, next to the timers.
void cslua_http_run();

// Disowns everything in flight. The workers are left alone: joining them here
// would stall lua_reload for as long as the slowest request's timeout.
void cslua_http_reset();

// Stops the workers and waits. Process exit only.
void cslua_http_shutdown();

// Forgets one plugin's pending requests.
void cslua_http_remove_plugin(int plugin_index);

// How many requests are waiting or running, for lua_list.
int cslua_http_pending();

// Blocking GET on the calling thread, for engine code that runs before any Lua
// state or worker thread exists (cslua_bootstrap.cpp). Never call once plugins
// are running.
bool cslua_http_get_sync(const std::string &url,
	const std::vector<std::pair<std::string, std::string> > &headers,
	int timeout_ms, std::string &out_body, int &out_status, std::string &out_error);
