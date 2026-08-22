#include "cslua.h"
#include "cslua_visibility.h"

#include <entity_state.h>
#include <vector>

namespace {

struct Row
{
	bool active = false;
	CsluaRenderOverride v = {};
};

// One per entity index; a fixed CSLUA_MAXPLAYERS-sized row array rather than
// a map, so cslua_visibility_apply - the hot path, once per (recipient,
// visible entity) on every network update - is a couple of array indexes and
// nothing else once s_active_total says there is anything to look up at all.
struct Slot
{
	// -1 never matches a real serialnumber, so a fresh (never-resized-into)
	// slot reads as "nothing here" without a separate flag.
	int serial = -1;
	Row rows[CSLUA_MAXPLAYERS];
};

std::vector<Slot> s_slots;

// Total active rows across every entity, checked first in the hot path.
// Every server that never calls e:render_for/e:visible_to pays exactly one
// int compare per pfnAddToFullPack call for this feature existing at all.
int s_active_total = 0;

Slot *slot_for(edict_t *e, bool create)
{
	if (!e)
		return nullptr;

	int index = ENTINDEX(e);
	if (index <= 0)
		return nullptr;

	if ((size_t)index >= s_slots.size()) {
		if (!create)
			return nullptr;
		s_slots.resize(index + 1);
	}

	Slot &slot = s_slots[index];

	if (slot.serial != e->serialnumber) {
		if (!create)
			return nullptr;

		// A different life of this index - whatever the previous occupant
		// had here is not this entity's to keep.
		for (int i = 0; i < CSLUA_MAXPLAYERS; i++) {
			if (slot.rows[i].active) {
				slot.rows[i].active = false;
				s_active_total--;
			}
		}
		slot.serial = e->serialnumber;
	}

	return &slot;
}

} // namespace

void cslua_visibility_set(edict_t *e, int recipient, const CsluaRenderOverride &values)
{
	if (recipient < 1 || recipient >= CSLUA_MAXPLAYERS)
		return;

	Slot *slot = slot_for(e, /*create=*/true);
	if (!slot)
		return;

	Row &row = slot->rows[recipient];
	if (!row.active)
		s_active_total++;
	row.active = true;
	row.v = values;
}

void cslua_visibility_clear(edict_t *e, int recipient)
{
	if (recipient < 1 || recipient >= CSLUA_MAXPLAYERS)
		return;

	Slot *slot = slot_for(e, /*create=*/false);
	if (!slot)
		return;

	Row &row = slot->rows[recipient];
	if (row.active) {
		row.active = false;
		s_active_total--;
	}
}

bool cslua_visibility_get(edict_t *e, int recipient, CsluaRenderOverride &out)
{
	if (recipient < 1 || recipient >= CSLUA_MAXPLAYERS)
		return false;

	Slot *slot = slot_for(e, /*create=*/false);
	if (!slot || !slot->rows[recipient].active)
		return false;

	out = slot->rows[recipient].v;
	return true;
}

void cslua_visibility_apply(entity_state_s *state, int index, edict_t *ent, edict_t *host)
{
	if (s_active_total == 0)
		return;

	if (!state || !ent || !host || index <= 0 || (size_t)index >= s_slots.size())
		return;

	const Slot &slot = s_slots[index];
	if (slot.serial != ent->serialnumber)
		return;

	int recipient = ENTINDEX(host);
	if (recipient < 1 || recipient >= CSLUA_MAXPLAYERS)
		return;

	const Row &row = slot.rows[recipient];
	if (!row.active)
		return;

	state->rendermode = row.v.mode;
	state->renderamt = (int)row.v.amount;
	state->rendercolor.r = (byte)row.v.r;
	state->rendercolor.g = (byte)row.v.g;
	state->rendercolor.b = (byte)row.v.b;
	state->renderfx = row.v.fx;
}

void cslua_visibility_reset()
{
	s_slots.clear();
	s_active_total = 0;
}
