#include "cslua.h"
#include "lua_http.h"
#include "lua_engine.h"
#include "lua_natives.h"
#include "platform.h"

#include <condition_variable>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
	#include <windows.h>
	#include <winhttp.h>
#endif

// ---------------------------------------------------------------------------
// What crosses between the threads
//
// Plain bytes only. A worker never sees lua_State, and the game thread never
// sees a socket - which is the whole reason this is safe to do at all while a
// map is running.

namespace {

struct Header
{
	std::string name;
	std::string value;
};

struct Request
{
	int id;
	int plugin;				// index into LuaEngine::plugins(), for cleanup
	int callback;			// registry ref, only ever touched on the game thread
	int generation;			// which Lua state asked; see s_generation

	std::string method;
	std::string url;
	std::string body;
	std::vector<Header> headers;
	int timeout_ms;
};

struct Response
{
	int id;
	int plugin;
	int callback;
	int generation;

	bool ok;
	int status;
	std::string body;
	std::string error;
	std::vector<Header> headers;
};

// Two queues and one lock each. A request goes in at the top, comes out on a
// worker, and its answer goes back through the second queue to be drained on
// the next frame.
std::deque<Request> s_pending;
std::deque<Response> s_done;

std::mutex s_pending_lock;
std::mutex s_done_lock;
std::condition_variable s_wake;

std::vector<std::thread> s_workers;
bool s_stopping = false;
int s_next_id = 1;

// Bumped every time the Lua state is rebuilt. A worker cannot be interrupted
// mid-request, so a reload cannot wait for it - instead the reply comes back
// stamped with the generation that asked for it, and the drain throws away
// anything from a state that no longer exists. That is what keeps lua_reload
// instant while a request is on the wire.
int s_generation = 1;

// In flight or waiting. Kept separately because a request leaves s_pending the
// moment a worker picks it up and only reappears in s_done when it finishes.
int s_inflight = 0;
std::mutex s_inflight_lock;

// Two workers: enough that one slow endpoint does not stall another plugin,
// few enough that a plugin looping over a player list cannot spawn a thread
// storm. Anything past that queues, which is the correct backpressure.
const size_t WORKER_COUNT = 2;

// A body this large is a bug on a game server, not a feature. Refusing beats
// letting one reply eat the heap on a 32-bit process.
const size_t MAX_BODY = 4 * 1024 * 1024;

} // namespace

// ---------------------------------------------------------------------------
// Transport
//
// One function per platform, both blocking. The queue above is what makes them
// safe to call; making the transport itself async would buy nothing and cost a
// state machine.

#ifdef _WIN32

static std::wstring widen(const std::string &s)
{
	if (s.empty())
		return std::wstring();

	int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), NULL, 0);
	std::wstring out((size_t)n, L'\0');
	MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &out[0], n);
	return out;
}

static std::string narrow(const wchar_t *s, int len)
{
	if (len <= 0)
		return std::string();

	int n = WideCharToMultiByte(CP_UTF8, 0, s, len, NULL, 0, NULL, NULL);
	std::string out((size_t)n, '\0');
	WideCharToMultiByte(CP_UTF8, 0, s, len, &out[0], n, NULL, NULL);
	return out;
}

