#pragma once

#include <lua.hpp>

// Menus.
//
// The engine side is two halves: ShowMenu draws the panel, and the client
// answers with the "menuselect N" console command. This file owns both - the
// raw send and the interception - and hands the choice to Lua as an event.
// The friendly builder API lives in core/menu.lua on top of it.

void cslua_register_menu(lua_State *L);

// Called from the ClientCommand hook. Returns true when the command was a
// menuselect that a script is waiting for, so the game never sees it.
bool cslua_menu_handle_select(int id, const char *cmd);

// Drops whatever menu state a slot holds. Called when a client comes or goes:
// the next occupant must not inherit an open panel.
void cslua_menu_reset(int id);
