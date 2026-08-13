#pragma once

#include <extdll.h>

// Engine-function hooks (MessageBegin/Write*/MessageEnd) that swallow the
// native ClCorpse ragdoll notification while a script has asked for it to be
// hidden. Wired into gMetaFunctionTable's pfnGetEngineFunctions in
// meta_api.cpp - see cslua_corpse.cpp for why this needs ten near-identical
// hooks instead of one.
extern enginefuncs_t g_CsluaCorpseEngineFuncs;

// Reference-counted rather than a single flag: two corpses can be tracked at
// once, and native suppression has to stay armed until the last one clears,
// not the first. There is no per-player targeting - see cslua_corpse.cpp for
// why - so this is all-or-nothing: while the count is above zero, every
// ClCorpse for every player is swallowed.
void cslua_corpse_hide_ref(bool hide);
