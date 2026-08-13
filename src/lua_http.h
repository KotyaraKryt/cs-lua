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
// The request runs on a worker thread; the callback runs on the game thread,
// from the same drain that ticks timers. Nothing a plugin writes ever touches
// a socket, and nothing a worker touches ever touches the Lua state - only
// plain bytes cross between them.
//
// Transport is whatever the platform already ships, so the module stays a
// single file you drop next to metamod: WinHTTP on Windows (Windows SDK, TLS
// through Schannel), libcurl opened at first use on Linux. No vendored TLS,
// no build-time dependency, and a server without libcurl gets a clear error
// instead of a link failure.

void cslua_register_http(lua_State *L);

// Runs finished requests' callbacks. Called once per frame, next to the
// timers: a callback is ordinary plugin code and belongs on the game thread.
void cslua_http_run();

// Disowns everything in flight. The Lua state is going away, so no callback
// may fire again - but the workers are left alone deliberately: joining them
// here would stall lua_reload for as long as the slowest request still on the
// wire, up to its whole timeout. A reply that lands afterwards belongs to a
// past generation and is dropped by the drain.
void cslua_http_reset();

// Stops the workers and waits for them. Only at process exit, where a pause is
// free because the server is leaving anyway.
void cslua_http_shutdown();

// Forgets one plugin's pending requests. Its callbacks reference an
// environment that is being torn down; a reply arriving afterwards would run
// against a plugin that no longer exists.
void cslua_http_remove_plugin(int plugin_index);

// How many requests are waiting or running, for lua_list.
int cslua_http_pending();

// Blocking GET on the calling thread, using the same transport as the async
// API above. For engine code that runs before any Lua state or worker thread
// exists - see cslua_bootstrap.cpp. Never call this once plugins are
// running: there is no queue here, the caller simply waits.
bool cslua_http_get_sync(const std::string &url,
	const std::vector<std::pair<std::string, std::string> > &headers,
	int timeout_ms, std::string &out_body, int &out_status, std::string &out_error);
