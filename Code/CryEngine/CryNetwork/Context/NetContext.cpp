// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Config.h"
#include "NetContext.h"
#include "DemoRecordListener.h"
#include "DemoPlaybackListener.h"
#include "RMILogger.h"
#include "VoiceContext.h"
#include "Utils.h"
#include <CrySystem/ITextModeConsole.h>
#include "SyncedFileSet.h"
#include "SyncedFilePak.h"

#include "DebugKit/DebugKit.h"
#include "Protocol/NetChannel.h"
#include "Streams/ByteStream.h"
#include "NetProfile.h"

#if ENABLE_SESSION_IDS
static int SessionIDCounter = 10101;
#endif

/*
 * Helper functions
 */

string GetEventName(ENetChannelEvent evt)
{
	switch (evt)
	{
	case eNCE_InvalidEvent:
		return "InvalidEvent";
	case eNCE_ChannelDestroyed:
		return "ChannelDestroyed";
	case eNCE_RevokedAuthority:
		return "RevokedAuthority";
	case eNCE_SetAuthority:
		return "SetAuthority";
	case eNCE_UpdatedObject:
		return "UpdatedObject";
	case eNCE_BoundObject:
		return "BoundObject";
	case eNCE_UnboundObject:
		return "UnboundObject";
	case eNCE_SentUpdate:
		return "SentUpdate";
	case eNCE_AckedUpdate:
		return "AckedUpdate";
	case eNCE_NackedUpdate:
		return "NackedUpdate";
	case eNCE_InGame:
		return "InGame";
	case eNCE_DeclareWitness:
		return "DeclareWitness";
	}

	string temp;
	temp.Format("%.8x", evt);
	return temp;
}

string GetEventName(ENetObjectEvent evt)
{
	switch (evt)
	{
	case eNOE_InvalidEvent:
		return "InvalidEvent";
	case eNOE_BindObject:
		return "BindObject";
	case eNOE_UnbindObject:
		return "UnbindObject";
	case eNOE_Reset:
		return "Reset";
	case eNOE_ObjectAspectChange:
		return "ObjectAspectChange";
	case eNOE_UnboundObject:
		return "UnboundObject";
	case eNOE_ChangeContext:
		return "ChangeContext";
	case eNOE_EstablishedContext:
		return "EstablishedContext";
	case eNOE_ReconfiguredObject:
		return "ReconfiguredObject";
	case eNOE_BindAspects:
		return "BindAspects";
	case eNOE_UnbindAspects:
		return "UnbindAspects";
	case eNOE_ContextDestroyed:
		return "ContextDestroyed";
	case eNOE_SetAuthority:
		return "SetAuthority";
	case eNOE_InGame:
		return "InGame";
	case eNOE_SendVoicePackets:
		return "SendVoicePackets";
	case eNOE_RemoveRMIListener:
		return "RemoveRMIListener";
	case eNOE_SetAspectProfile:
		return "SetAspectProfile";
	case eNOE_SyncWithGame_Start:
		return "SyncWithGame_Start";
	case eNOE_SyncWithGame_End:
		return "SyncWithGame_End";
	case eNOE_GotBreakage:
		return "GotBreakage";
	}

	string temp;
	temp.Format("%.8x", evt);
	return temp;
}

/*
 * CNetContext
 */

CNetContext::CNetContext(IGameContext* pGameContext, uint32 flags) :
	INetworkMember(eNMUO_Context),
	m_serverControllerOnlyAspects(0),
	m_delegatableAspects(0),
	m_regularlyUpdatedAspects(0),
	m_serverManagedProfileAspects(0),
	m_timestampedAspects(0),
	m_disabledCompressionAspects(0),
	m_pGameContext(pGameContext),
	m_bDead(false),
	m_nAllowDead(0),
	m_declaredAspects(0),
	m_ctxSerial(100),
	m_backgroundPassthrough(0)
#if ENABLE_SESSION_IDS
	,
	m_sessionID(gEnv->pNetwork->GetHostName(), SessionIDCounter++, time(NULL))
