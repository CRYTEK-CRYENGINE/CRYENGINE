// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "VoiceContext.h"
#include "NetContext.h"
#include "Network.h"
#include "VOIP/VoiceManager.h"
#include <iterator>

#ifndef OLD_VOICE_SYSTEM_DEPRECATED

static const uint32 CONTEXT_EVENTS = eNOE_UnbindObject | eNOE_Reset | eNOE_SyncWithGame_End | eNOE_SyncWithGame_Start | eNOE_SendVoicePackets | eNOE_ChangeContext;

static const int UPDATE_RATE = 10;

//derived constants
static const float UPDATE_INTERVAL = 1.0f / UPDATE_RATE;

	#define TEST_SIMPLE_COMPRESSION 0
	#define TEST_PLAYBACK           0

typedef std::set<SNetObjectID> TObjectIDSet;

static IVoiceEncoder* CreateEncoder()
{
	return CVoiceManager::CreateEncoder(CVARS.pVoiceCodec->GetString());
}

static IVoiceDecoder* CreateDecoder()
{
	return CVoiceManager::CreateDecoder(CVARS.pVoiceCodec->GetString());
}

class CVoiceContext::CVoiceGroup : public IVoiceGroup
{
public:
	CVoiceGroup(CVoiceContext* ctx) : m_pVoiceContext(ctx), m_refCnt(0)
	{
		++g_objcnt.voiceContext;
	}

	~CVoiceGroup()
	{
		if (m_pVoiceContext)
		{
			CryAutoLock<CVoiceContext::TVoiceMutex> lock(m_pVoiceContext->m_Mutex);
			m_pVoiceContext->RemoveVoiceGroup(this);
		}
		--g_objcnt.voiceContext;
	}

	virtual void AddEntity(const EntityId id)
	{
		if (!m_pVoiceContext)
			return;
		SNetObjectID netId;
		{
			SCOPED_GLOBAL_LOCK;
			netId = m_pVoiceContext->GetNetContextState()->GetNetID(id);
		}
		CryAutoLock<CVoiceContext::TVoiceMutex> lock(m_pVoiceContext->m_Mutex);
		//NET_ASSERT(netId!=InvalidNetObjectID);
		if (!netId)
			m_notBound.insert(id);
		else
			m_objects.insert(netId);

		m_pVoiceContext->m_allObjectsInvalid = true;
	}

	virtual void RemoveEntity(const EntityId id)
	{
		if (!m_pVoiceContext)
			return;
		SNetObjectID netId;
		{
			SCOPED_GLOBAL_LOCK;
			netId = m_pVoiceContext->GetNetContextState()->GetNetID(id);
		}
		CryAutoLock<CVoiceContext::TVoiceMutex> lock(m_pVoiceContext->m_Mutex);
		if (netId)
			m_objects.erase(netId);
		m_notBound.erase(id);

		m_pVoiceContext->m_allObjectsInvalid = true;
	}

	virtual void AddRef()
	{
		m_refCnt++;
	}

	virtual void Release()
	{
		m_refCnt--;
		if (!m_refCnt)
			delete this;
	}

	bool ContainsBoth(SNetObjectID obj1, SNetObjectID obj2)
	{
		if (!m_pVoiceContext)
			return false;

		CryAutoLock<CVoiceContext::TVoiceMutex> lock(m_pVoiceContext->m_Mutex);
		if (m_objects.find(obj1) == m_objects.end())
			return false;

		if (m_objects.find(obj2) == m_objects.end())
			return false;

		return true;
	}

	void CheckBinding()
	{
		CryAutoLock<CVoiceContext::TVoiceMutex> lock(m_pVoiceContext->m_Mutex);
		for (std::set<EntityId>::iterator itr = m_notBound.begin(); itr != m_notBound.end(); )
		{
			SNetObjectID netId = m_pVoiceContext->GetNetContextState()->GetNetID(*itr);
			if (netId)
			{
				std::set<EntityId>::iterator next = itr;
				++next;
				m_notBound.erase(itr);
				m_objects.insert(netId);
				itr = next;
			}
			else
			{
				++itr;
			}
		}
	}

	TObjectIDSet       m_objects;
	CVoiceContext*     m_pVoiceContext;
	std::set<EntityId> m_notBound;

private:
	int m_refCnt;
};

