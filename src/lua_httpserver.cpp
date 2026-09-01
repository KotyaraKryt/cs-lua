// winsock2.h must be seen before windows.h (pulled in by cslua.h) or the two
// headers' definitions collide.
#ifdef _WIN32
	#include <winsock2.h>
	#include <ws2tcpip.h>
#endif

#include "cslua.h"
#include "lua_httpserver.h"
#include "lua_engine.h"
#include "lua_natives.h"
#include "platform.h"

#include <atomic>
#include <condition_variable>
#include <ctype.h>
#include <deque>
#include <memory>
#include <mutex>
#include <stdlib.h>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
	typedef SOCKET cslua_socket_t;
	static const cslua_socket_t CSLUA_BAD_SOCKET = INVALID_SOCKET;
	#define CSLUA_CLOSESOCKET(s) closesocket(s)
#else
	#include <arpa/inet.h>
	#include <netinet/in.h>
	#include <strings.h>
	#include <sys/socket.h>
	#include <sys/select.h>
	#include <unistd.h>
	typedef int cslua_socket_t;
	static const cslua_socket_t CSLUA_BAD_SOCKET = -1;
	#define CSLUA_CLOSESOCKET(s) close(s)
#endif

// ---------------------------------------------------------------------------
// What crosses between the threads: a request is parsed entirely on its worker
// before the game thread sees it. The socket never leaves the worker.

namespace {

int cs_stricmp(const char *a, const char *b)
{
#ifdef _WIN32
	return _stricmp(a, b);
#else
	return strcasecmp(a, b);
#endif
}

struct Header
{
	std::string name;
	std::string value;
};

struct ParsedRequest
{
	std::string method;
	std::string path;			// no query string
	std::string query;			// raw, after '?'
	std::string body;
	std::vector<Header> headers;
};

struct Response
{
	int status = 200;
	std::string body;
	std::vector<Header> headers;

	// Set when a stream route's guard returned true: the worker hands the
	// socket to run_sse_session and status/body/headers are meaningless.
	bool upgrade_to_stream = false;
};

// One accepted connection. The worker owns the socket; the game thread only
// touches `req` and fills `res`.
struct PendingConn
{
	ParsedRequest req;

