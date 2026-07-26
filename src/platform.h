#pragma once

// snprintf and friends live here, and the aliases below are useless without
// their declarations.
#include <stdio.h>
#include <string.h>

#include <string>
#include <vector>

// The three things this module does that the OS spells differently: listing a
// directory, asking whether a file is there, and reaching into a module that is
// already loaded into the process. Everything else is portable as written.

// MSVC spells these with a leading underscore. The C99 versions always
// terminate, which _snprintf does not - every call site here terminates by
// hand anyway, so the two behave identically for us.
#ifdef _WIN32
	#define cslua_snprintf   _snprintf
	#define cslua_vsnprintf  _vsnprintf
	#define cslua_strdup     _strdup
#else
	#define cslua_snprintf   snprintf
	#define cslua_vsnprintf  vsnprintf
	#define cslua_strdup     strdup
#endif

struct CsLuaDirEntry
{
	std::string name;
	bool is_dir;
};

// True only for a regular file; a directory is not a file here.
bool cslua_file_exists(const std::string &path);

// Creates one directory level. A directory that already exists counts as
// success - the callers here want the path to be there, not to have made it.
bool cslua_make_dir(const std::string &path);

// Every entry of a directory, "." and ".." left out. False when the directory
// cannot be opened - which is not an error in itself, an optional folder that
// is simply absent looks exactly the same.
bool cslua_list_dir(const std::string &path, std::vector<CsLuaDirEntry> &out);

// Handle to a module that is ALREADY in the process, NULL when it is not.
// Never loads anything: the engine and the game DLL are both loaded long
// before us, and pulling in a second copy would be a disaster.
//
// `name` may be a full path or a bare file name. On Linux a bare name is
// resolved against the process's own mappings first, because the loader
// remembers the path the engine was opened with, not its basename.
void *cslua_module_open(const char *name);
void *cslua_module_symbol(void *handle, const char *symbol);
void cslua_module_close(void *handle);

// What the engine module is called on this platform, NULL-terminated list, in
// the order worth trying.
extern const char *const CSLUA_ENGINE_MODULES[];
