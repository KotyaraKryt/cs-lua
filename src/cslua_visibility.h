#pragma once

#include <extdll.h>

// Per-(entity, recipient) render overrides - the only way to make an entity
// look different to one client than to everyone else. entvars_t (and
// e:render() in lua_entity.cpp, which just writes it) is one value the
// engine networks to every client alike; the only point where a *specific*
// client's copy of an entity can be repainted differently is
// pfnAddToFullPack, the engine's per-recipient snapshot build - see
// dllapi.cpp's AddToFullPack_Post, the only caller of cslua_visibility_apply.
//
// That hook runs on every (recipient, potentially-visible entity) pair on
// every network update - there is no affording a Lua round trip there. So
// the split is: lua_entity.cpp's e:render_for/e:clear_render_for/e:visible_to
// only ever touch the table this file owns (a plain write, off the hot
// path); cslua_visibility_apply reads it back on the hot path with no Lua
// involved at all, and costs one branch (the s_active_total check) when
// nothing anywhere has ever set an override - which is every server that
// does not use this.

struct CsluaRenderOverride
{
	int mode;
	float amount;
	float r, g, b;
	int fx;
};

// Sets (or replaces) what recipient sees on e instead of its ordinary,
// everyone-alike render. recipient is a player slot, 1..32.
void cslua_visibility_set(edict_t *e, int recipient, const CsluaRenderOverride &values);

// Removes the override; recipient goes back to seeing e's normal render.
void cslua_visibility_clear(edict_t *e, int recipient);

// Reads the current override for (e, recipient), if any. Returns false (and
// leaves *out untouched) when there is none - either nobody ever set one, or
// e's slot has since been reused by a different entity's life.
bool cslua_visibility_get(edict_t *e, int recipient, CsluaRenderOverride &out);

// pfnAddToFullPack post-hook body. index/ent identify the entity the engine
// just finished building `state` for; host is who it is being sent to.
// Overwrites state's render fields in place when an override exists for
// this exact pair - never touches whether the entity is sent at all.
void cslua_visibility_apply(struct entity_state_s *state, int index, edict_t *ent, edict_t *host);

// Map ending: every override dies with it, same as the edicts themselves.
void cslua_visibility_reset();