	std::mutex mtx;
	std::condition_variable cv;
	bool done = false;
	bool abandoned = false;		// the worker gave up waiting
	Response res;
};

struct Route
{
	std::string method;					// upper-cased
	std::vector<std::string> segments;		// path split on '/'
	int plugin;
	int callback;							// registry ref
};

// A live (SSE) subscriber. http_server.push() drops a message in every
// client's outbox and wakes it; the connection's own thread writes the socket.
struct SseClient
{
	std::mutex mtx;
	std::condition_variable cv;
	std::deque<std::string> outbox;
	bool stop = false;
};

std::deque<std::shared_ptr<PendingConn>> s_pending;
std::mutex s_pending_lock;

std::vector<Route> s_routes;
// Kept separate from s_routes only so a plain GET and a stream can't collide.
std::vector<Route> s_streams;
std::mutex s_routes_lock;

std::vector<std::shared_ptr<SseClient>> s_sse_clients;
std::mutex s_sse_lock;

// An idle stream gets a comment line this often - turns a dead peer into a
// failed send instead of a thread parked forever.
const int SSE_PING_MS = 15000;

// A slow subscriber must not grow without bound; drop the oldest update.
const size_t SSE_OUTBOX_MAX = 200;

std::deque<cslua_socket_t> s_conn_queue;
std::mutex s_conn_lock;
std::condition_variable s_conn_wake;

std::vector<std::thread> s_workers;
std::thread s_acceptor;
cslua_socket_t s_listen_fd = CSLUA_BAD_SOCKET;
std::atomic<bool> s_stopping(false);
int s_port = 0;

#ifdef _WIN32
bool s_wsa_started = false;
#endif

// Higher than lua_http.cpp's pool: a couple of these are SSE subscribers that
// hold their worker for as long as a browser tab stays open. Bounding the pool
// bounds how many can be connected at once, which is a feature here.
const size_t WORKER_COUNT = 8;
const size_t MAX_BODY = 1 * 1024 * 1024;
const size_t MAX_HEADER = 16 * 1024;
const int RECV_TIMEOUT_MS = 5000;
const int RESPONSE_WAIT_MS = 5000;

// A connection flood that never sends a byte must not grow s_conn_queue.
const size_t MAX_CONN_QUEUE = 64;

// ---------------------------------------------------------------------------
// Wire parsing - deliberately minimal. No keep-alive, no chunked bodies, no
// pipelining: one request, one response, then closed.

bool recv_line(cslua_socket_t fd, std::string &buf, std::string &line)
{
	for (;;) {
		size_t pos = buf.find("\r\n");
		if (pos != std::string::npos) {
			line = buf.substr(0, pos);
			buf.erase(0, pos + 2);
			return true;
		}

		if (buf.size() > MAX_HEADER)
			return false;

		char chunk[2048];
		int n = recv(fd, chunk, sizeof chunk, 0);
		if (n <= 0)
			return false;
		buf.append(chunk, (size_t)n);
	}
}

std::string trim(const std::string &s)
{
	size_t a = s.find_first_not_of(" \t");
	if (a == std::string::npos)
		return "";
	size_t b = s.find_last_not_of(" \t");
	return s.substr(a, b - a + 1);
}

// Reads request line, headers and body. False on anything malformed or too
// large - the caller answers 400 and closes.
bool read_request(cslua_socket_t fd, ParsedRequest &out)
{
	std::string buf;
	std::string line;

	if (!recv_line(fd, buf, line) || line.empty())
		return false;

	size_t sp1 = line.find(' ');
	size_t sp2 = sp1 == std::string::npos ? std::string::npos : line.find(' ', sp1 + 1);
	if (sp1 == std::string::npos || sp2 == std::string::npos)
		return false;

	out.method = line.substr(0, sp1);
	for (size_t i = 0; i < out.method.size(); i++)
		out.method[i] = (char)toupper((unsigned char)out.method[i]);

	std::string target = line.substr(sp1 + 1, sp2 - sp1 - 1);
	size_t q = target.find('?');
	if (q == std::string::npos) {
		out.path = target;
	} else {
		out.path = target.substr(0, q);
		out.query = target.substr(q + 1);
	}
	if (out.path.empty())
		out.path = "/";

	size_t content_length = 0;
	for (;;) {
		if (!recv_line(fd, buf, line))
			return false;
		if (line.empty())
			break;

		size_t colon = line.find(':');
		if (colon == std::string::npos)
			continue;

		Header h;
		h.name = trim(line.substr(0, colon));
		h.value = trim(line.substr(colon + 1));
		out.headers.push_back(h);

		if (!cs_stricmp(h.name.c_str(), "content-length"))
			content_length = (size_t)strtoul(h.value.c_str(), NULL, 10);
	}

	if (content_length > MAX_BODY)
		return false;

	// Whatever arrived in the same packet as the headers counts first.
	out.body = buf;
	while (out.body.size() < content_length) {
		char chunk[4096];
		size_t want = content_length - out.body.size();
		int n = recv(fd, chunk, (int)(want < sizeof chunk ? want : sizeof chunk), 0);
		if (n <= 0)
			return false;
		out.body.append(chunk, (size_t)n);
	}
	out.body.resize(content_length);

	return true;
}

const char *reason_phrase(int status)
{
	switch (status) {
		case 200: return "OK";
		case 201: return "Created";
		case 204: return "No Content";
		case 301: return "Moved Permanently";
		case 302: return "Found";
		case 400: return "Bad Request";
		case 401: return "Unauthorized";
		case 403: return "Forbidden";
		case 404: return "Not Found";
		case 405: return "Method Not Allowed";
		case 500: return "Internal Server Error";
		case 503: return "Service Unavailable";
		default:  return "OK";
	}
}

void write_response(cslua_socket_t fd, const Response &res)
{
	std::string out;
	char status_line[64];
	cslua_snprintf(status_line, sizeof status_line, "HTTP/1.1 %d %s\r\n",
		res.status, reason_phrase(res.status));
	out += status_line;

	bool has_content_type = false;
	for (size_t i = 0; i < res.headers.size(); i++) {
		const std::string &name = res.headers[i].name;
		const std::string &value = res.headers[i].value;

		// A handler reflecting request data into a header must not split the
		// response - drop the header rather than sanitize it.
		if (name.find_first_of("\r\n") != std::string::npos ||
			value.find_first_of("\r\n") != std::string::npos)
			continue;

		if (!cs_stricmp(name.c_str(), "content-type"))
			has_content_type = true;
		out += name;
		out += ": ";
		out += value;
		out += "\r\n";
	}

	if (!has_content_type)
		out += "Content-Type: text/plain; charset=utf-8\r\n";

	char len_line[64];
	cslua_snprintf(len_line, sizeof len_line, "Content-Length: %u\r\n", (unsigned)res.body.size());
	out += len_line;
	out += "Connection: close\r\n\r\n";
	out += res.body;

	size_t sent = 0;
	while (sent < out.size()) {
		int n = send(fd, out.data() + sent, (int)(out.size() - sent), 0);
		if (n <= 0)
			return;
		sent += (size_t)n;
	}
}

// A client that stops reading its response parks send() forever with only
// SO_RCVTIMEO set, so set both.
void set_socket_timeouts(cslua_socket_t fd, int ms)
{
#ifdef _WIN32
	DWORD timeout = (DWORD)ms;
	setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout, sizeof timeout);
	setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, (const char *)&timeout, sizeof timeout);
