cmake_minimum_required(VERSION 3.20)

project(cslua CXX)

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 4)
	message(FATAL_ERROR "cs-lua must be built for x86. Configure with: cmake -S . -B build -A Win32")
endif()

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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

if(NOT EXISTS "${LUAJIT_ROOT}/lua51.lib")
	message(FATAL_ERROR "lua51.lib not found. Build LuaJIT first: open a x86 dev prompt, cd ${LUAJIT_ROOT} && msvcbuild.bat static")
endif()

add_library(lua_mm SHARED
	src/h_export.cpp
	src/meta_api.cpp
	src/dllapi.cpp
	src/commands.cpp
	src/players.cpp
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

set_target_properties(lua_mm PROPERTIES
	PREFIX ""
	OUTPUT_NAME "lua_mm"
	MSVC_RUNTIME_LIBRARY "MultiThreaded"
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

target_compile_definitions(lua_mm PRIVATE
	_CRT_SECURE_NO_WARNINGS
	WIN32_LEAN_AND_MEAN
	NOMINMAX
)

target_link_libraries(lua_mm PRIVATE "${LUAJIT_ROOT}/lua51.lib")

# GiveFnptrsToDll must be exported undecorated (stdcall), and metamod writes
# into the plugin's .data section, so it has to stay writable.
set_target_properties(lua_mm PROPERTIES
	LINK_FLAGS "/DEF:\"${CMAKE_SOURCE_DIR}/msvc/lua_mm.def\""
)

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
		COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:lua_mm>" "${SERVER_LUA_DIR}/lua_mm.dll"
		COMMAND "${CMAKE_COMMAND}" -E copy_directory "${CMAKE_SOURCE_DIR}/scripts/core" "${SERVER_LUA_DIR}/core"
		COMMAND "${CMAKE_COMMAND}" -E copy_directory "${CMAKE_SOURCE_DIR}/scripts/include" "${SERVER_LUA_DIR}/include"
		COMMENT "Installing lua_mm.dll and the core layer into ${SERVER_LUA_DIR}"
	)
endif()
