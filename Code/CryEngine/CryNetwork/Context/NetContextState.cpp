// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "NetContextState.h"
#include "NetContext.h"
#include "Protocol/NetChannel.h"
#include "ContextView.h"
#include <CrySystem/ITextModeConsole.h>
#include "UpdateAspectDataContext.h"
#include "AutoFreeHandle.h"
#include "Config.h"

#include <CryGame/IGameFramework.h>       // LOCAL_PLAYER_ENTITY_ID
#include <CryEntitySystem/IEntitySystem.h>
#include <CryString/StringUtils.h>

#if ENABLE_DEBUG_KIT
	#include "ClientContextView.h"
#endif

#if ENABLE_NET_DEBUG_ENTITY_INFO
	#include <CryRenderer/IRenderAuxGeom.h>
#endif

static uint8 GetDefaultProfileForAspect(EntityId id, EEntityAspects aspectID)
{
	IEntity* pEntity = gEnv->pEntitySystem->GetEntity(id);
	if (!pEntity)
	{
		CRY_ASSERT_MESSAGE(gEnv->IsEditor(), "Trying to get default profile for aspect %d on unknown entity %d", aspectID, id);
		return ~uint8(0);
	}
	INetEntity* pNetEntity = pEntity->GetNetEntity();
	return pNetEntity ? pNetEntity->GetDefaultProfile(aspectID) : 0;
}

static void SetEntityAspectProfile(EntityId id, NetworkAspectType aspectBit, uint8 profile)
{
	CRY_ASSERT(0 == (aspectBit & (aspectBit - 1)));

	if (id == INVALID_ENTITYID)
	{
		// Object is unbound while waiting for profile change propagation
		return;
	}

	if (IEntity* pEntity = gEnv->pEntitySystem->GetEntity(id))
	{
		if (INetEntity* pNetEntity = pEntity->GetNetEntity())
		{
			pNetEntity->SetAspectProfile(static_cast<EEntityAspects>(aspectBit), profile, true);
			return;
		}
	}
	NetWarning("SetEntityAspectProfile: unable to find entity %08x", id);
}

CNetContextState::CNetContextState(CNetContext* pContext, int token, CNetContextState* pPrev) : m_pMMM(new CMementoMemoryManager(string().Format("NetContextState[%d]", token)))
{
	MMM_REGION(m_pMMM);

	m_pNetIDs.reset(new TNetIDMap);
	m_pLoggedBreakage.reset(new TNetIntBreakDescriptionList);

	++g_objcnt.netContextState;

	m_pContext = pContext;
	m_pGameContext = pContext->GetGameContext();
	m_token = token;
	m_multiplayer = pContext->IsMultiplayer();
	m_established = false;
	m_bInCleanup = false;
	m_bInGame = false;
	m_cleanupMember = 0;
	m_localPhysicsTime = m_pGameContext ? m_pGameContext->GetPhysicsTime() : 0.0f;
	m_spawnedObjectId = 0;

	if (pPrev && pPrev->m_vObjects.size())
	{
		m_vObjects.resize(pPrev->m_vObjects.size());
		if (m_multiplayer)
		{
			m_vObjectsEx.resize(pPrev->m_vObjectsEx.size());
		}
		for (size_t i = 0; i < m_vObjects.size(); i++)
		{
			m_vObjects[i].salt = pPrev->m_vObjects[i].salt + 2;
			AddToFreeObjects(static_cast<uint16>(i));
		}
	}

	extern void DownloadConfig();
	DownloadConfig();

	m_netAspectSendTime = 0;
	m_lastPacketSendRate = 0;
}

CNetContextState::~CNetContextState()
{
	SCOPED_GLOBAL_LOCK;
	MMM_REGION(m_pMMM);

	--g_objcnt.netContextState;

	if (m_multiplayer)
	{
		for (size_t i = 0; i < m_vObjectsEx.size(); i++)
		{
			for (int j = 0; j < NumAspects; j++)
			{
				MMM().FreeHdl(m_vObjectsEx[i].vAspectData[j]);
				MMM().FreeHdl(m_vObjectsEx[i].vRemoteAspectData[j]);
			}
		}
	}

	m_vObjects.resize(0);
	m_vObjectsEx.resize(0);

	m_pNetIDs.reset();
	m_pLoggedBreakage.reset();

	m_allEstablishers.clear();
	m_currentEstablishers.clear();
}

void CNetContextState::Die()
{
	MMM_REGION(m_pMMM);

	// take control of everything
	for (uint32 i = 0; i < m_vObjects.size(); i++)
	{
		if (m_vObjects[i].bOwned && m_vObjects[i].userID)
		{
			ASSERT_PRIMARY_THREAD;
			NC_DelegateAuthority(m_vObjects[i].userID, NULL);
		}
	}
	// forget changes
	m_vNetChangeLog.clear();
	m_vNetChangeLogUnique.clear();

	NET_ASSERT(m_pContext != NULL);
	m_pContext = 0;
}

bool CNetContextState::IsDead()
{
	return m_pContext == 0;
}

void CNetContextState::AttachRMILogger(IRMILogger* pLogger)
{
	ASSERT_GLOBAL_LOCK;
	stl::push_back_unique(m_rmiLoggers, pLogger);
}

void CNetContextState::DetachRMILogger(IRMILogger* pLogger)
{
	ASSERT_GLOBAL_LOCK;
	m_rmiLoggers.erase(
	  std::remove(m_rmiLoggers.begin(), m_rmiLoggers.end(), pLogger),
	  m_rmiLoggers.end());
}

void CNetContextState::ChangeSubscription(INetContextListenerPtr pListener, unsigned events)
{
	MMM_REGION(m_pMMM);

	ASSERT_GLOBAL_LOCK;
	HandleSubscriptionDelta(pListener, ChangeSubscription(m_subscriptions, pListener, events), events);
}

void CNetContextState::ChangeSubscription(INetContextListenerPtr pListener, INetChannel* pChannel, unsigned events)
{
	MMM_REGION(m_pMMM);

	ASSERT_GLOBAL_LOCK;
	ChangeSubscription(m_channelSubscriptions[pChannel], pListener, events);
}

unsigned CNetContextState::ChangeSubscription(TSubscriptions& subscriptions, INetContextListenerPtr pListener, unsigned events)
{
	ASSERT_GLOBAL_LOCK;
	for (TSubscriptions::iterator iter = subscriptions.begin(); iter != subscriptions.end(); ++iter)
	{
		if (iter->pListener == pListener)
		{
			unsigned out = iter->events;
			if (!events)
			{
				subscriptions.erase(iter);
			}
			else
			{
				iter->events = events;
			}
			return out;
		}
	}
	if (events)
		subscriptions.push_back(SSubscription(pListener, events));
	return 0;
}

void CNetContextState::BroadcastChannelEvent(INetChannel* pFrom, SNetChannelEvent* pEvent)
{
	MMM_REGION(m_pMMM);

	ASSERT_GLOBAL_LOCK;
	std::map<INetChannel*, TSubscriptions>::const_iterator iter;
	if (pFrom)
	{
		iter = m_channelSubscriptions.find(pFrom);
		if (iter != m_channelSubscriptions.end())
			BroadcastChannelEventTo(iter->second, pFrom, pEvent);
	}
	iter = m_channelSubscriptions.find(static_cast<INetChannel*>(NULL));
	if (iter != m_channelSubscriptions.end())
		BroadcastChannelEventTo(iter->second, pFrom, pEvent);
}

void CNetContextState::BroadcastChannelEventTo(const TSubscriptions& subscriptions, INetChannel* pFrom, SNetChannelEvent* pEvent)
{
	ASSERT_GLOBAL_LOCK;
	m_tempSubscriptions = subscriptions;
	for (TSubscriptions::const_iterator iter = m_tempSubscriptions.begin(); iter != m_tempSubscriptions.end(); ++iter)
		if (iter->events & pEvent->event)
			iter->pListener->OnChannelEvent(this, pFrom, pEvent);
	m_tempSubscriptions.resize(0);
}

ILINE void CNetContextState::SendEventTo(SNetObjectEvent* event, INetContextListener* pListener)
{
	MMM_REGION(0);

#if LOG_NETCONTEXT_MESSAGES
	NetLog("[netobjmsg] %s -> %s", GetEventName(event->event).c_str(), pListener->GetName().c_str());
#endif
	pListener->OnObjectEvent(this, event);
}

void CNetContextState::HandleSubscriptionDelta(INetContextListenerPtr pListener, unsigned oldEvents, unsigned newEvents)
{
	ASSERT_GLOBAL_LOCK;
	if (TurnedOnBit(eNOE_RemoveStaticEntity, oldEvents, newEvents))
		SendRemoveStaticEntitiesTo(pListener);
	if (TurnedOnBit(eNOE_BindObject, oldEvents, newEvents))
		SendBindEventsTo(pListener, TurnedOnBit(eNOE_SetAuthority, oldEvents, newEvents));
	if (TurnedOnBit(eNOE_BindAspects, oldEvents, newEvents))
		SendBindAspectsTo(pListener);
	if (TurnedOnBit(eNOE_SetAspectProfile, oldEvents, newEvents))
		SendSetAspectProfileEventsTo(pListener);
	if (TurnedOnBit(eNOE_GotBreakage, oldEvents, newEvents))
		SendBreakageTo(pListener);
	if (m_bInGame && TurnedOnBit(eNOE_InGame, oldEvents, newEvents))
	{
		SNetObjectEvent event;
		event.event = eNOE_InGame;
		SendEventTo(&event, pListener);
	}

	if (IsContextEstablished() && TurnedOnBit(eNOE_EstablishedContext, oldEvents, newEvents))
	{
		SNetObjectEvent event;
		event.event = eNOE_EstablishedContext;
		SendEventTo(&event, pListener);
	}
}

void CNetContextState::SendRemoveStaticEntitiesTo(INetContextListenerPtr pListener)
{
	for (std::vector<EntityId>::iterator iter = m_removedStaticEntities.begin(); iter != m_removedStaticEntities.end(); ++iter)
	{
		SNetObjectEvent event;
		event.event = eNOE_RemoveStaticEntity;
		event.userID = *iter;
		SendEventTo(&event, pListener);
	}
}

void CNetContextState::SendBindEventsTo(INetContextListenerPtr pListener, bool alsoAuth)
{
	if (!m_pContext)
	{
		return;
	}

	ASSERT_GLOBAL_LOCK;
	SNetObjectEvent event;
	event.event = eNOE_BindObject;
	for (TContextObjects::iterator iter = m_vObjects.begin(); iter != m_vObjects.end(); ++iter)
	{
		if (iter->bAllocated)
		{
			uint16 idx = (uint16)(iter - m_vObjects.begin());
			event.id = SNetObjectID(idx, iter->salt);

			SendEventTo(&event, pListener);

			if (alsoAuth && iter->pController == pListener)
			{
				event.event = eNOE_SetAuthority;
				event.authority = true;
				SendEventTo(&event, pListener);
				event.event = eNOE_BindObject;
			}
		}
	}
}

void CNetContextState::SendSetAspectProfileEventsTo(INetContextListenerPtr pListener)
{
	if (!m_pContext)
		return;

	ASSERT_GLOBAL_LOCK;
	SNetObjectEvent event;
	event.event = eNOE_SetAspectProfile;
	for (size_t it = 0; it < m_vObjects.size(); ++it)
	{
		if (m_vObjects[it].bAllocated)
		{
			event.id = SNetObjectID(static_cast<uint16>(it), m_vObjects[it].salt);
			CBitIter itAspects(m_pContext->ServerManagedProfileAspects());
			NetworkAspectID i;
			while (itAspects.Next(i))
			{
				if (m_vObjects[it].vAspectProfiles[i] != m_vObjects[it].vAspectDefaultProfiles[i])
				{
					event.aspectID = i;
					event.profile = m_vObjects[it].vAspectProfiles[i];
					SendEventTo(&event, pListener);
				}
			}
		}
	}
}

void CNetContextState::SendBreakageTo(INetContextListenerPtr pListener)
{
	ASSERT_GLOBAL_LOCK;
	SNetObjectEvent event;
	event.event = eNOE_GotBreakage;
	for (TNetIntBreakDescriptionList::iterator it = m_pLoggedBreakage->begin(); it != m_pLoggedBreakage->end(); ++it)
	{
		event.pBreakage = &*it;
		SendEventTo(&event, pListener);
	}
}

void CNetContextState::Broadcast(SNetObjectEvent* pEvent)
{
	ASSERT_GLOBAL_LOCK;
	for (TSubscriptions::iterator iter = m_subscriptions.begin(); iter != m_subscriptions.end(); ++iter)
		if (iter->events & pEvent->event)
			SendEventTo(pEvent, iter->pListener);
}

#if ENABLE_THIN_BINDS
void CNetContextState::UpdateBindAspectMask(SNetObjectID& netID, NetworkAspectType dirtyAspects)
{
	if (m_multiplayer)
	{
		SContextObjectEx& objx = m_vObjectsEx[netID.id];
		objx.bindAspectMask |= dirtyAspects;
	}
}

NetworkAspectType CNetContextState::GetBindAspectMask(SNetObjectID& netID)
{
	NetworkAspectType aspectMask = NET_ASPECT_ALL;

	if (m_multiplayer)
	{
		SContextObjectEx& objx = m_vObjectsEx[netID.id];
		aspectMask = objx.bindAspectMask;
	}

	return aspectMask;
}
#endif // ENABLE_THIN_BINDS

bool CNetContextState::SendEventToChannelListener(INetChannel* pIChannel, SNetObjectEvent* pEvent)
{
	ASSERT_GLOBAL_LOCK;
	if (pIChannel)
	{
		CNetChannel* pCChannel = (CNetChannel*)pIChannel;
		if (INetContextListenerPtr pListener = pCChannel->GetContextView())
			return SendEventToListener(pListener, pEvent);
	}
	return false;
}

bool CNetContextState::SendEventToListener(INetContextListenerPtr pListener, SNetObjectEvent* pEvent)
{
	ASSERT_GLOBAL_LOCK;
#if ENABLE_DEBUG_KIT
	// temporary debug hack
	if (pEvent->event == eNOE_SetAuthority && CVARS.LogLevel > 0)
		NetLog("Send SetAuthority(%d) event to %s", pEvent->authority, pListener->GetName().c_str());
	// ~temporary debug hack
#endif

	bool accepts = false;
	for (TSubscriptions::iterator iter = m_subscriptions.begin(); iter != m_subscriptions.end(); ++iter)
	{
		if (iter->pListener == pListener)
		{
			accepts = (iter->events & pEvent->event) != 0;
			if (accepts)
				SendEventTo(pEvent, iter->pListener);
			break;
		}
	}
#if ENABLE_DEBUG_KIT
	if (!accepts)
	{
		NetComment("Could not send event %s to specific listener %s", GetEventName(pEvent->event).c_str(), pListener->GetName().c_str());
	}
#endif
	return false;
}