#else
	struct timeval tv;
	tv.tv_sec = ms / 1000;
	tv.tv_usec = (ms % 1000) * 1000;
	setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof tv);
	setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, (const char *)&tv, sizeof tv);
#endif
}

// Runs an SSE connection to completion: headers, then messages as they arrive,
// until the socket fails or the server is stopping. The accepting worker stays
// here for the connection's whole life.
void run_sse_session(cslua_socket_t fd)
{
	static const char *HEADERS =
		"HTTP/1.1 200 OK\r\n"
		"Content-Type: text/event-stream; charset=utf-8\r\n"
		"Cache-Control: no-cache\r\n"
		"Connection: keep-alive\r\n"
		"\r\n";

	{
		size_t len = strlen(HEADERS);
		if (send(fd, HEADERS, (int)len, 0) != (int)len) {
			CSLUA_CLOSESOCKET(fd);
			return;
		}
	}

	auto client = std::make_shared<SseClient>();
	{
		std::lock_guard<std::mutex> guard(s_sse_lock);
		s_sse_clients.push_back(client);
	}

	for (;;) {
		std::string message;
		bool have_message = false;

		{
			std::unique_lock<std::mutex> guard(client->mtx);
			client->cv.wait_for(guard, std::chrono::milliseconds(SSE_PING_MS), [&] {
				return s_stopping.load() || client->stop || !client->outbox.empty();
			});

			if (s_stopping || client->stop)
				break;

			if (!client->outbox.empty()) {
				message = client->outbox.front();
				client->outbox.pop_front();
				have_message = true;
			}
		}

		bool ok;
		if (have_message) {
			std::string frame = "data: " + message + "\n\n";
			ok = send(fd, frame.data(), (int)frame.size(), 0) == (int)frame.size();
		} else {
			// Idle timeout: send a comment line to notice a dead socket.
			static const char PING[] = ": ping\n\n";
			ok = send(fd, PING, sizeof(PING) - 1, 0) == sizeof(PING) - 1;
		}

		if (!ok)
			break;
	}

	{
		std::lock_guard<std::mutex> guard(s_sse_lock);
		for (size_t i = 0; i < s_sse_clients.size(); i++) {
			if (s_sse_clients[i] == client) {
				s_sse_clients.erase(s_sse_clients.begin() + i);
				break;
			}
		}
	}

	CSLUA_CLOSESOCKET(fd);
}

