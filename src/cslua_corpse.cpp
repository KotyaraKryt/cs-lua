#include "cslua.h"
#include "cslua_corpse.h"

// How many active corpses currently want ClCorpse hidden. Not per-player:
// MessageBegin's own arguments (msg_dest, pOrigin, the destination edict)
// don't say which player's death a ClCorpse belongs to - the original AMXX
// plugin this was ported from only knew because it read a positional
// argument out of the message body (get_msg_arg_int(12)), a wire format this
// project has no header for (ClCorpse is a ReGameDLL/CS gamedll message,
// registered at runtime, not declared anywhere in the vendored SDK). Rather
// than guess byte offsets against a closed-source format, this is
// all-or-nothing: while any corpse anywhere wants hiding, every ClCorpse for
// every player is swallowed. Fine for this plugin's actual policy (every
// tracked death hides its corpse, uniformly); a mixed policy would need the
// wire format figured out for real.
static int s_hide_refs = 0;

void cslua_corpse_hide_ref(bool hide)
{
	s_hide_refs += hide ? 1 : -1;
	if (s_hide_refs < 0)
		s_hide_refs = 0;
}

// True from a suppressed MessageBegin(ClCorpse) through its MessageEnd. Every
// Write* in between belongs to that one message and has to be dropped too:
// MessageBegin never actually opened the engine's message buffer (we
// superceded it), so letting a Write* through would write into a buffer that
// was never started - the same corruption fakemeta's register_message avoids
// by swallowing the whole call sequence, not just the first call.
static bool s_suppressing = false;

// Resolved on first use rather than at load: the game DLL registers its
// custom messages during its own startup, some time after Meta_Attach runs,
// so asking too early would cache a 0. Every message id lookup elsewhere in
// this codebase (lua_message.cpp's SayText/TextMsg/MOTD) does the same thing.
static int clcorpse_msg_id()
{
	static int id = 0;
	if (!id)
		id = GET_USER_MSG_ID(PLID, "ClCorpse", NULL);
	return id;
}

static void Hook_MessageBegin(int msg_dest, int msg_type, const float *pOrigin, edict_t *ed)
{
	s_suppressing = s_hide_refs > 0 && msg_type == clcorpse_msg_id();
	if (s_suppressing)
		RETURN_META(MRES_SUPERCEDE);
	RETURN_META(MRES_IGNORED);
}

static void Hook_MessageEnd(void)
{
	if (s_suppressing) {
		s_suppressing = false;
		RETURN_META(MRES_SUPERCEDE);
	}
	RETURN_META(MRES_IGNORED);
}

static void Hook_WriteByte(int iValue)
{
	if (s_suppressing) RETURN_META(MRES_SUPERCEDE);
	RETURN_META(MRES_IGNORED);
}

static void Hook_WriteChar(int iValue)
{
	if (s_suppressing) RETURN_META(MRES_SUPERCEDE);
	RETURN_META(MRES_IGNORED);
}

static void Hook_WriteShort(int iValue)
{
	if (s_suppressing) RETURN_META(MRES_SUPERCEDE);
	RETURN_META(MRES_IGNORED);
}

static void Hook_WriteLong(int iValue)
{
	if (s_suppressing) RETURN_META(MRES_SUPERCEDE);
	RETURN_META(MRES_IGNORED);
}

static void Hook_WriteAngle(float flValue)
{
	if (s_suppressing) RETURN_META(MRES_SUPERCEDE);
	RETURN_META(MRES_IGNORED);
}

static void Hook_WriteCoord(float flValue)
{
	if (s_suppressing) RETURN_META(MRES_SUPERCEDE);
	RETURN_META(MRES_IGNORED);
}

static void Hook_WriteString(const char *sz)
{
	if (s_suppressing) RETURN_META(MRES_SUPERCEDE);
	RETURN_META(MRES_IGNORED);
}

static void Hook_WriteEntity(int iValue)
{
	if (s_suppressing) RETURN_META(MRES_SUPERCEDE);
	RETURN_META(MRES_IGNORED);
}

