#include "cslua.h"
#include "cslua_netwatch.h"
#include "players.h"
#include "platform.h"

#include <rehlds_api.h>
#include "rehlds.h"

#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <time.h>

// How far back a drop's explanation looks. Long enough to catch a burst that
// built up over a few frames, short enough that the log stays about the kick
// and not about the last five minutes of normal chatter.
static const double WINDOW_SECONDS = 8.0;

// Per-slot ring buffer. 512 messages is generous for eight seconds of normal
// traffic (a few dozen messages/sec at most) and cheap to keep 33 of.
static const int RING_SIZE = 512;

struct Record
{
	double time;
	int msg_type;
	int bytes;
};

struct Slot
{
	Record ring[RING_SIZE];
	int next = 0;
	int count = 0;

	// Message currently being built (between MessageBegin and MessageEnd).
	bool building = false;
	int building_type = 0;
	int building_bytes = 0;

	void reset()
	{
		next = 0;
		count = 0;
		building = false;
	}

	void push(int msg_type, int bytes)
	{
		ring[next] = Record{ cslua_now_seconds(), msg_type, bytes };
		next = (next + 1) % RING_SIZE;
		if (count < RING_SIZE)
			count++;
	}
};

static Slot s_slots[CSLUA_MAXPLAYERS];

// Which slot is currently between MessageBegin/MessageEnd, or 0 if none -
// cslua_corpse.cpp's Hook_MessageBegin gives us the destination edict but the
// Write* hooks that follow it only give us bytes, so we have to remember it.
static int s_active_id = 0;

static bool valid(int id) { return id >= 1 && id < CSLUA_MAXPLAYERS; }

// The builtin (non-user) message opcodes, in engine order - see
// third_party/metamod-r/metamod/src/mutil.cpp's own copy of the same table.
// msg_type values at or above this table's length are custom usermessages
// registered at runtime (pfnRegUserMsg), which this module has no name for.
static const char *s_builtin_names[] =
{
	"svc_bad", "svc_nop", "svc_disconnect", "svc_event", "svc_version",
	"svc_setview", "svc_sound", "svc_time", "svc_print", "svc_stufftext",
	"svc_setangle", "svc_serverinfo", "svc_lightstyle", "svc_updateuserinfo",
	"svc_deltadescription", "svc_clientdata", "svc_stopsound", "svc_pings",
	"svc_particle", "svc_damage", "svc_spawnstatic", "svc_event_reliable",
	"svc_spawnbaseline", "svc_temp_entity", "svc_setpause", "svc_signonnum",
	"svc_centerprint", "svc_killedmonster", "svc_foundsecret",
	"svc_spawnstaticsound", "svc_intermission", "svc_finale", "svc_cdtrack",
	"svc_restore", "svc_cutscene", "svc_weaponanim", "svc_decalname",
	"svc_roomtype", "svc_addangle", "svc_newusermsg", "svc_packetentities",
	"svc_deltapacketentities", "svc_choke", "svc_resourcelist",
	"svc_newmovevars", "svc_resourcerequest", "svc_customization",
	"svc_crosshairangle", "svc_soundfade", "svc_filetxferfailed", "svc_hltv",
	"svc_director", "svc_voiceinit", "svc_voicedata", "svc_sendextrainfo",
	"svc_timescale", "svc_resourcelocation", "svc_sendcvarvalue",
	"svc_sendcvarvalue2",
};
static const int BUILTIN_COUNT = sizeof(s_builtin_names) / sizeof(s_builtin_names[0]);

// Custom usermessages are assigned an id at runtime (pfnRegUserMsg), not
// known ahead of time - filled in by the post-hook below as the game DLL and
// other plugins register theirs. GoldSrc wires msg_type through the network
// as a byte, so 256 slots covers every id that can exist.
static std::string s_custom_names[256];