int CVoiceContext::m_voiceDebug = 0;

CNetContextState* CVoiceContext::GetNetContextState()
{
	// very possibly buggy
	return GetNetContext()->GetCurrentState();
}

CVoiceContext::CVoiceContext(CNetContext* pContext)
	: m_statsFrameCount(0)
	, m_pVoiceDataReader(nullptr)
	, m_proximityInvalid(false)
	, m_pNetContext(pContext)
	, m_timeInitialized(false)
	, m_routingInvalidated(false)
	, m_allObjectsInvalid(false)
{
	GetNetContextState()->ChangeSubscription(this, CONTEXT_EVENTS);

	IConsole* pConsole = gEnv->pConsole;

	if (!pConsole->GetCVar("voice_debug"))
		REGISTER_CVAR2("voice_debug", &m_voiceDebug, 0, 0, "Voice debug flags");

	m_timer = TIMER.ADDTIMER(g_time, TimerCallback, this, "CVoiceContext::CVoiceContext() m_timer");
}

void CVoiceContext::ConfigureCallback(IVoicePacketListenerPtr pListener, EVoiceDirection dir, SNetObjectID witness)
{
	CryAutoLock<TVoiceMutex> lock(m_Mutex);
	if (!witness)
		return;

	TListenerId id(witness, dir);

	if (pListener)
		m_packetListeners[id] = pListener;
	else
		m_packetListeners.erase(id);

	m_allObjectsInvalid = true;
	m_routingInvalidated = true;
}

void CVoiceContext::RemoveAllSessions()
{
	CryAutoLock<TVoiceMutex> lock(m_Mutex);

	while (!m_encSessions.empty())
		RemoveEncSession(m_encSessions.begin()->first);
	while (!m_decSessions.empty())
		RemoveDecSession(m_decSessions.begin()->first);

	m_packetListeners.clear();
	m_allObjects.clear();
	m_allObjectsBackup.clear();
}

CVoiceContext::~CVoiceContext()
{
	CryAutoLock<TVoiceMutex> lock(m_Mutex);
	Die();
	for (uint32 i = 0; i < m_voiceGroups.size(); i++)
	{
		m_voiceGroups[i]->m_pVoiceContext = 0;
	}
}

void CVoiceContext::Die()
{
	{
		SCOPED_GLOBAL_LOCK;
		TIMER.CancelTimer(m_timer);
	}
	CryAutoLock<TVoiceMutex> lock(m_Mutex);

	RemoveAllSessions();
	if (m_pNetContext)
	{
		GetNetContextState()->ChangeSubscription(this, 0);
		m_pNetContext = 0;
	}
}

void CVoiceContext::OnChannelEvent(CNetContextState* pState, INetChannel* pFrom, SNetChannelEvent* pEvent)
{
	if (pState != GetNetContextState())
		return;
}