// Every handle here has to be closed on every path, including the error ones,
// so each stage checks and jumps to one exit.
static void perform(const Request &req, Response &res)
{
	res.ok = false;
	res.status = 0;

	std::wstring wurl = widen(req.url);

	URL_COMPONENTS parts;
	memset(&parts, 0, sizeof parts);
	parts.dwStructSize = sizeof parts;
	parts.dwSchemeLength = (DWORD)-1;
	parts.dwHostNameLength = (DWORD)-1;
	parts.dwUrlPathLength = (DWORD)-1;
	parts.dwExtraInfoLength = (DWORD)-1;

	if (!WinHttpCrackUrl(wurl.c_str(), (DWORD)wurl.size(), 0, &parts)) {
		res.error = "cannot parse the URL";
		return;
	}

	std::wstring host(parts.lpszHostName, parts.dwHostNameLength);
	std::wstring path(parts.lpszUrlPath, parts.dwUrlPathLength);
	if (parts.dwExtraInfoLength)
		path.append(parts.lpszExtraInfo, parts.dwExtraInfoLength);
	if (path.empty())
		path = L"/";

	bool secure = parts.nScheme == INTERNET_SCHEME_HTTPS;

	HINTERNET session = NULL, connect = NULL, request = NULL;

	session = WinHttpOpen(L"cs-lua", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
		WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
	if (!session) {
		res.error = "WinHttpOpen failed";
		goto done;
	}

	WinHttpSetTimeouts(session, req.timeout_ms, req.timeout_ms,
		req.timeout_ms, req.timeout_ms);

	connect = WinHttpConnect(session, host.c_str(), parts.nPort, 0);
	if (!connect) {
		res.error = "cannot connect to " + narrow(host.c_str(), (int)host.size());
		goto done;
	}

	request = WinHttpOpenRequest(connect, widen(req.method).c_str(), path.c_str(),
		NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
		secure ? WINHTTP_FLAG_SECURE : 0);
	if (!request) {
		res.error = "cannot open the request";
		goto done;
	}

	for (size_t i = 0; i < req.headers.size(); i++) {
		std::wstring line = widen(req.headers[i].name + ": " + req.headers[i].value);
		WinHttpAddRequestHeaders(request, line.c_str(), (DWORD)-1,
			WINHTTP_ADDREQ_FLAG_ADD | WINHTTP_ADDREQ_FLAG_REPLACE);
	}

	if (!WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
			req.body.empty() ? WINHTTP_NO_REQUEST_DATA : (LPVOID)req.body.data(),
			(DWORD)req.body.size(), (DWORD)req.body.size(), 0)) {
		res.error = "the request could not be sent";
		goto done;
	}

	if (!WinHttpReceiveResponse(request, NULL)) {
		res.error = "no response";
		goto done;
	}

	{
		DWORD status = 0, size = sizeof status;
		WinHttpQueryHeaders(request,
			WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
			WINHTTP_HEADER_NAME_BY_INDEX, &status, &size, WINHTTP_NO_HEADER_INDEX);
		res.status = (int)status;

		for (;;) {
			DWORD available = 0;
			if (!WinHttpQueryDataAvailable(request, &available) || available == 0)
				break;

			size_t before = res.body.size();
			if (before + available > MAX_BODY) {
				res.error = "the response is too large";
				goto done;
			}

			res.body.resize(before + available);
			DWORD read = 0;
			if (!WinHttpReadData(request, &res.body[before], available, &read)) {
				res.error = "the response was cut short";
				goto done;
			}
			res.body.resize(before + read);
		}

		res.ok = true;
	}

done:
	if (request) WinHttpCloseHandle(request);
	if (connect) WinHttpCloseHandle(connect);
	if (session) WinHttpCloseHandle(session);
}

#else

