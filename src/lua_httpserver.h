#pragma once

#include <lua.hpp>

// Inbound HTTP for plugins - the other half of http.cpp.
//
//   http_server.listen(8080)
//
//   http_server.route("GET", "/api/players", function(req)
//       return { status = 200, body = json.encode(names), headers = {
//           ["Content-Type"] = "application/json" } }
//   end)
//
// A connection is accepted and its request parsed on a worker thread, then
// queued for the game thread - the same split as the outbound client, just
// with the roles reversed: here it is the worker that blocks, waiting for the
// game thread to hand back a response, because the socket has to be answered
// on the same connection it arrived on. Route handlers are ordinary plugin
// code and only ever run from the frame tick, next to timers and hook
// callbacks - never from the accepting thread.
//
// This is a small HTTP/1.1 server, not a general-purpose one: no keep-alive,
// no chunked bodies, no TLS. It exists to let a plugin serve a JSON API and a
// handful of static pages (an admin panel) directly from the game process,
// not to face the public internet.

void cslua_register_httpserver(lua_State *L);

// Runs finished (fully parsed) requests' route handlers. Called once per
// frame, next to http_run().
void cslua_httpserver_run();

// Drops every route belonging to one plugin. Called on a single-plugin
// reload; the listener itself is left running.
void cslua_httpserver_remove_plugin(int plugin_index);

// Forgets every route. The Lua state is going away; requests already parsed
// and waiting for a handler are answered 503 so their worker does not hang
// until its timeout. The listener socket is left open - it belongs to the
// process, not to one generation of the Lua state.
void cslua_httpserver_reset();

// Closes the listener, if any, and waits for every worker to notice and
// finish. Only at process exit, same as http_shutdown.
void cslua_httpserver_shutdown();