// ---------------------------------------------------------------------------
// Workers: a fixed pool, each pulling one accepted socket at a time. A worker
// blocks twice - reading the request, waiting for the game thread's answer.

void handle_connection(cslua_socket_t fd)
{
	set_socket_timeouts(fd, RECV_TIMEOUT_MS);

	auto conn = std::make_shared<PendingConn>();
	if (!read_request(fd, conn->req)) {
		Response bad;
		bad.status = 400;
		bad.body = "bad request";
		write_response(fd, bad);
		CSLUA_CLOSESOCKET(fd);
		return;
	}

	{
		std::lock_guard<std::mutex> guard(s_pending_lock);
		s_pending.push_back(conn);
	}

	bool answered;
	{
		std::unique_lock<std::mutex> guard(conn->mtx);
		answered = conn->cv.wait_for(guard, std::chrono::milliseconds(RESPONSE_WAIT_MS),
			[&] { return conn->done; });

		if (!answered)
			conn->abandoned = true;
	}

	if (!answered) {
		Response timeout;
		timeout.status = 503;
		timeout.body = "no plugin answered in time";
		write_response(fd, timeout);
		CSLUA_CLOSESOCKET(fd);
		return;
	}

	// A stream route's guard said yes: the socket becomes an SSE subscriber.
	if (conn->res.upgrade_to_stream) {
		run_sse_session(fd);
		return;
	}

	write_response(fd, conn->res);
	CSLUA_CLOSESOCKET(fd);
}

void worker_loop()
{
	for (;;) {
		cslua_socket_t fd;

		{
			std::unique_lock<std::mutex> guard(s_conn_lock);
			s_conn_wake.wait(guard, [] { return s_stopping || !s_conn_queue.empty(); });

			if (s_stopping)
				return;

			fd = s_conn_queue.front();
			s_conn_queue.pop_front();
		}

		handle_connection(fd);
	}
}

// One thread, purely to call accept(). Never blocks longer than the select()
// timeout, so shutdown can stop it without closing the socket out from under it.
void accept_loop()
{
	for (;;) {
		fd_set set;
		FD_ZERO(&set);
		FD_SET(s_listen_fd, &set);

		struct timeval tv;
		tv.tv_sec = 0;
		tv.tv_usec = 250 * 1000;

		int n = select((int)s_listen_fd + 1, &set, NULL, NULL, &tv);

		if (s_stopping)
			return;

		if (n <= 0)
			continue;

		cslua_socket_t fd = accept(s_listen_fd, NULL, NULL);
		if (fd == CSLUA_BAD_SOCKET)
			continue;

		{
			std::lock_guard<std::mutex> guard(s_conn_lock);
			if (s_conn_queue.size() >= MAX_CONN_QUEUE) {
				CSLUA_CLOSESOCKET(fd);
				continue;
			}
			s_conn_queue.push_back(fd);
		}
		s_conn_wake.notify_one();
	}
}

// ---------------------------------------------------------------------------
// Routing

bool split_path(const std::string &path, std::vector<std::string> &out)
{
	if (path.empty() || path[0] != '/')
		return false;

	size_t i = 1;
	while (i <= path.size()) {
		size_t next = path.find('/', i);
		if (next == std::string::npos) {
			out.push_back(path.substr(i));
			break;
		}
		out.push_back(path.substr(i, next - i));
		i = next + 1;
	}
	return true;
}

// Matches `segments` against a route, filling `params` with ":name" captures.
bool match_route(const Route &route, const std::vector<std::string> &segments,
	std::vector<Header> &params)
{
	if (route.segments.size() != segments.size())
		return false;

	for (size_t i = 0; i < segments.size(); i++) {
		const std::string &want = route.segments[i];
		if (!want.empty() && want[0] == ':') {
			Header p;
			p.name = want.substr(1);
			p.value = segments[i];
			params.push_back(p);
		} else if (want != segments[i]) {
			return false;
		}
	}
	return true;
}