void CVoiceContext::OnObjectEvent(CNetContextState* pState, SNetObjectEvent* pEvent)
{
	if (pEvent->event == eNOE_ChangeContext)
	{
		RemoveAllSessions();
		pEvent->pNewState->ChangeSubscription(this, CONTEXT_EVENTS);
	}

	if (pState != GetNetContextState())
		return;

	switch (pEvent->event)
	{
	case eNOE_SendVoicePackets:
		break;
	case eNOE_Reset:
		RemoveAllSessions();
		break;
	case eNOE_UnbindObject:
		{
			CryAutoLock<TVoiceMutex> lock(m_Mutex);

			RemoveDecSession(pEvent->id);
			RemoveEncSession(pEvent->id);

			for (uint32 i = 0; i < m_mutes.size(); i++)
			{
				if (m_mutes[i].first == pEvent->id || m_mutes[i].second == pEvent->id)
				{
					m_mutes.erase(m_mutes.begin() + i);
					i--;
				}
			}

			NET_ASSERT(m_tempListenersRemove.empty());
			TListenerId temp(pEvent->id, eVD_Null);
			for (TPacketListeners::iterator it = m_packetListeners.lower_bound(temp); it != m_packetListeners.end() && it->first.first == pEvent->id; ++it)
				m_tempListenersRemove.push_back(it->first);
			while (!m_tempListenersRemove.empty())
			{
				m_packetListeners.erase(m_tempListenersRemove.back());
				m_tempListenersRemove.pop_back();
			}

			for (uint32 i = 0; i < m_voiceGroups.size(); i++)
			{
				m_voiceGroups[i]->RemoveEntity(pEvent->userID);
			}

			m_routingInvalidated = true;
			m_allObjectsInvalid = true;
		}
	case eNOE_SyncWithGame_End:
		{
			for (uint32 i = 0; i < m_newDataReaders.size(); )
			{
				SNetObjectID netId = GetNetContextState()->GetNetID(m_newDataReaders[i].first);
				if (netId)
				{
					TEncSessions::iterator j = m_encSessions.find(netId);
					if (j == m_encSessions.end())
						j = m_encSessions.insert(TEncSessions::value_type(netId, SEncodingDesc())).first;

					j->second.m_pReader = m_newDataReaders[i].second;
					if (!j->second.m_pSession)
						j->second.m_pSession = new CVoiceEncodingSession(CreateEncoder());

					m_newDataReaders.erase(m_newDataReaders.begin() + i);
				}
				else
					i++;
			}

			for (TEncSessions::iterator it = m_encSessions.begin(); it != m_encSessions.end(); ++it)
			{
				SContextObjectRef obj = GetNetContextState()->GetContextObject(it->first);
				if (obj.main && obj.main->userID)
				{
					IVoiceDataReader* pReader = it->second.m_pReader;

					SEncodingDesc& ed = it->second;

					bool reading = pReader->Update();
					uint32 sz = std::min(pReader->GetSampleCount(), 1024U);
					int16* pSamples = pReader->GetSamples();

					class CSink : public IVoicePacketSink
					{
					public:
						CSink(CVoiceContext* pContext, SNetObjectID objId) : m_pContext(pContext), m_objId(objId) {}
						void AddPacket(const TVoicePacketPtr& pkt)
						{
							if (pkt->GetLength())
								m_pContext->OnPacketFrom(m_objId, eVD_To, pkt);
						}

					private:
						CVoiceContext* m_pContext;
						SNetObjectID   m_objId;
					};

					CSink sink(this, it->first);
					ed.m_pSession->AddVoiceFragment((int)sz, pSamples, &sink);

					bool reset = !reading && it->second.m_reading;

					if (reset)
						ed.m_pSession->Reset(&sink);
				}
			}

			if (m_voiceDebug > 0)
			{
				CryAutoLock<TVoiceMutex> lock(m_Mutex);

				m_statsFrameCount++;
				if (m_statsFrameCount == 50)
				{
					m_statsFrameCount = 0;
					for (TDecSessions::iterator i = m_decSessions.begin(); i != m_decSessions.end(); ++i)
					{
						if (i->second.m_pSession)
							i->second.m_pSession->GetStats(i->second.m_stats);
					}
				}

				float clr[] = { 1.0f, 1.0f, 1.0f, 1.0f };
				float y = 100.0f;

				for (TDecSessions::iterator iter = m_decSessions.begin(); iter != m_decSessions.end(); ++iter)
				{
					const SDecodingStats& stats = iter->second.m_stats;

					IRenderAuxText::Draw2dLabel(100, y, 1, clr, false, "netid:%s minpp:%d maxpp:%d 0-samps:%d skip-samps:%d pending-pkts:%d first:%.8x ctr:%.8x",
					                             iter->first.GetText(),
					                             stats.MinPendingPackets,
					                             stats.MaxPendingPackets,
					                             stats.ZeroSamples,
					                             stats.SkippedSamples,
					                             iter->second.m_pSession->GetPendingPackets(),
					                             stats.IDFirst,
					                             stats.IDCounter);

					y += 10.0f;
				}

				y += 30.0f;

				IRenderAuxText::Draw2dLabel(100, y, 1, clr, false, "allocated packets: %d", CVoicePacket::GetCount());

				y += 30.0f;

				for (size_t i = 0; i < m_voiceGroups.size(); ++i)
				{
					IRenderAuxText::Draw2dLabel(100, y, 1, clr, false, "VG%d %d players", (int)i, (int)m_voiceGroups[i]->m_objects.size());
					y += 10.0f;
					//          if(m_voiceDebug > 1)
					//          {
					//            // display contents of voice groups too
					//            TObjectIDSet::iterator it = m_voiceGroups[i]->m_objects.begin();
					//            for(; it != m_voiceGroups[i]->m_objects.end(); ++it)
					//            {
					//              IRenderAuxText::Draw2dLabel(120, y, 1, clr, false, "%d", *it);
					//              y+=10.0f;
					//            }
					//          }
				}

				y += 30.0f;
			}
		}
		break;
	}
}

