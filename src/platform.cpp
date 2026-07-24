#ifndef _WIN32
	// RTLD_NOLOAD is a GNU extension, and it is the whole point of the module
	// lookup below: we want a handle to what is already there, never a load.
	#define _GNU_SOURCE
#endif

#include "platform.h"

#include <string.h>

#ifdef _WIN32
	#include <windows.h>
#else
	#include <dirent.h>
	#include <dlfcn.h>
	#include <stdio.h>
	#include <sys/stat.h>
#endif

#ifdef _WIN32

// The dedicated server keeps the engine in swds.dll; the listen-server builds
// keep it in hw.dll (hardware) or sw.dll (software renderer).
const char *const CSLUA_ENGINE_MODULES[] = { "swds.dll", "hw.dll", "sw.dll", NULL };

bool cslua_file_exists(const std::string &path)
{
	DWORD attr = GetFileAttributesA(path.c_str());
	return attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY);
}

bool cslua_list_dir(const std::string &path, std::vector<CsLuaDirEntry> &out)
{
	WIN32_FIND_DATAA fd;
	HANDLE h = FindFirstFileA((path + "/*").c_str(), &fd);
	if (h == INVALID_HANDLE_VALUE)
		return false;

	do {
		if (!strcmp(fd.cFileName, ".") || !strcmp(fd.cFileName, ".."))
			continue;

		CsLuaDirEntry entry;
		entry.name = fd.cFileName;
		entry.is_dir = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
		out.push_back(entry);
	} while (FindNextFileA(h, &fd));

	FindClose(h);
	return true;
}

void *cslua_module_open(const char *name)
{
	return GetModuleHandleA(name);
}

void *cslua_module_symbol(void *handle, const char *symbol)
{
	return handle ? (void *)GetProcAddress((HMODULE)handle, symbol) : NULL;
}

void cslua_module_close(void *)
{
	// GetModuleHandle does not take a reference, so there is nothing to give
	// back. Kept so callers read the same on both platforms.
}

#else	// Linux

// ReHLDS ships engine_i486.so; stock HLDS has carried a few other names over
// the years, and 64-bit builds use the amd64 one.
const char *const CSLUA_ENGINE_MODULES[] = {
	"engine_i486.so", "engine_i686.so", "engine_amd64.so", "engine.so", NULL
};

bool cslua_file_exists(const std::string &path)
{
	struct stat st;
	return stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode);
}

bool cslua_list_dir(const std::string &path, std::vector<CsLuaDirEntry> &out)
{
	DIR *dir = opendir(path.c_str());
	if (!dir)
		return false;

	while (struct dirent *e = readdir(dir)) {
		if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, ".."))
			continue;

		CsLuaDirEntry entry;
		entry.name = e->d_name;

		// d_type is an optimisation, not a guarantee: some filesystems answer
		// DT_UNKNOWN and the only way left is to ask the file itself.
		if (e->d_type == DT_DIR) {
			entry.is_dir = true;
		} else if (e->d_type == DT_UNKNOWN) {
			struct stat st;
			entry.is_dir = stat((path + "/" + entry.name).c_str(), &st) == 0
				&& S_ISDIR(st.st_mode);
		} else {
			entry.is_dir = false;
		}

		out.push_back(entry);
	}

	closedir(dir);
	return true;
}

// dlopen(RTLD_NOLOAD) matches on the name the library was opened with, and the
// engine was opened by path, not by basename. So for a bare name we look up
// what the process actually has mapped and hand dlopen the full path it knows.
static std::string mapped_path_of(const char *basename)
{
	FILE *maps = fopen("/proc/self/maps", "r");
	if (!maps)
		return std::string();

	std::string found;
	char line[4096];
	size_t len = strlen(basename);

	while (fgets(line, sizeof line, maps)) {
		// Every mapping backed by a file ends in its absolute path.
		char *slash = strrchr(line, '/');
		if (!slash)
			continue;

		char *name = slash + 1;
		char *end = name + strcspn(name, " \t\r\n");
		if ((size_t)(end - name) != len || strncmp(name, basename, len))
			continue;

		char *start = strchr(line, '/');
		*end = '\0';
		found = start;
		break;
	}

	fclose(maps);
	return found;
}

void *cslua_module_open(const char *name)
{
	if (!name || !*name)
		return NULL;

	// A path we were handed (metamod knows exactly which file it loaded as the
	// game DLL) needs no searching.
	if (strchr(name, '/')) {
		void *handle = dlopen(name, RTLD_NOW | RTLD_NOLOAD);
		if (handle)
			return handle;
	}

	std::string path = mapped_path_of(name);
	if (path.empty())
		return NULL;

	return dlopen(path.c_str(), RTLD_NOW | RTLD_NOLOAD);
}

void *cslua_module_symbol(void *handle, const char *symbol)
{
	return handle ? dlsym(handle, symbol) : NULL;
}

void cslua_module_close(void *handle)
{
	// RTLD_NOLOAD still takes a reference; give it back. Whoever loaded the
	// module for real keeps it alive, so symbols stay valid.
	if (handle)
		dlclose(handle);
}

#endif