// A trivial x-www-form-urlencoded / query-string decoder: '+' and '%XX'.
std::string url_decode(const std::string &s)
{
	std::string out;
	out.reserve(s.size());

	for (size_t i = 0; i < s.size(); i++) {
		if (s[i] == '+') {
			out += ' ';
		} else if (s[i] == '%' && i + 2 < s.size() && isxdigit((unsigned char)s[i + 1])
				&& isxdigit((unsigned char)s[i + 2])) {
			int hi = s[i + 1] <= '9' ? s[i + 1] - '0' : (tolower(s[i + 1]) - 'a' + 10);
			int lo = s[i + 2] <= '9' ? s[i + 2] - '0' : (tolower(s[i + 2]) - 'a' + 10);
			out += (char)((hi << 4) | lo);
			i += 2;
		} else {
			out += s[i];
		}
	}
	return out;
}

void parse_query(const std::string &raw, std::vector<Header> &out)
{
	size_t i = 0;
	while (i < raw.size()) {
		size_t amp = raw.find('&', i);
		std::string pair = amp == std::string::npos ? raw.substr(i) : raw.substr(i, amp - i);

		size_t eq = pair.find('=');
		Header h;
		if (eq == std::string::npos) {
			h.name = url_decode(pair);
		} else {
			h.name = url_decode(pair.substr(0, eq));
			h.value = url_decode(pair.substr(eq + 1));
		}
		if (!h.name.empty())
			out.push_back(h);

		if (amp == std::string::npos)
			break;
		i = amp + 1;
	}
}

void push_pairs_table(lua_State *L, const std::vector<Header> &pairs)
{
	lua_newtable(L);
	for (size_t i = 0; i < pairs.size(); i++) {
		lua_pushlstring(L, pairs[i].value.data(), pairs[i].value.size());
		lua_setfield(L, -2, pairs[i].name.c_str());
	}
}

// Builds the table every route handler gets: method, path, query, params,
// headers, body.
void push_request_table(lua_State *L, const ParsedRequest &req, const std::vector<Header> &params)
{
	lua_newtable(L);

	lua_pushstring(L, req.method.c_str());
	lua_setfield(L, -2, "method");

	lua_pushstring(L, req.path.c_str());
	lua_setfield(L, -2, "path");

	lua_pushlstring(L, req.body.data(), req.body.size());
	lua_setfield(L, -2, "body");

	std::vector<Header> query;
	parse_query(req.query, query);
	push_pairs_table(L, query);
	lua_setfield(L, -2, "query");

	push_pairs_table(L, params);
	lua_setfield(L, -2, "params");

	push_pairs_table(L, req.headers);
	lua_setfield(L, -2, "headers");
}

// Reads a handler's return value(s) into a Response.
//   return { status = 200, body = "...", headers = {...} }
//   return "plain body"
//   return 404, "not found"
void read_response(lua_State *L, int nresults, Response &out)
{
	out.status = 200;

	if (nresults <= 0)
		return;

	int first = lua_gettop(L) - nresults + 1;

	if (lua_isnumber(L, first)) {
		out.status = (int)lua_tointeger(L, first);
		if (nresults >= 2 && lua_isstring(L, first + 1)) {
			size_t len = 0;
			const char *s = lua_tolstring(L, first + 1, &len);
			out.body.assign(s, len);
		}
		return;
	}

	if (lua_isstring(L, first)) {
		size_t len = 0;
		const char *s = lua_tolstring(L, first, &len);
		out.body.assign(s, len);
		return;
	}

	if (!lua_istable(L, first))
		return;

	lua_getfield(L, first, "status");
	if (lua_isnumber(L, -1))
		out.status = (int)lua_tointeger(L, -1);
	lua_pop(L, 1);

	lua_getfield(L, first, "body");
	if (lua_isstring(L, -1)) {
		size_t len = 0;
		const char *s = lua_tolstring(L, -1, &len);
		out.body.assign(s, len);
	}
	lua_pop(L, 1);

	lua_getfield(L, first, "headers");
	if (lua_istable(L, -1)) {
		lua_pushnil(L);
		while (lua_next(L, -2)) {
			if (lua_isstring(L, -2) && lua_isstring(L, -1)) {
				Header h;
				h.name = lua_tostring(L, -2);
				h.value = lua_tostring(L, -1);
				out.headers.push_back(h);
			}
			lua_pop(L, 1);
		}
	}
	lua_pop(L, 1);
}

} // namespace