static const char *msg_name(int msg_type, char *buf, size_t buflen)
{
	if (msg_type >= 0 && msg_type < BUILTIN_COUNT)
		return s_builtin_names[msg_type];
	if (msg_type >= 0 && msg_type < 256 && !s_custom_names[msg_type].empty())
		return s_custom_names[msg_type].c_str();
	cslua_snprintf(buf, buflen, "usermsg#%d", msg_type);
	return buf;
}

static int Hook_RegUserMsg_Post(const char *pszName, int iSize)
{
	int id = META_RESULT_ORIG_RET(int);
	if (pszName && id >= 0 && id < 256)
		s_custom_names[id] = pszName;
	RETURN_META_VALUE(MRES_IGNORED, id);
}

// Every field NULL except pfnRegUserMsg: metamod fills a NULL slot from the
// next plugin (or the real engine) itself, same convention as
// g_CsluaCorpseEngineFuncs in cslua_corpse.cpp - see that file for why the
// field order/count has to match metamod-r's enginefuncs_t layout exactly.
// This is the POST variant (meta_api.cpp's GetEngineFunctions_Post): a
// pre-hook fires before the real call, before an id even exists to record.
enginefuncs_t g_CsluaNetwatchPostEngineFuncs =
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
	NULL,					// pfnMessageBegin()
	NULL,					// pfnMessageEnd()
	NULL,					// pfnWriteByte()
	NULL,					// pfnWriteChar()
	NULL,					// pfnWriteShort()
	NULL,					// pfnWriteLong()
	NULL,					// pfnWriteAngle()
	NULL,					// pfnWriteCoord()
	NULL,					// pfnWriteString()
	NULL,					// pfnWriteEntity()
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
	Hook_RegUserMsg_Post,	// pfnRegUserMsg()
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

// A drop is a rare event (this is a diagnostic for something going wrong,
// not a per-frame path), so opening and closing the file each call is fine -
// no handle to leak or flush-on-crash to worry about.
static void log_line(const char *fmt, ...)
{
	std::string dir = cslua_base_dir() + "/logs";
	cslua_make_dir(dir);
	FILE *fh = fopen((dir + "/netwatch.log").c_str(), "a");
	if (!fh)
		return;

	time_t now = time(NULL);
	char stamp[32];
	strftime(stamp, sizeof stamp, "%Y-%m-%d %H:%M:%S", localtime(&now));

	char msg[512];
	va_list ap;
	va_start(ap, fmt);
	cslua_vsnprintf(msg, sizeof msg - 1, fmt, ap);
	va_end(ap);
	msg[sizeof msg - 1] = '\0';

	fprintf(fh, "[%s] %s\n", stamp, msg);
	fclose(fh);
}

void cslua_netwatch_message_begin(edict_t *dest, int msg_type)
{
	s_active_id = 0;
	// Broadcasts (dest is NULL/world for MSG_ALL etc.) aren't any one
	// client's reliable buffer problem; only per-player sends are tracked.
	if (!dest)
		return;
	int id = g_engfuncs.pfnIndexOfEdict(dest);
	if (!valid(id) || !g_players.is_connected(id))
		return;

	s_active_id = id;
	Slot &slot = s_slots[id];
	slot.building = true;
	slot.building_type = msg_type;
	slot.building_bytes = 0;
}

void cslua_netwatch_add_bytes(int bytes)
{
	if (!valid(s_active_id))
		return;
	Slot &slot = s_slots[s_active_id];
	if (slot.building)
		slot.building_bytes += bytes;
}

void cslua_netwatch_message_end()
{
	if (!valid(s_active_id))
		return;
	Slot &slot = s_slots[s_active_id];
	if (slot.building) {
		slot.push(slot.building_type, slot.building_bytes);
		slot.building = false;
	}
	s_active_id = 0;
}

void cslua_netwatch_forget(int id)
{
	if (!valid(id))
		return;
	s_slots[id].reset();
	if (s_active_id == id)
		s_active_id = 0;
}

// One breakdown bucket while summarizing a slot's window.
struct Bucket
{
	int msg_type;
	int messages;
	int bytes;
};