#endif
{
	++g_objcnt.netContext;

	m_demoPlayback = false;
	m_demoRecording = false;

	m_multiplayer = (flags& INetwork::eNCCF_Multiplayer) != 0;

	// use default compression (less memory usage) in single player game
	CNetwork::Get()->GetCompressionManager().Reset(m_multiplayer, false);

	m_pState = new CNetContextState(this, m_ctxSerial++, NULL);

#ifndef OLD_VOICE_SYSTEM_DEPRECATED
	if (m_multiplayer)
	{
		// voice stuff disabled in single player game
		m_pVoiceContext = new CVoiceContext(this);
	}
#endif

	NET_PROFILE_INITIALISE(m_multiplayer);

#if NET_PROFILE_ENABLE || NET_MINI_PROFILE
	memset(&g_socketBandwidth.bandwidthStats, 0, sizeof(SBandwidthStats));
#endif

	for (uint32 a = 0; a < NUM_ASPECTS; a++)
	{
		m_channelMasks[a] = ChannelMaskType(-1);
	}
}

void CNetContext::SetAspectChannelMask(NetworkAspectType aspectBit, ChannelMaskType mask)
{
	NetworkAspectID aspectIdx;

	if (CountBits(aspectBit) != 1)
	{
		CryFatalError("Cannot set mask on multiple aspects at the same time");
	}
	aspectIdx = BitIndex(aspectBit);

	m_channelMasks[aspectIdx] = mask;
}

bool CNetContext::IsDead()
{
	if (m_nAllowDead == 0)
		return m_bDead;
	else
		return false;
}

bool CNetContext::IsSuicidal()
{
	return m_bDead;
}

void CNetContext::Die()
{
#define SAFE_DIE(x) if (x) { x->Die(); x = 0; }
	SAFE_DIE(m_pAspectBandwidthDebugger);
#ifndef OLD_VOICE_SYSTEM_DEPRECATED
	SAFE_DIE(m_pVoiceContext);
#endif
	SAFE_DIE(m_pDemo);

	SNetObjectEvent event;
	event.event = eNOE_ContextDestroyed;
	m_pState->Broadcast(&event);

	TIMER.CancelTimer(m_backgroundPassthrough);
	m_backgroundPassthrough = 0;

	m_pState->Die();

	m_bDead = true;
}

#if ENABLE_SESSION_IDS
void CNetContext::SetSessionID(const CSessionID& id)
{
	CDebugKit::Get().SetSessionID(id);
}
#endif

CNetContext::~CNetContext()
{
	SCOPED_GLOBAL_LOCK;
	TIMER.CancelTimer(m_backgroundPassthrough);

	CNetwork::Get()->GetCompressionManager().Reset(false, true);
	--g_objcnt.netContext;

	NET_PROFILE_SHUTDOWN();
}

void CNetContext::DeleteContext()
{
	SCOPED_GLOBAL_LOCK;
	Die();
}

string CNetContext::GetName()
{
	return "NetContext";
}

#if ENABLE_SESSION_IDS
// yes, this is nothing but a hack; it's a good thing that it won't be compiled in at ship
void CNetContext::EnableSessionDebugging()
{
	CNetSerialize::m_bEnableLogging = true;
}
#endif

void CNetContext::ActivateDemoRecorder(const char* filename)
{
	SCOPED_GLOBAL_LOCK;
#if INCLUDE_DEMO_RECORDING
	SAFE_DIE(m_pDemo);
	m_pDemo = new CDemoRecordListener(this, filename);
	m_demoRecording = true;
#else
	NetWarning("Demo recording not enabled");
#endif
}

INetChannel* CNetContext::GetDemoRecorderChannel() const
{
	if (m_pDemo)
		return m_pDemo->GetAssociatedChannel();
	return NULL;
}

void CNetContext::ActivateDemoPlayback(const char* filename, INetChannel* pClient, INetChannel* pServer)
{
	SCOPED_GLOBAL_LOCK;
#if INCLUDE_DEMO_RECORDING
	NET_ASSERT(m_pDemo == NULL);
	m_pDemo =
	  new CDemoPlaybackListener(
	    this,
	    filename,
	    (CNetChannel*)pClient,
	    (CNetChannel*)pServer);
	m_demoPlayback = true;
#else
	NetWarning("Demo recording not enabled");
#endif
}

