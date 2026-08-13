#pragma once

// Fetches addons/lua/{core,include,data} from the project's latest tagged
// GitHub release the first time the module runs somewhere that only has the
// binary - so dropping lua_mm.dll/lua_mm_i386.so next to metamod is enough
// to get a working server, instead of also hand-copying the Lua runtime
// layer out of the repo.
//
// A no-op the moment core/access.lua exists: this never overwrites anything,
// so a server owner's own edits to core/ are never at risk from a module
// update, and a server with no internet access just keeps whatever is
// already on disk.
void cslua_bootstrap_if_missing();