void CNetContextState::EstablishedContext()
{
	NET_ASSERT(!IsContextEstablished());

	m_established = true;

	SNetObjectEvent event;
	event.event = eNOE_EstablishedContext;
	Broadcast(&event);
}

void CNetContextState::FetchAndPropogateChangesFromGame(bool allowFetch)
{
	MMM_REGION(m_pMMM);

	//cleanup cache, as all FROM_GAME queue items have been executed by that time
	m_aspectProfileCache.resize(0);

	// if we don't need any changes, just empty the changed object set
	if (!m_pContext || !m_multiplayer)
	{
		m_vGameChangedObjects.Flush();
		return;
	}

#if !NEW_BANDWIDTH_MANAGEMENT
	int psr = CNetCVars::Get().PacketSendRate;

	if (psr != 0)
	{
		int64 currentPacketSendTime = (gEnv->pTimer->GetAsyncTime().GetMilliSecondsAsInt64() / (1000 / psr));

		if (m_lastPacketSendRate != psr)
		{
			// force a send this time through
			m_netAspectSendTime = 0;
			m_lastPacketSendRate = psr;
		}

		if (currentPacketSendTime <= m_netAspectSendTime)
			return;

		m_netAspectSendTime = currentPacketSendTime;
	}
#endif // !NEW_BANDWIDTH_MANAGEMENT

	// if there's any changes that have been flagged by the game
	CChangeList<SNetObjectAspectChange>::Objects& changed = m_vGameChangedObjects.GetObjects();
	if (!changed.empty())
	{
		// fetch changes from game
		CRY_PROFILE_FUNCTION(PROFILE_NETWORK);

		// we'll need the timestamp (probably)
		CTimeValue newPhysicsTime = m_pGameContext->GetPhysicsTime();
		if (newPhysicsTime < m_localPhysicsTime)
			NetWarning("Local physics time went backwards");
		else
			m_localPhysicsTime = newPhysicsTime;

		NET_ASSERT(m_changeBuffer.empty());
		for (size_t i = 0; i < changed.size(); i++)
		{
			if (!allowFetch || IsLocked(changed[i].first, eCOL_GameDataSync))
			{
				// we're not allowed to sync this object at this point
				// so remember what had changed
				m_changeBuffer.push_back(changed[i]);
				// and clear the change mask for now (it'll get restored later)
				changed[i].second.aspectsChanged = 0;
			}
			else
			{
				NetworkAspectType aspectsChanged = changed[i].second.aspectsChanged;

#if FILTER_DELEGATED_ASPECTS
				// this is here to stop the server from 'updating' aspects it has delegated to clients with it's own version of the data
				if (gEnv->bServer)
				{
					SContextObject& obj = m_vObjects[changed[i].first.id];
					if (obj.pController) // if valid, we have delegated the entity to a client
					{
						// we don't want to filter out changes if the server's client channel is
						// responsible for the change, doing so means we don't get and forward the data to clients
						INetChannel* pChannel = obj.pController->GetAssociatedChannel();
						if (pChannel && !pChannel->IsLocal())
						{
							SContextObjectEx& objx = m_vObjectsEx[changed[i].first.id];
							aspectsChanged &= ~NetworkAspectType(m_pContext->DelegatableAspects() & objx.delegatableMask);
						}
					}
				}
#endif
				// ask the game for the data that has changed
				NetworkAspectType aspectBuffersUpdated = UpdateAspectData(changed[i].first, aspectsChanged);
				// what we tell channels have changed is what we've been forced to, and what's really changed
				changed[i].second.aspectsChanged = changed[i].second.forceChange | aspectBuffersUpdated;
			}
		}

		// add a terminator
		changed.push_back(std::make_pair(SNetObjectID(), SNetObjectAspectChange()));

		// broadcast the event
		SNetObjectEvent event;
		event.event = eNOE_ObjectAspectChange;
		event.pChanges = &changed[0];
		Broadcast(&event);

		// clear the change list
		m_vGameChangedObjects.Flush();

		// and re-add anything that we couldn't deal with this frame
		while (!m_changeBuffer.empty())
		{
			m_vGameChangedObjects.AddChange(m_changeBuffer.back().first, m_changeBuffer.back().second);
			m_changeBuffer.pop_back();
		}
	}

	bool bIsMultithreaded = CNetwork::Get()->IsMultithreaded();
	for (uint32 i = 0; i < m_subscriptions.size(); /* intentionally left blank */)
	{
		INetContextListenerPtr pListener = m_subscriptions[i].pListener;
		bool removed = false;

		if (pListener)
		{
			INetChannel* pChannel = pListener->GetAssociatedChannel();
			if (pChannel)
			{
				if (bIsMultithreaded)
				{
					pChannel->RequestUpdate(g_time);
				}
				else
				{
					pChannel->CallUpdate(g_time);
				}
				removed = pListener->IsDead();
			}
		}

		if (!removed)
		{
			++i;
		}
	}
}

void CNetContextState::PropogateProfileChangesToGame()
{
	if (!m_changedProfiles.empty())
	{
		for (TChangedProfiles::iterator it = m_changedProfiles.begin(); it != m_changedProfiles.end(); ++it)
		{
			ASSERT_PRIMARY_THREAD;
			SetEntityAspectProfile(m_vObjects[it->obj.GetID().id].userID, it->aspect, it->profile);
			UnlockObject(it->obj.GetID(), eCOL_GameDataSync);
		}
		m_changedProfiles.resize(0);
	}
}

#define NO_CLIENT_FORCE_ASPECT_CHANGES (1)

class CNetContextState::CPropogateChangesToGameContext
{
public:
	CPropogateChangesToGameContext(CNetContextState& state)
		: m_state(state)
		, m_timestampedAspects(m_state.m_pContext->TimestampedAspects())
		, m_isTimestamped(false)
	{
		NET_ASSERT(m_state.m_vNetChangeLogUnique.empty());

		std::vector<SChannelChange>::iterator itUnique = state.m_vNetChangeLog.begin();
		std::vector<SChannelChange>::iterator itEnd = state.m_vNetChangeLog.end();

		std::sort(itUnique, itEnd, SChannelChange::SCompareObjectsThenReceiveTime());

		std::vector<SChannelChange>& out = state.m_vNetChangeLogUnique;
		out.push_back(*itUnique++);
		SChannelChange* pOutBack = &out.back();

		for (; itUnique != itEnd; ++itUnique)
		{
			if (itUnique->aspectIdx == pOutBack->aspectIdx)
				if (itUnique->obj == pOutBack->obj)
					continue;
			out.push_back(*itUnique);
			pOutBack = &out.back();
		}

		m_pCurChange = 0;
		m_pNextChange = out.begin();
		m_pEndChange = out.end();
		std::sort(m_pNextChange, m_pEndChange, SChannelChange::SCompareChannelThenTimeThenObject());
	}

	bool Next()
	{
		while (m_pNextChange != m_pEndChange)
		{
			m_pCurChange = &*m_pNextChange;
			++m_pNextChange;

			m_obj = m_state.GetContextObject(m_pCurChange->obj);
			if (!m_obj.main)
				continue;
			NET_ASSERT(m_obj.xtra);
			if (GetRemoteAspectDataHandle() == CMementoMemoryManager::InvalidHdl)
				continue;

			m_isTimestamped = 0 != (m_timestampedAspects & BIT(m_pCurChange->aspectIdx));

			return true;
		}
		return false;
	}

	TMemHdl& GetRemoteAspectDataHandle()
	{
		return const_cast<TMemHdl&>(m_obj.xtra->vRemoteAspectData[m_pCurChange->aspectIdx]);
	}

	TMemHdl& GetTargetAspectDataHandle()
	{
		return const_cast<TMemHdl&>(m_obj.xtra->vAspectData[m_pCurChange->aspectIdx]);
	}

	bool IsTimestamped()
	{
		return m_isTimestamped;
	}

	CNetChannel* GetChannel()
	{
		return m_pCurChange->pChannel;
	}

	CTimeValue GetFrameTime()
	{
		return m_pCurChange->channelTime;
	}

	NetworkAspectID GetAspectIndex()
	{
		return m_pCurChange->aspectIdx;
	}

	NetworkAspectType GetAspectBit()
	{
		return BIT(GetAspectIndex());
	}

	uint8 GetAspectProfile()
	{
		return m_obj.main->vAspectProfiles[GetAspectIndex()];
	}

	ChunkID GetChunkID()
	{
		return GetChunkIDFromObject(m_obj, GetAspectIndex());
	}

	const SNetObjectID& GetObjId()
	{
		return m_pCurChange->obj;
	}

	EntityId GetUserID()
	{
		return m_obj.main->userID;
	}

	void IncAspectDataVersion()
	{
		const_cast<uint32&>(m_obj.xtra->vAspectDataVersion[GetAspectIndex()])++;
	}

	bool ShouldTakeUpdate(TMemHdl tgtHdl, TMemHdl newHdl)
	{
		bool take = true;
		size_t newSize = MMM().GetHdlSize(newHdl);
		if (tgtHdl != CMementoMemoryManager::InvalidHdl && MMM().GetHdlSize(tgtHdl) == newSize)
		{
			int ofs = 0;
			if (IsTimestamped())
				ofs += sizeof(CTimeValue);
			const uint8* pOld = (const uint8*)MMM().PinHdl(tgtHdl);
			const uint8* pNew = (const uint8*)MMM().PinHdl(newHdl);
			int cmpSz = newSize - ofs;
			if (cmpSz > 0 && 0 == memcmp(pOld + ofs, pNew + ofs, cmpSz))
				take = false;
			else if (!cmpSz)
				take = false;
		}
		return take;
	}

	void Disconnect_ContextCorruption()
	{
		char buf[256];
		cry_sprintf(buf, "Failed to commit data from network to game! obj(eid:%.8x name:%s netid:%s aspect:%s)", GetUserID(), m_obj.main->GetName(), m_pCurChange->obj.GetText(), m_state.m_pContext->GetAspectName(GetAspectIndex()));
		m_pCurChange->pChannel->Disconnect(eDC_ContextCorruption, buf);
	}

	void SendPartialUpdate()
	{
		SNetObjectEvent evt;
		evt.event = eNOE_PartialUpdate;
		evt.aspectID = GetAspectIndex();
		evt.id = GetObjId();
		m_state.SendEventToChannelListener(m_pCurChange->pChannel, &evt);
	}

	bool SendToGameAndUpdateFreeHandleWith(CByteInputStream& in, CAutoFreeHandle& freeHdl, CCompressionManager& cman)
	{
		bool wasPartialUpdate = false; // in-out
		EntityId userId = GetUserID();
		NetworkAspectType aspectBit = GetAspectBit();
		uint8 aspectProfile = GetAspectProfile();
		ChunkID chunkId = GetChunkID();
		ESynchObjectResult sor = cman.GameContextWrite(m_state.m_pGameContext, &in, userId, aspectBit, aspectProfile, chunkId, wasPartialUpdate);
		TMemHdl& tgtHdl = GetTargetAspectDataHandle();
		switch (sor)
		{
		case eSOR_Failed:
			Disconnect_ContextCorruption();
			return false;
		case eSOR_Ok:
#if NO_CLIENT_FORCE_ASPECT_CHANGES
			if (gEnv->bServer)
#endif
			{
				m_state.m_vGameChangedObjects.AddChange(GetObjId(), SNetObjectAspectChange(0, aspectBit));
			}

#if HIGH_PRIORITY_LOGGING
			if (aspectBit & HIGH_PRIORITY_ASPECT_MASK)
			{
				NetLog("[PlayerUpdate]: Received player input aspect data for %s", m_obj.main->GetName());
			}
#endif // HIGH_PRIORITY_LOGGING

			if (wasPartialUpdate)
				SendPartialUpdate();
			if (ShouldTakeUpdate(tgtHdl, freeHdl.Peek()))
			{
				MMM().FreeHdl(tgtHdl);
				tgtHdl = freeHdl.Grab();
				IncAspectDataVersion();
				if (IsTimestamped())
					*static_cast<CTimeValue*>(MMM().PinHdl(tgtHdl)) = m_state.m_localPhysicsTime;
			}
			return true;
		case eSOR_Skip:
			return true;
		default:
			NET_ASSERT(false);
			return false;
		}
	}

#if LOG_BUFFER_UPDATES
	void Debug_LogPush()
	{
		if (CNetCVars::Get().LogBufferUpdates)
			NetLog("[buf] push %s:%s", m_pCurChange->obj.GetText(), m_state.m_pContext->GetAspectName(m_pCurChange->aspect));
	}
#else
	ILINE void Debug_LogPush() {}
#endif

private:
	CNetContextState&                     m_state;
	SChannelChange*                       m_pCurChange;
	std::vector<SChannelChange>::iterator m_pEndChange;
	std::vector<SChannelChange>::iterator m_pNextChange;
	SContextObjectRef                     m_obj;

	bool                    m_isTimestamped;

	const NetworkAspectType m_timestampedAspects;
};

#if ENABLE_DEBUG_KIT
class CNetContextState::CDebugKit_WorldUpdate
{
public:
	static const int DODGYHACK_PHYSICS_ASPECT_INDEX = 3;

	CDebugKit_WorldUpdate(CPropogateChangesToGameContext& ctx) : m_objId(ctx.GetObjId())
	{
		m_pClientContextView = NULL;

		if (ctx.GetAspectIndex() != DODGYHACK_PHYSICS_ASPECT_INDEX)
			return;
		if (!ctx.GetChannel())
			return;
		CContextView* pCtxView = ctx.GetChannel()->GetContextView();
		if (!pCtxView->IsClient())
			return;
		m_pClientContextView = static_cast<CClientContextView*>(pCtxView);
		m_pClientContextView->BeginWorldUpdate(m_objId);
	}

	~CDebugKit_WorldUpdate()
	{
		if (m_pClientContextView)
			m_pClientContextView->EndWorldUpdate(m_objId);
	}

private:
	SNetObjectID        m_objId;
	CClientContextView* m_pClientContextView;
};
#else
class CNetContextState::CDebugKit_WorldUpdate
{
public:
	ILINE CDebugKit_WorldUpdate(CPropogateChangesToGameContext& ctx) {}
};
#endif

class CChannelAndTimeTracker
{
public:
	CChannelAndTimeTracker(IGameContext* pGameContext) : m_pGameContext(pGameContext), m_pChannel(), m_frameTime(0.0f)
	{
	}