// Every field NULL except the ten message calls above: metamod fills a NULL
// slot from the next plugin (or the real engine) itself, so this only ever
// intercepts message building, nothing else. Field order/count copied from
// metamod-r's own enginefuncs_t layout (third_party/metamod-r) - it has to
// match exactly, this struct is memcpy'd by pointer, not read by name.
enginefuncs_t g_CsluaCorpseEngineFuncs =
{
	NULL,					// pfnPrecacheModel()
	NULL,					// pfnPrecacheSound()
	NULL,					// pfnSetModel()
	NULL,					// pfnModelIndex()
	NULL,					// pfnModelFrames()
	NULL,					// pfnSetSize()
	NULL,					// pfnChangeLevel()
	NULL,					// pfnGetSpawnParms()
	NULL,					// pfnSaveSpawnParms()
	NULL,					// pfnVecToYaw()
	NULL,					// pfnVecToAngles()
	NULL,					// pfnMoveToOrigin()
	NULL,					// pfnChangeYaw()
	NULL,					// pfnChangePitch()
	NULL,					// pfnFindEntityByString()
	NULL,					// pfnGetEntityIllum()
	NULL,					// pfnFindEntityInSphere()
	NULL,					// pfnFindClientInPVS()
	NULL,					// pfnEntitiesInPVS()
	NULL,					// pfnMakeVectors()
	NULL,					// pfnAngleVectors()
	NULL,					// pfnCreateEntity()
	NULL,					// pfnRemoveEntity()
	NULL,					// pfnCreateNamedEntity()
	NULL,					// pfnMakeStatic()
	NULL,					// pfnEntIsOnFloor()
	NULL,					// pfnDropToFloor()
	NULL,					// pfnWalkMove()
	NULL,					// pfnSetOrigin()
	NULL,					// pfnEmitSound()
	NULL,					// pfnEmitAmbientSound()
	NULL,					// pfnTraceLine()
	NULL,					// pfnTraceToss()
	NULL,					// pfnTraceMonsterHull()
	NULL,					// pfnTraceHull()
	NULL,					// pfnTraceModel()
	NULL,					// pfnTraceTexture()
	NULL,					// pfnTraceSphere()
	NULL,					// pfnGetAimVector()
	NULL,					// pfnServerCommand()
	NULL,					// pfnServerExecute()
	NULL,					// pfnClientCommand()
	NULL,					// pfnParticleEffect()
	NULL,					// pfnLightStyle()
	NULL,					// pfnDecalIndex()
	NULL,					// pfnPointContents()
	Hook_MessageBegin,		// pfnMessageBegin()
	Hook_MessageEnd,		// pfnMessageEnd()
	Hook_WriteByte,			// pfnWriteByte()
	Hook_WriteChar,			// pfnWriteChar()
	Hook_WriteShort,		// pfnWriteShort()
	Hook_WriteLong,			// pfnWriteLong()
	Hook_WriteAngle,		// pfnWriteAngle()
	Hook_WriteCoord,		// pfnWriteCoord()
	Hook_WriteString,		// pfnWriteString()
	Hook_WriteEntity,		// pfnWriteEntity()
	NULL,					// pfnCVarRegister()
	NULL,					// pfnCVarGetFloat()
	NULL,					// pfnCVarGetString()
	NULL,					// pfnCVarSetFloat()
	NULL,					// pfnCVarSetString()
	NULL,					// pfnAlertMessage()
	NULL,					// pfnEngineFprintf()
	NULL,					// pfnPvAllocEntPrivateData()
	NULL,					// pfnPvEntPrivateData()
	NULL,					// pfnFreeEntPrivateData()
	NULL,					// pfnSzFromIndex()
	NULL,					// pfnAllocString()
	NULL,					// pfnGetVarsOfEnt()
	NULL,					// pfnPEntityOfEntOffset()
	NULL,					// pfnEntOffsetOfPEntity()
	NULL,					// pfnIndexOfEdict()
	NULL,					// pfnPEntityOfEntIndex()
	NULL,					// pfnFindEntityByVars()
	NULL,					// pfnGetModelPtr()
	NULL,					// pfnRegUserMsg()
	NULL,					// pfnAnimationAutomove()
	NULL,					// pfnGetBonePosition()
	NULL,					// pfnFunctionFromName()
	NULL,					// pfnNameForFunction()
	NULL,					// pfnClientPrintf()
	NULL,					// pfnServerPrint()
	NULL,					// pfnCmd_Args()
	NULL,					// pfnCmd_Argv()
	NULL,					// pfnCmd_Argc()
	NULL,					// pfnGetAttachment()
	NULL,					// pfnCRC32_Init()
	NULL,					// pfnCRC32_ProcessBuffer()
	NULL,					// pfnCRC32_ProcessByte()
	NULL,					// pfnCRC32_Final()
	NULL,					// pfnRandomLong()
	NULL,					// pfnRandomFloat()
	NULL,					// pfnSetView()
	NULL,					// pfnTime()
	NULL,					// pfnCrosshairAngle()
	NULL,					// pfnLoadFileForMe()
	NULL,					// pfnFreeFile()
	NULL,					// pfnEndSection()
	NULL,					// pfnCompareFileTime()
	NULL,					// pfnGetGameDir()
	NULL,					// pfnCvar_RegisterVariable()
	NULL,					// pfnFadeClientVolume()
	NULL,					// pfnSetClientMaxspeed()
	NULL,					// pfnCreateFakeClient()
	NULL,					// pfnRunPlayerMove()
	NULL,					// pfnNumberOfEntities()
	NULL,					// pfnGetInfoKeyBuffer()
	NULL,					// pfnInfoKeyValue()
	NULL,					// pfnSetKeyValue()
	NULL,					// pfnSetClientKeyValue()
	NULL,					// pfnIsMapValid()
	NULL,					// pfnStaticDecal()
	NULL,					// pfnPrecacheGeneric()
	NULL,					// pfnGetPlayerUserId()
	NULL,					// pfnBuildSoundMsg()
	NULL,					// pfnIsDedicatedServer()
	NULL,					// pfnCVarGetPointer()
	NULL,					// pfnGetPlayerWONId()
	NULL,					// pfnInfo_RemoveKey()
	NULL,					// pfnGetPhysicsKeyValue()
	NULL,					// pfnSetPhysicsKeyValue()
	NULL,					// pfnGetPhysicsInfoString()
	NULL,					// pfnPrecacheEvent()
	NULL,					// pfnPlaybackEvent()
	NULL,					// pfnSetFatPVS()
	NULL,					// pfnSetFatPAS()
	NULL,					// pfnCheckVisibility()
	NULL,					// pfnDeltaSetField()
	NULL,					// pfnDeltaUnsetField()
	NULL,					// pfnDeltaAddEncoder()
	NULL,					// pfnGetCurrentPlayer()
	NULL,					// pfnCanSkipPlayer()
	NULL,					// pfnDeltaFindField()
	NULL,					// pfnDeltaSetFieldByIndex()
	NULL,					// pfnDeltaUnsetFieldByIndex()
	NULL,					// pfnSetGroupMask()
	NULL,					// pfnCreateInstancedBaseline()
	NULL,					// pfnCvar_DirectSet()
	NULL,					// pfnForceUnmodified()
	NULL,					// pfnGetPlayerStats()
	NULL,					// pfnAddServerCommand()

	// Added in SDK 2.2:
	NULL,					// pfnVoice_GetClientListening()
	NULL,					// pfnVoice_SetClientListening()

	// Added for HL 1109 (no SDK update):
	NULL,					// pfnGetPlayerAuthId()

	// Added 2003/11/10 (no SDK update):
	NULL,					// pfnSequenceGet()
	NULL,					// pfnSequencePickSentence()
	NULL,					// pfnGetFileSize()
	NULL,					// pfnGetApproxWavePlayLen()
	NULL,					// pfnIsCareerMatch()
	NULL,					// pfnGetLocalizedStringLength()
	NULL,					// pfnRegisterTutorMessageShown()
	NULL,					// pfnGetTimesTutorMessageShown()
	NULL,					// pfnProcessTutorMessageDecayBuffer()
	NULL,					// pfnConstructTutorMessageDecayBuffer()
	NULL,					// pfnResetTutorMessageDecayData()

	// Added 2005-08-11 (no SDK update)
	NULL,					// pfnQueryClientCvarValue()
	// Added 2005-11-22 (no SDK update)
	NULL,					// pfnQueryClientCvarValue2()

	// Added 2009-06-19 (no SDK update). Present unless CSSDK_COMPAT_OLD_METAMOD
	// is defined - this project never defines it, so eiface.h compiles this
	// field in and the table has to carry it too, or every field after it
	// would land one slot off.
	NULL,					// pfnEngCheckParm()
};
