#pragma once

#include <stdio.h>
#include <string.h>

#include <string>
#include <vector>

// MSVC spells these with a leading underscore. The C99 versions always
// terminate; every call site here terminates by hand anyway.
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

// True only for a regular file, not a directory.
bool cslua_file_exists(const std::string &path);

// Creates one directory level. An existing directory counts as success.
bool cslua_make_dir(const std::string &path);

// Every entry of a directory, "." and ".." left out. False when it cannot be
// opened - which also covers an optional folder that is simply absent.
bool cslua_list_dir(const std::string &path, std::vector<CsLuaDirEntry> &out);

// Handle to a module ALREADY in the process, NULL when it is not - never loads.
// `name` may be a full path or a bare file name; on Linux a bare name is
// resolved against the process's own mappings first.
void *cslua_module_open(const char *name);
void *cslua_module_symbol(void *handle, const char *symbol);
void cslua_module_close(void *handle);

extern const char *const CSLUA_ENGINE_MODULES[];