	~CChannelAndTimeTracker()
	{
		if (m_pChannel)
			m_pGameContext->EndUpdateObjects();
	}

	bool UpdateTimestampedAspect(CNetChannel* pChannel, CTimeValue frameTime)
	{
		if (pChannel->IsDead())
			return false;

		if (pChannel == m_pChannel && frameTime == m_frameTime)
			return true;

		if (m_pChannel)
			m_pGameContext->EndUpdateObjects();

		m_pChannel = pChannel;
		m_frameTime = frameTime;

		m_pGameContext->BeginUpdateObjects(frameTime, pChannel);

#if LOG_BUFFER_UPDATES
		if (CNetCVars::Get().LogBufferUpdates)
			NetLog("[sync] channel frame time is %f", frameTime.GetMilliSeconds());
#endif

		return true;
	}

	void UpdateNormalAspect()
	{
		if (m_pChannel)
		{
			m_pGameContext->EndUpdateObjects();
			m_pChannel = 0;
		}
		m_frameTime = 0.0f;
	}

	void ReadTimeFromStream(CByteInputStream& in)
	{
		CTimeValue when = in.GetTyped<CTimeValue>(); // skip time value for now
#if LOG_BUFFER_UPDATES
		if (CNetCVars::Get().LogBufferUpdates)
			NetLog("[sync] aspect time is %f", when.GetMilliSeconds());
#endif
#if ENABLE_DEBUG_KIT
		if (when != m_frameTime)
			NetQuickLog(true, 1, "Physics update commit %fms late", (when - m_frameTime).GetMilliSeconds());
#endif
	}

private:
	IGameContext*           m_pGameContext;
	_smart_ptr<CNetChannel> m_pChannel;
	CTimeValue              m_frameTime;
};

void CNetContextState::PropogateChangesToGame()
{
	CRY_PROFILE_FUNCTION(PROFILE_NETWORK);
	MMM_REGION(m_pMMM);

	if (!m_pContext || m_vNetChangeLog.empty())
		return;

	PropogateProfileChangesToGame();

	CPropogateChangesToGameContext ctx(*this);
	CChannelAndTimeTracker chantime(m_pGameContext);
	CCompressionManager& cman = CNetwork::Get()->GetCompressionManager();
	while (ctx.Next())
	{
		CAutoFreeHandle freeHdl(ctx.GetRemoteAspectDataHandle());

		if (!ctx.GetUserID())
			continue;

		if (ctx.IsTimestamped())
		{
			if (!chantime.UpdateTimestampedAspect(ctx.GetChannel(), ctx.GetFrameTime()))
				continue;
		}
		else
			chantime.UpdateNormalAspect();

		ctx.Debug_LogPush();

		TMemHdl remHdl = freeHdl.Peek();
		const uint8* remData = (const uint8*) MMM().PinHdl(remHdl);
		size_t remSize = MMM().GetHdlSize(remHdl);
		CByteInputStream in(remData, remSize);
		CDebugKit_WorldUpdate dbg_worldupdate(ctx);
		if (ctx.IsTimestamped())
			chantime.ReadTimeFromStream(in);
		if (!ctx.SendToGameAndUpdateFreeHandleWith(in, freeHdl, cman))
			break;
	}

	m_vNetChangeLog.resize(0);
	m_vNetChangeLogUnique.resize(0);
}

void CNetContextState::SendBindAspectsTo(INetContextListenerPtr pListener)
{
	if (!m_pContext)
		return;

	ASSERT_GLOBAL_LOCK;
	SNetObjectEvent event;
	event.event = eNOE_BindAspects;

	// TODO: check the logic here, it should only make sence for multiplayer game - Lin
	if (m_multiplayer)
	{
		for (size_t netID = 0; netID < m_vObjects.size(); netID++)
		{
			if (!m_vObjects[netID].bAllocated)
				continue;

			SContextObjectEx& objx = m_vObjectsEx[netID];
			event.id = SNetObjectID(static_cast<uint16>(netID), m_vObjects[netID].salt);
			event.aspects = objx.nAspectsEnabled;
			SendEventTo(&event, pListener);
		}
	}
}

void CNetContextState::SetParentObject(EntityId objId, EntityId parentId)
{
	FROM_GAME(&CNetContextState::GC_SetParentObject, this, objId, parentId);
}

SContextObjectRef CNetContextState::GetContextObject_Slow(SNetObjectID nID) const
{
	SContextObjectRef obj;
	if (m_vObjects.size() <= nID.id || !m_vObjects[nID.id].bAllocated || m_vObjects[nID.id].salt != nID.salt)
		return obj;
	obj.main = &m_vObjects[nID.id];

	static SContextObjectEx objx;
	obj.xtra = m_multiplayer ? &m_vObjectsEx[nID.id] : &objx;

	m_cacheObjectID = nID;
	m_cacheObjectRef = obj;

	return obj;
}

SNetObjectID CNetContextState::GetNetID(EntityId userID, bool ensureNotUnbinding)
{
	SNetObjectID id = stl::find_in_map(*m_pNetIDs, userID, SNetObjectID());
	if (ensureNotUnbinding && id)
		if (!m_vObjects[id.id].userID)
			id = SNetObjectID();
	if (id && m_vObjects[id.id].salt != id.salt)
		id = SNetObjectID();
	return id;
}

EntityId CNetContextState::GetEntityID(SNetObjectID netID)
{
	SContextObjectRef obj = GetContextObject(netID);
	if (obj.main)
		return obj.main->userID;
	else
		return 0;
}

void CNetContextState::CacheAspectProfileChange(SNetObjectID id, NetworkAspectType aspect, uint8 profile)
{
	for (TAspectProfileCache::iterator it = m_aspectProfileCache.begin(), eit = m_aspectProfileCache.end(); it != eit; ++it)
	{
		if (it->id == id && it->aspect == aspect)
		{
			it->profile = profile;
			return;
		}
	}
	SAspectProfileCacheEntry e;
	e.id = id;
	e.aspect = aspect;
	e.profile = profile;
	m_aspectProfileCache.push_back(e); //will be cleaned up in FetchAndPropogateChangesFromGame
}

bool CNetContextState::GetAspectProfileFromCache(SNetObjectID id, NetworkAspectType aspect, uint8& profile)
{
	for (TAspectProfileCache::iterator it = m_aspectProfileCache.begin(), eit = m_aspectProfileCache.end(); it != eit; ++it)
	{
		if (it->id == id && it->aspect == aspect)
		{
			profile = it->profile;
			return true;
		}
	}
	return false;
}

void CNetContextState::SetAspectProfile(EntityId id, NetworkAspectType aspectBit, uint8 profile)
{
	if (!m_pContext)
		return;

	SCOPED_GLOBAL_LOCK;
	SNetObjectID netID = GetNetID(id);
	bool lockedUpdate = false;

	if (profile > MaxProfilesPerAspect)
	{
		SContextObjectRef obj = GetContextObject(netID);
#if ENABLE_DEBUG_KIT
		if (CNetCVars::Get().LogLevel)
			NetWarning("WARNING: Illegal profile %d passed for aspect %s on entity %s (%d)", profile, m_pContext->GetAspectName(BitIndex(aspectBit)), obj.main ? obj.main->GetName() : "<<unknown>>", id);
#endif
		return;
	}

	if (!netID || !GetContextObject(netID).main)
	{
		// not sure if this is the correct thing to do, but should work
		// if the object is not bound, allow the SetAspectProfile to succeed anyway
		ASSERT_PRIMARY_THREAD;
		SetEntityAspectProfile(id, aspectBit, profile);
	}
	else
	{
		SContextObject& obj = m_vObjects[netID.id];
		if (obj.bOwned)
		{
			ASSERT_PRIMARY_THREAD;
			SetEntityAspectProfile(id, aspectBit, profile);
			LockObject(netID, eCOL_GameDataSync);
			lockedUpdate = true;
		}

		CacheAspectProfileChange(netID, aspectBit, profile); //unsuccessful set will be still cached but will generate warning anyway in DoSetAspectPorfile, so will be wrong anyway
	}

	FROM_GAME(&CNetContextState::NC_SetAspectProfile, this, id, aspectBit, profile, lockedUpdate);
}

void CNetContextState::NC_SetAspectProfile(EntityId userID, NetworkAspectType aspectBit, uint8 profile, bool lockedUpdate)
{
	ASSERT_GLOBAL_LOCK;
	DoSetAspectProfile(userID, aspectBit, profile, true, true, lockedUpdate);
}

uint8 CNetContextState::GetAspectProfile(EntityId userID, NetworkAspectType aspectBit)
{
	SCOPED_GLOBAL_LOCK;

	//[K01]: This very dangerous operation was replaced with aspect cache list
	// force all from-game messages out of the queue
	//if(!gEnv->pSystem->IsSerializingFile())
	//	gEnv->pNetwork->SyncWithGame(eNGS_FrameEnd);
	//[K01]

	SNetObjectID netID = GetNetID(userID);

	if (!netID || !GetContextObject(netID).main)
	{
		return 255;
	}

	NET_ASSERT(GetContextObject(netID).main);
	const SContextObject& obj = m_vObjects[netID.id];

	if (CountBits(aspectBit) != 1)
	{
		CryFatalError("Can only set one aspect profile at a time");
		return 255;
	}
	if (m_pContext && (m_pContext->ServerManagedProfileAspects() & aspectBit) == 0)
	{
#if ENABLE_DEBUG_KIT
		if (CNetCVars::Get().LogLevel > 2)
			NetWarning("aspect %s is not server managed", m_pContext ? m_pContext->GetAspectName(BitIndex(aspectBit)) : "unknown");
#endif
		return 255;
	}

	NetworkAspectID aspectIdx = BitIndex(aspectBit);

	uint8 cached_profile;
	if (GetAspectProfileFromCache(netID, aspectBit, cached_profile))
	{
		if (cached_profile != obj.vAspectProfiles[aspectIdx])
		{
			NetLogAlways("[Cached Aspect Mismatch] Cached aspect profile is used on %s, aspect: %x real %d, cached %d", netID.GetText(), aspectIdx, obj.vAspectProfiles[aspectIdx], cached_profile);
		}
		return cached_profile;
	}

	return obj.vAspectProfiles[aspectIdx];
}

void CNetContextState::LogChangedProfile(SNetObjectID id, NetworkAspectType aspect, uint8 profile)
{
	bool found = false;
	for (TChangedProfiles::iterator it = m_changedProfiles.begin(); it != m_changedProfiles.end(); ++it)
	{
		if (it->obj.GetID() == id && it->aspect == aspect)
		{
			found = true;
			if (it->profile == profile)
				return;
			else
			{
				m_changedProfiles.erase(it);
				break;
			}
		}
	}
	if (!found)
		LockObject(id, eCOL_GameDataSync);
	SChangedProfile p = { CNetObjectBindLock(this, id, "CHANGEPROFILE"), aspect, profile };
	m_changedProfiles.push_back(p);
}

bool CNetContextState::DoSetAspectProfile(EntityId userID, NetworkAspectType aspectBit, uint8 profile, bool checkOwnership, bool informedGame, bool unlockUpdate)
{
	MMM_REGION(m_pMMM);

	ASSERT_GLOBAL_LOCK;
	SNetObjectID netID = GetNetID(userID);

	if (!netID || !GetContextObject(netID).main)
		return false;

	SContextObjectRef obj = GetContextObject(netID);
	NET_ASSERT(obj.main);

	//NetLog("DoSetAspectProfile:%.8x:%.2x:%.2x:%d:%d:%d:obj_%.4x:chnk_%.4d:%s",userID,aspectBit,profile,checkOwnership,informedGame,unlockUpdate,netID,obj.vAspectProfileChunks[BitIndex(aspectBit)][profile],obj.GetLockString());

	if (CountBits(aspectBit) != 1)
	{
#if ENABLE_DEBUG_KIT
		CryFatalError("Can only set one aspect profile at a time");
#endif
		return false;
	}
	if (m_pContext && (m_pContext->ServerManagedProfileAspects() & aspectBit) == 0)
	{
		NetWarning("aspect %s is not server managed", m_pContext ? m_pContext->GetAspectName(BitIndex(aspectBit)) : "unknown");
		return false;
	}

	if (checkOwnership && !obj.main->bOwned)
	{
		NetWarning("Trying to change aspect profile on %s to %d but we are not the owner", m_pContext ? m_pContext->GetAspectName(BitIndex(aspectBit)) : "unknown", profile);
		return false;
	}

	if (!informedGame)
		LogChangedProfile(netID, aspectBit, profile);
	if (unlockUpdate)
		UnlockObject(netID, eCOL_GameDataSync);

	NetworkAspectID aspectIdx = BitIndex(aspectBit);

	SNetObjectEvent event;
	event.event = eNOE_SetAspectProfile;
	event.id = netID;
	event.aspectID = aspectIdx;
	event.profile = profile;
	Broadcast(&event);

	const_cast<SContextObject*>(obj.main)->vAspectProfiles[aspectIdx] = profile;

	if (m_multiplayer)
	{
		if (obj.xtra->vAspectData[aspectIdx] != CMementoMemoryManager::InvalidHdl)
		{
			MMM().FreeHdl(obj.xtra->vAspectData[aspectIdx]);
			const_cast<SContextObjectEx*>(obj.xtra)->vAspectData[aspectIdx] = CMementoMemoryManager::InvalidHdl;
		}
		MarkObjectChanged(netID, aspectBit);
	}

	return true;
}

void CNetContextState::MarkObjectChanged(SNetObjectID id, NetworkAspectType aspectsChanged)
{
	ASSERT_GLOBAL_LOCK;
	if (!m_vObjects[id.id].userID)
		return;
	SNetObjectAspectChange change(aspectsChanged, 0);
	m_vGameChangedObjects.AddChange(id, change);
}

void CNetContextState::HandleAspectChanges(EntityId userID, NetworkAspectType changedAspects)
{
	ASSERT_GLOBAL_LOCK;

	SNetObjectID netID = GetNetID(userID);
	if (!netID)
	{
		//<<TODO>> CHECK THIS!
		//NetWarning( "HandleAspectChanges (ChangeAspects or ResetAspects) called on unbound object %.8x", userID );
		return;
	}
	NET_ASSERT(GetContextObject(netID).main);

	MarkObjectChanged(netID, changedAspects);
}

void CNetContextState::ChangedAspects(EntityId userID, NetworkAspectType aspectBits)
{
	// don't need to track state in single player
	if (!m_multiplayer)
		return;

	// just log it to be processed at the end of the frame (avoids locking constantly during the main update loop)
	FROM_GAME(&CNetContextState::NC_ChangedAspects, this, userID, aspectBits);
}