// ---------------------------------------------------------------------------
// Lua side

static int l_route(lua_State *L)
{
	const char *method = luaL_checkstring(L, 1);
	const char *path = luaL_checkstring(L, 2);
	luaL_checktype(L, 3, LUA_TFUNCTION);

	Route route;
	route.method = method;
	for (size_t i = 0; i < route.method.size(); i++)
		route.method[i] = (char)toupper((unsigned char)route.method[i]);

	if (!split_path(path, route.segments))
		return luaL_error(L, "http_server.route: path must start with '/'");

	route.plugin = g_lua.current_index();

	lua_pushvalue(L, 3);
	route.callback = luaL_ref(L, LUA_REGISTRYINDEX);

	{
		std::lock_guard<std::mutex> guard(s_routes_lock);
		s_routes.push_back(route);
	}

	return 0;
}

// http_server.stream(path, guard) - a live-update endpoint. A GET runs
// guard(req) on the game thread; returning true upgrades to SSE, anything else
// is treated as a normal route response.
static int l_stream(lua_State *L)
{
	const char *path = luaL_checkstring(L, 1);
	luaL_checktype(L, 2, LUA_TFUNCTION);

	Route route;
	route.method = "GET";

	if (!split_path(path, route.segments))
		return luaL_error(L, "http_server.stream: path must start with '/'");

	route.plugin = g_lua.current_index();

	lua_pushvalue(L, 2);
	route.callback = luaL_ref(L, LUA_REGISTRYINDEX);

	std::lock_guard<std::mutex> guard(s_routes_lock);
	s_streams.push_back(route);
	return 0;
}

// http_server.push(data) - one SSE message to every subscriber. Returns how
// many clients got it.
static int l_push(lua_State *L)
{
	size_t len = 0;
	const char *data = luaL_checklstring(L, 1, &len);
	std::string message(data, len);

	std::lock_guard<std::mutex> guard(s_sse_lock);
	for (size_t i = 0; i < s_sse_clients.size(); i++) {
		std::shared_ptr<SseClient> &client = s_sse_clients[i];
		std::lock_guard<std::mutex> client_guard(client->mtx);

		if (client->outbox.size() >= SSE_OUTBOX_MAX)
			client->outbox.pop_front();
		client->outbox.push_back(message);
		client->cv.notify_one();
	}

	lua_pushinteger(L, (int)s_sse_clients.size());
	return 1;
}

static bool start_listening(int port, std::string &err)
{
#ifdef _WIN32
	if (!s_wsa_started) {
		WSADATA wsa;
		if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
			err = "WSAStartup failed";
			return false;
		}
		s_wsa_started = true;
	}
#endif

	cslua_socket_t fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (fd == CSLUA_BAD_SOCKET) {
		err = "socket() failed";
		return false;
	}

	int one = 1;
	setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (const char *)&one, sizeof one);

	struct sockaddr_in addr;
	memset(&addr, 0, sizeof addr);
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons((unsigned short)port);

	if (bind(fd, (struct sockaddr *)&addr, sizeof addr) != 0) {
		err = "bind() failed - port already in use?";
		CSLUA_CLOSESOCKET(fd);
		return false;
	}

	if (listen(fd, 16) != 0) {
		err = "listen() failed";
		CSLUA_CLOSESOCKET(fd);
		return false;
	}

	s_listen_fd = fd;
	s_port = port;
	s_stopping = false;

	s_acceptor = std::thread(accept_loop);
	for (size_t i = 0; i < WORKER_COUNT; i++)
		s_workers.push_back(std::thread(worker_loop));

	return true;
}