static void dump_slot(int id, edict_t *ed, const char *reason)
{
	Slot &slot = s_slots[id];
	if (slot.count == 0)
		return;

	double now = cslua_now_seconds();
	Bucket buckets[BUILTIN_COUNT + 32];
	int nbuckets = 0;
	int total_messages = 0;
	int total_bytes = 0;

	for (int i = 0; i < slot.count; i++) {
		const Record &r = slot.ring[i];
		if (now - r.time > WINDOW_SECONDS)
			continue;

		total_messages++;
		total_bytes += r.bytes;

		int b = 0;
		for (; b < nbuckets; b++)
			if (buckets[b].msg_type == r.msg_type)
				break;
		if (b == nbuckets) {
			if (nbuckets >= (int)(sizeof(buckets) / sizeof(buckets[0])))
				continue; // too many distinct types to bucket; still counted in totals
			buckets[b].msg_type = r.msg_type;
			buckets[b].messages = 0;
			buckets[b].bytes = 0;
			nbuckets++;
		}
		buckets[b].messages++;
		buckets[b].bytes += r.bytes;
	}

	if (total_messages == 0)
		return;

	// Simple selection sort, descending by bytes - nbuckets is at most a few
	// dozen, this runs once per drop, not worth pulling in <algorithm> for.
	for (int i = 0; i < nbuckets; i++) {
		int best = i;
		for (int j = i + 1; j < nbuckets; j++)
			if (buckets[j].bytes > buckets[best].bytes)
				best = j;
		if (best != i) {
			Bucket tmp = buckets[i];
			buckets[i] = buckets[best];
			buckets[best] = tmp;
		}
	}

	int ping = -1, loss = -1;
	if (ed)
		g_engfuncs.pfnGetPlayerStats(ed, &ping, &loss);

	log_line("%s (slot %d, ip %s, %s) dropped: %s",
		g_players.name(id), id, g_players.ip(id), g_players.authid(id), reason);
	log_line("  ping %d loss %d%% at the moment of the drop", ping, loss);
	log_line("  %d messages / %d bytes queued in the last %.0fs before the drop",
		total_messages, total_bytes, WINDOW_SECONDS);

	for (int i = 0; i < nbuckets; i++) {
		char namebuf[32];
		const char *name = msg_name(buckets[i].msg_type, namebuf, sizeof namebuf);
		log_line("    %-24s %5d msgs  %6d bytes", name, buckets[i].messages, buckets[i].bytes);
	}
}

// Every disconnect - a clean quit, a normal "kick", a timeout - goes through
// SV_DropClient, not just reliable-buffer overflow. Logging all of them would
// bury the one case this module exists for, so only reasons that actually
// mention overflow get a dump. ASCII case-insensitive: engine builds have
// spelled this "Overflow", "overflowed", "buffer overflow" depending on
// version, always with that word somewhere in it.
static bool mentions_overflow(const char *reason)
{
	if (!reason)
		return false;
	size_t needle_len = strlen("overflow");
	for (const char *p = reason; *p; p++) {
		size_t i = 0;
		for (; i < needle_len; i++) {
			char a = p[i];
			if (a >= 'A' && a <= 'Z') a += 'a' - 'A';
			char b = "overflow"[i];
			if (!a || a != b)
				break;
		}
		if (i == needle_len)
			return true;
	}
	return false;
}

static void hook_drop_client(IRehldsHook_SV_DropClient *chain, IGameClient *client, bool crash, const char *reason)
{
	if (client && mentions_overflow(reason)) {
		edict_t *ed = client->GetEdict();
		if (ed) {
			int id = g_engfuncs.pfnIndexOfEdict(ed);
			if (valid(id))
				dump_slot(id, ed, reason);
		}
	}
	chain->callNext(client, crash, reason);
}

void cslua_netwatch_install_hooks()
{
	IRehldsHookchains *hooks = cslua_rehlds_hooks();
	if (!hooks)
		return;
	hooks->SV_DropClient()->registerHook(&hook_drop_client);
}