void CVoiceContext::OnPacketFrom(SNetObjectID obj, EVoiceDirection dir, const TVoicePacketPtr& pkt)
{
	switch (dir)
	{
	case eVD_From:
		{
			TRoutingEntry temp(obj, NULL);
			TRoutingEntries::iterator it = std::lower_bound(m_routingEntries.begin(), m_routingEntries.end(), temp, RoutingEntryCompareFirst());
			while (it != m_routingEntries.end() && it->first == obj)
			{
				it->second->OnVoicePacket(obj, pkt);
				++it;
			}
		}
		break;
	case eVD_To:
		{
			TListenerId id(obj, eVD_To);
			for (TPacketListeners::iterator it = m_packetListeners.lower_bound(id); it != m_packetListeners.end() && it->first == id; ++it)
				it->second->OnVoicePacket(obj, pkt);
		}
		break;
	}
}

string CVoiceContext::GetName()
{
	return "Voice";
}

IVoiceGroup* CVoiceContext::CreateVoiceGroup()
{
	CryAutoLock<TVoiceMutex> lock(m_Mutex);

	CVoiceGroup* grp;

	if (!CVARS.EnableVoiceGroups)
	{
		if (m_voiceGroups.empty())
		{
			grp = new CVoiceGroup(this);
			m_voiceGroups.push_back(grp);
		}
		else
			grp = m_voiceGroups[0];
	}
	else
	{
		grp = new CVoiceGroup(this);
		m_voiceGroups.push_back(grp);
	}
	return grp;
}

void CVoiceContext::RemoveVoiceGroup(CVoiceGroup* grp)
{
	CryAutoLock<TVoiceMutex> lock(m_Mutex);
	stl::find_and_erase(m_voiceGroups, grp);
}

bool CVoiceContext::IsInSameGroup(SNetObjectID obj1, SNetObjectID obj2)
{
	CryAutoLock<TVoiceMutex> lock(m_Mutex);

	for (uint32 i = 0; i < m_voiceGroups.size(); ++i)
	{
		m_voiceGroups[i]->CheckBinding();
		if (m_voiceGroups[i]->ContainsBoth(obj1, obj2))
			return true;
	}

	return false;
}

void CVoiceContext::SetVoiceDataReader(EntityId id, IVoiceDataReader* rd)
{
	if (!CVARS.EnableVoiceChat)
		return;

	CryAutoLock<TVoiceMutex> lock(m_Mutex);
	m_newDataReaders.push_back(std::make_pair(id, rd));
}

//called from sound & network threads
bool CVoiceContext::GetDataFor(EntityId id, uint32 numSamples, int16* pData)
{
	if (!CVARS.EnableVoiceChat)
		return false;

	if (!m_pNetContext)
		return false;

	CryAutoLock<TVoiceMutex> lock(m_Mutex);

	//FIXME - serialize NetContext access
	SNetObjectID netId = GetNetContextState()->GetNetID(id);
	if (!netId)
		return false;

	TDecSessions::iterator i = m_decSessions.find(netId);
	if (i == m_decSessions.end())
		return false;

	if (!i->second.m_pSession)
		i->second.m_pSession = new CVoiceDecodingSession(CreateDecoder());

	i->second.m_pSession->GetSamples(int(numSamples), pData);

	return true;
}

void CVoiceContext::AddPacketToDecodingSession(SNetObjectID id, TVoicePacketPtr pkt)
{
	TDecSessions::iterator it = m_decSessions.find(id);
	if (it == m_decSessions.end())
	{
		SDecodingDesc desc;
		desc.m_pSession = new CVoiceDecodingSession(CreateDecoder());
		it = m_decSessions.insert(std::make_pair(id, desc)).first;
	}

	it->second.m_reading = (pkt->GetLength() != 0);
	it->second.m_pSession->AddPacket(pkt);
}

void CVoiceContext::OnClientVoicePacket(SNetObjectID id, TVoicePacketPtr pkt)
{
	CryAutoLock<TVoiceMutex> lock(m_Mutex);

	if (!m_pNetContext)
		return;

	AddPacketToDecodingSession(id, pkt);
}