static void stop_listening()
{
	if (s_listen_fd == CSLUA_BAD_SOCKET)
		return;

	{
		std::lock_guard<std::mutex> guard(s_conn_lock);
		s_stopping = true;
	}
	s_conn_wake.notify_all();

	// A worker parked in run_sse_session is asleep on its own cv, not
	// s_conn_wake - wake it so the join below finishes in milliseconds.
	{
		std::lock_guard<std::mutex> guard(s_sse_lock);
		for (size_t i = 0; i < s_sse_clients.size(); i++) {
			std::lock_guard<std::mutex> client_guard(s_sse_clients[i]->mtx);
			s_sse_clients[i]->stop = true;
			s_sse_clients[i]->cv.notify_one();
		}
	}

	if (s_acceptor.joinable())
		s_acceptor.join();
	for (size_t i = 0; i < s_workers.size(); i++) {
		if (s_workers[i].joinable())
			s_workers[i].join();
	}
	s_workers.clear();

	CSLUA_CLOSESOCKET(s_listen_fd);
	s_listen_fd = CSLUA_BAD_SOCKET;
	s_port = 0;

	std::lock_guard<std::mutex> guard(s_conn_lock);
	while (!s_conn_queue.empty()) {
		CSLUA_CLOSESOCKET(s_conn_queue.front());
		s_conn_queue.pop_front();
	}
}

static int l_listen(lua_State *L)
{
	int port = (int)luaL_checkinteger(L, 1);

	if (s_listen_fd != CSLUA_BAD_SOCKET) {
		lua_pushboolean(L, 1);
		return 1;
	}

	std::string err;
	if (!start_listening(port, err)) {
		lua_pushboolean(L, 0);
		lua_pushstring(L, err.c_str());
		return 2;
	}

	cslua_print("web panel listening on port %d", port);
	lua_pushboolean(L, 1);
	return 1;
}

static int l_stop(lua_State *L)
{
	stop_listening();
	return 0;
}

static int l_listening(lua_State *L)
{
	lua_pushboolean(L, s_listen_fd != CSLUA_BAD_SOCKET);
	if (s_listen_fd != CSLUA_BAD_SOCKET) {
		lua_pushinteger(L, s_port);
		return 2;
	}
	return 1;
}

static const luaL_Reg s_http_server[] =
{
	{ "listen",    l_listen },
	{ "stop",      l_stop },
	{ "listening", l_listening },
	{ "route",     l_route },
	{ "stream",    l_stream },
	{ "push",      l_push },
	{ NULL, NULL }
};

void cslua_register_httpserver(lua_State *L)
{
	cslua_register_namespace(L, "http_server", s_http_server);
}

// ---------------------------------------------------------------------------
// Draining, on the game thread