void CNetContextState::NC_ChangedAspects(EntityId userID, NetworkAspectType aspectBits)
{
	ASSERT_GLOBAL_LOCK;
	HandleAspectChanges(userID, aspectBits);
}

#if FULL_ON_SCHEDULING
void CNetContextState::ChangedTransform(EntityId userID, const Vec3& pos, const Quat& rot, float drawDist)
{
	FROM_GAME(&CNetContextState::NC_ChangedTransform, this, userID, pos, rot, drawDist);
}

void CNetContextState::NC_ChangedTransform(EntityId userID, Vec3 pos, Quat rot, float drawDist)
{
	ASSERT_GLOBAL_LOCK;
	if (!m_multiplayer)
		return; // TODO: make sure this is only relevant to multiplayer - Lin
	SNetObjectID id = GetNetID(userID);
	if (id)
	{
		SContextObject& obj = m_vObjects[id.id];
		SContextObjectEx& objx = m_vObjectsEx[id.id];
		if (obj.bAllocated)
		{
			objx.hasPosition = true;
			objx.position = pos;
			objx.hasDirection = true;
			objx.direction = rot * FORWARD_DIRECTION;
			objx.hasDrawDistance = drawDist > 0;
			objx.drawDistance = drawDist;
		}
	}
}

void CNetContextState::ChangedFov(EntityId id, float fov)
{
	FROM_GAME(&CNetContextState::NC_ChangedFov, this, id, fov);
}

void CNetContextState::NC_ChangedFov(EntityId userID, float fov)
{
	if (!m_multiplayer)
		return;
	SNetObjectID id = GetNetID(userID);
	if (id)
	{
		SContextObject& obj = m_vObjects[id.id];
		SContextObjectEx& objx = m_vObjectsEx[id.id];
		if (obj.bAllocated)
		{
			objx.hasFov = true;
			objx.fov = fov;
		}
	}
}
#endif

bool CNetContextState::SetSchedulingParams(EntityId objId, uint32 normal, uint32 owned)
{
	SCOPED_GLOBAL_LOCK;
	if (!m_multiplayer)
		return true;
	SNetObjectID netId = GetNetID(objId);
	if (!netId)
		return false;
	SContextObject& obj = m_vObjects[netId.id];
	SContextObjectEx& objx = m_vObjectsEx[netId.id];
	if (!obj.bAllocated)
		return false;
	objx.scheduler_owned = owned;
	objx.scheduler_normal = normal;
	return true;
}

bool CNetContextState::HandleRMI(SNetObjectID objID, bool bClient, TSerialize ser, CNetChannel* pChannel)
{
	ASSERT_GLOBAL_LOCK;
	uint8 funcId;
	ser.Value("funcID", funcId);
	//const SContextObject * pObj = GetContextObject(objID);
	SContextObjectRef obj = GetContextObject(objID);
	//if (!pObj)
	if (!obj.main)
		return false;

	//	ASSERT_PRIMARY_THREAD;  - this function is threadsafe
	INetAtSyncItem* pItem = m_pGameContext->HandleRMI(bClient, obj.main->userID, funcId, ser, pChannel);
	if (!pItem)
	{
		NetWarning("Failed handling %s RMI on entity %d @ index %d", bClient ? "client" : "server", obj.main->userID, funcId);
		return false;
	}

	if (m_pContext)
	{
		TO_GAME(pItem, pChannel);
	}
	else
	{
		pItem->DeleteThis();
	}
	return true;
}

void CNetContextState::LogRMI(const char* function, ISerializable* pParams)
{
	SCOPED_GLOBAL_LOCK;
	// check fast path
	if (m_rmiLoggers.empty())
		return;
	/* I am not using RMI logger to record CPPRMI's in demo recorder - Lin
	   #if INCLUDE_DEMO_RECORDING
	   CRMILoggerImpl loggerImpl( function, m_rmiLoggers );
	   CSimpleSerialize<CRMILoggerImpl> logger(loggerImpl);
	   pParams->SerializeWith( TSerialize(&logger) );
	   #else
	   NetWarning( "Demo recording not included" );
	   #endif
	 */
}

void CNetContextState::LogCppRMI(EntityId id, IRMICppLogger* pLogger)
{
	SCOPED_GLOBAL_LOCK;
	// check fast path
	if (m_rmiLoggers.empty())
		return;

#if INCLUDE_DEMO_RECORDING
	for (std::vector<IRMILogger*>::iterator iter = m_rmiLoggers.begin(); iter != m_rmiLoggers.end(); ++iter)
	{
		(*iter)->LogCppRMI(id, pLogger);
	}
#else
	NetWarning("Demo recording not included");
#endif
}

void CNetContextState::SetDelegatableMask(EntityId userID, NetworkAspectType aspectBits)
{
	SCOPED_GLOBAL_LOCK;
	SetDelegatableMask(GetNetID(userID), aspectBits);
}

void CNetContextState::SetDelegatableMask(SNetObjectID netID, NetworkAspectType aspectBits)
{
	ASSERT_GLOBAL_LOCK;

	if (!m_multiplayer)
	{
		// m_vObjectsEx is not filled in when running SP, this will lead to a crash if we
		// don't bail here
		return;
	}

	if (!netID)
	{
		NetLog("Attempting to set delegatable mask for non existant entity");
		return;
	}
	SContextObject& obj = m_vObjects[netID.id];
	SContextObjectEx& objx = m_vObjectsEx[netID.id];
	if (!obj.bAllocated)
	{
		NetLog("Attempting to set delegatable mask for non bound entity");
		return;
	}
	objx.delegatableMask = aspectBits;
}

void CNetContextState::BindObject(EntityId userID, EntityId parentId, NetworkAspectType aspectBits, bool bStatic)
{
	SCOPED_GLOBAL_LOCK;
	AllocateObject(userID, SNetObjectID(), aspectBits, true, bStatic ? eST_Static : eST_Normal, NULL);
	if (parentId != 0)
	{
		GC_SetParentObject(userID, parentId);
	}
}

void CNetContextState::RebindObject(SNetObjectID netId, EntityId userId)
{
	MMM_REGION(m_pMMM);

	ASSERT_GLOBAL_LOCK;

	if (!m_pContext)
		return;

	if (!GetContextObject(netId).main)
	{
		TO_GAME(&CNetContextState::GC_UnboundObject, this, userId);
	}
	else
	{
		SContextObject& obj = m_vObjects[netId.id];
		NET_ASSERT(!obj.userID);
		obj.refUserID = obj.userID = userId;

		for (NetworkAspectID i = 0; i < NumAspects; ++i)
			obj.vAspectProfiles[i] = obj.vAspectDefaultProfiles[i] = 
				GetDefaultProfileForAspect(userId, static_cast<EEntityAspects>(1 << i));

		// TODO: make sure only relevant to mp - Lin
		if (m_multiplayer)
		{
			SContextObjectEx& objx = m_vObjectsEx[netId.id];
			NetworkAspectType nAspect = BIT(0);
			NET_ASSERT(objx.nAspectsEnabled == 0);
			for (NetworkAspectID i = 0; i < NumAspects; i++)
			{
				ASSERT_PRIMARY_THREAD;
				NetworkAspectType skipCompression = nAspect & m_pContext->DisabledCompressionAspects();
				objx.vAspectData[i] = CMementoMemoryManager::InvalidHdl;
				objx.vRemoteAspectData[i] = CMementoMemoryManager::InvalidHdl;

				// probe object for which serialization chunks it will use for various profiles
				for (size_t c = 0; c < MaxProfilesPerAspect; c++)
				{
					objx.vAspectProfileChunks[i][c] = CNetwork::Get()->GetCompressionManager().GameContextGetChunkID(m_pGameContext, userId, nAspect, static_cast<uint8>(c), skipCompression);
				}
				nAspect <<= 1;
			}

			TurnAspectsOn(netId, 8);
			if (userId)
			{
				(*m_pNetIDs)[userId] = netId;
			}
		}

		SNetObjectEvent event;
		event.event = eNOE_BindObject;
		event.id = netId;
		Broadcast(&event);
	}
}

void CNetContextState::UnbindStaticObject(EntityId id)
{
	SNetObjectID netID = GetNetID(id);
	if (netID)
	{
		UnbindObject(netID, eUOF_CallGame);
	}
	else
	{
		TO_GAME(&CNetContextState::GC_UnboundObject, this, id);
	}
}

bool CNetContextState::UnbindObject(EntityId userID)
{
	SCOPED_GLOBAL_LOCK;

	SNetObjectID netID = GetNetID(userID);

	if (!netID)
	{
		NetWarning("Tried to unbind not bound user id %d", userID);
		return true;
	}

	if (GetContextObject(netID).main->spawnType == eST_Static)
	{
		stl::push_back_unique(m_removedStaticEntities, userID);
		SNetObjectEvent event;
		event.event = eNOE_RemoveStaticEntity;
		event.userID = userID;
		Broadcast(&event);
	}

	return UnbindObject(netID, 0);
}

bool CNetContextState::UnbindObject(SNetObjectID netID, uint32 flags)
{
	ASSERT_GLOBAL_LOCK;
	if (!GetContextObject(netID).main)
	{
		NetWarning("No such object %s", netID.GetText());
		return true;
	}
	SContextObject& obj = m_vObjects[netID.id];

	NetLog("UNBIND OBJECT netID=%s entityID=%d '%s'", netID.GetText(), obj.userID, obj.GetName());
	if (!obj.userID)
	{
		NetWarning("Already unbinding %s", netID.GetText());
		return true;
	}

	if (obj.spawnType == eST_Collected && 0 == (flags & eUOF_AllowCollected))
		return false;

	// make sure the object is not in the change lists
	m_vGameChangedObjects.Remove(netID);

	// make sure we don't track this network id as being valid anymore
	NET_ASSERT(GetContextObject(netID).main);
	NET_ASSERT(obj.userID);

	// Assumption: User-id 0 is null
	EntityId userId = obj.userID;
	obj.userID = 0;

	SNetObjectEvent event;
	event.event = eNOE_UnbindObject;
	event.id = netID;
	event.userID = userId;
	Broadcast(&event);

	if (flags & eUOF_CallGame)
	{
		TO_GAME(&CNetContextState::GC_UnboundObject, this, userId);
	}

	UnboundObject(netID, "BIND");
	return true;
}

bool CNetContextState::UnboundObject(SNetObjectID netID, const char* reason)
{
	MMM_REGION(m_pMMM);

	SCOPED_GLOBAL_LOCK;
	// TODO: temporary hack, shouldnt happen
	if (!GetContextObject(netID).main)
	{
		NetWarning("Object %s already unbound... not unbinding again", netID.GetText());
		return true;
	}

	// debug
	//NetLog("REF - %d %s [%d]", netID, reason, m_vObjects[netID].vLocks[eCOL_Reference]-1);
#if CHECKING_BIND_REFCOUNTS
	m_vObjects[netID.id].debug_whosLocking[reason]--;
#endif
	// ~debug

	SContextObject& obj = m_vObjects[netID.id];

	if (obj.spawnType == eST_Collected && obj.vLocks[eCOL_Reference] == 1)
		return false;

	if (UnlockObject(netID, eCOL_Reference))
	{
#if RESERVE_UNBOUND_ENTITIES
		//-- Ensure this netId is not in the reserved unbound entity list
		//-- We don't care about return value, so ignore it
		RemoveReservedUnboundEntityMapEntry(netID.id);
#endif

		ClearContextObjectCache();
		obj.bAllocated = false;
		AddToFreeObjects(netID.id);

		m_pNetIDs->erase(obj.refUserID);

		if (m_multiplayer)
		{
			SContextObjectEx& objx = m_vObjectsEx[netID.id];
			SNetObjectEvent event;
			event.event = eNOE_UnbindAspects;
			event.id = netID;
			event.aspects = objx.nAspectsEnabled;
			Broadcast(&event);
			for (int i = 0; i < NumAspects; i++)
			{
				MMM().FreeHdl(objx.vAspectData[i]);
				MMM().FreeHdl(objx.vRemoteAspectData[i]);
				objx.vAspectData[i] = CMementoMemoryManager::InvalidHdl;
				objx.vRemoteAspectData[i] = CMementoMemoryManager::InvalidHdl;
			}
#if ENABLE_THIN_BINDS
			objx.bindAspectMask = 0;
#endif // ENABLE_THIN_BINDS
		}

		SNetObjectEvent event;
		event.event = eNOE_UnboundObject;
		event.id = netID;
		Broadcast(&event);

		obj.pController = NULL;

		return true;
	}

	return false;
}

void CNetContextState::PulseObject(EntityId objId, uint32 pulseType)
{
	FROM_GAME(&CNetContextState::GC_PulseObject, this, objId, pulseType);
}

void CNetContextState::GC_PulseObject(EntityId objId, uint32 pulseType)
{
	ASSERT_GLOBAL_LOCK;
	if (!m_multiplayer)
		return;
	SNetObjectID id = GetNetID(objId);
	if (id)
	{
		SContextObject& obj = m_vObjects[id.id];
		SContextObjectEx& objx = m_vObjectsEx[id.id];
		if (obj.bAllocated)
		{
			objx.pPulseState->Pulse(pulseType);
		}
	}
}