void CNetContext::SyncWithGame(ENetworkGameSync type)
{
	ASSERT_GLOBAL_LOCK;
	CRY_PROFILE_FUNCTION(PROFILE_NETWORK);
	ENSURE_REALTIME;

	SNetObjectEvent syncEvent;

	switch (type)
	{
	case eNGS_FrameStart:
		ASSERT_PRIMARY_THREAD;

		m_pGameContext->OnStartNetworkFrame();
		m_pState->PropogateChangesToGame();

		syncEvent.event = eNOE_SyncWithGame_Start;

		break;

	case eNGS_FrameEnd:
		ASSERT_PRIMARY_THREAD;
		m_pGameContext->OnEndNetworkFrame();
		m_pState->FetchAndPropogateChangesFromGame(true);
		m_pState->DrawDebugScreens();

		syncEvent.event = eNOE_SyncWithGame_End;
		break;
	}

	if (syncEvent.event != eNOE_InvalidEvent)
		m_pState->Broadcast(&syncEvent);
}

void CNetContext::BackgroundPassthrough(NetTimerId, void* p, CTimeValue)
{
	CNetContext* pThis = static_cast<CNetContext*>(p);
	pThis->m_pState->FetchAndPropogateChangesFromGame(false);
	pThis->m_backgroundPassthrough = TIMER.ADDTIMER(g_time + 0.01f, BackgroundPassthrough, pThis, "CNetContext::BackgroundPassthrough() m_backgroundPassThrough");
}

void CNetContext::EnableBackgroundPassthrough(bool enable)
{
	SCOPED_GLOBAL_LOCK;
	// THIS IS A TEMPORARY HACK TO MAKE THE GAME PLAY NICELY, ASK peter@crytek WHY IT'S STILL HERE
	enable = false;

	if (enable && !m_backgroundPassthrough)
		m_backgroundPassthrough = TIMER.ADDTIMER(g_time, BackgroundPassthrough, this, "CNetContext::EnableBackgroundPassthrough() m_backgroundPassthrough");
	else
	{
		TIMER.CancelTimer(m_backgroundPassthrough);
		m_backgroundPassthrough = 0;
	}
}

void CNetContext::DeclareAspect(const char* name, NetworkAspectType aspectBit, uint8 aspectFlags)
{
	SCOPED_GLOBAL_LOCK;

	NetworkAspectID aspectIdx;

	if (CountBits(aspectBit) != 1)
	{
		CryFatalError("Can only declare one aspect at a time");
	}
	aspectIdx = BitIndex(aspectBit);
	if (m_declaredAspects & aspectBit)
	{
		CryFatalError("Net aspect %u declared twice", aspectIdx);
	}

	m_aspectNames[aspectIdx] = name;

	if (aspectFlags & eAF_ServerControllerOnly)
	{
		m_serverControllerOnlyAspects |= aspectBit;
	}

	if (aspectFlags & eAF_Delegatable)
		m_delegatableAspects |= aspectBit;
	if (aspectFlags & eAF_ServerManagedProfile)
		m_serverManagedProfileAspects |= aspectBit;
	if (aspectFlags & eAF_TimestampState)
		m_timestampedAspects |= aspectBit;
	if (aspectFlags & eAF_NoCompression)
		m_disabledCompressionAspects |= aspectBit;

	m_declaredAspects |= aspectBit;
}

bool CNetContext::ChangeContext()
{
	SCOPED_GLOBAL_LOCK;
	_smart_ptr<CNetContextState> pNewState = new CNetContextState(this, m_ctxSerial++, m_pState);
	m_pState->Die();
	SNetObjectEvent evt;
	evt.event = eNOE_ChangeContext;
	evt.pNewState = pNewState;
	m_pState->Broadcast(&evt);
	m_pState = pNewState;
	TO_GAME_LAZY(&CNetContextState::GC_BeginContext, &*m_pState, g_time);
	return true;
}

void CNetContext::EstablishedContext(int establishToken)
{
	SCOPED_GLOBAL_LOCK;
	if (m_pState->GetToken() != establishToken)
		return;
	m_pState->EstablishedContext();
}

void CNetContext::RegisterLocalIPs(const TNetAddressVec& ips, CContextView* pView)
{
	ASSERT_GLOBAL_LOCK;
	for (TNetAddressVec::const_iterator i = ips.begin(); i != ips.end(); ++i)
		m_mIPToContext[*i] = pView;
}

void CNetContext::DeregisterLocalIPs(const TNetAddressVec& ips)
{
	ASSERT_GLOBAL_LOCK;
	for (TNetAddressVec::const_iterator i = ips.begin(); i != ips.end(); ++i)
		m_mIPToContext.erase(*i);
}