void CVoiceContext::PauseDecodingFor(EntityId id, bool pause)
{
	CryAutoLock<TVoiceMutex> lock(m_Mutex);

	if (!m_pNetContext)
		return;

	SNetObjectID netId = GetNetContextState()->GetNetID(id);
	if (!netId)
		return;

	TDecSessions::iterator itDec = m_decSessions.find(netId);
	if (itDec != m_decSessions.end())
		itDec->second.m_pSession->Pause(pause);
}

void CVoiceContext::OnTimer()
{
	CryAutoLock<TVoiceMutex> lock(m_Mutex);

	if (!m_pNetContext)
		return;

	UpdateObjectSet();
	UpdateProximityList();
	UpdateRoutingTable();
}

void CVoiceContext::TimerCallback(NetTimerId, void* obj, CTimeValue time)
{
	((CVoiceContext*)obj)->m_timer = TIMER.ADDTIMER(g_time + UPDATE_INTERVAL, TimerCallback, obj, "CVoiceContext::TimerCallback() m_timer");
	((CVoiceContext*)obj)->OnTimer();
}

void CVoiceContext::Mute(EntityId requestor, EntityId id, bool mute)
{
	SCOPED_GLOBAL_LOCK;

	SNetObjectID requestorId = GetNetContextState()->GetNetID(requestor);
	if (!requestorId)
		return;
	SNetObjectID netId = GetNetContextState()->GetNetID(id);
	if (!netId)
		return;

	TDecSessions::iterator itDec = m_decSessions.find(netId);
	if (itDec != m_decSessions.end())
		itDec->second.m_pSession->Mute(mute);

	TObjectPair p(requestorId, netId);
	if (mute)
	{
		m_mutes.push_back(p);
		std::sort(m_mutes.begin(), m_mutes.end());
		m_mutes.resize(std::unique(m_mutes.begin(), m_mutes.end()) - m_mutes.begin());
		m_routingInvalidated = true;
	}
	else
	{
		TProximitySet::iterator it = std::lower_bound(m_mutes.begin(), m_mutes.end(), p);
		if (it != m_mutes.end() && *it == p)
		{
			m_mutes.erase(it);
			m_routingInvalidated = true;
		}
	}
}

bool CVoiceContext::IsMuted(EntityId requestor, EntityId id)
{
	SCOPED_GLOBAL_LOCK;

	SNetObjectID requestorId = GetNetContextState()->GetNetID(requestor);
	if (!requestorId)
		return false;
	SNetObjectID netId = GetNetContextState()->GetNetID(id);
	if (!netId)
		return false;

	TObjectPair p(requestorId, netId);
	TProximitySet::iterator it = std::lower_bound(m_mutes.begin(), m_mutes.end(), p);
	if (it != m_mutes.end() && *it == p)
		return true;

	return false;
}

bool CVoiceContext::IsEnabled()
{
	return CVARS.EnableVoiceChat != 0;
}

void CVoiceContext::UpdateObjectSet()
{
	if (!m_allObjectsInvalid)
		return;

	m_allObjectsBackup = m_allObjects;
	m_allObjects.resize(0);

	for (TEncSessions::iterator it = m_encSessions.begin(); it != m_encSessions.end(); ++it)
		m_allObjects.push_back(it->first);

	for (TDecSessions::iterator it = m_decSessions.begin(); it != m_decSessions.end(); ++it)
		m_allObjects.push_back(it->first);

	for (std::vector<CVoiceGroup*>::iterator it = m_voiceGroups.begin(); it != m_voiceGroups.end(); ++it)
		std::copy((*it)->m_objects.begin(), (*it)->m_objects.end(), std::back_inserter(m_allObjects));

	std::sort(m_allObjects.begin(), m_allObjects.end());
	m_allObjects.resize(std::unique(m_allObjects.begin(), m_allObjects.end()) - m_allObjects.begin());

	m_allObjectsInvalid = false;
	bool changed = (m_allObjects != m_allObjectsBackup);
	m_proximityInvalid |= changed;
	m_routingInvalidated |= changed;
}