bool CNetContextState::AllocateObject(EntityId userID, SNetObjectID netID, NetworkAspectType aspectBits,
                                      bool bOwned, ESpawnType spawnType, INetContextListenerPtr pController)
{
	MMM_REGION(m_pMMM);

	ASSERT_GLOBAL_LOCK;

	ClearContextObjectCache();

	if (!m_pContext)
		return false;

	uint8 profiles[NumAspects];

	if (userID)
	{
		for (NetworkAspectID i = 0; i < NumAspects; i++)
		{
			ASSERT_PRIMARY_THREAD;
			profiles[i] = GetDefaultProfileForAspect(userID, static_cast<EEntityAspects>(1 << i));
			if (profiles[i] >= MaxProfilesPerAspect)
			{
#if ENABLE_DEBUG_KIT
				NetLog("Trying to fetch aspect-profiles for object %d, but it doesn't seem to have any.", userID);
				NetLog("Refusing to bind this object");
#endif
				return false;
			}
		}
	}
	else
	{
		for (NetworkAspectID i = 0; i < NumAspects; i++)
			profiles[i] = 0;
	}

	if (GetNetID(userID))
	{
		//NetWarning( "Spawning an entity without removing it from the world first (entityid=%d; netid=%s); "
		//"This will 'probably' work... but no guarantees", userID, netID.GetText() );
		return false;
	}

	// allocate a reflecting network id for this user id
	// (userID's are likely sparse, and we can do better in the network if network id's are
	// dense, so we try to keep a reflecting structure here)
	if (!netID)
	{
		if (m_multiplayer)
		{
			CNetCVars& netCVars = CNetCVars::Get();
			bool lowBitID = false;
			int numObjects = m_vObjects.size();
			const char* pName = "<no name>";

			if (userID)
			{
				IEntity* pEntity = gEnv->pEntitySystem->GetEntity(userID);

				if (pEntity)
				{
					IEntityClass* pClass = pEntity->GetClass();

					pName = pEntity->GetName();

					if (CryStringUtils::stristr(pClass->GetName(), "player"))
					{
						lowBitID = true;
					}
				}
			}

			if (lowBitID)
			{
				if (m_freeLowBitObjects.empty())
				{
					if (numObjects < netCVars.net_numNetIDLowBitIDs)
					{
						netID.id = numObjects;
					}
					else
					{
						NetLog("AllocateObject: Failed to allocate a low bit netid for %s try increasing net_numNetIDLowBitBits (Currently %d). Will try to allocate from medium bit netids.", pName, netCVars.net_numNetIDLowBitBits);
					}
				}
				else
				{
					netID.id = m_freeLowBitObjects.top();
					m_freeLowBitObjects.pop();
				}
			}

			if (!netID && (spawnType != eST_Static))
			{
				if (m_freeMediumBitObjects.empty())
				{
					if (numObjects < netCVars.net_netIDHighBitStart)
					{
						if (numObjects > netCVars.net_numNetIDLowBitIDs)
						{
							netID.id = numObjects;
						}
						else
						{
							// netCVars.net_numNetIDLowBitIDs is used for invalid so start +1
							netID.id = netCVars.net_numNetIDLowBitIDs + 1;
						}
					}
					else
					{
						NetLog("AllocateObject: Failed to allocate a medium bit netid for %s try increasing net_numNetIDMediumBitBits (Currently %d). Will try to allocate from high bit netids.", pName, netCVars.net_numNetIDMediumBitBits);
					}
				}
				else
				{
					netID.id = m_freeMediumBitObjects.top();
					m_freeMediumBitObjects.pop();
				}
			}

			if (!netID)
			{
				if (m_freeHighBitObjects.empty())
				{
					if (numObjects < netCVars.net_numNetIDs)
					{
						if (numObjects > netCVars.net_netIDHighBitStart)
						{
							netID.id = numObjects;
						}
						else
						{
							netID.id = netCVars.net_netIDHighBitStart;
						}
					}
					else
					{
						NetLog("AllocateObject: Failed to allocate a high bit netid for %s try increasing net_numNetIDHighBitBits (Currently %d).", pName, netCVars.net_numNetIDHighBitBits);
						CryFatalError("AllocateObject: Failed to allocate a netid for %s. See Game.log for details.", pName);
						return false;
					}
				}
				else
				{
					netID.id = m_freeHighBitObjects.top();
					m_freeHighBitObjects.pop();
				}
			}

			while (numObjects <= netID.id)
			{
				if (numObjects < netID.id)
				{
					AddToFreeObjects(numObjects);
				}

				m_vObjects.push_back(SContextObject());
				m_vObjectsEx.push_back(SContextObjectEx());
				numObjects = m_vObjects.size();
			}
		}
		else
		{
			if (m_freeLowBitObjects.empty())
			{
				if (m_vObjects.size() == SNetObjectID::InvalidId)
				{
					CryFatalError("WAY too many synchronized objects");
				}

				netID.id = static_cast<uint16>(m_vObjects.size());
				m_vObjects.push_back(SContextObject());
			}
			else
			{
				netID.id = m_freeLowBitObjects.top();
				m_freeLowBitObjects.pop();
			}
		}

		NET_ASSERT(!m_vObjects[netID.id].bAllocated);

		do
		{
			netID.salt = ++m_vObjects[netID.id].salt;
		}
		while (!netID.salt);
	}
	else if (netID.id >= m_vObjects.size())
	{
		NET_ASSERT(netID.salt);
		m_vObjects.resize(netID.id + 1, SContextObject());
		if (m_multiplayer)
			m_vObjectsEx.resize(netID.id + 1, SContextObjectEx()); // IMPORTANT:
		m_vObjects[netID.id].salt = netID.salt;                  // ??? maybe the other way
	}
	else if (m_vObjects[netID.id].bAllocated)
	{
		NET_ASSERT(netID.salt);

#ifndef EXCLUDE_NORMAL_LOG
		char buf1[64], buf2[64];
		NetWarning("Trying to allocate already allocated network id %s spawnType %d (was %s spawnType %d)", netID.GetText(buf1), spawnType, SNetObjectID(netID.id, m_vObjects[netID.id].salt).GetText(buf2), m_vObjects[netID.id].spawnType);
#endif

		if (spawnType == eST_Collected && m_vObjects[netID.id].spawnType == eST_Collected)
			return true;
		NET_ASSERT(false);
		return false;
	}
	else
	{
		m_vObjects[netID.id].salt = netID.salt;
	}

	SContextObject& obj = m_vObjects[netID.id];

	NET_ASSERT(obj.salt == netID.salt);

	IEntity* pEntity = gEnv->pEntitySystem->GetEntity(userID);
	if (CVARS.LogLevel > 1)
	{
		NetLog("AllocateObject: userID:%.8x (%s) netID:%s aspectBits:%.2x [%s %s] controller:%p",
		       userID,
		       pEntity ? pEntity->GetName() : "",
		       netID.GetText(),
		       aspectBits,
		       bOwned ? "owned" : "not owned",
		       spawnType == eST_Static ? "static" : "dynamic",
		       pController.get());
	}

	SLICE_AND_SLEEP();

	// TODO: check we're setting everything ok
	obj.pController = pController;
	obj.bAllocated = true;
	obj.bOwned = bOwned;
	obj.bControlled = bOwned;
	obj.spawnType = spawnType;
	obj.refUserID = obj.userID = userID;
	obj.parent = SNetObjectID();
	for (NetworkAspectID i = 0; i < NumAspects; ++i)
		obj.vAspectProfiles[i] = obj.vAspectDefaultProfiles[i] = profiles[i];

	for (int i = 0; i < eCOL_NUM_LOCKS; i++)
	{
		obj.vLocks[i] = 0;
	}

	if (m_multiplayer)
	{
		SContextObjectEx& objx = m_vObjectsEx[netID.id];
		objx.nAspectsEnabled = 0;
#if FULL_ON_SCHEDULING
		objx.hasPosition = false;
		objx.hasDirection = false;
		objx.hasDrawDistance = false;
		objx.hasFov = false;
#endif
		objx.scheduler_normal = 0;
		objx.scheduler_owned = 0;
		//static CPriorityPulseState* pDefaultPulseState = NULL;
		//if (pDefaultPulseState == NULL)
		//	pDefaultPulseState = new CPriorityPulseState(); // since pPulseState is ref counted, and it requires dynamic allocation - Lin
		//objx.pPulseState = m_multiplayer ? new CPriorityPulseState() : pDefaultPulseState;
		class CPriorityPulseStateWrapper : public CPriorityPulseState
		{
		public:
			CPriorityPulseStateWrapper()
			{
				++g_objcnt.priorityPulseState;
			}
			~CPriorityPulseStateWrapper()
			{
				--g_objcnt.priorityPulseState;
			}
		};
		objx.pPulseState = new CPriorityPulseStateWrapper();

		if (obj.userID)
		{
			NetworkAspectType serverManagedProfileAspects = m_pContext->ServerManagedProfileAspects();
			NetworkAspectType nAspect = BIT(0);
			for (NetworkAspectID i = 0; i < NumAspects; i++)
			{
				ASSERT_PRIMARY_THREAD;
				if (nAspect & aspectBits)
				{
					NetworkAspectType skipCompression = nAspect & m_pContext->DisabledCompressionAspects();
					objx.vAspectData[i] = CMementoMemoryManager::InvalidHdl;
					objx.vRemoteAspectData[i] = CMementoMemoryManager::InvalidHdl;
					objx.vAspectDataVersion[i] = 0;

					if (nAspect & serverManagedProfileAspects)
					{
						// aspect supports multiple profiles
						// probe object for which serialization chunks it will use for various profiles
						for (size_t c = 0; c < MaxProfilesPerAspect; c++)
						{
							objx.vAspectProfileChunks[i][c] = CNetwork::Get()->GetCompressionManager().GameContextGetChunkID(m_pGameContext, userID, nAspect, static_cast<uint8>(c), skipCompression);
							SLICE_AND_SLEEP();
						}
					}
					else
					{
						// aspect does not support multiple profiles
						objx.vAspectProfileChunks[i][0] = CNetwork::Get()->GetCompressionManager().GameContextGetChunkID(m_pGameContext, userID, nAspect, 0, skipCompression);
						for (size_t c = 1; c < MaxProfilesPerAspect; c++)
						{
							objx.vAspectProfileChunks[i][c] = InvalidChunkID;
						}
					}
				}
				else
				{
					objx.vAspectData[i] = CMementoMemoryManager::InvalidHdl;
					objx.vRemoteAspectData[i] = CMementoMemoryManager::InvalidHdl;
					objx.vAspectDataVersion[i] = 0;

					for (size_t c = 0; c < MaxProfilesPerAspect; c++)
					{
						objx.vAspectProfileChunks[i][c] = InvalidChunkID;
					}
				}

				nAspect <<= 1;
			}
		}
		else
		{
			for (NetworkAspectID i = 0; i < NumAspects; i++)
			{
				// fill in some garbage data that the rest of the system knows about here... we'll get
				// real data on Rebind()
				objx.vAspectData[i] = CMementoMemoryManager::InvalidHdl;
				objx.vRemoteAspectData[i] = CMementoMemoryManager::InvalidHdl;
				objx.vAspectDataVersion[i] = 0;

				// probe object for which serialization chunks it will use for various profiles
				for (size_t c = 0; c < MaxProfilesPerAspect; c++)
					objx.vAspectProfileChunks[i][c] = InvalidChunkID;
			}
		}

		MarkObjectChanged(netID, objx.nAspectsEnabled);

		TurnAspectsOn(netID, aspectBits * (spawnType != eST_Collected || bOwned));

		// Leaving this here so the mistake is not repeated:
		//  the ContextView will set all changed bits on when it binds the object into the view
		//  (as new views need to sync everything anyway) - hence marking things as changed here
		//  is a bad idea (costs performance for zero benefit)
		//	ChangedAspects( userID, aspectBits );
	}

#if CHECKING_BIND_REFCOUNTS
	obj.debug_whosLocking.clear();
#endif

	if (userID)
	{
		(*m_pNetIDs)[userID] = netID;
	}

	UpdateAuthority(netID, false, false);

	SNetObjectEvent event;
	event.event = eNOE_BindObject;
	event.id = netID;
	Broadcast(&event);

	BoundObject(netID, "BIND");

	return true;
}

void CNetContextState::SpawnedObject(EntityId userID)
{
	SCOPED_GLOBAL_LOCK;
	NET_ASSERT(m_spawnedObjectId == 0);
	m_spawnedObjectId = userID;
}

#if RESERVE_UNBOUND_ENTITIES
EntityId CNetContextState::AddReservedUnboundEntityMapEntry(uint16 partialNetID, EntityId eID)
{
	if (partialNetID != SNetObjectID::InvalidId)
	{
		TReservedUnboundEntityMap::const_iterator it = m_ReservedUnboundEntityMap.find(partialNetID);
		if (it == m_ReservedUnboundEntityMap.end())
		{
			if (eID == 0)
			{
				eID = gEnv->pEntitySystem->ReserveUnknownEntityId();
			}
			if (eID)
			{
	#if LOG_ENTITYID_ERRORS
				if (CNetCVars::Get().LogLevel > 1)
				{
					NetLog("AddReservedUnboundEntityMapEntry: Adding unbound entity map lookup for netID %d, set to EntityID %d", partialNetID, eID);
				}
	#endif
				m_ReservedUnboundEntityMap.insert(std::pair<uint16, EntityId>(partialNetID, eID));
			}
			return eID;
		}
		else
		{
			//- already exists!
	#if LOG_ENTITYID_ERRORS
			if (CNetCVars::Get().LogLevel > 1)
			{
				NetLog("AddReservedUnboundEntityMapEntry: Unbound entity map lookup for netID %d already exists!, set to EntityID %d", partialNetID, it->second);
			}
	#endif
			return it->second;
		}
	}
	return 0;
}

EntityId CNetContextState::RemoveReservedUnboundEntityMapEntry(uint16 partialNetID)
{
	if (partialNetID != SNetObjectID::InvalidId)
	{
		EntityId id = GetReservedUnboundEntityMapEntry(partialNetID);
		if (id != 0)
		{
	#if LOG_ENTITYID_ERRORS
			if (CNetCVars::Get().LogLevel > 1)
			{
				NetLog("RemoveReservedUnboundEntityMapEntry: Unbound entity map lookup for netID %d found EntityID %d", partialNetID, id);
			}
	#endif
			m_ReservedUnboundEntityMap.erase(partialNetID);
		}
		else
		{
	#if LOG_ENTITYID_ERRORS
			if (CNetCVars::Get().LogLevel > 1)
			{
				NetLog("RemoveReservedUnboundEntityMapEntry: Unbound entity map lookup for netID %d found nothing. Using 0", partialNetID);
			}
	#endif
		}
		return id;
	}
	return 0;
}

EntityId CNetContextState::GetReservedUnboundEntityMapEntry(uint16 partialNetID)
{
	if (partialNetID != SNetObjectID::InvalidId)
	{
		TReservedUnboundEntityMap::iterator it = m_ReservedUnboundEntityMap.find(partialNetID);
		if (it != m_ReservedUnboundEntityMap.end())
		{
			return it->second;
		}
	}
	return 0;
}
#endif

void CNetContextState::DelegateAuthority(EntityId id, INetChannel* pControlling)
{
	FROM_GAME(&CNetContextState::NC_DelegateAuthority, this, id, pControlling);
}