CContextView* CNetContext::GetLocalContext(const TNetAddress& localAddr) const
{
	ASSERT_GLOBAL_LOCK;
	CContextView* out = NULL;
	std::map<TNetAddress, CContextView*>::const_iterator i = m_mIPToContext.find(localAddr);
	if (i != m_mIPToContext.end())
		out = i->second;
	return out;
}

void CNetContext::NetDump(ENetDumpType type)
{
	m_pState->NetDump(type);
}

void CNetContext::PerformRegularCleanup()
{
	m_pState->PerformRegularCleanup();
}

void CNetContext::LogRMI(const char* function, ISerializable* pParams)
{
	m_pState->LogRMI(function, pParams);
}

void CNetContext::LogCppRMI(EntityId id, IRMICppLogger* pLogger)
{
	m_pState->LogCppRMI(id, pLogger);
}

void CNetContext::SetAspectProfile(EntityId id, NetworkAspectType aspectBit, uint8 profile)
{
	m_pState->SetAspectProfile(id, aspectBit, profile);
}

uint8 CNetContext::GetAspectProfile(EntityId id, NetworkAspectType aspectBit)
{
	return m_pState->GetAspectProfile(id, aspectBit);
}

void CNetContext::SetDelegatableMask(EntityId id, NetworkAspectType aspectBits)
{
	m_pState->SetDelegatableMask(id, aspectBits);
}

void CNetContext::BindObject(EntityId id, EntityId parentId, NetworkAspectType aspectBits, bool bStatic)
{
	m_pState->BindObject(id, parentId, aspectBits, bStatic);
}

void CNetContext::SafelyUnbind(EntityId id)
{
	m_pState->SafelyUnbind(id);
}

bool CNetContext::IsBound(EntityId userID)
{
	return m_pState->IsBound(userID);
}

#if RESERVE_UNBOUND_ENTITIES
EntityId CNetContext::RemoveReservedUnboundEntityMapEntry(uint16 partialNetID)
{
	return m_pState->RemoveReservedUnboundEntityMapEntry(partialNetID);
}
#endif

void CNetContext::SpawnedObject(EntityId userID)
{
	m_pState->SpawnedObject(userID);
}

bool CNetContext::UnbindObject(EntityId id)
{
	return m_pState->UnbindObject(id);
}

void CNetContext::EnableAspects(EntityId id, NetworkAspectType aspectBits, bool enabled)
{
	m_pState->EnableAspects(id, aspectBits, enabled);
}

void CNetContext::ChangedAspects(EntityId id, NetworkAspectType aspectBits)
{
	m_pState->ChangedAspects(id, aspectBits);
}

#if FULL_ON_SCHEDULING
void CNetContext::ChangedTransform(EntityId id, const Vec3& pos, const Quat& rot, float drawDist)
{
	m_pState->ChangedTransform(id, pos, rot, drawDist);
}

void CNetContext::ChangedFov(EntityId id, float fov)
{
	m_pState->ChangedFov(id, fov);
}
#endif

void CNetContext::DelegateAuthority(EntityId id, INetChannel* pControlling)
{
	m_pState->DelegateAuthority(id, pControlling);
}

void CNetContext::RemoveRMIListener(IRMIListener* pListener)
{
	m_pState->RemoveRMIListener(pListener);
}

bool CNetContext::RemoteContextHasAuthority(INetChannel* pChannel, EntityId id)
{
	return m_pState->RemoteContextHasAuthority(pChannel, id);
}

void CNetContext::SetParentObject(EntityId objId, EntityId parentId)
{
	m_pState->SetParentObject(objId, parentId);
}

void CNetContext::LogBreak(const SNetBreakDescription& breakage)
{
	m_pState->LogBreak(breakage);
}

bool CNetContext::SetSchedulingParams(EntityId objId, uint32 normal, uint32 owned)
{
	return m_pState->SetSchedulingParams(objId, normal, owned);
}

void CNetContext::PulseObject(EntityId objId, uint32 pulseType)
{
	m_pState->PulseObject(objId, pulseType);
}

#ifndef OLD_VOICE_SYSTEM_DEPRECATED
IVoiceContext* CNetContext::GetVoiceContext()
{
	return GetVoiceContextImpl();
}
#endif

int CNetContext::RegisterPredictedSpawn(INetChannel* pChannel, EntityId id)
{
	return m_pState->RegisterPredictedSpawn(pChannel, id);
}