void CVoiceContext::UpdateProximityList()
{
	m_proximityBackup = m_proximity;
	m_proximity.resize(0);

	if (m_allObjects.size() < 2)
		return;

	const float maxDistanceSq = CVARS.VoiceProximity * CVARS.VoiceProximity;

	TObjects::iterator end1 = m_allObjects.end();
	--end1;
	for (TObjects::iterator iter1 = m_allObjects.begin(); iter1 != end1; ++iter1)
	{
		//const SContextObject * pObj1 = m_pNetContext->GetContextObject(iter1->first);
		SContextObjectRef obj1 = GetNetContextState()->GetContextObject(*iter1);
		if (!obj1.main || !obj1.xtra->hasPosition)
			continue;

		TObjects::iterator start2 = iter1;
		++start2;
		for (TObjects::iterator iter2 = start2; iter2 != m_allObjects.end(); ++iter2)
		{
			//const SContextObject * pObj2 = m_pNetContext->GetContextObject(iter2->first);
			SContextObjectRef obj2 = GetNetContextState()->GetContextObject(*iter2);
			if (!obj2.main || !obj2.xtra->hasPosition)
				continue;

			float distSq = obj1.xtra->position.GetSquaredDistance(obj2.xtra->position);
			if (distSq < maxDistanceSq)
			{
				TObjectPair p;
				if (*iter1 < *iter2)
					p = TObjectPair(*iter1, *iter2);
				else
					p = TObjectPair(*iter2, *iter1);
				m_proximity.push_back(p);
			}
		}
	}
	std::sort(m_proximity.begin(), m_proximity.end());

	m_routingInvalidated |= (m_proximityBackup != m_proximity);
	m_proximityInvalid = false;
}

void CVoiceContext::UpdateRoutingTable()
{
	if (!m_routingInvalidated)
		return;

	enum ERouteOp
	{
		eRO_Discard,
		eRO_Send,
	};

	m_routingEntries.resize(0);

	for (TPacketListeners::iterator iterTo = m_packetListeners.begin(); iterTo != m_packetListeners.end(); ++iterTo)
	{
		if (iterTo->first.second == eVD_To)
			;
		else
			for (TObjects::iterator iterFrom = m_allObjects.begin(); iterFrom != m_allObjects.end(); ++iterFrom)
			{
				SNetObjectID idTo = iterTo->first.first;
				SNetObjectID idFrom = *iterFrom;
				if (idTo == idFrom)
					continue;

				ERouteOp op = eRO_Discard;

				TObjectPair p;
				// check mutes
				p = TObjectPair(idTo, idFrom);
				if (!std::binary_search(m_mutes.begin(), m_mutes.end(), p))
				{
					// check proximity
					if (idFrom < idTo)
						p = TObjectPair(idFrom, idTo);
					else
						p = TObjectPair(idTo, idFrom);
					TProximitySet::iterator iterProx = std::lower_bound(m_proximity.begin(), m_proximity.end(), p);
					if (iterProx != m_proximity.end() && *iterProx == p)
						op = eRO_Send;
					else
						for (uint32 i = 0; op != eRO_Send && i < m_voiceGroups.size(); i++)
						{
							// check voice groups
							CVoiceGroup& vg = *m_voiceGroups[i];

							if (vg.ContainsBoth(idFrom, idTo))
								op = eRO_Send;
						}
				}

				switch (op)
				{
				case eRO_Send:
					m_routingEntries.push_back(TRoutingEntry(idFrom, iterTo->second));
					break;
				}
			}
	}

	std::sort(m_routingEntries.begin(), m_routingEntries.end(), RoutingEntryCompareFirst());
	m_routingEntries.resize(std::unique(m_routingEntries.begin(), m_routingEntries.end()) - m_routingEntries.begin());

	m_routingInvalidated = false;
}

void CVoiceContext::RemoveEncSession(SNetObjectID id)
{
	TEncSessions::iterator it = m_encSessions.find(id);
	if (it != m_encSessions.end())
	{
		delete it->second.m_pSession;
		m_encSessions.erase(it);
		m_allObjectsInvalid = true;
	}
}

void CVoiceContext::RemoveDecSession(SNetObjectID id)
{
	TDecSessions::iterator it = m_decSessions.find(id);
	if (it != m_decSessions.end())
	{
		delete it->second.m_pSession;
		m_decSessions.erase(it);
		m_allObjectsInvalid = true;
	}
}

//
#endif
