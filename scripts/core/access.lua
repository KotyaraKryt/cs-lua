-- Core: rights. A permission is a named node, so a plugin declares as many as
-- it needs without coordinating with anyone.
--
--   p:can("shop.vip.buy")                       -- checked anywhere
--   cmd.add("kick", fn, { perm = "admin.kick" }) -- checked for you
--
-- A node is a dotted name: "admin.kick", "shop.vip.buy". Grants may use a
-- wildcard tail ("shop.vip.*", "*") and a leading "-" for an explicit denial
-- ("-admin.ban"). Rights come from groups (data/groups.lua) and from personal
-- entries (data/users.lua); a player may hold both.
--
-- Which one wins is decided, not left to file order. Every grant that matches
-- the node gets compared on, in order:
--
--   1. source     personal entry (3) > own group (2) > inherited group (1)
--                 > a permission's own `default` (0)
--   2. weight     the heavier group wins among groups of the same source
--   3. precision  "shop.vip.buy" beats "shop.vip.*" beats "shop.*" beats "*"
--   4. denial     on a full tie, the deny wins
--
-- Console and rcon have no player object and are never checked: whoever holds
-- the server console already owns the machine.

local datafile = require("datafile")

access = {}

-- Everyone is in this group, including players with no entry at all.
local BASE_GROUP = "default"

local groups = {}		-- name -> { weight, inherit = {...}, allow, deny, where }
local users  = {}		-- key  -> { name, password, groups, allow, deny, until_, where }
local perms  = {}		-- node -> { desc, default, plugin }
local rules  = {}		-- node -> function(p, ctx)

-- Per slot: the flattened grant list, whom it was built for, and when it stops
-- being true. Rebuilt lazily, never on a timer.
local cache = {}

-- Bumped by anything that can change the answer, so live caches fall out.
local generation = 0

access.config = {
	-- Answer to a player who lacks the right. nil to stay silent.
	denied = "{green}[Access]{default} You are not allowed to do that",
	-- Answer when the target outranks the caller.
	immune = "{green}[Access]{default} %s outranks you",
	-- Warn in the console when code checks a node nobody declared. Catches
	-- typos; turn it off if you do not use access.declare().
	warn_unknown = true,
}

--------------------------------------------------------------------------
-- nodes
--------------------------------------------------------------------------

local function segments(node)
	local n = 1
	for _ in node:gmatch("%.") do
		n = n + 1
	end
	return n
end

-- How specific a grant is, as a number we can compare. Exact matches sort
-- above a wildcard of the same depth: "a.b" (5) > "a.b.*" (4) > "a.*" (2).
local function precision(node)
	if node == "*" then
		return 0
	end
	local base = node:match("^(.*)%.%*$")
	if base then
		return segments(base) * 2
	end
	return segments(node) * 2 + 1
end

