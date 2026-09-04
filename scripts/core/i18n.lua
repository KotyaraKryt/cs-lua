-- Core: string translation, shared across plugins.
--
-- A plugin registers what it has, scoped to itself automatically (same
-- ownership trick as export() in exports.lua):
--
--   -- locale/en.lua returns { welcome = "Welcome, {name}!" }
--   lang.register({
--     en = require("locale.en"),
--     ru = require("locale.ru"),
--   })
--
-- and looks strings up by key:
--
--   p:chat(lang.t(p, "welcome", { name = p:name() }))
--
-- Placeholders are named ({name}, not %s) on purpose: word order differs
-- between languages, and positional args can't be reordered per-translation.
--
-- Player language, highest priority first:
--   1. an explicit choice this session      (lang.set)
--   2. a soft guess this session, e.g. geoip (lang.set_default)
--   3. the cslua_lang server default
--   4. "en"
--
-- 1 and 2 live only in memory and die with the connection. There is no
-- per-player persistence - a preference most players never touch isn't worth
-- owning a data file for. If usage ever says otherwise, add it then.

lang = {}

local registry = {}   -- [plugin_id] = { [code] = { [key] = string } }
local override = {}   -- [player.id] = code, explicit choice (lang.set)
local guessed  = {}   -- [player.id] = code, soft guess (lang.set_default)
local warned   = {}   -- "plugin\0code\0key" seen once already, don't spam

local server_default = sv.cvar_register("cslua_lang", "en", { archive = true })

local function owner()
	return plugin.id() or "core"
end

function lang.register(langs)
	assert(type(langs) == "table", "lang.register: expected { code = strings, ... }")

	local id = owner()
	local mine = registry[id] or {}
	registry[id] = mine

	for code, strings in pairs(langs) do
		assert(type(strings) == "table", "lang.register: '" .. tostring(code) .. "' must be a table")
		mine[code] = strings
	end
end

