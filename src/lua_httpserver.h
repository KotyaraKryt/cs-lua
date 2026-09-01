#pragma once

#include <lua.hpp>

// Inbound HTTP for plugins - the other half of http.cpp.
//
//   http_server.listen(8080)
//   http_server.route("GET", "/api/players", function(req)
//       return { status = 200, body = json.encode(names) }
//   end)
//
// A connection is accepted and parsed on a worker, then queued for the game
// thread; here it is the worker that blocks, because the socket has to be
// answered on the connection it arrived on. Route handlers run only from the
// frame tick, next to timers.
//
// A small HTTP/1.1 server: no keep-alive, no chunked bodies, no TLS. For an
// admin panel served from the game process, not the public internet.

void cslua_register_httpserver(lua_State *L);

// Runs finished requests' route handlers. Once per frame, next to http_run().
void cslua_httpserver_run();

// Drops every route belonging to one plugin. The listener is left running.
void cslua_httpserver_remove_plugin(int plugin_index);

// Forgets every route; parsed-and-waiting requests get a 503 so their worker
// does not hang. The listener socket is left open.
void cslua_httpserver_reset();

// Closes the listener and waits for every worker. Process exit only.
void cslua_httpserver_shutdown();