void CNetContext::RegisterValidatedPredictedSpawn(INetChannel* pChannel, int predictionHandle, EntityId id)
{
	m_pState->RegisterValidatedPredictedSpawn(pChannel, predictionHandle, id);
}

void CNetContext::RequestRemoteUpdate(EntityId id, NetworkAspectType aspects)
{
	m_pState->RequestRemoteUpdate(id, aspects);
}

void CNetContext::GetMemoryStatistics(ICrySizer* pSizer)
{
	SIZER_COMPONENT_NAME(pSizer, "CNetContext");

	pSizer->Add(*this);

	{
		SIZER_SUBCOMPONENT_NAME(pSizer, "CNetContext::m_mIPToContext");
		pSizer->AddContainer(m_mIPToContext);
	}

	{
		SIZER_SUBCOMPONENT_NAME(pSizer, "CNetContext::m_aspectNames");
		for (int i = 0; i < NumAspects; ++i)
			pSizer->AddString(m_aspectNames[i]);
	}

	if (m_pState)
		m_pState->GetMemoryStatistics(pSizer);
}

void CNetContext::GetProfilingStatistics(SNetworkProfilingStats* const pProfilingStats)
{
#ifdef ENABLE_PROFILING_CODE
	m_pState->GetProfilingStatistics(pProfilingStats);
#endif // #ifdef ENABLE_PROFILING_CODE
}

SNetObjectID CNetContext::GetNetID(EntityId userID, bool ensureNotUnbinding)
{
	return m_pState->GetNetID(userID, ensureNotUnbinding);
}

EntityId CNetContext::GetEntityID(SNetObjectID netID)
{
	return m_pState->GetEntityID(netID);
}

void CNetContext::Resaltify(SNetObjectID& netID)
{
	m_pState->Resaltify(netID);
}

#if SERVER_FILE_SYNC_MODE
void CNetContext::RegisterServerControlledFile(const char* filename)
{
	SCOPED_GLOBAL_LOCK;
	GetSyncedFileSet(true)->AddFile(filename);
}

CSyncedFileSet* CNetContext::GetSyncedFileSet(bool require)
{
	ASSERT_GLOBAL_LOCK;
	if (require && !m_pFileSet.get())
		m_pFileSet.reset(new CSyncedFileSet);
	return m_pFileSet.get();
}

static ICryPak* FallbackGetICryPak()
{
	return gEnv->pCryPak;
}

ICryPak* CNetContext::GetServerControlledICryPak()
{
	SCOPED_GLOBAL_LOCK;

	if (!ServerFileSyncEnabled())
		return FallbackGetICryPak();

	CSyncedFileSet* pSFS = GetSyncedFileSet(false);
	if (!pSFS)
		return FallbackGetICryPak();

	if (!m_pFilePak.get())
		m_pFilePak.reset(new CSyncedFilePak(*pSFS));
	return m_pFilePak.get(); // early out
}

static XmlNodeRef FallbackXmlLoad(const char* filename, const char* msg)
{
	if (msg)
		CryLog("Server synced XML load of '%s' failed, using on-disk version: %s", filename, msg);

	return gEnv->pSystem->LoadXmlFromFile(filename);
}

XmlNodeRef CNetContext::GetServerControlledXml(const char* filename)
{
	if (!ServerFileSyncEnabled())
		return FallbackXmlLoad(filename, NULL);

	CSyncedFileSet* pSFS = GetSyncedFileSet(false);
	if (!pSFS)
		return FallbackXmlLoad(filename, NULL);

	CSyncedFilePtr pFile = pSFS->GetSyncedFile(filename);
	if (!pFile)
		return FallbackXmlLoad(filename, NULL);

	CSyncedFileDataLock lk(pFile);
	if (!lk.Ok())
		return NULL;

	return gEnv->pSystem->LoadXmlFromBuffer((const char*)lk.GetData(), lk.GetLength());
}
#else // !SERVER_FILE_SYNC_MODE
void CNetContext::RegisterServerControlledFile(const char* filename)
{
}

ICryPak* CNetContext::GetServerControlledICryPak()
{
	return gEnv->pCryPak;
}

XmlNodeRef CNetContext::GetServerControlledXml(const char* filename)
{
	return gEnv->pSystem->LoadXmlFromFile(filename);
}
#endif // SERVER_FILE_SYNC_MODE
