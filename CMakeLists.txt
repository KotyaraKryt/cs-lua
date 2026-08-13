cmake_minimum_required(VERSION 3.20)

# GoldSrc is 32-bit, and so is everything the module links against. On Linux
# that means -m32, and it has to be in place before project() runs: that call
# probes the compiler, and the probe is what decides CMAKE_SIZEOF_VOID_P.
# Hence the _INIT variables and CMAKE_HOST_UNIX - the plain UNIX variable does
# not exist yet at this point.
if(CMAKE_HOST_UNIX)
	set(CMAKE_C_FLAGS_INIT "-m32")
	set(CMAKE_CXX_FLAGS_INIT "-m32")
	set(CMAKE_SHARED_LINKER_FLAGS_INIT "-m32")
	set(CMAKE_EXE_LINKER_FLAGS_INIT "-m32")
endif()

# C as well as C++: the vendored SQLite amalgamation is C, and without the
# language enabled its -m32 would never be set - the build would only fail at
# link time, with a message about nothing in particular.
project(cslua C CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
	if(WIN32)
		message(FATAL_ERROR "cs-lua must be built for x86. Configure with: cmake -S . -B build -A Win32")
	else()
		message(FATAL_ERROR "cs-lua must be built for x86, and -m32 did not produce a 32-bit compiler. "
			"Install the multilib toolchain: gcc-multilib g++-multilib (Debian/Ubuntu) "
			"or glibc-devel.i686 libstdc++-devel.i686 (Fedora).")
	endif()
endif()

set(MM_ROOT     "${CMAKE_SOURCE_DIR}/third_party/metamod-r/metamod")
# One SDK for everything: the HLSDK metamod needs, the CS class hierarchy and
# the ReGameDLL API. Two copies of the SDK in one translation unit conflict,
# so this replaces metamod's include/ tree entirely. See the README next to it
# for where this copy comes from and why that particular one.
set(HLSDK_ROOT  "${CMAKE_SOURCE_DIR}/third_party/regamedll/cssdk")
set(LUAJIT_ROOT "${CMAKE_SOURCE_DIR}/third_party/luajit/src")
set(SQLITE_ROOT "${CMAKE_SOURCE_DIR}/third_party/sqlite")

if(NOT EXISTS "${HLSDK_ROOT}/dlls/regamedll_api.h")
	message(FATAL_ERROR "ReGameDLL SDK not found at ${HLSDK_ROOT}. See third_party/regamedll/README.md.")
endif()

if(NOT EXISTS "${SQLITE_ROOT}/sqlite3.c")
	message(FATAL_ERROR "SQLite amalgamation not found at ${SQLITE_ROOT}. See third_party/sqlite/README.md.")
endif()

if(WIN32)
	set(LUAJIT_LIB "${LUAJIT_ROOT}/lua51.lib")
	set(LUAJIT_HINT "open a x86 dev prompt, cd ${LUAJIT_ROOT} && msvcbuild.bat static")
else()
	set(LUAJIT_LIB "${LUAJIT_ROOT}/libluajit.a")
	# -fPIC is not optional: the default static objects are position-dependent,
	# and linking those into a shared object fails on relocations.
	set(LUAJIT_HINT "cd ${LUAJIT_ROOT} && make CC=\"gcc -m32\" BUILDMODE=static TARGET_CFLAGS=-fPIC -j")
endif()

if(NOT EXISTS "${LUAJIT_LIB}")
	message(FATAL_ERROR "LuaJIT not built: ${LUAJIT_LIB} is missing. Build it first: ${LUAJIT_HINT}")
endif()

# SQLite gets its own target rather than joining the source list below, for two
# reasons. The Linux branch of lua_mm defines _strdup=strdup and friends for the
# whole target (see further down) - those must not land on 250k lines of foreign
# C that defines the same names itself. And -Wall on a file we never fix is just
# a wall of noise hiding our own warnings.
add_library(sqlite3 STATIC "${SQLITE_ROOT}/sqlite3.c")
target_include_directories(sqlite3 PUBLIC "${SQLITE_ROOT}")

target_compile_definitions(sqlite3 PRIVATE
	# The module is single-threaded and the database is only ever touched from
	# the frame thread. This drops every mutex, and with it the pthread
	# dependency that -Wl,--no-undefined would otherwise refuse to leave open.
	SQLITE_THREADSAFE=0

	# SELECT load_extension('...') must not be a way to pull an arbitrary shared
	# library into the game server. Also removes the need for dlopen.
	SQLITE_OMIT_LOAD_EXTENSION=1

	# "x" is an identifier, not a string. SQLite's default is to fall back to a
	# string literal when the identifier does not resolve, which turns a typo in
	# a column name into a query that silently returns the wrong thing.
	SQLITE_DQS=0

	SQLITE_DEFAULT_MEMSTATUS=0
	SQLITE_OMIT_DEPRECATED
	SQLITE_OMIT_AUTOINIT
)

if(WIN32)
	# Must match lua_mm's own runtime, set further down; mixing a static and a
	# dynamic CRT in one module does not link.
	set_target_properties(sqlite3 PROPERTIES MSVC_RUNTIME_LIBRARY "MultiThreaded")
else()
	set_target_properties(sqlite3 PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()

add_library(lua_mm SHARED
	src/h_export.cpp
	src/meta_api.cpp
	src/cslua_bootstrap.cpp
	src/dllapi.cpp
	src/commands.cpp
	src/players.cpp
	src/platform.cpp
	src/lua_engine.cpp
	src/lua_events.cpp
	src/lua_natives.cpp
	src/lua_player.cpp
	src/lua_message.cpp
	src/lua_timers.cpp
	src/lua_cvar.cpp
	src/lua_command.cpp
	src/lua_sound.cpp
	src/lua_menu.cpp
	src/lua_entity.cpp
	src/lua_fx.cpp
	src/lua_db.cpp
	src/lua_http.cpp
	src/lua_httpserver.cpp
	src/regamedll.cpp
	src/rehlds.cpp
	src/cslua_corpse.cpp
)

target_include_directories(lua_mm PRIVATE
	src
	"${HLSDK_ROOT}/common"
	"${HLSDK_ROOT}/dlls"
	"${HLSDK_ROOT}/engine"
	"${HLSDK_ROOT}/game_shared"
	"${HLSDK_ROOT}/pm_shared"
	"${HLSDK_ROOT}/public"
	# Just the seven plugin-facing metamod headers. Pointing at metamod/src
	# instead would shadow cssdk headers with metamod's internal ones.
	"${MM_ROOT}/extra/example/include/metamod"
	"${LUAJIT_ROOT}"
)

target_link_libraries(lua_mm PRIVATE "${LUAJIT_LIB}" sqlite3)

if(WIN32)
	# lua_mm.dll next to metamod's own plugins.
	set_target_properties(lua_mm PROPERTIES
		PREFIX ""
		OUTPUT_NAME "lua_mm"
		MSVC_RUNTIME_LIBRARY "MultiThreaded"
		# GiveFnptrsToDll must be exported undecorated (stdcall), and metamod
		# writes into the plugin's .data section, so it stays writable.
		LINK_FLAGS "/DEF:\"${CMAKE_SOURCE_DIR}/msvc/lua_mm.def\""
	)

	target_compile_definitions(lua_mm PRIVATE
		_CRT_SECURE_NO_WARNINGS
		WIN32_LEAN_AND_MEAN
		NOMINMAX
	)

	# HTTP goes through WinHTTP: it ships with Windows and does TLS through
	# Schannel, so the module stays one file with no OpenSSL beside it.
	# ws2_32 is the inbound side (http_server): plain sockets, no TLS - an
	# admin panel is meant to sit behind a tunnel or a LAN, not the open net.
	target_link_libraries(lua_mm PRIVATE winhttp ws2_32)
else()
	# metamod's own convention for a 32-bit Linux module: no "lib" prefix, an
	# _i386 suffix.
	set_target_properties(lua_mm PROPERTIES
		PREFIX ""
		OUTPUT_NAME "lua_mm_i386"
		POSITION_INDEPENDENT_CODE ON
	)

	# Workers need pthreads; libcurl is opened at run time, so it is dl that
	# has to be here and not curl itself - no -dev package to install, and a
	# server without libcurl still loads the module.
	find_package(Threads REQUIRED)
	target_link_libraries(lua_mm PRIVATE Threads::Threads ${CMAKE_DL_LIBS})

	# The vendored SDK headers call a few MSVC names outright - dbg.h uses
	# _vsnprintf, static_map.h uses _stricmp. The HL codebase maps them exactly
	# like this on Linux. Doing it here rather than editing the copy keeps
	# third_party/regamedll a clean import, and -D lands before any include,
	# which a header of our own could not.
	#
	# _strupr and _strlwr are deliberately absent: strtools.h defines those as
	# real inline functions on Linux, and a macro would break them.
	target_compile_definitions(lua_mm PRIVATE
		_vsnprintf=vsnprintf
		_snprintf=snprintf
		_strdup=strdup
		_stricmp=strcasecmp
		_strnicmp=strncasecmp
	)

	target_compile_options(lua_mm PRIVATE
		-Wall
		# Both of these fire only inside the SDK's MSVC-era API: it carries
		# #pragma warning() everywhere, and its function signatures take char*
		# where a string literal is the only sane argument. Left on, they bury
		# the warnings that are actually ours.
		-Wno-unknown-pragmas
		-Wno-write-strings
		-fno-strict-aliasing	# the SDK casts entvars around constantly
		# Only what the plugin API asks for leaves the module. Anything else
		# would be visible to the engine and to every other metamod plugin.
		-fvisibility=hidden
		-fvisibility-inlines-hidden
	)

	target_link_libraries(lua_mm PRIVATE dl)

	# The version script is what actually keeps LuaJIT's ~450 symbols out of the
	# dynamic table; -fvisibility=hidden cannot, it does not reach into a static
	# library. See the script for why that matters. Static libgcc/libstdc++ keep
	# the module loadable on servers older than the box that built it.
	target_link_options(lua_mm PRIVATE
		"-Wl,--version-script=${CMAKE_SOURCE_DIR}/linux/lua_mm.lds"
		-Wl,--no-undefined
		-static-libgcc
		-static-libstdc++
	)
	set_property(TARGET lua_mm APPEND PROPERTY
		LINK_DEPENDS "${CMAKE_SOURCE_DIR}/linux/lua_mm.lds")
endif()

# Drop the build straight into a test server, so a rebuild is the whole
# deploy step. Point CSLUA_SERVER_DIR at your own <gamedir> to use a server
# living somewhere else; the default is the one next to the sources, which is
# gitignored (see the README on setting it up).
set(CSLUA_SERVER_DIR "${CMAKE_SOURCE_DIR}/server/cstrike"
	CACHE PATH "Game directory to install into; empty disables the copy")

if(CSLUA_SERVER_DIR)
	set(SERVER_LUA_DIR "${CSLUA_SERVER_DIR}/addons/lua")

	# scripts/ is copied too: keeping a second hand-maintained copy of the core
	# layer under the server was the surest way to have the two drift apart.
	# Plugins are deliberately not touched - they are not ours to overwrite.
	add_custom_command(TARGET lua_mm POST_BUILD
		COMMAND "${CMAKE_COMMAND}" -E make_directory "${SERVER_LUA_DIR}"
		COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:lua_mm>" "${SERVER_LUA_DIR}/$<TARGET_FILE_NAME:lua_mm>"
		COMMAND "${CMAKE_COMMAND}" -E copy_directory "${CMAKE_SOURCE_DIR}/scripts/core" "${SERVER_LUA_DIR}/core"
		COMMAND "${CMAKE_COMMAND}" -E copy_directory "${CMAKE_SOURCE_DIR}/scripts/include" "${SERVER_LUA_DIR}/include"
		# Порядок загрузки правит владелец сервера, поэтому существующий файл
		# не затирается - копируется только если его там ещё нет.
		COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${CMAKE_SOURCE_DIR}/scripts/load_order.txt" "${SERVER_LUA_DIR}/load_order.txt.example"
		COMMENT "Installing $<TARGET_FILE_NAME:lua_mm> and the core layer into ${SERVER_LUA_DIR}"
	)
endif()
