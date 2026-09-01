#include "cslua.h"
#include "cslua_corpse.h"
#include "cslua_netwatch.h"
#include "lua_msg.h"
#include "lua_events.h"
#include "lua_player.h"
#include "players.h"

#include <string.h>
#include <string>
#include <vector>

// How many active corpses want ClCorpse hidden. Not per-player: MessageBegin's
// arguments don't say which player's death a ClCorpse belongs to, and ClCorpse
// is a runtime-registered CS message with no wire format in the vendored SDK.
// So it is all-or-nothing: while any corpse wants hiding, every ClCorpse for
// every player is swallowed.
static int s_hide_refs = 0;

void cslua_corpse_hide_ref(bool hide)
{
	s_hide_refs += hide ? 1 : -1;
	if (s_hide_refs < 0)
		s_hide_refs = 0;
}

// True from a suppressed MessageBegin(ClCorpse) through its MessageEnd. Every
// Write* between must be dropped too: MessageBegin never opened the engine's
// buffer, so letting a Write* through would corrupt it.
static bool s_suppressing = false;

// hook.add("msg:Name", ...) backing: buffers the one message currently between
// MessageBegin and MessageEnd. `active` is decided once in MessageBegin, from
// whether anyone listens, so messages with no listener stay cheap.
struct MsgField
{
	FieldKind kind;
	double num;
	std::string str;
};

struct OpenMsgHook
{
	bool active;
	std::string name;
	int dest;
	edict_t *target;
	std::vector<MsgField> fields;
};
static OpenMsgHook s_open_hook;

// Resolved on first use: the game DLL registers custom messages after
// Meta_Attach.
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
	if (s_suppressing) {
		s_open_hook.active = false;
		RETURN_META(MRES_SUPERCEDE);
	}
	// A suppressed message never reaches the wire, so netwatch never counts it.
	cslua_netwatch_message_begin(ed, msg_type);

	char namebuf[32];
	const char *name = cslua_netwatch_msg_name(msg_type, namebuf, sizeof namebuf);
	s_open_hook.active = g_events.any_custom(std::string("msg:") + name);
	if (s_open_hook.active) {
		s_open_hook.name = name;
		s_open_hook.dest = msg_dest;
		s_open_hook.target = ed;
		s_open_hook.fields.clear();
	}

	RETURN_META(MRES_IGNORED);
}

static void Hook_MessageEnd(void)
{
	if (s_suppressing) {
		s_suppressing = false;
		RETURN_META(MRES_SUPERCEDE);
	}

	bool cancelled = false;

	if (s_open_hook.active) {
		OpenMsgHook &m = s_open_hook;

		cancelled = g_events.run_custom(std::string("msg:") + m.name, [&](lua_State *L) {
			lua_pushinteger(L, m.dest);
			lua_setfield(L, -2, "dest");

			int idx = (m.target && !m.target->free) ? ENTINDEX(m.target) : -1;
			if (idx >= 1 && idx < CSLUA_MAXPLAYERS && g_players.is_connected(idx))
				cslua_push_player(L, idx);
			else
				lua_pushnil(L);
			lua_setfield(L, -2, "player");

			lua_newtable(L);
			for (size_t i = 0; i < m.fields.size(); i++) {
				const MsgField &f = m.fields[i];
				if (f.kind == MSGF_STRING)
					lua_pushstring(L, f.str.c_str());
				else
					lua_pushnumber(L, f.num);
				lua_rawseti(L, -2, (int)i + 1);
			}
			lua_setfield(L, -2, "fields");
		});

		s_open_hook.active = false;
	}

	if (cancelled)
		RETURN_META(MRES_SUPERCEDE);

	cslua_netwatch_message_end();
	RETURN_META(MRES_IGNORED);
}

// Records one field for the watched message. Only called when s_open_hook.active.
static void record_field(FieldKind kind, double num, const char *str = NULL)
{
	MsgField f;
	f.kind = kind;
	f.num = num;
	if (str)
		f.str = str;
	s_open_hook.fields.push_back(f);
}

static void Hook_WriteByte(int iValue)
{
	if (s_suppressing) RETURN_META(MRES_SUPERCEDE);
	if (s_open_hook.active) record_field(MSGF_BYTE, iValue);
	cslua_netwatch_add_bytes(1);
	RETURN_META(MRES_IGNORED);
}

static void Hook_WriteChar(int iValue)
{
	if (s_suppressing) RETURN_META(MRES_SUPERCEDE);
	if (s_open_hook.active) record_field(MSGF_CHAR, iValue);
	cslua_netwatch_add_bytes(1);
	RETURN_META(MRES_IGNORED);
}

static void Hook_WriteShort(int iValue)
{
	if (s_suppressing) RETURN_META(MRES_SUPERCEDE);
	if (s_open_hook.active) record_field(MSGF_SHORT, iValue);
	cslua_netwatch_add_bytes(2);
	RETURN_META(MRES_IGNORED);
}

static void Hook_WriteLong(int iValue)
{
	if (s_suppressing) RETURN_META(MRES_SUPERCEDE);
	if (s_open_hook.active) record_field(MSGF_LONG, iValue);
	cslua_netwatch_add_bytes(4);
	RETURN_META(MRES_IGNORED);
}

static void Hook_WriteAngle(float flValue)
{
	if (s_suppressing) RETURN_META(MRES_SUPERCEDE);
	if (s_open_hook.active) record_field(MSGF_ANGLE, flValue);
	cslua_netwatch_add_bytes(1);
	RETURN_META(MRES_IGNORED);
}

static void Hook_WriteCoord(float flValue)
{
	if (s_suppressing) RETURN_META(MRES_SUPERCEDE);
	if (s_open_hook.active) record_field(MSGF_COORD, flValue);
	cslua_netwatch_add_bytes(2);
	RETURN_META(MRES_IGNORED);
}

static void Hook_WriteString(const char *sz)
{
	if (s_suppressing) RETURN_META(MRES_SUPERCEDE);
	if (s_open_hook.active) record_field(MSGF_STRING, 0, sz ? sz : "");
	cslua_netwatch_add_bytes(sz ? (int)strlen(sz) + 1 : 1);
	RETURN_META(MRES_IGNORED);
}

static void Hook_WriteEntity(int iValue)
{
	if (s_suppressing) RETURN_META(MRES_SUPERCEDE);
	if (s_open_hook.active) record_field(MSGF_ENTITY, iValue);
	cslua_netwatch_add_bytes(2);
	RETURN_META(MRES_IGNORED);
}

// Every field NULL except the ten message calls above; metamod fills NULL slots
// from the next plugin. Field order/count must match metamod-r's enginefuncs_t
// layout exactly - this struct is memcpy'd by pointer, not read by name.
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

	// Added 2009-06-19 (no SDK update). Compiled in unless CSSDK_COMPAT_OLD_METAMOD.
	NULL,					// pfnEngCheckParm()
};
