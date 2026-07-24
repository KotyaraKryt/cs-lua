cmake_minimum_required(VERSION 3.20)

# GoldSrc is 32-bit, and so is everything the module links against. On Linux
# that means -m32, and it has to be in place before project() runs: that call
# probes the compiler, and the probe is what decides CMAKE_SIZEOF_VOID_P.
# Hence the _INIT variables and CMAKE_HOST_UNIX - the plain UNIX variable does
# not exist yet at this point.
if(CMAKE_HOST_UNIX)
	set(CMAKE_CXX_FLAGS_INIT "-m32")
	set(CMAKE_SHARED_LINKER_FLAGS_INIT "-m32")
	set(CMAKE_EXE_LINKER_FLAGS_INIT "-m32")
endif()

project(cslua CXX)

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

if(NOT EXISTS "${HLSDK_ROOT}/dlls/regamedll_api.h")
	message(FATAL_ERROR "ReGameDLL SDK not found at ${HLSDK_ROOT}. See third_party/regamedll/README.md.")
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

add_library(lua_mm SHARED
	src/h_export.cpp
	src/meta_api.cpp
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
	src/regamedll.cpp
	src/rehlds.cpp
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

target_link_libraries(lua_mm PRIVATE "${LUAJIT_LIB}")

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
else()
	# metamod's own convention for a 32-bit Linux module: no "lib" prefix, an
	# _i386 suffix.
	set_target_properties(lua_mm PROPERTIES
		PREFIX ""
		OUTPUT_NAME "lua_mm_i386"
		POSITION_INDEPENDENT_CODE ON
	)

	target_compile_options(lua_mm PRIVATE
		-Wall
		-fno-strict-aliasing	# the SDK casts entvars around constantly
		# Only what the plugin API asks for leaves the module. Anything else
		# would be visible to the engine and to every other metamod plugin.
		-fvisibility=hidden
		-fvisibility-inlines-hidden
	)

	target_link_libraries(lua_mm PRIVATE dl)

	# LuaJIT on x86 needs its exported symbols reachable from the module's own
	# code, and static libgcc/libstdc++ keep the module working on servers with
	# an older toolchain than the one that built it.
	target_link_options(lua_mm PRIVATE
		-Wl,--no-undefined
		-static-libgcc
		-static-libstdc++
	)
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
		COMMENT "Installing $<TARGET_FILE_NAME:lua_mm> and the core layer into ${SERVER_LUA_DIR}"
	)
endif()
