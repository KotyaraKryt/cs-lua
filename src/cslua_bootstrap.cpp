#include "cslua.h"
#include "cslua_bootstrap.h"
#include "lua_http.h"
#include "platform.h"

#include <stdio.h>
#include <utility>
#include <vector>

namespace {

const char *const REPO = "KotyaraKryt/cs-lua";

// core/, include/ and data/ have never had subfolders, so a flat, hand-kept
// list is simpler than teaching this step to walk GitHub's directory-listing
// JSON for a tree that will not grow deep. Add a line here when a file is
// added to one of these three folders.
struct BootstrapFile
{
	const char *dir;
	const char *name;
};

const BootstrapFile FILES[] =
{
	{ "core", "access.lua" },
	{ "core", "commands.lua" },
	{ "core", "exports.lua" },
	{ "core", "hook.lua" },
	{ "core", "i18n.lua" },
	{ "core", "perms.lua" },
	{ "core", "ui.lua" },
	{ "include", "class.lua" },
	{ "include", "color.lua" },
	{ "include", "datafile.lua" },
	{ "include", "json.lua" },
	{ "include", "store.lua" },
	{ "include", "text.lua" },
	{ "data", "groups.lua" },
	{ "data", "users.lua" },
};

// Pulls "tag_name":"v1.0.0" out of the /releases/latest response by hand.
// The module has no other use for a JSON parser anywhere in its own C++, and
// one field of one response at startup does not earn it one.
bool extract_tag(const std::string &json, std::string &tag)
{
	const std::string key = "\"tag_name\"";

	size_t pos = json.find(key);
	if (pos == std::string::npos)
		return false;

	pos = json.find('"', pos + key.size());
	if (pos == std::string::npos)
		return false;
	pos++;

	size_t end = json.find('"', pos);
	if (end == std::string::npos)
		return false;

	tag = json.substr(pos, end - pos);
	return !tag.empty();
}

bool write_file(const std::string &path, const std::string &body)
{
	FILE *fh = fopen(path.c_str(), "wb");
	if (!fh)
		return false;

	bool ok = body.empty() || fwrite(body.data(), 1, body.size(), fh) == body.size();
	fclose(fh);
	return ok;
}

} // namespace

void cslua_bootstrap_if_missing()
{
	// access.lua is the sentinel: if it is there, the runtime layer counts as
	// installed and this never runs again.
	std::string sentinel = cslua_base_dir() + "/core/access.lua";
	if (cslua_file_exists(sentinel))
		return;

	cslua_print("addons/lua/core is missing - fetching the runtime layer from "
		"github.com/%s (latest release)...", REPO);

	std::vector<std::pair<std::string, std::string> > api_headers;
	api_headers.push_back(std::make_pair("Accept", "application/vnd.github+json"));

	std::string body, error;
	int status = 0;
	std::string api_url = "https://api.github.com/repos/" + std::string(REPO) + "/releases/latest";

	if (!cslua_http_get_sync(api_url, api_headers, 10000, body, status, error) || status != 200) {
		cslua_error("bootstrap: could not reach github.com (%s) - copy scripts/core, "
			"scripts/include and scripts/data from the repo into addons/lua by hand",
			!error.empty() ? error.c_str() : "request failed");
		return;
	}

	std::string tag;
	if (!extract_tag(body, tag)) {
		cslua_error("bootstrap: could not find a release tag in github's response");
		return;
	}

	cslua_make_dir(cslua_base_dir());
	cslua_make_dir(cslua_base_dir() + "/core");
	cslua_make_dir(cslua_base_dir() + "/include");
	cslua_make_dir(cslua_base_dir() + "/data");

	int done = 0, failed = 0;
	std::vector<std::pair<std::string, std::string> > no_headers;

	for (size_t i = 0; i < sizeof(FILES) / sizeof(FILES[0]); i++) {
		const BootstrapFile &f = FILES[i];

		std::string raw_url = "https://raw.githubusercontent.com/" + std::string(REPO) +
			"/" + tag + "/scripts/" + f.dir + "/" + f.name;

		std::string file_body, file_error;
		int file_status = 0;

		if (!cslua_http_get_sync(raw_url, no_headers, 10000, file_body, file_status, file_error) ||
				file_status != 200) {
			cslua_error("bootstrap: could not fetch %s/%s (%s)", f.dir, f.name,
				!file_error.empty() ? file_error.c_str() : "not found");
			failed++;
			continue;
		}

		std::string path = cslua_base_dir() + "/" + f.dir + "/" + f.name;
		if (write_file(path, file_body)) {
			done++;
		} else {
			cslua_error("bootstrap: could not write %s", path.c_str());
			failed++;
		}
	}

	// data/ is seed content, not a hard requirement - access.lua treats a
	// missing groups.lua/users.lua as "nobody has rights yet", not an error.
	// core/ and include/ are not: a plugin whose first line is `require`ing
	// something from include/ dies immediately if this silently left it out.
	if (failed == 0)
		cslua_print("bootstrap: installed %d file(s) from release %s", done, tag.c_str());
	else
		cslua_error("bootstrap: installed %d file(s), %d failed - "
			"the server will likely not start cleanly until that is fixed by hand", done, failed);
}