void CNetContextState::NC_DelegateAuthority(EntityId userID, INetChannel* pChannel)
{
	ASSERT_GLOBAL_LOCK;
	SNetObjectID netID = GetNetID(userID);

	if (!netID || !GetContextObject(netID).main || IsDead())
	{
		return;
	}

	NET_ASSERT(GetContextObject(netID).main);
	SContextObject& obj = m_vObjects[netID.id];

#if ENABLE_DEBUG_KIT
	struct DebugHelper
	{
		DebugHelper(SContextObject& obj) : m_obj(obj) { Dump(">"); }
		SContextObject& m_obj;
		~DebugHelper() { Dump("<"); }
		void            Dump(const char* prefix)
		{
			if (CVARS.LogLevel > 1)
				NetLog("%s DelegateAuthority on '%s' cur: %s",
				       prefix,
				       m_obj.GetName(),
				       m_obj.pController ? m_obj.pController->GetName().c_str() : "none");
		}
	};
	DebugHelper debugHelper(obj);
#endif

	INetContextListenerPtr pController = NULL;
	if (pChannel)
	{
		pController = ((CNetChannel*)pChannel)->GetContextView();
		NET_ASSERT(pController != NULL);
	}

#if ENABLE_DEBUG_KIT
	NetLog("[OWNERSHIP] DelegateAuthority on '%s' controller: %s -> %s", obj.GetName(),
	       obj.pController ? obj.pController->GetName().c_str() : "none",
	       pController ? pController->GetName().c_str() : "none");
#endif

	if (obj.pController != pController)
	{
		if (obj.pController)
		{
			SNetObjectEvent event;
			event.event = eNOE_SetAuthority;
			event.authority = false;
			event.id = netID;
			SendEventToListener(obj.pController, &event);
		}
		if (pController)
		{
			SNetObjectEvent event;
			event.event = eNOE_SetAuthority;
			event.authority = true;
			event.id = netID;
			SendEventToListener(pController, &event);
		}

		obj.pController = pController;
	}
}

void CNetContextState::UpdateAuthority(SNetObjectID id, bool bAuth, bool bLocal)
{
	ASSERT_GLOBAL_LOCK;
	NET_ASSERT(GetContextObject(id).main);
	SContextObject& obj = m_vObjects[id.id];
	obj.bControlled = (obj.bOwned ^ bAuth) || (bAuth && bLocal);
#if ENABLE_DEBUG_KIT
	string className;
	if (IEntity* pEntity = gEnv->pEntitySystem->GetEntity(obj.userID))
		className = pEntity->GetClass()->GetName();

	NetLog("[OWNERSHIP] UpdateAuthority on %.8x[%s] '%s': controlled = %s (owned:%s auth:%s local:%s) class = %s, controller:%s",
	       obj.userID,
	       id.GetText(),
	       obj.GetName(),
	       obj.bControlled ? "true" : "false",
	       obj.bOwned ? "true" : "false",
	       bAuth ? "true" : "false",
	       bLocal ? "true" : "false",
	       className.c_str(),
	       obj.pController ? obj.pController->GetName().c_str() : "none"
	       );
#endif
	TO_GAME(&CNetContextState::GC_ControlObject, this, id, obj.bControlled, LockObject(id, "CONT"));
}

void CNetContextState::EnableAspects(EntityId userID, NetworkAspectType aspectBits, bool enabled)
{
	SCOPED_GLOBAL_LOCK;
	SNetObjectID netID = GetNetID(userID);
	if (!netID)
	{
		NET_ASSERT(false);
#if ENABLE_DEBUG_KIT
		NetWarning("EnableAspects called for invalid user id 0x%.8x", userID);
#endif
		return;
	}
	if (!m_multiplayer)
		return;
	SContextObject& obj = m_vObjects[netID.id];
	SContextObjectEx& objx = m_vObjectsEx[netID.id];
	if (enabled)
		aspectBits |= objx.nAspectsEnabled;
	else
		aspectBits = objx.nAspectsEnabled & ~aspectBits;
	ReconfigureObject(netID, aspectBits);

	SNetObjectEvent event;
	event.event = eNOE_ReconfiguredObject;
	event.id = netID;
	event.aspects = aspectBits;
	Broadcast(&event);
}

bool CNetContextState::IsBound(EntityId userID)
{
	SCOPED_GLOBAL_LOCK;
	return GetNetID(userID);
}

void CNetContextState::ReconfigureObject(SNetObjectID netID, NetworkAspectType aspects)
{
	ASSERT_GLOBAL_LOCK;
	if (!m_multiplayer)
		return;
	SContextObject& obj = m_vObjects[netID.id];
	SContextObjectEx& objx = m_vObjectsEx[netID.id];
	// what has changed
	NetworkAspectType aspectsReconfigured = aspects ^ objx.nAspectsEnabled;
	// what has been turned on
	NetworkAspectType aspectsOn = aspectsReconfigured & aspects;
	// what has been turned off
	NetworkAspectType aspectsOff = aspectsReconfigured & objx.nAspectsEnabled;

	NET_ASSERT((aspectsOn & aspectsOff) == 0);

	//	NetLog("ReconfigureObject %s aspectsOn:%d aspectsOff:%d", netID.GetText(), aspectsOn, aspectsOff);

	TurnAspectsOn(netID, aspectsOn);
	TurnAspectsOff(netID, aspectsOff);
}

void CNetContextState::TurnAspectsOn(SNetObjectID netID, NetworkAspectType aspects)
{
	ASSERT_GLOBAL_LOCK;
	if (!m_multiplayer)
		return;
	SContextObject& obj = m_vObjects[netID.id];
	SContextObjectEx& objx = m_vObjectsEx[netID.id];

	NET_ASSERT((aspects & objx.nAspectsEnabled) == 0);
	// safety
	aspects &= ~objx.nAspectsEnabled;

	// setup an initial aspect structure
	NetworkAspectID i;
	CBitIter itAspects(aspects);
	while (itAspects.Next(i))
	{
		if (0 == (m_pContext->DeclaredAspects() & (1 << i)))
		{
#if ENABLE_DEBUG_KIT
			NetWarning("Attempt to enable an undeclared aspect (%d)", i);
#endif
			aspects &= ~(1 << i);
			continue;
		}
		else if (0 == (m_pContext->ServerManagedProfileAspects() & (1 << i)))
		{
			bool allEmpty = true;
			for (int a = 0; allEmpty && a < MaxProfilesPerAspect; a++)
				allEmpty &= CNetwork::Get()->GetCompressionManager().IsChunkEmpty(objx.vAspectProfileChunks[i][a]);
			if (allEmpty)
			{
				aspects &= ~(1 << i);
				continue;
			}
		}
	}

	SNetObjectEvent event;
	event.event = eNOE_BindAspects;
	event.id = netID;
	event.aspects = aspects;
	Broadcast(&event);

	objx.nAspectsEnabled |= aspects;

	MarkObjectChanged(netID, aspects);
}

void CNetContextState::TurnAspectsOff(SNetObjectID netID, NetworkAspectType aspects)
{
	ASSERT_GLOBAL_LOCK;
	if (!m_multiplayer)
		return;
	SContextObject& obj = m_vObjects[netID.id];
	SContextObjectEx& objx = m_vObjectsEx[netID.id];

	NET_ASSERT((aspects & objx.nAspectsEnabled) == aspects);

	NetworkAspectID i;
	CBitIter itAspects(aspects);
	while (itAspects.Next(i))
	{
		if (0 == (m_pContext->DeclaredAspects() & (1 << i)))
		{
#if ENABLE_DEBUG_KIT
			NetWarning("Attempt to disable an undeclared aspect (%d)", i);
#endif
			aspects &= ~(1 << i);
			continue;
		}
	}

	SNetObjectEvent event;
	event.event = eNOE_UnbindAspects;
	event.id = netID;
	event.aspects = aspects;
	Broadcast(&event);

	objx.nAspectsEnabled &= ~aspects;
}

bool CNetContextState::RemoteContextHasAuthority(INetChannel* pChannel, EntityId id)
{
	SCOPED_GLOBAL_LOCK;
	SNetObjectID netID = GetNetID(id);
	if (!netID)
		return false;
	//const SContextObject * pContextObject = GetContextObject(netID);
	SContextObjectRef obj = GetContextObject(netID);
	if (!obj.main)
		return false;
	if (!obj.main->pController)
		return false;
	return ((CContextView*)&*obj.main->pController)->Parent() == pChannel;
}

void CNetContextState::RemoveRMIListener(IRMIListener* pListener)
{
	SCOPED_GLOBAL_LOCK;
	SNetObjectEvent evt;
	evt.event = eNOE_RemoveRMIListener;
	evt.pRMIListener = pListener;
	Broadcast(&evt);
}

void CNetContextState::PerformRegularCleanup()
{
	ASSERT_GLOBAL_LOCK;
	if (m_subscriptions.empty())
		return;

	if (m_bInCleanup)
		return;
	m_bInCleanup = true;
	INetContextListenerPtr pListener = m_subscriptions[m_cleanupMember % m_subscriptions.size()].pListener;
	pListener->PerformRegularCleanup();
	m_bInCleanup = false;
}

void CNetContextState::GetAllObjects(std::vector<SNetObjectID>& objs)
{
	objs.resize(0);
	for (TContextObjects::iterator iter = m_vObjects.begin(); iter != m_vObjects.end(); ++iter)
	{
		if (iter->bAllocated && iter->userID)
			objs.push_back(SNetObjectID((uint16)(iter - m_vObjects.begin()), iter->salt));
	}
}

INetContextListenerPtr CNetContextState::FindListener(const char* name)
{
	ASSERT_GLOBAL_LOCK;
	if (name[0] == 0)
		return 0;
	for (TSubscriptions::iterator iter = m_subscriptions.begin(); iter != m_subscriptions.end(); ++iter)
		if (iter->pListener->GetName().find(name) != string::npos)
			return iter->pListener;
	return NULL;
}

bool CNetContextState::AllInOrPastState(EContextViewState state)
{
	for (TSubscriptions::iterator iter = m_subscriptions.begin(); iter != m_subscriptions.end(); ++iter)
	{
		INetChannel* pChannel = iter->pListener->GetAssociatedChannel();
		if (pChannel)
		{
			if (pChannel->GetName() == string("DemoRecorder"))
				continue; // HACK!!!
			CContextView* pView = ((CNetChannel*)pChannel)->GetContextView();
			if (!pView->IsBeforeState(eCVS_InGame))
				return false;
			if (!pView->IsPastOrInState(state))
				return false;
		}
	}
	return true;
}

NetworkAspectType CNetContextState::UpdateAspectData(SNetObjectID id, NetworkAspectType fetchAspects)
{
	MMM_REGION(m_pMMM);

	CRY_PROFILE_FUNCTION(PROFILE_NETWORK);
	ASSERT_GLOBAL_LOCK;
	NET_ASSERT(GetContextObject(id).main);
	if (!m_multiplayer || !m_pContext)
		return 0;
	SContextObject& obj = m_vObjects[id.id];
	SContextObjectEx& objx = m_vObjectsEx[id.id];
	CUpdateAspectDataContext ctx(m_pContext, obj, objx, m_localPhysicsTime);

	ctx.RequestFetchAspects(fetchAspects);

	CBitIter fetchiter(ctx.GetFetchAspects());
	NetworkAspectID i;
	while (fetchiter.Next(i))
		ctx.FetchDataFromGame(i);

	return ctx.GetTakenAspects();
}

void CNetContextState::LogBreak(const SNetBreakDescription& breakage)
{
	SCOPED_GLOBAL_LOCK;
	MMM_REGION(m_pMMM);

	m_pLoggedBreakage->push_back(SNetIntBreakDescription());
	SNetIntBreakDescription& des = m_pLoggedBreakage->back();

	des.pMessagePayload = breakage.pMessagePayload;
	des.flags = breakage.flags;
	des.breakageEntity = breakage.breakageEntity;
	if (CNetCVars::Get().breakageSyncEntities)
	{
		for (int i = 0; i < breakage.nEntities; i++)
		{
			if (gEnv->bServer)
			{
				AllocateObject(breakage.pEntities[i], SNetObjectID(), 8, true, eST_Collected, NULL);
				NetLog("Allocated collected object %s for index %d; entity id = %d", GetNetID(breakage.pEntities[i]).GetText(), i, breakage.pEntities[i]);
			}
			des.spawnedObjects.push_back(breakage.pEntities[i]);
		}
	}

	if ((breakage.flags & eNBF_SendOnlyOnClientJoin) == 0)
	{
		// Send break event immediately
		SNetObjectEvent event;
		event.event = eNOE_GotBreakage;
		event.pBreakage = &des;
		Broadcast(&event);
	}
}

void CNetContextState::NotifyGameOfAspectUpdate(SNetObjectID objID, NetworkAspectID aspectIdx, CNetChannel* pChannel, CTimeValue remoteTime)
{
	if (m_pContext)
		m_vNetChangeLog.push_back(SChannelChange(objID, aspectIdx, pChannel, remoteTime));
}

void CNetContextState::ForceUnbindObject(SNetObjectID id)
{
	/*
	   SContextObjectRef obj = GetContextObject(id);
	   if (obj.main)
	   {
	    TO_GAME(&CNetContextState::GC_UnboundObject, this, obj.main->userID);
	   }
	 */
	UnbindObject(id, eUOF_CallGame | eUOF_AllowCollected);
}

/*
 * GameContext calls from the work queue
 */

void CNetContextState::GC_UnboundObject(EntityId id)
{
	ASSERT_GLOBAL_LOCK;
	ASSERT_PRIMARY_THREAD;
	ENSURE_REALTIME;
	m_pGameContext->UnboundObject(id);
}

void CNetContextState::GC_BeginContext(CTimeValue waitStarted)
{
	ASSERT_PRIMARY_THREAD;

	SCOPED_GLOBAL_LOCK;
	CContextEstablisherPtr pEstablisher(new CContextEstablisher);
	m_pGameContext->InitGlobalEstablishmentTasks(pEstablisher, m_token);
	RegisterEstablisher(NULL, pEstablisher);
	SetEstablisherState(NULL, eCVS_InGame);
}

void CNetContextState::GC_ControlObject(SNetObjectID id, bool controlled, CNetObjectBindLock lk)
{
	ASSERT_GLOBAL_LOCK;
	ASSERT_PRIMARY_THREAD;
	ENSURE_REALTIME;
	if (const EntityId eid = m_vObjects[id.id].userID)
	{
		if (IEntity* pEntity = gEnv->pEntitySystem->GetEntity(eid))
		{
			pEntity->GetNetEntity()->SetAuthority(controlled);
		}
		m_pGameContext->ControlObject(eid, controlled);
	}
}

void CNetContextState::GC_BoundObject(const EntityId eid)
{
	ASSERT_GLOBAL_LOCK;
	ASSERT_PRIMARY_THREAD;
	ENSURE_REALTIME;

	IEntity* pEntity = gEnv->pEntitySystem->GetEntity(eid);
	CRY_ASSERT_MESSAGE(pEntity, "[net] notification of binding non existant entity %.8x received", eid);
	if (INetEntity* pNetEntity = (pEntity ? pEntity->GetNetEntity() : nullptr))
		pNetEntity->BecomeBound();
}

