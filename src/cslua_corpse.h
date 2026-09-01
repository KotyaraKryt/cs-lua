#pragma once

#include <extdll.h>

// Engine-function hooks (MessageBegin/Write*/MessageEnd) that swallow the
// native ClCorpse ragdoll notification while a script has asked for it to be
// hidden. Wired into gMetaFunctionTable's pfnGetEngineFunctions in meta_api.cpp.
extern enginefuncs_t g_CsluaCorpseEngineFuncs;

// Reference-counted: two corpses can be tracked at once, and suppression stays
// armed until the last one clears. No per-player targeting - while the count is
// above zero, every ClCorpse for every player is swallowed.
void cslua_corpse_hide_ref(bool hide);
