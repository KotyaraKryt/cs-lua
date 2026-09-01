#pragma once

#include <extdll.h>

// Per-(entity, recipient) render overrides - the only way to make an entity
// look different to one client than to everyone else. entvars_t (and e:render())
// networks one value to every client alike; pfnAddToFullPack is the only point
// a specific client's copy can be repainted - see dllapi.cpp's
// AddToFullPack_Post.
//
// That hook runs on every (recipient, visible entity) pair every network
// update, so no Lua round trip: e:render_for/e:visible_to only touch the table
// this file owns (off the hot path); cslua_visibility_apply reads it back with
// no Lua involved and costs one branch (s_active_total) when nothing uses it.

struct CsluaRenderOverride
{
	int mode;
	float amount;
	float r, g, b;
	int fx;
};

// Sets (or replaces) what recipient sees on e. recipient is a player slot, 1..32.
void cslua_visibility_set(edict_t *e, int recipient, const CsluaRenderOverride &values);

// Removes the override.
void cslua_visibility_clear(edict_t *e, int recipient);

// Reads the current override for (e, recipient). False (and *out untouched)
// when there is none.
bool cslua_visibility_get(edict_t *e, int recipient, CsluaRenderOverride &out);

// pfnAddToFullPack post-hook body. Overwrites state's render fields in place
// when an override exists for this exact pair; never touches whether the entity
// is sent at all.
void cslua_visibility_apply(struct entity_state_s *state, int index, edict_t *ent, edict_t *host);

// Map ending: every override dies with it.
void cslua_visibility_reset();