void CNetContextState::GC_SendPostSpawnEntities(CContextViewPtr pView)
{
	ASSERT_GLOBAL_LOCK;
	ENSURE_REALTIME;
	if (pView->IsDead())
		return;
	for (TContextObjects::iterator iter = m_vObjects.begin(); iter != m_vObjects.end(); ++iter)
	{
		if (iter->bAllocated && iter->userID)
		{
			ASSERT_PRIMARY_THREAD;
			if (!m_pGameContext->SendPostSpawnObject(iter->userID, pView->Parent()))
			{
				SNetObjectID id((uint16)(iter - m_vObjects.begin()), iter->salt);
#if ENABLE_DEBUG_KIT
				NetWarning("Entity %d (%s) was not found during post spawning of objects: removing it", iter->userID, id.GetText());
#endif
				UnbindObject(iter->userID);
				return;
			}
		}
	}
	//pView->FinishLocalState();
}

void CNetContextState::GC_SetAspectProfile(NetworkAspectType aspect, uint8 profile, SNetObjectID netID, CNetObjectBindLock lock)
{
	ASSERT_GLOBAL_LOCK;
	ENSURE_REALTIME;
	ASSERT_PRIMARY_THREAD;
	if (m_vObjects[netID.id].userID)
		SetEntityAspectProfile(m_vObjects[netID.id].userID, aspect, profile);
	UnlockObject(netID, eCOL_GameDataSync);
}

void CNetContextState::GC_EndContext()
{
	ASSERT_GLOBAL_LOCK;
	ASSERT_PRIMARY_THREAD;
	ENSURE_REALTIME;
	//m_pGameContext->EndContext();
}

void CNetContextState::GC_SetParentObject(EntityId objId, EntityId parentId)
{
	ASSERT_GLOBAL_LOCK;
	ENSURE_REALTIME;
	SNetObjectID objNetId = GetNetID(objId);
	SNetObjectID parentNetId = GetNetID(parentId);
	if (!GetContextObject(objNetId).main || !GetContextObject(parentNetId).main)
	{
		if (parentId == 0)
		{
			if (!objNetId)
			{
				IEntity* pEntity = gEnv->pEntitySystem->GetEntity(objId);

				NetLog("CNetContextState::GC_SetParentObject() Being asked to clear parent object but object is not bound (obj %s(%d))", (pEntity != NULL) ? pEntity->GetName() : "<unknown>", objId);
				return;
			}
			if (m_vObjects[objNetId.id].parent == 0)
			{
				IEntity* pEntity = gEnv->pEntitySystem->GetEntity(objId);

				NetLog("CNetContextState::GC_SetParentObject() Being asked to clear parent object but object has no parent (obj %s(%d))", (pEntity != NULL) ? pEntity->GetName() : "<unknown>", objId);
				return;
			}
		}
		else
		{
			IEntity* pEntity = gEnv->pEntitySystem->GetEntity(objId);
			IEntity* pParentEntity = gEnv->pEntitySystem->GetEntity(parentId);

			NetLog("CNetContextState::GC_SetParentObject() Unable to set parent object (obj %s(%d) parent %s(%d))", (pEntity != NULL) ? pEntity->GetName() : "<unknown>", objId, (pParentEntity != NULL) ? pParentEntity->GetName() : "<unknown>", parentId);
			return;
		}
	}
	m_vObjects[objNetId.id].parent = parentNetId;
}

// context view configure context stuff

void CNetContextState::NC_BroadcastSimpleEvent(ENetObjectEvent event)
{
	ENSURE_REALTIME;
	SNetObjectEvent ev;
	ev.event = event;
	Broadcast(&ev);
}

void CNetContextState::RegisterEstablisher(INetContextListenerPtr pListener, CContextEstablisherPtr pEst)
{
	ASSERT_GLOBAL_LOCK;
	m_allEstablishers[pListener] = pEst;
	if (m_allEstablishers.size() == 1)
	{
		TO_GAME_LAZY(&CNetContextState::GC_Lazy_TickEstablishers, this);
	}
}

void CNetContextState::SetEstablisherState(INetContextListenerPtr pListener, EContextViewState state)
{
	ASSERT_GLOBAL_LOCK;
	EstablishersMap::iterator iter = m_allEstablishers.find(pListener);
	if (iter == m_allEstablishers.end())
#if ENABLE_DEBUG_KIT
		NetWarning("Couldn't find establisher trying to set state %d for %s", state, pListener ? pListener->GetName().c_str() : "Context");
#else
		;
#endif
	else
		iter->second.state = state;
}

void CNetContextState::GC_Lazy_TickEstablishers()
{
	_smart_ptr<CNetContextState> pThis = this;

	if (!gEnv->pGameFramework->GetNetContext())
	{
		// Can get in here if the host leaves while we're in the middle of loading, need to abort the tick to avoid
		// several tasks crashing
		return;
	}

	// fetch
	{
		SCOPED_GLOBAL_LOCK;
		m_currentEstablishers = m_allEstablishers;
	}

	// execute
	for (EstablishersMap::iterator iter = m_currentEstablishers.begin(); iter != m_currentEstablishers.end(); ++iter)
	{
		INetContextListenerPtr pListener = iter->first;
		SContextEstablisher& est = iter->second;

		if (IsDead() || (pListener && pListener->IsDead()))
		{
			est.pEst->Fail(eDC_Unknown, "Couldn't establish context");
			est.phase = eCEP_Dead;
		}
		else
		{
#if ENABLE_DEBUG_KIT
			est.pEst->DebugDraw();
#endif
			bool done = false;
			SContextEstablishState s;
			s.contextState = est.state;
			s.pSender = pListener ? pListener->GetAssociatedChannel() : NULL;
			while (!done)
			{
				switch (est.phase)
				{
				case eCEP_Fresh:
					est.pEst->Start();
					est.phase = eCEP_Working;
					break;
				case eCEP_Working:
					if (!est.pEst->StepTo(s) || est.pEst->IsDone())
					{
#ifndef _RELEASE
						est.pEst->OutputTiming();
#endif
						est.phase = eCEP_Dead;
					}
					else
						done = true;
					break;
				case eCEP_Dead:
					done = true;
					break;
				}
			}
		}
	}

	// commit
	SCOPED_GLOBAL_LOCK;
	for (EstablishersMap::iterator iterCur = m_currentEstablishers.begin(); iterCur != m_currentEstablishers.end(); ++iterCur)
	{
		EstablishersMap::iterator iterEst = m_allEstablishers.find(iterCur->first);

		SContextEstablisher& estCur = iterCur->second;
		SContextEstablisher& estEst = iterEst->second;
		if (estEst.pEst != estCur.pEst)
		{
			estEst.pEst->Fail(eDC_Unknown, "Supersceded");
		}
		else if (estCur.phase != eCEP_Dead)
		{
			estEst.phase = estCur.phase;
		}
		else
		{
			iterEst = m_allEstablishers.erase(iterEst);
		}
	}
	m_currentEstablishers.clear();
	if (!m_allEstablishers.empty())
	{
		TO_GAME_LAZY(&CNetContextState::GC_Lazy_TickEstablishers, this);
	}
}

int CNetContextState::RegisterPredictedSpawn(INetChannel* pChannel, EntityId id)
{
	SCOPED_GLOBAL_LOCK;

	if (!pChannel)
	{
		NET_ASSERT(false);
#if ENABLE_DEBUG_KIT
		NetWarning("Spawn predicted on a null channel; this is not valid");
#endif
		return 0;
	}

	if (!id)
	{
		NET_ASSERT(false);
#if ENABLE_DEBUG_KIT
		NetWarning("Spawn predicted on an invalid entity id; this is not valid -- ignoring");
#endif
		return 0;
	}

	if (GetNetID(id))
	{
		NET_ASSERT(false);
#if ENABLE_DEBUG_KIT
		NetWarning("Entity id %d was predicted to be spawned but is already bound to the network system; this is invalid (already bound as %s)", id, GetNetID(id).GetText());
#endif
		return 0;
	}

	SNetObjectEvent event;
	event.event = eNOE_PredictSpawn;
	event.predictedEntity = id;
	SendEventToChannelListener(pChannel, &event);

	return id;
}

void CNetContextState::RegisterValidatedPredictedSpawn(INetChannel* pChannel, int predictionHandle, EntityId id)
{
	SCOPED_GLOBAL_LOCK;

	if (!pChannel)
	{
		NET_ASSERT(false);
#if ENABLE_DEBUG_KIT
		NetWarning("Spawn predicted on a null channel; this is not valid");
#endif
		return;
	}

	if (!id)
	{
		NET_ASSERT(false);
#if ENABLE_DEBUG_KIT
		NetWarning("Spawn predicted on an invalid entity id; this is not valid -- ignoring");
#endif
		return;
	}

	if (GetNetID(id))
	{
		NET_ASSERT(false);
#if ENABLE_DEBUG_KIT
		NetWarning("Entity id %d was predicted to be spawned but is already bound to the network system; this is invalid (already bound as %s)", id, GetNetID(id).GetText());
#endif
		return;
	}

	SNetObjectEvent event;
	event.event = eNOE_ValidatePrediction;
	event.predictedEntity = id;
	event.predictedReference = predictionHandle;
	SendEventToChannelListener(pChannel, &event);
}

CNetObjectBindLock CNetContextState::LockObject(SNetObjectID id, const char* why)
{
	return CNetObjectBindLock(this, id, why);
}

void CNetContextState::Resaltify(SNetObjectID& id)
{
	if (!id)
		return;
	if (m_vObjects.size() <= id.id)
	{
		id.salt = 1;
	}
	else
	{
		id.salt = m_vObjects[id.id].salt;
	}
}

IGameContext* CNetContextState::GetGameContext()
{
	return m_pGameContext;
}

void CNetContextState::RequestRemoteUpdate(EntityId id, NetworkAspectType aspects)
{
	FROM_GAME(&CNetContextState::NC_RequestRemoteUpdate, this, id, aspects);
}

void CNetContextState::NC_RequestRemoteUpdate(EntityId id, NetworkAspectType aspects)
{
	SNetObjectEvent event;
	event.event = eNOE_PartialUpdate;
	event.id = GetNetID(id);
	if (!event.id)
		return;

	CBitIter itAspects(aspects);
	NetworkAspectID i;
	while (itAspects.Next(i))
	{
		event.aspectID = i;
		Broadcast(&event);
	}

	MarkObjectChanged(event.id, aspects);
}

/*
 * Debug support routines go here
 */

void CNetContextState::GetMemoryStatistics(ICrySizer* pSizer)
{
	SIZER_COMPONENT_NAME(pSizer, "CNetContextState");

	MMM_REGION(m_pMMM);

	pSizer->Add(*this);

	{
		SIZER_SUBCOMPONENT_NAME(pSizer, "CNetContext::m_subscriptions");
		pSizer->AddContainer(m_subscriptions);
	}

	{
		SIZER_SUBCOMPONENT_NAME(pSizer, "CNetContext::m_vObjects");
		pSizer->AddContainer(m_vObjects);
	}

	{
		SIZER_SUBCOMPONENT_NAME(pSizer, "CNetContext::m_vObjectsEx");
		pSizer->AddContainer(m_vObjectsEx);
	}

	{
		SIZER_SUBCOMPONENT_NAME(pSizer, "CNetContext::m_mNetIDs");
		pSizer->AddHashMap(*m_pNetIDs);
	}

	{
		SIZER_SUBCOMPONENT_NAME(pSizer, "CNetContext::m_channelSubscriptions");
		pSizer->AddContainer(m_channelSubscriptions);
	}

	{
		SIZER_SUBCOMPONENT_NAME(pSizer, "CNetContext::m_tempSubscriptions");
		pSizer->AddContainer(m_tempSubscriptions);
	}

	{
		SIZER_SUBCOMPONENT_NAME(pSizer, "CNetContext::m_removedStaticEntities");
		pSizer->AddContainer(m_removedStaticEntities);
	}

	/*
	   if (m_pVoiceContext)
	    m_pVoiceContext->GetMemoryStatistics(pSizer);
	 */

	{
		SIZER_SUBCOMPONENT_NAME(pSizer, "CNetContext::m_allEstablishers");
		pSizer->AddContainer(m_allEstablishers);
	}

	{
		SIZER_SUBCOMPONENT_NAME(pSizer, "CNetContext::m_currentEstablishers");
		pSizer->AddContainer(m_currentEstablishers);
	}

	{
		SIZER_SUBCOMPONENT_NAME(pSizer, "CNetContext::m_changeBuffer");
		pSizer->AddContainer(m_changeBuffer);
	}

	{
		SIZER_SUBCOMPONENT_NAME(pSizer, "CNetContext::m_loggedBreakage");
		pSizer->AddContainer(*m_pLoggedBreakage);
	}

	m_vGameChangedObjects.GetMemoryStatistics(pSizer);
	pSizer->AddContainer(m_vNetChangeLog);

	{
		SIZER_SUBCOMPONENT_NAME(pSizer, "CNetContext.m_vObjects.TMemHdl");
		for (size_t i = 0; i < m_vObjectsEx.size(); ++i)
			for (NetworkAspectID j = 0; j < NumAspects; ++j)
				MMM().AddHdlToSizer(m_vObjectsEx[i].vAspectData[j], pSizer);
	}

	//	m_pContext->GetMemoryStatistics(pSizer);
}

void CNetContextState::GetProfilingStatistics(SNetworkProfilingStats* const pProfilingStats)
{
#ifdef ENABLE_PROFILING_CODE
	int numBoundObjects = 0;
	const size_t maxNumBoundObjects = m_vObjects.size();
	for (size_t netID = 0; netID < maxNumBoundObjects; netID++)
	{
		if (!m_vObjects[netID].bAllocated)
			continue;
		numBoundObjects++;
	}

	pProfilingStats->m_numBoundObjects = numBoundObjects;
	pProfilingStats->m_maxBoundObjects = maxNumBoundObjects;
#endif // #ifdef ENABLE_PROFILING_CODE
}