// libcurl is opened at first use rather than linked: that keeps HTTP out of
// the build entirely - no 32-bit -dev package on the build machine, no CI
// step, no shipped .so - and turns "libcurl is missing" into a message a
// server owner can act on instead of a module that refuses to load.
namespace {

typedef void CURL;

typedef CURL *(*curl_easy_init_fn)(void);
typedef int (*curl_easy_setopt_fn)(CURL *, int, ...);
typedef int (*curl_easy_perform_fn)(CURL *);
typedef int (*curl_easy_getinfo_fn)(CURL *, int, ...);
typedef void (*curl_easy_cleanup_fn)(CURL *);
typedef const char *(*curl_easy_strerror_fn)(int);
typedef struct curl_slist *(*curl_slist_append_fn)(struct curl_slist *, const char *);
typedef void (*curl_slist_free_all_fn)(struct curl_slist *);

struct Curl
{
	void *handle;
	curl_easy_init_fn easy_init;
	curl_easy_setopt_fn easy_setopt;
	curl_easy_perform_fn easy_perform;
	curl_easy_getinfo_fn easy_getinfo;
	curl_easy_cleanup_fn easy_cleanup;
	curl_easy_strerror_fn easy_strerror;
	curl_slist_append_fn slist_append;
	curl_slist_free_all_fn slist_free_all;
	bool tried;
};

Curl s_curl = { 0 };
std::mutex s_curl_lock;

// The option and info numbers we use, spelled out so no curl header is needed.
const int OPT_URL = 10002;
const int OPT_WRITEFUNCTION = 20011;
const int OPT_WRITEDATA = 10001;
const int OPT_TIMEOUT_MS = 155;
const int OPT_FOLLOWLOCATION = 52;
const int OPT_MAXREDIRS = 68;
const int OPT_POSTFIELDS = 10015;
const int OPT_POSTFIELDSIZE = 60;
const int OPT_CUSTOMREQUEST = 10036;
const int OPT_HTTPHEADER = 10023;
const int OPT_USERAGENT = 10018;
const int OPT_NOSIGNAL = 99;
const int INFO_RESPONSE_CODE = 0x200002;

bool curl_ready()
{
	std::lock_guard<std::mutex> guard(s_curl_lock);

	if (s_curl.tried)
		return s_curl.handle != NULL;

	s_curl.tried = true;

	// Whatever the distribution called it.
	static const char *const names[] = {
		"libcurl.so.4", "libcurl.so.3", "libcurl.so", NULL
	};

	for (int i = 0; names[i] && !s_curl.handle; i++)
		s_curl.handle = dlopen(names[i], RTLD_LAZY | RTLD_LOCAL);

	if (!s_curl.handle)
		return false;

	#define CSLUA_CURL_SYM(field, name) \
		s_curl.field = (field##_fn)dlsym(s_curl.handle, name)

	CSLUA_CURL_SYM(easy_init, "curl_easy_init");
	CSLUA_CURL_SYM(easy_setopt, "curl_easy_setopt");
	CSLUA_CURL_SYM(easy_perform, "curl_easy_perform");
	CSLUA_CURL_SYM(easy_getinfo, "curl_easy_getinfo");
	CSLUA_CURL_SYM(easy_cleanup, "curl_easy_cleanup");
	CSLUA_CURL_SYM(easy_strerror, "curl_easy_strerror");
	CSLUA_CURL_SYM(slist_append, "curl_slist_append");
	CSLUA_CURL_SYM(slist_free_all, "curl_slist_free_all");

	#undef CSLUA_CURL_SYM

	if (!s_curl.easy_init || !s_curl.easy_setopt || !s_curl.easy_perform ||
		!s_curl.easy_cleanup || !s_curl.slist_append || !s_curl.slist_free_all) {
		dlclose(s_curl.handle);
		s_curl.handle = NULL;
		return false;
	}

	return true;
}

size_t collect(char *data, size_t size, size_t count, void *user)
{
	std::string *body = (std::string *)user;
	size_t bytes = size * count;

	if (body->size() + bytes > MAX_BODY)
		return 0;			// non-matching count aborts the transfer

	body->append(data, bytes);
	return bytes;
}

} // namespace

static void perform(const Request &req, Response &res)
{
	res.ok = false;
	res.status = 0;

	if (!curl_ready()) {
		res.error = "libcurl is not installed on this server "
			"(apt install libcurl4 / yum install libcurl)";
		return;
	}

	CURL *curl = s_curl.easy_init();
	if (!curl) {
		res.error = "curl_easy_init failed";
		return;
	}

	struct curl_slist *headers = NULL;
	for (size_t i = 0; i < req.headers.size(); i++)
		headers = s_curl.slist_append(headers,
			(req.headers[i].name + ": " + req.headers[i].value).c_str());

	s_curl.easy_setopt(curl, OPT_URL, req.url.c_str());
	s_curl.easy_setopt(curl, OPT_WRITEFUNCTION, collect);
	s_curl.easy_setopt(curl, OPT_WRITEDATA, &res.body);
	s_curl.easy_setopt(curl, OPT_TIMEOUT_MS, (long)req.timeout_ms);
	s_curl.easy_setopt(curl, OPT_FOLLOWLOCATION, 1L);
	s_curl.easy_setopt(curl, OPT_MAXREDIRS, 5L);
	s_curl.easy_setopt(curl, OPT_USERAGENT, "cs-lua");
	// Without this libcurl installs signal handlers for its resolver timeouts,
	// which on a game server means fighting the engine over SIGALRM.
	s_curl.easy_setopt(curl, OPT_NOSIGNAL, 1L);

	if (req.method != "GET")
		s_curl.easy_setopt(curl, OPT_CUSTOMREQUEST, req.method.c_str());

	if (!req.body.empty()) {
		s_curl.easy_setopt(curl, OPT_POSTFIELDS, req.body.c_str());
		s_curl.easy_setopt(curl, OPT_POSTFIELDSIZE, (long)req.body.size());
	}

	if (headers)
		s_curl.easy_setopt(curl, OPT_HTTPHEADER, headers);

	int rc = s_curl.easy_perform(curl);

	if (rc != 0) {
		res.error = s_curl.easy_strerror
			? s_curl.easy_strerror(rc) : "the request failed";
	} else {
		long status = 0;
		if (s_curl.easy_getinfo)
			s_curl.easy_getinfo(curl, INFO_RESPONSE_CODE, &status);
		res.status = (int)status;
		res.ok = true;
	}

	if (headers)
		s_curl.slist_free_all(headers);
	s_curl.easy_cleanup(curl);
}

#endif

// ---------------------------------------------------------------------------
// Workers

static void worker_loop()
{
	for (;;) {
		Request req;

		{
			std::unique_lock<std::mutex> guard(s_pending_lock);
			s_wake.wait(guard, [] { return s_stopping || !s_pending.empty(); });

			if (s_stopping)
				return;

			req = s_pending.front();
			s_pending.pop_front();
		}

		Response res;
		res.id = req.id;
		res.plugin = req.plugin;
		res.callback = req.callback;
		res.generation = req.generation;

		perform(req, res);

		{
			std::lock_guard<std::mutex> guard(s_done_lock);
			s_done.push_back(res);
		}
	}
}

static void start_workers()
{
	if (!s_workers.empty())
		return;

	s_stopping = false;
	for (size_t i = 0; i < WORKER_COUNT; i++)
		s_workers.push_back(std::thread(worker_loop));
}

// ---------------------------------------------------------------------------
// Lua side

// Reads { headers = { ["X"] = "y" } } off an options table.
static void read_headers(lua_State *L, int index, std::vector<Header> &out)
{
	lua_getfield(L, index, "headers");
	if (!lua_istable(L, -1)) {
		lua_pop(L, 1);
		return;
	}

	lua_pushnil(L);
	while (lua_next(L, -2)) {
		if (lua_isstring(L, -2) && lua_isstring(L, -1)) {
			Header h;
			h.name = lua_tostring(L, -2);
			h.value = lua_tostring(L, -1);
			out.push_back(h);
		}
		lua_pop(L, 1);
	}

	lua_pop(L, 1);
}

// The one place a request is born, whatever the shorthand that led here.
static int queue_request(lua_State *L, const std::string &method,
	const std::string &url, const std::string &body, int opts_index, int callback_index)
{
	luaL_checktype(L, callback_index, LUA_TFUNCTION);

	Request req;
	req.id = s_next_id++;
	req.plugin = g_lua.current_index();
	req.generation = s_generation;
	req.method = method;
	req.url = url;
	req.body = body;
	req.timeout_ms = 10000;

	if (opts_index && lua_istable(L, opts_index)) {
		read_headers(L, opts_index, req.headers);

		lua_getfield(L, opts_index, "timeout");
		if (lua_isnumber(L, -1))
			req.timeout_ms = (int)(lua_tonumber(L, -1) * 1000.0);
		lua_pop(L, 1);
	}

	if (req.timeout_ms < 100)
		req.timeout_ms = 100;

	lua_pushvalue(L, callback_index);
	req.callback = luaL_ref(L, LUA_REGISTRYINDEX);

	start_workers();

	{
		std::lock_guard<std::mutex> guard(s_inflight_lock);
		s_inflight++;
	}
	{
		std::lock_guard<std::mutex> guard(s_pending_lock);
		s_pending.push_back(req);
	}
	s_wake.notify_one();

	lua_pushinteger(L, req.id);
	return 1;
}

// http.get(url[, opts], fn)
static int l_get(lua_State *L)
{
	const char *url = luaL_checkstring(L, 1);
	bool has_opts = lua_istable(L, 2);

	return queue_request(L, "GET", url, "", has_opts ? 2 : 0, has_opts ? 3 : 2);
}

// http.post(url, body[, opts], fn)
static int l_post(lua_State *L)
{
	const char *url = luaL_checkstring(L, 1);
	size_t len = 0;
	const char *body = luaL_checklstring(L, 2, &len);
	bool has_opts = lua_istable(L, 3);

	return queue_request(L, "POST", url, std::string(body, len),
		has_opts ? 3 : 0, has_opts ? 4 : 3);
}

// http.request({ url =, method =, body =, headers =, timeout = }, fn)
static int l_request(lua_State *L)
{
	luaL_checktype(L, 1, LUA_TTABLE);

	lua_getfield(L, 1, "url");
	if (!lua_isstring(L, -1))
		return luaL_error(L, "http.request: url is required");
	std::string url = lua_tostring(L, -1);
	lua_pop(L, 1);

	lua_getfield(L, 1, "method");
	std::string method = lua_isstring(L, -1) ? lua_tostring(L, -1) : "GET";
	lua_pop(L, 1);

	lua_getfield(L, 1, "body");
	std::string body;
	if (lua_isstring(L, -1)) {
		size_t len = 0;
		const char *s = lua_tolstring(L, -1, &len);
		body.assign(s, len);
	}
	lua_pop(L, 1);

	for (size_t i = 0; i < method.size(); i++)
		method[i] = (char)toupper((unsigned char)method[i]);

	return queue_request(L, method, url, body, 1, 2);
}

// http.cancel(id) - forget the callback. The request itself may already be on
// the wire and is left to finish; what this guarantees is that nothing of the
// plugin's runs when it does.
static int l_cancel(lua_State *L)
{
	int id = (int)luaL_checkinteger(L, 1);
	bool found = false;

	{
		std::lock_guard<std::mutex> guard(s_pending_lock);
		for (std::deque<Request>::iterator it = s_pending.begin();
			it != s_pending.end(); ++it) {
			if (it->id != id)
				continue;

			luaL_unref(L, LUA_REGISTRYINDEX, it->callback);
			s_pending.erase(it);
			found = true;
			break;
		}
	}

	if (found) {
		std::lock_guard<std::mutex> guard(s_inflight_lock);
		s_inflight--;
	} else {
		// Already running: mark the reply as unwanted so the drain drops it.
		std::lock_guard<std::mutex> guard(s_done_lock);
		for (size_t i = 0; i < s_done.size(); i++) {
			if (s_done[i].id == id) {
				s_done[i].callback = LUA_NOREF;
				found = true;
			}
		}
	}

	lua_pushboolean(L, found);
	return 1;
}

static const luaL_Reg s_http[] =
{
	{ "get",     l_get },
	{ "post",    l_post },
	{ "request", l_request },
	{ "cancel",  l_cancel },
	{ NULL, NULL }
};

void cslua_register_http(lua_State *L)
{
	cslua_register_namespace(L, "http", s_http);
}

// ---------------------------------------------------------------------------
// Draining, on the game thread

void cslua_http_run()
{
	lua_State *L = g_lua.state();
	if (!L)
		return;

	std::deque<Response> batch;
	{
		std::lock_guard<std::mutex> guard(s_done_lock);
		if (s_done.empty())
			return;
		batch.swap(s_done);
	}

	{
		std::lock_guard<std::mutex> guard(s_inflight_lock);
		s_inflight -= (int)batch.size();
		if (s_inflight < 0)
			s_inflight = 0;
	}

	for (size_t i = 0; i < batch.size(); i++) {
		Response &res = batch[i];

		// Cancelled, or its plugin went away while the request was on the wire.
		if (res.callback == LUA_NOREF)
			continue;

		// Queued by a Lua state that has since been torn down: the ref points
		// into a registry that no longer exists.
		if (res.generation != s_generation)
			continue;

		PluginScope scope(res.plugin);
		int errfunc = g_lua.push_errfunc();

		lua_rawgeti(L, LUA_REGISTRYINDEX, res.callback);

		// One table, the same shape every handler in this API gets.
		lua_newtable(L);

		lua_pushboolean(L, res.ok && res.status >= 200 && res.status < 400);
		lua_setfield(L, -2, "ok");

		lua_pushinteger(L, res.status);
		lua_setfield(L, -2, "status");

		lua_pushlstring(L, res.body.data(), res.body.size());
		lua_setfield(L, -2, "body");

		if (!res.error.empty()) {
			lua_pushstring(L, res.error.c_str());
			lua_setfield(L, -2, "error");
		} else if (res.ok && (res.status < 200 || res.status >= 400)) {
			// A 404 is not a transport failure, but it is not success either -
			// give res.error something to print so `if not res.ok` can just
			// report it without special-casing.
			lua_pushfstring(L, "HTTP %d", res.status);
			lua_setfield(L, -2, "error");
		}

		if (lua_pcall(L, 1, 0, errfunc) != 0)
			g_lua.report_error("http callback");

		lua_remove(L, errfunc);

		luaL_unref(L, LUA_REGISTRYINDEX, res.callback);
	}
}

void cslua_http_remove_plugin(int plugin_index)
{
	lua_State *L = g_lua.state();

	{
		std::lock_guard<std::mutex> guard(s_pending_lock);
		for (std::deque<Request>::iterator it = s_pending.begin();
			it != s_pending.end(); ) {
			if (it->plugin != plugin_index) {
				++it;
				continue;
			}
			if (L)
				luaL_unref(L, LUA_REGISTRYINDEX, it->callback);
			it = s_pending.erase(it);
		}
	}

	// Already on the wire: the worker still owns it, so only the reply is
	// disarmed.
	std::lock_guard<std::mutex> guard(s_done_lock);
	for (size_t i = 0; i < s_done.size(); i++) {
		if (s_done[i].plugin == plugin_index) {
			if (L)
				luaL_unref(L, LUA_REGISTRYINDEX, s_done[i].callback);
			s_done[i].callback = LUA_NOREF;
		}
	}
}

int cslua_http_pending()
{
	std::lock_guard<std::mutex> guard(s_inflight_lock);
	return s_inflight;
}

void cslua_http_reset()
{
	// Everything queued from now on belongs to the next state; everything
	// already out there is answered into the void.
	s_generation++;

	{
		std::lock_guard<std::mutex> guard(s_pending_lock);
		s_pending.clear();
	}
	{
		std::lock_guard<std::mutex> guard(s_done_lock);
		s_done.clear();
	}
	{
		std::lock_guard<std::mutex> guard(s_inflight_lock);
		s_inflight = 0;
	}

	// The registry refs are not unref'd: the state they belong to is being
	// closed, which frees the whole registry at once.
}

void cslua_http_shutdown()
{
	cslua_http_reset();

	{
		std::lock_guard<std::mutex> guard(s_pending_lock);
		s_stopping = true;
	}
	s_wake.notify_all();

	// Joined rather than detached: a worker writing into s_done after this
	// translation unit's statics are gone is a crash. The wait is bounded by
	// the request timeout, and the process is exiting anyway.
	for (size_t i = 0; i < s_workers.size(); i++) {
		if (s_workers[i].joinable())
			s_workers[i].join();
	}
	s_workers.clear();
}