-- lang.of(p) -> resolved code. Also takes a code string directly (so callers
-- that already know the language don't need a player object) or nil for the
-- server default.
function lang.of(target)
	if type(target) == "string" then
		return target
	end
	if type(target) == "table" and target.id then
		return override[target.id] or guessed[target.id] or server_default:str()
	end
	return server_default:str()
end

-- An explicit choice, e.g. from a `!lang ru` command. Wins over everything
-- else until the player disconnects.
function lang.set(p, code)
	assert(p and p.id, "lang.set: expected a player")
	override[p.id] = code
end

-- A soft guess - geoip, browser header, whatever a plugin can infer. Only
-- applies while the player hasn't chosen a language themselves.
function lang.set_default(p, code)
	assert(p and p.id, "lang.set_default: expected a player")
	if not override[p.id] then
		guessed[p.id] = code
	end
end

local function interpolate(str, args)
	if not args then
		return str
	end
	return (str:gsub("{(%w+)}", function(key)
		local v = args[key]
		if v == nil then
			return "{" .. key .. "}"
		end
		return tostring(v)
	end))
end

local function lookup(id, code, key)
	local mine = registry[id]
	return mine and mine[code] and mine[code][key]
end

-- Missing key: falls back to "en", then to the key itself (logged once)
-- rather than crashing a chat line over a forgotten translation.
local function translate(id, target, key, args)
	local code = lang.of(target)

	local str = lookup(id, code, key)
	if not str and code ~= "en" then
		str = lookup(id, "en", key)
	end

	if not str then
		local warn_key = id .. "\0" .. code .. "\0" .. key
		if not warned[warn_key] then
			warned[warn_key] = true
			print(("[lang] %s: no translation for '%s' (%s)"):format(id, key, code))
		end
		return key
	end

	return interpolate(str, args)
end

-- lang.t(target, key[, args]) - target is whatever lang.of() accepts.
--
-- Owner is read from plugin.id() *at call time*. That is correct from a
-- hook handler or a console command - the engine re-enters PluginScope for
-- those - but a chat command (`!foo`) is routed entirely in Lua by
-- core/commands.lua, which calls the handler as a plain nested call with no
-- scope of its own. plugin.id() there reports "core", not the plugin that
-- owns the command. Use lang.bind() from a chat-command handler instead.
function lang.t(target, key, args)
	return translate(owner(), target, key, args)
end

--------------------------------------------------------------------------
-- plurals
--------------------------------------------------------------------------

-- Plural category resolvers, one per language family - not one per
-- language, since ru/uk share the same CLDR categories (one/few/many).
-- New languages register their own resolver instead of extending this
-- table's logic: see lang.add_plural_rule below.
local plural_rules = {}

-- English: "one" only for exactly 1, "other" for everything else (0, 2, 3...).
plural_rules.en = function(n)
	return n == 1 and "one" or "other"
end

-- Russian and Ukrainian: same three CLDR categories (one/few/many).
-- 1, 21, 31... -> one | 2-4, 22-24... -> few | 0, 5-20, 25-30... -> many.
local function slavic_plural(n)
	n = math.abs(n) % 100
	local n1 = n % 10

	if n1 == 1 and n ~= 11 then return "one" end
	if n1 >= 2 and n1 <= 4 and (n < 10 or n >= 20) then return "few" end
	return "many"
end

plural_rules.ru = slavic_plural
plural_rules.uk = slavic_plural

-- lang.add_plural_rule(code, fn) - register a category resolver for a
-- language not covered above. fn(n) returns the category name that
-- lang.plural then looks up in the caller's forms table for that code.
function lang.add_plural_rule(code, fn)
	assert(type(fn) == "function", "lang.add_plural_rule: expected a function(n) -> category")
	plural_rules[code] = fn
end

-- lang.plural(target, n, forms) -> "n <word>" in the right form for
-- target's language.
--
--   lang.plural(p, days, {
--     en = { one = "day",   other = "days" },
--     ru = { one = "день",  few = "дня",  many = "дней" },
--     uk = { one = "день",  few = "дні",  many = "днів" },
--   })
--
-- Missing forms for the resolved language fall back to "en", same as
-- lang.t() falls back for a missing key.
function lang.plural(target, n, forms)
	assert(type(forms) == "table", "lang.plural: expected a forms table")

	local code = lang.of(target)
	local rule = plural_rules[code] or plural_rules.en
	local set = forms[code] or forms.en
	if not set then
		return tostring(n)
	end

	local category = rule(n)
	local word = set[category] or set.other or set.many or set.few or set.one

	return n .. " " .. tostring(word)
end

-- lang.bind() -> a t(target, key[, args]) closure that remembers who called
-- it, captured right now rather than re-read on every call. Call this once
-- at the top of a file (manifest.lua or init.lua, outside any callback) and
-- keep the result - that is the only place plugin.id() is guaranteed
-- correct regardless of how the caller ends up invoked later.
function lang.bind()
	local id = owner()
	return function(target, key, args)
		return translate(id, target, key, args)
	end
end

hook.add("client:disconnect", "core.lang_cleanup", function(e)
	override[e.player.id] = nil
	guessed[e.player.id] = nil
end)

-- e.plugin is nil when the whole state goes away; nothing to clean up then,
-- the registry dies with it (same reasoning as exports.lua).
hook.add("module:plugin_unload", "core.lang_cleanup_plugin", function(e)
	if e.plugin then
		registry[e.plugin] = nil
	end
end)

--------------------------------------------------------------------------
-- debugging
--------------------------------------------------------------------------

cmd.add("lua_lang", function(ctx)
	local target = ctx.args[1]

	if target == "list" then
		local plugins = {}
		for id in pairs(registry) do
			plugins[#plugins + 1] = id
		end
		table.sort(plugins)
		if #plugins == 0 then
			return ctx.reply("no plugin has registered any strings")
		end
		for _, id in ipairs(plugins) do
			local codes = {}
			for code in pairs(registry[id]) do
				codes[#codes + 1] = code
			end
			table.sort(codes)
			ctx.reply(("  %-20s %s"):format(id, table.concat(codes, ", ")))
		end
		return
	end

	local p = ctx.player
	if not p then
		return ctx.reply("lua_lang: run from chat, or 'lua_lang list' in console")
	end
	ctx.reply(("your language: %s (explicit: %s, guessed: %s, default: %s)")
		:format(lang.of(p), override[p.id] or "-", guessed[p.id] or "-", server_default:str()))
end, { source = { "server", "chat" } })