local function matches(grant, node)
	if grant == "*" or grant == node then
		return true
	end
	local base = grant:match("^(.*)%.%*$")
	if not base then
		return false
	end
	return node == base or node:sub(1, #base + 1) == base .. "."
end

--------------------------------------------------------------------------
-- time and context
--------------------------------------------------------------------------

-- until_ accepts a unix timestamp, "30d" / "12h" / "45m", or "map" for
-- "until this map ends". Returns the timestamp, or nil for "no limit", plus
-- true when it is map-scoped.
local UNITS = { s = 1, m = 60, h = 3600, d = 86400, w = 604800 }

local function deadline(value)
	if value == nil then
		return nil, false
	end
	if value == "map" then
		return nil, true
	end
	if type(value) == "number" then
		return value, false
	end

	local amount, unit = tostring(value):match("^(%d+)%s*([smhdw])$")
	if amount then
		return os.time() + tonumber(amount) * UNITS[unit], false
	end
	return nil, false
end

-- access.expand("30d") -> a timestamp callers can store in a data file.
function access.expand(value)
	local at, map_scoped = deadline(value)
	return map_scoped and "map" or at
end

local function expired(entry)
	local at = entry.until_
	if at == nil then
		return false
	end
	if at == "map" then
		return entry.map ~= nil and entry.map ~= sv.map()
	end
	return type(at) == "number" and at <= os.time()
end

-- Each group a player holds is its own record ({ name, until_, map }), not
-- just a name in a flat list - see normalize_membership() below. Expiry is
-- checked per record, independent of the entry's own until_/map (which now
-- only gates the entry's personal allow/deny). One key holding several
-- groups from several sources (GameCMS granting one service per row, say)
-- used to share a single until_ on the whole entry instead - whichever
-- grant call ran last silently capped every other group the player held,
-- permanent ones included.
local function group_record_expired(rec)
	local at = rec.until_
	if at == nil then
		return false
	end
	if at == "map" then
		return rec.map ~= nil and rec.map ~= sv.map()
	end
	return type(at) == "number" and at <= os.time()
end

-- where = { map = "de_dust2" } or { maps = { ... } }. Missing means "always".
local function in_context(where)
	if not where then
		return true
	end
	local here = sv.map()
	if where.map and where.map ~= here then
		return false
	end
	if where.maps then
		local found = false
		for _, m in ipairs(where.maps) do
			if m == here then
				found = true
				break
			end
		end
		if not found then
			return false
		end
	end
	return true
end

--------------------------------------------------------------------------
-- data
--------------------------------------------------------------------------

local function as_list(value)
	if value == nil then
		return {}
	end
	if type(value) == "string" then
		return { value }
	end
	return value
end

-- A user's `groups` field is a mix of plain names ("vip", meaning forever)
-- and tables ({ name = "filantrop", until_ = 1757337413 }, meaning until
-- then) - written that way in data/users.lua, or built up the same way at
-- runtime by access.grant(). Normalized here into one shape so nothing else
-- has to handle both.
local function normalize_membership(raw)
	local out = {}
	for _, g in ipairs(as_list(raw)) do
		if type(g) == "table" then
			out[#out + 1] = {
				name   = tostring(g.name):lower(),
				until_ = g.until_,
				map    = g.map,
			}
		else
			out[#out + 1] = { name = tostring(g):lower() }
		end
	end
	return out
end

-- The inverse, for access.save(): a record with no expiry writes back as
-- just its name (matches how a permanent grant reads by hand), one with an
-- until_ keeps the table shape.
local function serialize_membership(list)
	local out = {}
	for _, rec in ipairs(list) do
		if rec.until_ == nil then
			out[#out + 1] = rec.name
		else
			out[#out + 1] = { name = rec.name, until_ = rec.until_, map = rec.map }
		end
	end
	return out
end

local function find_membership(list, name)
	for i, rec in ipairs(list) do
		if rec.name == name then
			return i
		end
	end
	return nil
end

-- One group, with defaults filled in, so nothing downstream has to test for
-- missing fields.
local function normalize_group(name, raw)
	return {
		name    = name,
		weight  = tonumber(raw.weight) or 0,
		inherit = as_list(raw.inherit),
		allow   = as_list(raw.allow),
		deny    = as_list(raw.deny),
		where   = raw.where,
	}
end

local DEFAULT_GROUPS = {
	[BASE_GROUP] = { weight = 0 },
	admin = { weight = 50, inherit = BASE_GROUP, allow = { "admin.*" } },
	owner = { weight = 100, inherit = "admin", allow = { "*" } },
}

function access.reload()
	local raw_groups, gerr = datafile.load("groups", DEFAULT_GROUPS)
	local raw_users, uerr = datafile.load("users", {})

	-- A broken file drops that half of the data instead of half-loading it:
	-- running with a partial admin list is worse than running with none.
	if gerr then
		print("[access] groups.lua: " .. tostring(gerr))
		raw_groups = DEFAULT_GROUPS
	end
	if uerr then
		print("[access] users.lua: " .. tostring(uerr))
		raw_users = {}
	end

	groups = {}
	for name, raw in pairs(raw_groups) do
		groups[name:lower()] = normalize_group(name:lower(), raw)
	end
	if not groups[BASE_GROUP] then
		groups[BASE_GROUP] = normalize_group(BASE_GROUP, { weight = 0 })
	end

	users = {}
	local dropped = 0
	local pruned = 0
	for key, raw in pairs(raw_users) do
		local entry = {
			name     = raw.name,
			password = raw.password,
			groups   = normalize_membership(raw.groups),
			allow    = as_list(raw.allow),
			deny     = as_list(raw.deny),
			until_   = raw.until_,
			map      = raw.map,
			where    = raw.where,
		}

		-- Each group is pruned on its own schedule now, not the whole entry
		-- at once - one still-valid group must not vanish just because
		-- another one on the same key ran out.
		local kept = {}
		for _, rec in ipairs(entry.groups) do
			if group_record_expired(rec) then
				pruned = pruned + 1
			else
				kept[#kept + 1] = rec
			end
		end
		entry.groups = kept

		-- The entry's own until_/map only ever covered its personal
		-- allow/deny - clear those once expired, same as before, but no
		-- longer as a reason to drop groups that are still good.
		if expired(entry) then
			entry.allow = {}
			entry.deny = {}
			entry.until_ = nil
			entry.map = nil
		end

		if #entry.groups == 0 and #entry.allow == 0 and #entry.deny == 0 then
			dropped = dropped + 1
		else
			users[key] = entry
		end
	end

	generation = generation + 1
	cache = {}

	if pruned > 0 then
		print(("[access] pruned %d expired group grant(s)"):format(pruned))
	end
	if dropped > 0 then
		print(("[access] dropped %d expired entr%s")
			:format(dropped, dropped == 1 and "y" or "ies"))
	end
	return true
end

-- Writes users back. Groups are hand-edited (comments and all), so they are
-- never rewritten from here.
function access.save()
	local out = {}
	for key, e in pairs(users) do
		out[key] = {
			name     = e.name,
			password = e.password,
			groups   = #e.groups > 0 and serialize_membership(e.groups) or nil,
			allow    = #e.allow > 0 and e.allow or nil,
			deny     = #e.deny > 0 and e.deny or nil,
			until_   = e.until_,
			map      = e.map,
			where    = e.where,
		}
	end

	local ok, err = datafile.save("users", out, "Access rights, keyed by steamid or name:<nick>.")
	if not ok then
		print("[access] could not save users.lua: " .. tostring(err))
	end
	return ok, err
end

--------------------------------------------------------------------------
-- declaring permissions
--------------------------------------------------------------------------

-- access.declare("shop.vip.buy", { desc = "...", default = "vip" })
--
-- Optional, but worth it: lua_perms list then shows every right on the server
-- with its owner, and a typo in p:can() gets a console warning instead of a
-- silent no.
function access.declare(node, opts)
	assert(type(node) == "string", "access.declare: node must be a string")
	assert(not node:find("%*"), "access.declare: declare concrete nodes, not wildcards")

	opts = opts or {}
	assert(type(opts) == "table", "access.declare: options must be a table")

	-- Every real access.declare now runs from a plugin's manifest.lua, so
	-- plugin.id() already knows the owner - opts.plugin only overrides it for
	-- the odd node core.lua declares on nobody's behalf.
	perms[node] = {
		desc    = opts.desc,
		default = opts.default,		-- group name, or true for everyone
		plugin  = opts.plugin or plugin.id(),
	}

	generation = generation + 1		-- a new default can widen existing rights
	return node
end

-- access.rule("shop.buy", function(p, ctx) return p:playtime() > 3600 end)
--
-- The escape hatch for rights no table can express. It is consulted only when
-- nothing granted or denied the node outright, so an explicit deny in the data
-- files always stands.
function access.rule(node, fn)
	assert(type(node) == "string", "access.rule: node must be a string")
	assert(fn == nil or type(fn) == "function", "access.rule: handler must be a function")
	rules[node] = fn
end

--------------------------------------------------------------------------
-- building a player's rights
--------------------------------------------------------------------------

local function add_grants(list, nodes, allow, source, weight)
	for _, node in ipairs(nodes) do
		local negated = node:sub(1, 1) == "-"
		if negated then
			node = node:sub(2)
		end
		list[#list + 1] = {
			node   = node,
			allow  = allow and not negated,
			source = source,
			weight = weight,
			prec   = precision(node),
		}
	end
end

-- Walks a group and everything it inherits. Own groups score higher than
-- inherited ones, and a cycle in inherit stops at the first repeat instead of
-- hanging the server.
local function collect_group(name, source, list, names, seen)
	name = tostring(name):lower()
	if seen[name] then
		return
	end
	seen[name] = true

	local g = groups[name]
	if not g or not in_context(g.where) then
		return
	end

	names[#names + 1] = name
	add_grants(list, g.allow, true, source, g.weight)
	add_grants(list, g.deny, false, source, g.weight)

	for _, parent in ipairs(g.inherit) do
		collect_group(parent, 1, list, names, seen)
	end

	return g.weight
end

-- Which entry in users.lua this player is. SteamID first; a nick+password
-- entry only counts once the password from the client's setinfo matches.
local function identify(p)
	local steamid = p:steamid()
	if users[steamid] then
		return steamid
	end

	local key = "name:" .. p:name()
	local entry = users[key]
	if entry then
		if not entry.password or entry.password == "" then
			return key		-- nick only: the server owner asked for it
		end
		if p:info("_pw") == entry.password then
			return key
		end
	end

	return nil
end

local function build(p)
	local list, names, seen = {}, {}, {}
	local weight = 0

	local base = collect_group(BASE_GROUP, 1, list, names, seen)
	weight = math.max(weight, base or 0)

	local key = identify(p)
	local entry = key and users[key]

	if entry and in_context(entry.where) then
		-- Each group checked against its own expiry, not the entry's -
		-- see group_record_expired().
		for _, rec in ipairs(entry.groups) do
			if not group_record_expired(rec) then
				local g = groups[rec.name]
				if g and in_context(g.where) then
					weight = math.max(weight, g.weight)
				end
				collect_group(rec.name, 2, list, names, seen)
			end
		end
		-- Personal nodes outrank every group the player is in. Still gated
		-- by the entry's own until_/map - that field is exactly what these
		-- (and only these) mean now.
		if not expired(entry) then
			add_grants(list, entry.allow, true, 3, weight)
			add_grants(list, entry.deny, false, 3, weight)
		end
	end

	-- Nodes declared with default = "vip" / true come in underneath
	-- everything, so a data file can always take them back.
	local in_group = {}
	for _, name in ipairs(names) do
		in_group[name] = true
	end
	for node, spec in pairs(perms) do
		if spec.default == true or (spec.default and in_group[tostring(spec.default):lower()]) then
			add_grants(list, { node }, true, 0, 0)
		end
	end

	-- Recheck at least this often, so a timed grant - the entry's own, or
	-- any one group's - stops mattering the moment it runs out and not at
	-- the next reconnect. The earliest of all of them, entry included.
	local expires = entry and type(entry.until_) == "number" and entry.until_ or nil
	if entry then
		for _, rec in ipairs(entry.groups) do
			if type(rec.until_) == "number" and (not expires or rec.until_ < expires) then
				expires = rec.until_
			end
		end
	end

	return {
		generation = generation,
		map        = sv.map(),
		key        = key,
		entry      = entry,
		grants     = list,
		groups     = names,
		weight     = weight,
		expires    = expires,
	}
end

local function rights(p)
	local slot = p.id
	local c = cache[slot]

	if c and (c.generation ~= generation or c.map ~= sv.map()
		or (c.expires and c.expires <= os.time())) then
		c = nil
	end

	if not c then
		c = build(p)
		cache[slot] = c
	end
	return c
end

--------------------------------------------------------------------------
-- checking
--------------------------------------------------------------------------

-- Later grant wins only if it is genuinely stronger; a tie goes to the denial.
local function stronger(a, b)
	if a.source ~= b.source then
		return a.source > b.source
	end
	if a.weight ~= b.weight then
		return a.weight > b.weight
	end
	if a.prec ~= b.prec then
		return a.prec > b.prec
	end
	return not a.allow and b.allow
end

local warned = {}

local function check(p, node)
	if access.config.warn_unknown and not perms[node] and not warned[node] then
		warned[node] = true
		print(("[access] nobody declared '%s' - typo, or a missing access.declare()?"):format(node))
	end

	local r = rights(p)

	local best
	for _, g in ipairs(r.grants) do
		if matches(g.node, node) and (not best or stronger(g, best)) then
			best = g
		end
	end

	if best then
		return best.allow
	end

	-- Nothing had an opinion: a dynamic rule gets the last word.
	local rule = rules[node]
	if rule then
		return rule(p, { node = node, map = sv.map() }) and true or false
	end

	return false
end

function access.can(p, node)
	-- No player means the server console or rcon, which is above all rights.
	if p == nil then
		return true
	end
	assert(type(node) == "string", "can: node must be a string")
	return check(p, node)
end

function access.invalidate(p)
	if p then
		cache[p.id] = nil
	else
		cache = {}
	end
end

function access.groups(p)
	if p == nil then
		return {}
	end
	return rights(p).groups
end

function access.weight(p)
	if p == nil then
		return math.huge		-- the console outranks everyone
	end
	return rights(p).weight
end

-- Acting on another player (kick, slay, mute) needs strictly more weight than
-- they have. Equal weight loses on purpose: two admins of the same rank cannot
-- fight each other.
function access.outranks(p, other)
	if p == nil then
		return true
	end
	if other == nil then
		return true
	end
	return access.weight(p) > access.weight(other)
end

--------------------------------------------------------------------------
-- editing rights at runtime
--------------------------------------------------------------------------

local function entry_for(key)
	local e = users[key]
	if not e then
		e = { groups = {}, allow = {}, deny = {} }
		users[key] = e
	end
	return e
end

local function remove_value(list, value)
	for i = #list, 1, -1 do
		if list[i] == value then
			table.remove(list, i)
		end
	end
end

-- access.grant("STEAM_0:1:1", { groups = "vip", until_ = "30d" })
-- access.grant("STEAM_0:1:1", { allow = { "shop.*" }, where = { map = "de_dust2" } })
--
-- until_ scopes to exactly what this call granted: each name in `groups`
-- gets its own expiry (a second call for a different group, even on the
-- same key, never touches the first one's), and `allow`/`deny` still share
-- the entry's own until_ same as always. A group not re-listed here keeps
-- whatever expiry it already had - passing groups without until_ does not
-- reset them to forever.
function access.grant(key, spec)
	assert(type(key) == "string", "access.grant: key must be a steamid or name:<nick>")
	assert(type(spec) == "table", "access.grant: expects a table")

	local e = entry_for(key)
	local granted_groups = as_list(spec.groups)
	local granted_allow = as_list(spec.allow)
	local granted_deny = as_list(spec.deny)

	local until_given = spec.until_ ~= nil
	local at, map_at
	if until_given then
		at = access.expand(spec.until_)
		map_at = at == "map" and sv.map() or nil
	end

	for _, name in ipairs(granted_groups) do
		name = tostring(name):lower()
		if not groups[name] then
			return false, "no such group: " .. name
		end
		local idx = find_membership(e.groups, name)
		local rec
		if until_given then
			rec = { name = name, until_ = at, map = map_at }
		elseif idx then
			rec = e.groups[idx]	-- re-granted with no until_: keep its expiry
		else
			rec = { name = name }	-- brand new, no until_ given: forever
		end
		if idx then
			table.remove(e.groups, idx)
		end
		e.groups[#e.groups + 1] = rec
	end

	for _, node in ipairs(granted_allow) do
		remove_value(e.allow, node)
		e.allow[#e.allow + 1] = node
	end
	for _, node in ipairs(granted_deny) do
		remove_value(e.deny, node)
		e.deny[#e.deny + 1] = node
	end

	if spec.name then
		e.name = spec.name
	end
	if spec.password then
		e.password = spec.password
	end
	if spec.where ~= nil then
		e.where = spec.where
	end

	-- The entry's own until_ (personal allow/deny) is set whenever this
	-- call actually touched allow/deny, or touched nothing group-shaped at
	-- all - the plain "just set it" case a call with no `groups` is.
	if until_given and (#granted_allow > 0 or #granted_deny > 0 or #granted_groups == 0) then
		e.until_ = at
		e.map = map_at
	end

	generation = generation + 1
	return true
end

-- access.revoke(key)                 -- the whole entry
-- access.revoke(key, "vip")          -- one group
-- access.revoke(key, nil, "shop.*")  -- one personal node
function access.revoke(key, group, node)
	local e = users[key]
	if not e then
		return false, "no entry for " .. key
	end

	if group == nil and node == nil then
		users[key] = nil
	end
	if group then
		local idx = find_membership(e.groups, tostring(group):lower())
		if idx then
			table.remove(e.groups, idx)
		end
	end
	if node then
		remove_value(e.allow, node)
		remove_value(e.deny, node)
	end

	generation = generation + 1
	return true
end

-- access.copy(from_key, to_key) - wipes to_key's entry and replaces it with
-- a deep copy of from_key's (groups, personal allow/deny, until_/map, all
-- of it). For testing as someone else: no DB involved, nothing written to
-- users.lua until `lua_perms save` - a reload or a fresh access.grant (e.g.
-- the next GameCMS resync) is enough to undo it.
function access.copy(from_key, to_key)
	local src = users[from_key]
	if not src then
		users[to_key] = nil
		generation = generation + 1
		return true
	end

	local groups_copy = {}
	for i, rec in ipairs(src.groups) do
		groups_copy[i] = { name = rec.name, until_ = rec.until_, map = rec.map }
	end
	local allow_copy, deny_copy = {}, {}
	for i, node in ipairs(src.allow) do allow_copy[i] = node end
	for i, node in ipairs(src.deny) do deny_copy[i] = node end

	users[to_key] = {
		groups = groups_copy,
		allow  = allow_copy,
		deny   = deny_copy,
		until_ = src.until_,
		map    = src.map,
		where  = src.where,
	}

	generation = generation + 1
	return true
end

function access.user(key)
	return users[key]
end

function access.users()
	return users
end

function access.group(name)
	return groups[tostring(name):lower()]
end

function access.all_groups()
	return groups
end

function access.permissions()
	return perms
end

--------------------------------------------------------------------------
-- player object
--------------------------------------------------------------------------

players.method("can", function(self, node)
	return access.can(self, node)
end)

players.method("groups", function(self)
	return access.groups(self)
end)

-- p:group("vip") - true if the player is in it, inheritance included.
players.method("group", function(self, name)
	name = tostring(name):lower()
	for _, held in ipairs(access.groups(self)) do
		if held == name then
			return true
		end
	end
	return false
end)

players.method("weight", function(self)
	return access.weight(self)
end)

players.method("outranks", function(self, other)
	return access.outranks(self, other)
end)

--------------------------------------------------------------------------
-- wiring
--------------------------------------------------------------------------

-- The steamid is worthless at connect time (Steam answers a moment later), so
-- rights are built from player_authorized onward. Anything before that sees
-- the default group only.
hook.add("client:authorized", "core.access_refresh", function(e)
	access.invalidate(e.player)
end)

hook.add("client:disconnect", "core.access_cleanup", function(e)
	cache[e.player.id] = nil
end)

access.reload()
