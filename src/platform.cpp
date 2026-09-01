// RTLD_NOLOAD is a GNU extension and the whole point of the module lookup
// below. g++ defines _GNU_SOURCE already; a bare #define would warn.
#if !defined(_WIN32) && !defined(_GNU_SOURCE)
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

// swds.dll = dedicated server; hw.dll / sw.dll = listen-server builds.
const char *const CSLUA_ENGINE_MODULES[] = { "swds.dll", "hw.dll", "sw.dll", NULL };

bool cslua_file_exists(const std::string &path)
{
	DWORD attr = GetFileAttributesA(path.c_str());
	return attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY);
}

bool cslua_make_dir(const std::string &path)
{
	if (CreateDirectoryA(path.c_str(), NULL))
		return true;

	return GetLastError() == ERROR_ALREADY_EXISTS;
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
	// GetModuleHandle takes no reference, so nothing to give back.
}

#else	// Linux

const char *const CSLUA_ENGINE_MODULES[] = {
	"engine_i486.so", "engine_i686.so", "engine_amd64.so", "engine.so", NULL
};

bool cslua_file_exists(const std::string &path)
{
	struct stat st;
	return stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode);
}

bool cslua_make_dir(const std::string &path)
{
	if (mkdir(path.c_str(), 0755) == 0)
		return true;

	struct stat st;
	return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
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

		// d_type is an optimisation, not a guarantee: DT_UNKNOWN needs a stat.
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

// dlopen(RTLD_NOLOAD) matches on the name a library was opened with, and the
// engine was opened by path. For a bare name, find what the process has mapped.
static std::string mapped_path_of(const char *basename)
{
	FILE *maps = fopen("/proc/self/maps", "r");
	if (!maps)
		return std::string();

	std::string found;
	char line[4096];
	size_t len = strlen(basename);

	while (fgets(line, sizeof line, maps)) {
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

	if (strchr(name, '/')) {
		void *handle = dlopen(name, RTLD_NOW | RTLD_NOLOAD);
		if (handle)
			return handle;
	}

	// metamod may hand a differently spelled path to the same file; fall back
	// to the basename and ask the process what it has.
	const char *slash = strrchr(name, '/');
	std::string path = mapped_path_of(slash ? slash + 1 : name);
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
	// RTLD_NOLOAD still takes a reference; give it back.
	if (handle)
		dlclose(handle);
}

#endif