void CNetContextState::NetDump(ENetDumpType type)
{
	ASSERT_GLOBAL_LOCK;

	NetLog("[NetDump] Start");
	switch (type)
	{
	case eNDT_ObjectState:

		for (uint32 nLoop = 0; nLoop < 2; nLoop++)
		{
			if (nLoop == 0)
			{
				NetLog("[NetDump] Static Bound objects:");
			}
			else
			{
				NetLog("[NetDump] Dynamic Bound objects:");
			}

			std::map<string, int> classesMap;
			uint32 numObjects = 0;

			for (TNetIDMap::const_iterator iterNetIDs = m_pNetIDs->begin(); iterNetIDs != m_pNetIDs->end(); ++iterNetIDs)
			{
				IEntity* pEntity = gEnv->pEntitySystem->GetEntity(iterNetIDs->first);
				if (pEntity)
				{
					bool bIsDynamic = (pEntity->GetFlags() & ENTITY_FLAG_NEVER_NETWORK_STATIC) || (pEntity->GetId() >= LOCAL_PLAYER_ENTITY_ID);
					const char* name = pEntity ? pEntity->GetName() : "<<no name>>";

					if (((nLoop == 0) && !bIsDynamic) || ((nLoop == 1) && bIsDynamic))
					{
						string nom(pEntity->GetClass() ? pEntity->GetClass()->GetName() : "Unknown");
						classesMap[nom]++;
						numObjects++;

						SContextObjectRef obj = GetContextObject(iterNetIDs->second);
						if (obj.main && obj.xtra)
						{
							INetChannel* pChan = obj.main->pController ? obj.main->pController->GetAssociatedChannel() : NULL;
							//IGameChannel* pGameChan = pChan ? pChan-> : NULL;
							TNetChannelID chanid = pChan ? pChan->GetLocalChannelID() : 0;

							NetLog("  %d %s %s flags(alc,ctrl,stc,own):%d%d%d%d aspects:%.2x class %s channel=%p controlchan %d",
							       iterNetIDs->first, iterNetIDs->second.GetText(), name,
							       obj.main->bAllocated, obj.main->bControlled, obj.main->spawnType, obj.main->bOwned,
							       obj.xtra->nAspectsEnabled, nom.c_str(), pChan, chanid);
						}
						else
						{
							NetLog("  %d %s %s class %s", iterNetIDs->first, iterNetIDs->second.GetText(), name, nom.c_str());
						}
					}
				}
			}

			if (nLoop == 0)
			{
				NetLog("[NetDump] Found %d Static objects in %" PRISIZE_T " classes", numObjects, classesMap.size());
			}
			else
			{
				NetLog("[NetDump] Found %d Dynamic objects in %" PRISIZE_T " classes", numObjects, classesMap.size());
			}

			for (std::map<string, int>::iterator classIter = classesMap.begin(); classIter != classesMap.end(); ++classIter)
			{
				NetLog("  %d of type %s", classIter->second, classIter->first.c_str());
			}
		}
		break;
	}
	NetLog("[NetDump] End");
}

const char* SContextObject::GetName() const
{
	ASSERT_GLOBAL_LOCK;
	if (userID)
	{
		if (IEntity* pEntity = gEnv->pEntitySystem->GetEntity(userID))
			return pEntity->GetName();
	}
	return "<unknown entity>";
}

const char* SContextObject::GetClassName() const
{
	ASSERT_GLOBAL_LOCK;
	if (userID)
	{
		if (IEntity* pEntity = gEnv->pEntitySystem->GetEntity(userID))
			return pEntity->GetClass()->GetName();
	}
	return "<unknown class>";
}

void CNetContextState::SafelyUnbind(EntityId id)
{
	FROM_GAME(&CNetContextState::GC_CompleteUnbind, this, id);
}

void CNetContextState::GC_CompleteUnbind(EntityId id)
{
	ASSERT_PRIMARY_THREAD;
	m_pGameContext->CompleteUnbind(id);
}

void CNetContextState::DrawDebugScreens()
{
#if ENABLE_DEBUG_KIT

	SCOPED_GLOBAL_LOCK;
	MMM_REGION(m_pMMM);

	if (CNetCVars::Get().ShowObjLocks)
	{
		ITextModeConsole* pTMC = gEnv->pSystem->GetITextModeConsole();
		int textY = 0;
		string out;
		for (TContextObjects::const_iterator iter = m_vObjects.begin(); iter != m_vObjects.end(); ++iter)
		{
			out.resize(0);
			if (iter->bAllocated)
			{
				out += string().Format("%s: eid=%.8x nid=%s", iter->GetName(), iter->userID, SNetObjectID(iter - m_vObjects.begin(), iter->salt).GetText());
				for (int i = 0; i < eCOL_NUM_LOCKS; i++)
					out += string().Format(" %d", iter->vLocks[i]);
	#if CHECKING_BIND_REFCOUNTS
				for (std::map<string, int>::const_iterator it = iter->debug_whosLocking.begin(); it != iter->debug_whosLocking.end(); ++it)
					if (it->second)
						out += string().Format(" %s:%d", it->first.c_str(), it->second);
	#endif
				NetQuickLog(false, 0.0f, "%s", out.c_str());
				if (pTMC)
					pTMC->PutText(0, textY++, out.c_str());
			}
		}
	}
#endif

#if ENABLE_NETWORK_MEM_INFO
	/*
	 * MEMORY USAGE DEBUGGING
	 */
	if (CVARS.MemInfo & eDMM_Context)
	{
		typedef std::map<IEntityClass*, TNetObjectIDs> ClassMap;
		ClassMap cm;
		for (TContextObjects::const_iterator iter = m_vObjects.begin(); iter != m_vObjects.end(); ++iter)
		{
			if (iter->bAllocated && iter->userID)
				cm[gEnv->pEntitySystem->GetEntity(iter->userID)->GetClass()].push_back(SNetObjectID((uint16)(iter - m_vObjects.begin()), iter->salt));
		}

		static const float nameWidth = 250.f;
		static const float colWidth = 150.f;
		static const float rowHeight = 10.f;

		float white[] = { 1.0f, 1.0f, 1.0f, 1.0f };
		float yellow[] = { 1.0f, 1.0f, 0.0f, 1.0f };
		float orange[] = { 1.0f, 0.4f, 0.0f, 1.0f };

		float x, y;

		x = nameWidth;
		y = 100.f;

		IRenderAuxText::Draw2dLabel(x, y, 1, orange, false, "TOTAL");
		x += colWidth + colWidth;

		for (TSubscriptions::const_iterator iter = m_subscriptions.begin(); iter != m_subscriptions.end(); ++iter)
		{
			IRenderAuxText::Draw2dLabel(x, y, 1, orange, false, "%s", iter->pListener->GetName().c_str());
			x += colWidth;
		}
		y += rowHeight;

		SObjectMemUsage muAll;
		static SObjectMemUsage muMax;

		muAll.required += sizeof(uint16) * m_freeLowBitObjects.size();                      //-- m_freeLowBitObjects vector approximate
		muAll.required += sizeof(uint16) * m_freeMediumBitObjects.size();                   //-- m_freeMediumBitObjects vector approximate
		muAll.required += sizeof(uint16) * m_freeHighBitObjects.size();                     //-- m_freeHighBitObjects vector approximate
		muAll.required += (sizeof(EntityId) + sizeof(SNetObjectID)) * m_pNetIDs->size();    //-- m_pNetIds hash_map approximate

		for (ClassMap::const_iterator cmiter = cm.begin(); cmiter != cm.end(); ++cmiter)
		{
			IRenderAuxText::Draw2dLabel(0.f, y, 1, white, false, "%s", cmiter->first->GetName());

			x = nameWidth + colWidth + colWidth;

			SObjectMemUsage muClassObjectsTotal;
			SObjectMemUsage muClassObjectsBase;
			SObjectMemUsage muClassObjectsAspectData;

			for (TNetObjectIDs::const_iterator noiter = cmiter->second.begin(); noiter != cmiter->second.end(); ++noiter)
			{
				uint32 index = noiter->id;
				muClassObjectsBase.required += sizeof(m_vObjects[index]) + sizeof(m_vObjectsEx[index]);
				SContextObjectEx& objx = m_vObjectsEx[index];

				muClassObjectsBase.required += sizeof(*(objx.pPulseState));

				for (int j = 0; j < NumAspects; j++)
				{
					if (objx.vAspectData[j] != CMementoMemoryManager::InvalidHdl)
					{
						muClassObjectsAspectData.required += MMM().GetHdlSize(objx.vAspectData[j]);
					}
					if (objx.vRemoteAspectData[j] != CMementoMemoryManager::InvalidHdl)
					{
						muClassObjectsAspectData.required += MMM().GetHdlSize(objx.vRemoteAspectData[j]);
					}
				}
			}

			for (TSubscriptions::const_iterator iter = m_subscriptions.begin(); iter != m_subscriptions.end(); ++iter)
			{
				SObjectMemUsage muClassObjectsInView;

				for (TNetObjectIDs::const_iterator noiter = cmiter->second.begin(); noiter != cmiter->second.end(); ++noiter)
				{
					muClassObjectsInView += iter->pListener->GetObjectMemUsage(*noiter);
				}

				IRenderAuxText::Draw2dLabel(x, y, 1, white, false, "%" PRISIZE_T " [%" PRISIZE_T "]", muClassObjectsInView.required, muClassObjectsInView.instances);
				x += colWidth;

				muClassObjectsTotal += muClassObjectsInView;
			}

			muClassObjectsTotal += muClassObjectsBase;
			muClassObjectsTotal += muClassObjectsAspectData;

			IRenderAuxText::Draw2dLabel(nameWidth, y, 1, white, false, "%" PRISIZE_T, muClassObjectsTotal.required);
			IRenderAuxText::Draw2dLabel(nameWidth + (colWidth * 0.5f), y, 1, white, false, "%" PRISIZE_T " / %" PRISIZE_T " / %" PRISIZE_T,
			                            muClassObjectsBase.required,
			                            muClassObjectsAspectData.required,
			                            muClassObjectsTotal.used);
			y += rowHeight;

			muAll += muClassObjectsTotal;
		}

		if (muAll.required > muMax.required)
		{
			muMax.required = muAll.required;
		}
		if (muAll.used > muMax.used)
		{
			muMax.used = muAll.used;
		}

		IRenderAuxText::Draw2dLabel(0.f, y, 1, orange, false, "TOTAL");
		IRenderAuxText::Draw2dLabel(nameWidth, y, 1, orange, false, "%" PRISIZE_T, muAll.required);
		IRenderAuxText::Draw2dLabel(nameWidth + (colWidth * 0.5f), y, 1, orange, false, "%" PRISIZE_T " / %" PRISIZE_T, muAll.required - muAll.used, muAll.used);
		IRenderAuxText::Draw2dLabel(nameWidth + colWidth + colWidth, y, 1, orange, false, "%" PRISIZE_T " [%" PRISIZE_T "]", muAll.used / m_subscriptions.size(), muAll.instances / m_subscriptions.size());
		y += rowHeight;
		IRenderAuxText::Draw2dLabel(0.f, y, 1, yellow, false, "MAX");
		IRenderAuxText::Draw2dLabel(nameWidth, y, 1, yellow, false, "%" PRISIZE_T, muMax.required);
		IRenderAuxText::Draw2dLabel(nameWidth + (colWidth * 0.5f), y, 1, yellow, false, "%" PRISIZE_T " / %" PRISIZE_T, muMax.required - muMax.used, muMax.used);
	}
#endif

#if ENABLE_NET_DEBUG_ENTITY_INFO
	if (CNetCVars::Get().netDebugEntityInfo)
	{
		IRenderer* pRenderer = gEnv->pRenderer;
		const float textSize = 2.0f;
		const char* pClassName = CNetCVars::Get().netDebugEntityInfoClassName->GetString();

		if (pRenderer)
		{
			IRenderAuxGeom* pRenderAuxGeom = pRenderer->GetIRenderAuxGeom();

			if (pRenderAuxGeom)
			{
				for (uint32 pass = 0; pass < 2; pass++)
				{
					IEntityItPtr it = gEnv->pEntitySystem->GetEntityIterator();

					while (IEntity* pEntity = it->Next())
					{
						SNetObjectID netID = GetNetID(pEntity->GetId());

						// Net bound entity info is generally more useful so do 2 passes unbound entities on the first pass and bound entities on the second.
						if (!netID)
						{
							if (pass == 1)
							{
								continue;
							}
						}
						else
						{
							if (pass == 0)
							{
								continue;
							}
						}

						if ((pClassName[0] == 0) || CryStringUtils::stristr(pEntity->GetClass()->GetName(), pClassName))
						{
							ColorB color = !netID ? ColorB(128, 128, 128, 255) : ColorB(255, 255, 255, 255);
							AABB aabb;

							pEntity->GetLocalBounds(aabb);
							pRenderAuxGeom->DrawAABB(aabb, pEntity->GetWorldTM(), false, color, eBBD_Faceted);

							Vec3 aabbCenterPos = pEntity->GetWorldTM() * aabb.GetCenter();
							Vec3 viewPos = GetISystem()->GetViewCamera().GetPosition();
							Vec3 dir = (viewPos - aabbCenterPos).GetNormalized();
							Vec3 textPos;

							textPos = aabbCenterPos + aabb.GetRadius() * dir;

							char buff[160];
							cry_sprintf(buff, "%s\n%s\nAct %d Hid %d Inv %d\nID %u\nNetID %s\nGUID %s",
							           pEntity->GetName(), pEntity->GetClass()->GetName(), pEntity->IsActivatedForUpdates(), pEntity->IsHidden(), pEntity->IsInvisible(), pEntity->GetId(), netID.GetText(), pEntity->GetGuid().ToDebugString());

							IRenderAuxText::DrawText(textPos, textSize, color, eDrawText_DepthTest | eDrawText_FixedSize | eDrawText_Center | eDrawText_CenterV | eDrawText_800x600, buff);
						}
					}
				}
			}
		}
	}
#endif
}

void CNetContextState::AddToFreeObjects(uint16 id)
{
	CNetCVars& netCVars = CNetCVars::Get();

	if (id != netCVars.net_invalidNetID)
	{
		if (m_multiplayer)
		{
			if (id < netCVars.net_numNetIDLowBitIDs)
			{
				m_freeLowBitObjects.push(id);
			}
			else
			{
				if (id < netCVars.net_netIDHighBitStart)
				{
					m_freeMediumBitObjects.push(id);
				}
				else
				{
					m_freeHighBitObjects.push(id);
				}
			}
		}
		else
		{
			m_freeLowBitObjects.push(id);
		}
	}
}

void CNetContextState::ClearFreeObjects()
{
	// Empty the free objects queue (no clear() member!)
	while (!m_freeLowBitObjects.empty())
	{
		m_freeLowBitObjects.pop();
	}

	while (!m_freeMediumBitObjects.empty())
	{
		m_freeMediumBitObjects.pop();
	}

	while (!m_freeHighBitObjects.empty())
	{
		m_freeHighBitObjects.pop();
	}
}