void cslua_httpserver_run()
{
	lua_State *L = g_lua.state();
	if (!L)
		return;

	std::deque<std::shared_ptr<PendingConn>> batch;
	{
		std::lock_guard<std::mutex> guard(s_pending_lock);
		if (s_pending.empty())
			return;
		batch.swap(s_pending);
	}

	for (size_t i = 0; i < batch.size(); i++) {
		std::shared_ptr<PendingConn> conn = batch[i];

		{
			std::lock_guard<std::mutex> guard(conn->mtx);
			if (conn->abandoned)
				continue;
		}

		std::vector<std::string> segments;
		split_path(conn->req.path, segments);

		Route match;
		bool found = false;
		bool is_stream = false;
		bool path_exists = false;
		std::vector<Header> params;

		{
			std::lock_guard<std::mutex> guard(s_routes_lock);

			// Streams first: a subscribe request should win on its own endpoint
			// rather than fall through to a same-path route's 405.
			for (size_t r = 0; r < s_streams.size() && !found; r++) {
				std::vector<Header> try_params;
				if (!match_route(s_streams[r], segments, try_params))
					continue;
				path_exists = true;
				if (conn->req.method == "GET") {
					match = s_streams[r];
					params = try_params;
					found = true;
					is_stream = true;
				}
			}

			for (size_t r = 0; r < s_routes.size() && !found; r++) {
				std::vector<Header> try_params;
				if (!match_route(s_routes[r], segments, try_params))
					continue;

				path_exists = true;
				if (s_routes[r].method == conn->req.method) {
					match = s_routes[r];
					params = try_params;
					found = true;
				}
			}
		}

		Response res;

		if (!found) {
			res.status = path_exists ? 405 : 404;
			res.body = path_exists ? "method not allowed" : "not found";
		} else {
			PluginScope scope(match.plugin);
			int errfunc = g_lua.push_errfunc();

			lua_rawgeti(L, LUA_REGISTRYINDEX, match.callback);
			push_request_table(L, conn->req, params);

			if (lua_pcall(L, 1, LUA_MULTRET, errfunc) != 0) {
				g_lua.report_error(is_stream ? "http_server stream guard" : "http_server route");
				res.status = 500;
				res.body = "plugin error";
			} else if (is_stream) {
				int nresults = lua_gettop(L) - errfunc;
				int first = errfunc + 1;

				if (nresults >= 1 && lua_isboolean(L, first) && lua_toboolean(L, first)) {
					res.upgrade_to_stream = true;
				} else if (nresults >= 1 && lua_istable(L, first)) {
					read_response(L, nresults, res);
				} else {
					res.status = 401;
					res.body = "unauthorized";
				}
			} else {
				int nresults = lua_gettop(L) - errfunc;
				read_response(L, nresults, res);
			}

			lua_settop(L, errfunc - 1);
		}

		{
			std::lock_guard<std::mutex> guard(conn->mtx);
			if (conn->abandoned)
				continue;
			conn->res = res;
			conn->done = true;
		}
		conn->cv.notify_one();
	}
}

void cslua_httpserver_remove_plugin(int plugin_index)
{
	lua_State *L = g_lua.state();

	std::lock_guard<std::mutex> guard(s_routes_lock);
	for (std::vector<Route>::iterator it = s_routes.begin(); it != s_routes.end(); ) {
		if (it->plugin != plugin_index) {
			++it;
			continue;
		}
		if (L)
			luaL_unref(L, LUA_REGISTRYINDEX, it->callback);
		it = s_routes.erase(it);
	}

	for (std::vector<Route>::iterator it = s_streams.begin(); it != s_streams.end(); ) {
		if (it->plugin != plugin_index) {
			++it;
			continue;
		}
		if (L)
			luaL_unref(L, LUA_REGISTRYINDEX, it->callback);
		it = s_streams.erase(it);
	}
}

void cslua_httpserver_reset()
{
	// Anything already parsed and waiting gets a straight answer now.
	std::deque<std::shared_ptr<PendingConn>> batch;
	{
		std::lock_guard<std::mutex> guard(s_pending_lock);
		batch.swap(s_pending);
	}

	for (size_t i = 0; i < batch.size(); i++) {
		std::shared_ptr<PendingConn> conn = batch[i];
		std::lock_guard<std::mutex> guard(conn->mtx);
		if (conn->abandoned)
			continue;
		conn->res = Response();
		conn->res.status = 503;
		conn->res.body = "server reloading";
		conn->done = true;
		conn->cv.notify_one();
	}

	{
		std::lock_guard<std::mutex> guard(s_routes_lock);
		// The registry refs are not unref'd: the whole registry is freed at once.
		s_routes.clear();
		// Not the connected clients: the reloaded plugin re-registers the path
		// and picks up where push() left off.
		s_streams.clear();
	}
}

void cslua_httpserver_shutdown()
{
	cslua_httpserver_reset();
	stop_listening();

#ifdef _WIN32
	if (s_wsa_started) {
		WSACleanup();
		s_wsa_started = false;
	}
#endif
}
