// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include <StdAfx.h>
#include "MessageQueue.h"
#include "MessageMapper.h"
#include "Utils.h"
#include "NetCVars.h"
#include "Network.h"
#include "DebugKit/NetworkInspector.h"
#include <CrySystem/ITextModeConsole.h>
#include "NullSendable.h"
#include "STLMementoAllocator.h"
#include "CTPEndpoint.h"

#include "NetChannel.h"

#if !NEW_BANDWIDTH_MANAGEMENT

	#pragma warning ( disable : 4244 )

static CMTRand_int32 randgen;

static int m_messageQueueCount = 0;

	#if LOG_MESSAGE_QUEUE
		#define MESSAGE_HISTORY_SIZE (1024)
struct SMessageHistory
{
	uint32 m_size;
	uint32 m_index;
};
static SMessageHistory g_messageHistory[MESSAGE_HISTORY_SIZE];
	#endif // LOG_SEND_QUEUE

class CMessageQueue::CCompareMsgEnts
{
public:
	CCompareMsgEnts(std::vector<SMsgSlot>* pSlots) : m_pSlots(pSlots) {}

	ILINE int BasicCompare(const SMsgEntOrderingInfo& a, const SMsgEntOrderingInfo& b)
	{
		if (a.latencyClass < b.latencyClass)
			return -1;
		else if (a.latencyClass > b.latencyClass)
			return 1;

		NET_ASSERT(a.latencyClass == b.latencyClass);

		if (a.latencyClass == eLC_DontCare)
		{
			if (a.bandwidthExceeded && !b.bandwidthExceeded)
				return -1;
			else if (!a.bandwidthExceeded && b.bandwidthExceeded)
				return 1;

			NET_ASSERT(a.bandwidthExceeded == b.bandwidthExceeded);
		}

		if (a.schedulingOrder > b.schedulingOrder)
			return -1;
		else if (a.schedulingOrder < b.schedulingOrder)
			return 1;
		else
			return 0;
	}

	ILINE bool operator()(const SMsgEntOrderingInfo& a, const SMsgEntOrderingInfo& b)
	{
		return BasicCompare(a, b) < 0;
	}
	ILINE bool operator()(const SMsgSlot* a, const SMsgSlot* b)
	{
		switch (BasicCompare((*m_pSlots)[a->sortOrderingSlot].ordering, (*m_pSlots)[b->sortOrderingSlot].ordering))
		{
		case 0:
			return a->depth < b->depth;
		case -1:
			return true;
		case 1:
			return false;
		default:
			NET_ASSERT(false);
			return false;
		}
	}

private:
	std::vector<SMsgSlot>* m_pSlots;
};

class CMessageQueue::CCompareMsgDepth
{
public:
	CCompareMsgDepth(std::vector<SMsgSlot>* pSlots) : m_pSlots(pSlots) {}

	ILINE bool operator()(const SMsgSlot* a, const SMsgSlot* b)
	{
		return a->depth < b->depth;
	}
	ILINE bool operator()(uint32 a, uint32 b)
	{
		return (*m_pSlots)[a].depth < (*m_pSlots)[b].depth;
	}

private:
	std::vector<SMsgSlot>* m_pSlots;
};

ILINE int CMessageQueue::SMsgEntOrderingInfo::LatencyBucket() const
{
	return int(latencyClass) * 2 + (latencyClass == eLC_DontCare ? bandwidthExceeded : 0);
}

class CMessageQueue::CConfig
{
public:
	bool Read(XmlNodeRef n)
	{
		for (int i = 0; i < n->getChildCount(); i++)
		{
			XmlNodeRef child = n->getChild(i);
			if (0 == strcmp("Group", child->getTag()))
			{
				SAccountingGroupPolicy pol;
				pol.maxBandwidth = -1;
				pol.maxLatency = -1;
				pol.discardLatency = -1;
				pol.priority = 0;
				pol.numPulses = 0;
				pol.drawn = false;
				child->getAttr("bandwidth", pol.maxBandwidth);
				child->getAttr("latency", pol.maxLatency);
				child->getAttr("priority", pol.priority);
				child->getAttr("discardLatency", pol.discardLatency);
				child->getAttr("drawn", pol.drawn);

				const char* name = child->getAttr("name");
				if (!name[0])
					return false;

				if (!pol.distanceScaler.Load(child))
				{
					NetWarning("Distance scaling for group %s failed", name);
					return false;
				}
				if (!pol.dirScaler.Load(child))
				{
					NetWarning("Direction scaling for group %s failed", name);
					return false;
				}

				for (int j = 0; j < child->getChildCount(); j++)
				{
					XmlNodeRef cfg = child->getChild(j);
					if (0 == strcmp("Pulse", cfg->getTag()))
					{
						if (pol.numPulses == MAXIMUM_PULSES_PER_STATE)
						{
							NetWarning("Too many pulses for group");
							return false;
						}
						SAccountingGroupPulse& pulse = pol.pulses[pol.numPulses++];
						if (!pulse.scaler.Load(cfg))
							return false;
						const char* keyName = cfg->getAttr("name");
						if (!keyName[0])
							return false;
						if (!StringToKey(keyName, pulse.name))
						{
							NetWarning("Couldn't load pulse %s", keyName);
							return false;
						}
					}
					else
					{
						NetWarning("Unknown configuration object %s", child->getTag());
						return false;
					}
				}

				std::sort(pol.pulses, pol.pulses + pol.numPulses);

				uint32 key = 0;
				if (!StringToKey(name, key))
					return false;
				m_policy[key] = pol;
			}
			else
				return false;
		}
		return true;
	}

	void GetMemoryStatistics(ICrySizer* pSizer)
	{
		SIZER_COMPONENT_NAME(pSizer, "CMessageQueue::CConfig");

		pSizer->Add(*this);
		pSizer->AddContainer(m_policy);
	}

	std::map<uint32, SAccountingGroupPolicy> m_policy;
};

CMessageQueue::CConfig* CMessageQueue::LoadConfig(const char* name)
{
	XmlNodeRef root = gEnv->pSystem->LoadXmlFromFile(name);
	if (!root)
		return 0;
	CConfig* pConfig = new CConfig;
	if (!pConfig->Read(root))
	{
		delete pConfig;
		return 0;
	}
	return pConfig;
}

void CMessageQueue::DestroyConfig(CConfig* pConfig)
{
	delete pConfig;
}

static ILINE bool IsReliable(ENetReliabilityType nrt)
{
	switch (nrt)
	{
	case eNRT_ReliableOrdered:
	case eNRT_ReliableUnordered:
		return true;
	default:
		return false;
	}
}

static ILINE bool IsOrdered(ENetReliabilityType nrt)
{
	switch (nrt)
	{
	case eNRT_ReliableOrdered:
	case eNRT_UnreliableOrdered:
		return true;
	default:
		return false;
	}
}

static ILINE float ToPower(float x, int n)
{
	for (int i = 1; i < n; i++)
		x *= x;
	return x;
}

bool CDistanceScaler::Load(XmlNodeRef n)
{
	float normalDistance = 10;
	float maxBump = 0;
	float minBump = 0;

	bool ok = true;
	ok &= n->getAttr("normalDistance", normalDistance);
	ok &= n->getAttr("close", maxBump);
	ok &= n->getAttr("far", minBump);

	if (minBump < 0 || maxBump < 0)
		ok = false;

	if (ok)
	{
		if (minBump <= 0.0f)
			minBump = 0.001f;
		minBump = -minBump;

		if (maxBump <= 0.0f)
			maxBump = 0.001f;

		m_a = maxBump - minBump;
		m_b = -logf(-minBump / (maxBump - minBump)) / ToPower(normalDistance, DEGREE);
		m_c = minBump;
	}
	else
	{
		if (!n->haveAttr("normalDistance") && !n->haveAttr("close") && !n->haveAttr("far"))
		{
			m_a = 0;
			m_b = 0;
			m_c = 0;
			ok = true;
		}
	}

	return ok;
}

float CDistanceScaler::GetBump(float dist)
{
	return m_a * expf(-m_b * ToPower(dist, DEGREE)) + m_c;
}

bool CPulseScaler::Load(XmlNodeRef n)
{
	float bump = 0.0f;
	float halfLife = 0.3f;
	bool ok = true;
	ok &= n->getAttr("bump", bump);
	ok &= n->getAttr("decayTime", halfLife);
	ok &= halfLife > 0.01f;
	if (ok)
	{
		m_a = bump;
		m_b = logf(2.0f) / halfLife;
	}
	return ok;
}

float CPulseScaler::GetBump(float t)
{
	return m_a * expf(-m_b * t);
}

bool CDirScaler::Load(XmlNodeRef n)
{
	bool ok = true;
	float foi = 0, front = 0, back = 0;
	ok &= n->getAttr("foi", foi);
	ok &= n->getAttr("front", front);
	ok &= n->getAttr("back", back);
	ok &= foi > 10;
	ok &= front >= 0;
	ok &= back >= 0;
	if (ok && front == 0 && back == 0)
	{
		m_a = m_b = 0;
	}
	else if (ok)
	{
		m_pow = logf(1.0f - 0.98f) / logf(0.5f - 0.5f * cosf(foi / 2 / 180.0f * gf_PI));
		m_a = front + back;
		m_b = -back;
	}
	else if (!n->haveAttr("foi") && !n->haveAttr("front") && !n->haveAttr("back"))
	{
		m_a = m_b = m_pow = 0;
		ok = true;
	}
	return ok;
}

float CDirScaler::GetBump(float cosang)
{
	float scale = 1.0f - powf(0.5f - 0.5f * CLAMP(cosang, 0, 1), m_pow);
	return m_a * scale + m_b;
}

class CMessageQueue::CActiveElemIterator
{
public:
	ILINE CActiveElemIterator(CMessageQueue* pQ, EMsgSlotState root) : m_pSlotState(&pQ->m_slotState[0]), m_pSlots(&pQ->m_slots[0])
	{
		m_cur = m_pSlotState[pQ->m_rootSlots[root].id].next;
	}

	ILINE SMsgSlot* Next()
	{
		if (m_pSlotState[m_cur].state != eMSS_Root)
		{
			SMsgSlot* out = m_pSlots + m_cur;
			m_cur = m_pSlotState[m_cur].next;
			return out;
		}
		else
		{
			return 0;
		}
	}

private:
	uint32      m_cur;
	SSlotState* m_pSlotState;
	SMsgSlot*   m_pSlots;
};

class CMessageQueue::CCompareAccountingGroupsById
{
public:
	ILINE bool operator()(const SAccountingGroup& a, const SAccountingGroup& b) const
	{
		return a.id < b.id;
	}
};

class CMessageQueue::CIncrementalSorter
{
public:
	CIncrementalSorter(CMessageQueue* pQ) : m_liveList(pQ->m_liveList), m_pSlots(&pQ->m_slots)
	{
		CRY_PROFILE_FUNCTION(PROFILE_NETWORK);

		m_liveList.resize(0);

		for (int i = 0; i < 2 * eLC_NUM_CLASSES; i++)
		{
			m_latencyBuckets[i] = uint32(-1);
		}

		// initial radix sort
		CActiveElemIterator iter(pQ, eMSS_Active);
		while (SMsgSlot* pSlot = iter.Next())
		{
			int bkt = (*m_pSlots)[pSlot->sortOrderingSlot].ordering.LatencyBucket();
			pSlot->nextLatencySort = m_latencyBuckets[bkt];
			m_latencyBuckets[bkt] = pSlot - &*m_pSlots->begin();
		}

		m_curLatencyBucket = -1;
		m_curOrderingBucket = m_orderingBuckets.end();
		m_curLiveListElem = 0;
	}

	uint32 Next()
	{
		CRY_PROFILE_FUNCTION(PROFILE_NETWORK);

		while (m_curLiveListElem == m_liveList.size())
		{
			m_liveList.resize(0);
			m_curLiveListElem = 0;
			if (m_curOrderingBucket != m_orderingBuckets.end())
				++m_curOrderingBucket;

			while (m_curOrderingBucket == m_orderingBuckets.end())
			{
				m_curLatencyBucket++;

				if (m_curLatencyBucket >= 2 * eLC_NUM_CLASSES)
					return uint32(-1);

				if (m_latencyBuckets[m_curLatencyBucket] == uint32(-1))
					continue;

				m_orderingBuckets.clear();

				uint32 cur = m_latencyBuckets[m_curLatencyBucket];
				while (cur + 1)
				{
					NET_ASSERT(cur < m_pSlots->size());
					uint32 bkt = uint32(10000.0f * (*m_pSlots)[(*m_pSlots)[cur].sortOrderingSlot].ordering.schedulingOrder + 0.5f);
					TBucketMap::iterator it = m_orderingBuckets.lower_bound(bkt);
					if (it == m_orderingBuckets.end() || it->first != bkt)
						it = m_orderingBuckets.insert(it, std::make_pair(bkt, uint32(-1)));
					uint32& bktHeadRef = it->second;
					(*m_pSlots)[cur].nextOrderingSort = bktHeadRef;
					bktHeadRef = cur;
					cur = (*m_pSlots)[cur].nextLatencySort;
				}

				m_curOrderingBucket = m_orderingBuckets.begin();
			}

			uint32 cur = m_curOrderingBucket->second;
			while (cur + 1)
			{
				NET_ASSERT(cur < m_pSlots->size());
				m_liveList.push_back(cur);
				cur = (*m_pSlots)[cur].nextOrderingSort;
			}

			std::sort(m_liveList.begin(), m_liveList.end(), CCompareMsgDepth(m_pSlots));
		}

		return m_liveList[m_curLiveListElem++];
	}

private:
	#if USE_SYSTEM_ALLOCATOR
	typedef std::map<uint32, uint32, std::greater<uint32>>                                                 TBucketMap;
	#else
	typedef std::map<uint32, uint32, std::greater<uint32>, STLMementoAllocator<std::pair<const uint32, uint32>>> TBucketMap;
	#endif

	int                    m_curLatencyBucket;
	TBucketMap::iterator   m_curOrderingBucket;
	int                    m_curLiveListElem;

	static const int       NUM_ORDERINGS = 256;

	uint32                 m_latencyBuckets[2 * eLC_NUM_CLASSES];
	TBucketMap             m_orderingBuckets;
	std::vector<uint32>&   m_liveList;
	std::vector<SMsgSlot>* m_pSlots;
};

CMessageQueue::CMessageQueue(CConfig* pConfig)
	: m_pParent(NULL)
	, m_nBlockingMessages(0)
	, m_nAlertedMessages(0)
	, m_nMessages(0)
	, m_emptyMode(false)
	, m_reliableSeq(0)
	, m_flags(0)
	, m_inWrite(false)
	, m_nAccountingGroups(0)
{
	STATIC_CHECK(STATS_MAX_MESSAGEQUEUE_ACCOUNTING_GROUPS >= MAX_ACCOUNTING_GROUPS, _Increase__STATS_MAX_MESSAGEQUEUE_ACCOUNTING_GROUPS);
	for (int i = 0; i < MAX_ACCOUNTING_GROUPS; i++)
		m_accountingGroups[i].id = ~uint32(0);
	if (pConfig)
	{
		SetConfig(pConfig, CNetwork::Get()->GetMessageQueueConfigVersion());
	}

	for (int i = 0; i < eMSS_NUM_LIVE_SLOT_TYPES; i++)
	{
		SMsgSlot* pSlot = AllocSlot(m_rootSlots[i], eMSS_Root);
	}

	++m_messageQueueCount;
}

CMessageQueue::~CMessageQueue()
{
	Empty(true);
}

void CMessageQueue::GetMemoryStatistics(ICrySizer* pSizer, bool countingThis /* = false */)
{
	SIZER_COMPONENT_NAME(pSizer, "CMessageQueue");

	if (countingThis)
		pSizer->Add(*this);

	#define CONTAINER(name) { SIZER_SUBCOMPONENT_NAME(pSizer, "CMessageQueue::" # name); pSizer->AddContainer(name); }
	CONTAINER(m_slots);
	CONTAINER(m_slotState);
	CONTAINER(m_salt);
	CONTAINER(m_freeSlots);
	CONTAINER(m_liveList);
	CONTAINER(m_objectHeads);
	CONTAINER(m_depNodes);
	CONTAINER(m_freeDepLinks);
	CONTAINER(m_recurseCache);
	CONTAINER(m_pendingRemovals);
	CONTAINER(m_freeSlotNumElems);
	#undef CONTAINER

	for (size_t i = 0; i < m_slots.size(); i++)
	{
		if (GetState(i) != eMSS_Free)
			if (m_slots[i].msg.pSendable)
				pSizer->AddObject(m_slots[i].msg.pSendable.get(), m_slots[i].msg.pSendable->GetSize());
	}
}

CMessageQueue::SMsgSlot* CMessageQueue::AllocSlot(SSendableHandle& hdl, EMsgSlotState initState)
{
	uint32 id;

	bool found = false;
	for (uint32 i = 0; i < m_freeSlotNumElems.size(); i++)
	{
		if (m_freeSlotNumElems[i] > 0)
		{
			id = m_freeSlots[i].elems[--m_freeSlotNumElems[i]];
			found = true;
			break;
		}
	}
	if (!found)
	{
		id = m_slots.size();
		m_slots.resize(m_slots.size() + 1);
		if (m_salt.size() < m_slots.size())
			m_salt.resize(m_slots.size());
		m_slotState.resize(m_slots.size(), SSlotState());
	}
	NET_ASSERT(id < m_salt.size());
	NET_ASSERT(id < m_slots.size());
	SMsgSlot& slot = m_slots[id];
	NET_ASSERT(GetState(id) == eMSS_Free);
	SSlotState& slotState = m_slotState[id];
	if (initState < eMSS_NUM_LIVE_SLOT_TYPES)
	{
		uint32 rootId = m_rootSlots[initState].id;
		slotState.next = rootId;
		slotState.prev = m_slotState[rootId].prev;
		m_slotState[slotState.prev].next = id;
		m_slotState[slotState.next].prev = id;
		slotState.state = initState;
	}
	else if (initState == eMSS_Root)
	{
		slotState.next = slotState.prev = id;
		slotState.state = eMSS_Root;
	}
	else
	{
		CryFatalError("Invalid initial state for message slot %d", initState);
		return 0;
	}
	m_slots[id].msg.firstDepNodeId = InvalidDepNodeId;
	m_slots[id].msg.numDepNodes = 0;
	hdl.id = id;
	hdl.salt = ++m_salt[id];
	NET_ASSERT(!!hdl);
	return &slot;
}

CMessageQueue::SMsgSlot* CMessageQueue::GetSlotSafe(SSendableHandle hdl)
{
	if (hdl.id >= m_slots.size())
		return 0;
	NET_ASSERT(hdl.id < m_salt.size());
	SMsgSlot* pSlot = &m_slots[hdl.id];
	if ((m_salt[hdl.id]) != hdl.salt)
		return 0;
	if (GetState(hdl.id) == eMSS_Free)
		return 0;
	return pSlot;
}

ILINE CMessageQueue::SMsgSlot* CMessageQueue::GetSlot(SSendableHandle hdl)
{
	#ifndef _DEBUG
	return &m_slots[hdl.id];
	#else
	SMsgSlot* pOut = GetSlotSafe(hdl);
	NET_ASSERT(pOut);
	return pOut;
	#endif
}

ILINE void CMessageQueue::VerifyBlocking() const
{
	#if 0
		#ifdef _DEBUG
	int nBlocking = 0, nAlerted = 0;
	for (std::vector<SMsgSlot>::const_iterator it = m_slots.begin(); it != m_slots.end(); ++it)
	{
		if (it->state != eMSS_Free)
		{
			nBlocking += it->msg.pSendable->CheckParallelFlag(eMPF_BlocksStateChange);
			nAlerted += !it->msg.pSendable->CheckParallelFlag(eMPF_DontAwake);
		}
	}
	NET_ASSERT(nBlocking == m_nBlockingMessages);
	NET_ASSERT(nAlerted == m_nAlertedMessages);
		#endif
	#endif
}

ILINE void CMessageQueue::ValidateHandles() const
{
}

CMessageQueue::ELatencyClass CMessageQueue::GetLatencyClassForGroup(float expectedTimeNow, float expectedTimeNext, SAccountingGroup* pGrp)
{
	NET_ASSERT(expectedTimeNow < expectedTimeNext);
	if (!pGrp)
		return eLC_DontCare;
	if (pGrp->policy.discardLatency > 0 && expectedTimeNow > pGrp->policy.discardLatency)
		return eLC_Discardable;
	if (pGrp->policy.maxLatency < 0)
		return eLC_DontCare;
	if (expectedTimeNow > pGrp->policy.maxLatency)
		return eLC_Expired;
	if (expectedTimeNext > pGrp->policy.maxLatency)
		return eLC_NearlyExpired;
	return eLC_DontCare;
}

void CMessageQueue::AddDependencyToSlot(SSendableHandle hdl, SMsgSlot* pSlot)
{
	if (!hdl)
		return;

	SMsgSlot* pParentSlot = GetSlotSafe(hdl);
	if (!pParentSlot)
		return;

	if (!IsReliable(pParentSlot->msg.pSendable->GetReliability()))
	{
		for (uint32 depNodeId = pParentSlot->msg.firstDepNodeId; depNodeId != InvalidDepNodeId; depNodeId = m_depNodes[depNodeId].next)
			if (SMsgSlot* pGrandparentSlot = GetSlotSafe(hdl))
				if (IsReliable(pGrandparentSlot->msg.pSendable->GetReliability()))
					AddDependencyToSlot(m_depNodes[depNodeId].hdl, pSlot);
	}

	uint32 id = InvalidDepNodeId;
	if (m_freeDepLinks.empty())
	{
		id = m_depNodes.size();
		m_depNodes.push_back(SDepNode());
	}
	else
	{
		id = m_freeDepLinks.back();
		m_freeDepLinks.pop_back();
	}
	SDepNode& n = m_depNodes[id];
	n.hdl = hdl;
	n.next = pSlot->msg.firstDepNodeId;
	pSlot->msg.firstDepNodeId = id;
	pSlot->msg.numDepNodes++;
}

void CMessageQueue::FreeDependencies(SMsgSlot* pSlot)
{
	for (uint32 depNodeId = pSlot->msg.firstDepNodeId; depNodeId != InvalidDepNodeId; )
	{
		uint32 next = m_depNodes[depNodeId].next;
		m_depNodes[depNodeId] = SDepNode();
		m_freeDepLinks.push_back(depNodeId);
		depNodeId = next;
	}
	pSlot->msg.firstDepNodeId = InvalidDepNodeId;
	pSlot->msg.numDepNodes = 0;
}

EMessageQueueAddSendableResult CMessageQueue::AddSendable(INetSendable* pMsg, int nAfterHandle, const SSendableHandle* afterHandles, SSendableHandle* pHandle, bool subs)
{
	VerifyBlocking();

	if (subs)
	{
		NET_ASSERT(pHandle);
		NET_ASSERT(!m_inWrite);
		if (!pHandle)
			return eMQASR_Failed;
		if (SMsgSlot* pSlot = GetSlotSafe(*pHandle))
		{
			switch (GetState(pHandle->id))
			{
			case eMSS_Active:
			case eMSS_JustQueued:
	#if LOG_MESSAGE_DROPS
				if (CNetCVars::Get().LogDroppedMessages())
					NetLog("SUBSTITUTED MESSAGE BY REQUEST: [%s] %s --> %s", pHandle->GetText(), pSlot->msg.pSendable->GetDescription(), pMsg->GetDescription());
	#endif
				pSlot->msg.pSendable = pMsg;
				return eMQASR_Ok;
			case eMSS_Waiting:
				SetState(pHandle->id, eMSS_Limbo);
				break;
			case eMSS_Limbo:
			case eMSS_Dead:
			case eMSS_Free:
				break;
			}
		}
	}

	bool wasAlerted = (m_nAlertedMessages != 0);

	if (m_nBlockingMessages && pMsg->CheckParallelFlag(eMPF_StateChange))
		NET_ASSERT(!"Can't send state change with blocking messages still about");

	uint32 flags = 0;

	SSendableHandle hdl;
	SMsgSlot* pEnt = AllocSlot(hdl, m_inWrite ? eMSS_JustQueued : eMSS_Active);
	pEnt->msg.pSendable = pMsg;
	pEnt->msg.inserted = g_time;
	pEnt->msg.flags = 0;
	pEnt->msg.accWeight = 0;
	pEnt->liveness = eL_Fresh;
	#ifdef _DEBUG
	pEnt->lastName = pEnt->msg.pSendable->GetDescription();
	#endif

	for (int i = 0; i < nAfterHandle; i++)
		AddDependencyToSlot(afterHandles[i], pEnt);
	if (m_stateChangeHandle)
		AddDependencyToSlot(m_stateChangeHandle, pEnt);
	if (IsOrdered(pMsg->GetReliability()))
	{
		if (pEnt->msg.firstDepNodeId != InvalidDepNodeId && !nAfterHandle)
		{
			NetWarning("Traditionally ordered message found; this behaviour is deprecated and will be removed soon");
			NetWarning("Message was: %s", pMsg->GetDescription());
			AddDependencyToSlot(m_lastOrderedHandle, pEnt);
			if (IsReliable(pMsg->GetReliability()))
				flags |= eSMF_TraditionalReliable;
		}
	}

	AddToQueue(hdl);

	if (pHandle)
		*pHandle = hdl;

	VerifyBlocking();

	bool isAlerted = (m_nAlertedMessages != 0);

	if (!wasAlerted && isAlerted)
		return eMQASR_Ok_BecomeAlerted;
	else
		return eMQASR_Ok;
}

void CMessageQueue::AddToQueue(SSendableHandle hdl)
{
	ValidateHandles();
	SMsgSlot* pEnt = GetSlot(hdl);

	NET_ASSERT(GetState(hdl.id) == eMSS_Active || GetState(hdl.id) == eMSS_JustQueued);
	if (IsOrdered(pEnt->msg.pSendable->GetReliability()))
	{
		m_lastOrderedHandle = hdl;
	}
	if (pEnt->msg.pSendable->CheckParallelFlag(eMPF_StateChange))
	{
		m_stateChangeHandle = hdl;
		pEnt->msg.flags |= eSMF_HandleAck;
	}
	if (IsReliable(pEnt->msg.pSendable->GetReliability()))
	{
		pEnt->msg.flags |= eSMF_HandleAck;
	}
	pEnt->pAG = GetAccountingGroup(pEnt->msg.pSendable->GetGroup());
	m_nBlockingMessages += pEnt->msg.pSendable->CheckParallelFlag(eMPF_BlocksStateChange);
	m_nAlertedMessages += !pEnt->msg.pSendable->CheckParallelFlag(eMPF_DontAwake);
	m_nMessages++;
	ValidateHandles();
}

bool CMessageQueue::RemoveSendable(SSendableHandle handle)
{
	CRY_PROFILE_FUNCTION(PROFILE_NETWORK);

	if (m_inWrite)
	{
		m_pendingRemovals.push_back(handle);
	}

	if (!handle)
		return false;

	VerifyBlocking();
	ValidateHandles();

	bool removed = false;

	if (SMsgSlot* pSlot = GetSlotSafe(handle))
	{
		switch (GetState(handle.id))
		{
		case eMSS_Active:
		case eMSS_JustQueued:
	#if LOG_MESSAGE_DROPS
			if (CNetCVars::Get().LogDroppedMessages())
				NetLog("REJECTED MESSAGE BY REQUEST: [%s] %s", HandleFromPointer(pSlot).GetText(), pSlot->msg.pSendable->GetDescription());
	#endif
			pSlot->msg.pSendable->UpdateState(0, eNSSU_Rejected);
			SetState(handle.id, eMSS_Dead);
			removed = true;
			break;
		case eMSS_Waiting:
			SetState(handle.id, eMSS_Limbo);
			removed = true;
			break;
		case eMSS_Limbo:
		case eMSS_Dead:
		case eMSS_Free:
			break;
		}
	}

	NET_ASSERT(handle != m_stateChangeHandle);

	ValidateHandles();
	VerifyBlocking();

	return removed;
}

INetSendablePtr CMessageQueue::FindSendable(SSendableHandle handle)
{
	if (SMsgSlot* pSlot = GetSlotSafe(handle))
		return pSlot->msg.pSendable;
	return 0;
}

bool CMessageQueue::IsBlockingStateChange() const
{
	VerifyBlocking();
	return m_nBlockingMessages > 0;
}

void CMessageQueue::BeginAccountingFrame()
{
	for (int i = 0; i < m_nAccountingGroups; i++)
	{
		m_accountingGroups[i].bandwidthThisFrame = 0.0f;
	}
}

void CMessageQueue::PrepareLiveList()
{
	CRY_PROFILE_FUNCTION(PROFILE_NETWORK);
	CActiveElemIterator iter(this, eMSS_Active);
	uint32 activeRoot = m_rootSlots[eMSS_Active].id;
	m_slotState[activeRoot].nextTop = m_slotState[activeRoot].next;
	m_slotState[activeRoot].prevTop = m_slotState[activeRoot].prev;
	while (SMsgSlot* pSlot = iter.Next())
	{
		pSlot->childCount = 0;
		pSlot->childrenPatched = 0;
		SSlotState& state = m_slotState[pSlot - &*m_slots.begin()];
		state.nextTop = state.next;
		state.prevTop = state.prev;
	}
}

void CMessageQueue::CalculatePerFrameData(const SSchedulingParams& params)
{
	CRY_PROFILE_FUNCTION(PROFILE_NETWORK);

	ValidateHandles();

	#if FULL_ON_SCHEDULING
	float drawDistanceScale = 1.0f;
	if (params.haveWitnessFov)
		drawDistanceScale = 0.05f + 0.95f * (RAD2DEG(params.witnessFov) / 60.f); // from 3dengine
	#endif

	CActiveElemIterator iter(this, eMSS_Active);
	while (SMsgSlot* pEnt = iter.Next())
	{
		SMsgSlot& ent = *pEnt;
		// latencyClass
		float expectedTimeNow = (params.now - ent.msg.inserted).GetSeconds() + params.transportLatency;
		float expectedTimeNext = (params.next - ent.msg.inserted).GetSeconds() + params.transportLatency;
		ent.ordering.latencyClass = GetLatencyClassForGroup(expectedTimeNow, expectedTimeNext, ent.pAG);

		// bandwidthExceeded
		ent.ordering.bandwidthExceeded = false;
	#if FULL_ON_SCHEDULING
		if (ent.pAG)
			ent.ordering.bandwidthExceeded = ent.pAG->BandwidthExceeded(params.now, false);

		if (ent.ordering.bandwidthExceeded)
			ent.ordering.latencyClass = eLC_DontBother;
	#endif
		if ((m_flags & eMQF_IsAfterSpawning) == 0 && ent.msg.pSendable->CheckParallelFlag(eMPF_AfterSpawning))
			ent.ordering.latencyClass = eLC_CantSend;
		// schedulingOrder
		SMessagePositionInfo posInfo;
		ent.msg.pSendable->GetPositionInfo(posInfo);
		float priority = 8.0f;
		if (ent.pAG)
		{
			priority = ent.pAG->policy.priority;
	#if !FULL_ON_SCHEDULING
		}
	#else
			bool isWithinRenderedDistance = true;

			if (params.haveWitnessPosition && posInfo.havePosition)
			{
				float distanceFromWitness = params.witnessPosition.GetDistance(posInfo.position);
				priority += ent.pAG->policy.distanceScaler.GetBump(distanceFromWitness);
				if (posInfo.haveDrawDistance)
					isWithinRenderedDistance = (distanceFromWitness < (posInfo.drawDistance * drawDistanceScale));
				if (params.haveWitnessDirection)
				{
					Vec3 dirToMessage = (posInfo.position - params.witnessPosition).GetNormalizedSafe(params.witnessDirection);
					priority += ent.pAG->policy.dirScaler.GetBump(params.witnessDirection.Dot(dirToMessage));
				}
			}

			const CPriorityPulseState* pPulseState = ent.msg.pSendable->GetPulses();
			if (pPulseState && pPulseState->GetNumPulses() && ent.pAG->policy.numPulses)
			{
				const CPriorityPulseState::SPulse* pP0 = pPulseState->GetPulses();
				const CPriorityPulseState::SPulse* pP0_End = pPulseState->GetPulses() + pPulseState->GetNumPulses();
				SAccountingGroupPulse* pP1 = ent.pAG->policy.pulses;
				SAccountingGroupPulse* pP1_End = ent.pAG->policy.pulses + ent.pAG->policy.numPulses;
				while (pP0 != pP0_End && pP1 != pP1_End)
				{
					if (pP0->key < pP1->name)
						pP0++;
					else if (pP0->key > pP1->name)
						pP1++;
					else
					{
						priority += pP1->scaler.GetBump((params.now - pP0->tm).GetSeconds());
						pP0++;
						pP1++;
					}
				}
			}

			if (ent.pAG->policy.drawn && !isWithinRenderedDistance)
				priority = -32;
		}
	#endif
		priority += ent.msg.pSendable->GetPriorityDelta();
		priority = CLAMP(priority, 0, 16);
		// the following two parameters make low priority things increase somewhat randomly
		// but higher priority things increase less randomly
		// this helps prevent a huge group of low priority things from flooding high priority things at once
		float randomness = 1.0f / (priority * priority + 1.0f);
		float weight = (1.0f - randomness + randgen.GenerateFloat() * randomness) / (1.0f - randomness * 0.5f);
		ent.msg.accWeight += weight * powf(2.0f, priority) / 65535.0f;
		ent.ordering.schedulingOrder = ent.msg.accWeight;

		// now the real ordering data: preprocessing
		ent.depth = 0;
		ent.sortOrderingSlot = pEnt - &*m_slots.begin();
		ent.liveness = eL_Alive;

		if (posInfo.obj)
		{
			uint32 id = posInfo.obj.id;
			if (m_objectHeads.size() <= id)
				m_objectHeads.resize(id + 1, ~uint32(0));
			uint32& head = m_objectHeads[id];
			uint32 cur = ent.sortOrderingSlot;
			uint32& next = m_slotState[cur].nextObj;
			next = head;
			head = cur;
		}

		NET_ASSERT(GetState(&ent - &*m_slots.begin()) == eMSS_Active);

		for (uint32 depNodeId = ent.msg.firstDepNodeId; depNodeId != InvalidDepNodeId; depNodeId = m_depNodes[depNodeId].next)
		{
			SSendableHandle hdl = m_depNodes[depNodeId].hdl;
			if (hdl)
			{
				if (SMsgSlot* pDep = GetSlotSafe(hdl))
				{
					switch (GetState(hdl.id))
					{
					case eMSS_Active:
						// increment child count, and the first time we also remove it from the top-level active list
						// to avoid scanning it in PatchOrderedPriorities first loop
						if (1 == ++pDep->childCount)
						{
							SSlotState& slotState = m_slotState[hdl.id];
							m_slotState[slotState.prevTop].nextTop = slotState.nextTop;
							m_slotState[slotState.nextTop].prevTop = slotState.prevTop;
						}
						break;
					case eMSS_JustQueued:
					case eMSS_Free:
						NET_ASSERT(false);
					case eMSS_Dead:
					case eMSS_Limbo:
					case eMSS_Waiting:
						// cant send since we're waiting on something
						ent.ordering.latencyClass = eLC_CantSend;
						break;
					}
				}
			}
		}
	}

	ValidateHandles();
}

void CMessageQueue::PatchObjectGroupings()
{
	// this routine is equalizing the priority of all messages to one object
	// TODO: today (6 apr 2009) I'm not sure this is a good idea at all (Craig)
	//       it will save bandwidth by bunching entity ids together, but it may cost
	//       latency by transferring the wrong thing at the wrong time

	if (m_objectHeads.empty())
		return;
	CCompareMsgEnts compare(&m_slots);
	const uint32* pStart = &*m_objectHeads.begin();
	const uint32* pEnd = pStart + m_objectHeads.size();
	while (pStart != pEnd)
	{
		if (*pStart != ~uint32(0))
		{
			uint32 cur = *pStart;
			uint32 best = cur;
			cur = m_slotState[cur].nextObj;

			while (cur != ~uint32(0))
			{
				if (compare(m_slots[best].ordering, m_slots[cur].ordering)) // child < parent
					best = cur;
				cur = m_slotState[cur].nextObj;
			}
			cur = *pStart;
			while (cur != ~uint32(0))
			{
				m_slots[cur].sortOrderingSlot = best;
				cur = m_slotState[cur].nextObj;
			}
		}
		++pStart;
	}
}

void CMessageQueue::PatchOrderedPriorities()
{
	CRY_PROFILE_FUNCTION(PROFILE_NETWORK);

	ValidateHandles();

	NET_ASSERT(m_recurseCache.empty());

	CCompareMsgEnts compare(&m_slots);

	uint32 activeRoot = m_rootSlots[eMSS_Active].id;
	uint32 curTop = m_slotState[activeRoot].nextTop;
	while (m_slotState[curTop].state != eMSS_Root)
	{
		SMsgSlot* pEnt = &m_slots[curTop];
		NET_ASSERT(!pEnt->childCount);
		if (pEnt->msg.numDepNodes)
			m_recurseCache.push_back(pEnt);
		curTop = m_slotState[curTop].nextTop;
	}

	while (!m_recurseCache.empty())
	{
		SMsgSlot* pChildEnt = m_recurseCache.back();
		m_recurseCache.pop_back();

		NET_ASSERT(GetState(pChildEnt - &*m_slots.begin()) == eMSS_Active);
		for (uint32 depNodeId = pChildEnt->msg.firstDepNodeId; depNodeId != InvalidDepNodeId; depNodeId = m_depNodes[depNodeId].next)
		{
			SSendableHandle parentHdl = m_depNodes[depNodeId].hdl;
			if (SMsgSlot* pParentEnt = GetSlotSafe(parentHdl))
			{
				if (GetState(parentHdl.id) == eMSS_Active)
				{
					if (pParentEnt->ordering.latencyClass == eLC_CantSend)
						pChildEnt->ordering.latencyClass = eLC_CantSend;
					if (pParentEnt->depth >= pChildEnt->depth)
						pParentEnt->depth = pChildEnt->depth - 1;
					if (compare(m_slots[pChildEnt->sortOrderingSlot].ordering, m_slots[pParentEnt->sortOrderingSlot].ordering)) // child < parent
						pParentEnt->sortOrderingSlot = pChildEnt->sortOrderingSlot;
					pParentEnt->childrenPatched++;
					NET_ASSERT(pParentEnt->childrenPatched <= pParentEnt->childCount);
					if (pParentEnt->childrenPatched == pParentEnt->childCount)
						m_recurseCache.push_back(pParentEnt);
				}
			}
		}
	}

	ValidateHandles();
}

bool CMessageQueue::AreMessagesToWrite(const SSchedulingParams& params)
{
	CRY_PROFILE_FUNCTION(PROFILE_NETWORK);

	PrepareMessageList(params);

	CIncrementalSorter sorter(this);
	while (true)
	{
		uint32 idxEntSend = sorter.Next();
		if (idxEntSend == uint32(-1))
		{
			break;
		}

		SMsgSlot* pEntSend = &m_slots[idxEntSend];

		if (pEntSend->ordering.latencyClass == eLC_CantSend)
		{
			break;
		}
		ELatencyClass lcSort = m_slots[pEntSend->sortOrderingSlot].ordering.latencyClass;

		if (lcSort == eLC_DontBother)
		{
			continue;
		}

		if (pEntSend->liveness != eL_Alive)
		{
			continue;
		}

		if ((m_flags & eMQF_IsAfterSpawning) == 0 && pEntSend->msg.pSendable->CheckParallelFlag(eMPF_AfterSpawning))
		{
			continue;
		}

		if (pEntSend->pAG && lcSort == eLC_DontCare && pEntSend->pAG->BandwidthExceeded(params.now, true))
		{
			continue;
		}

		bool isBlocked = false;
		for (uint32 depNodeId = pEntSend->msg.firstDepNodeId; depNodeId != InvalidDepNodeId; depNodeId = m_depNodes[depNodeId].next)
		{
			SSendableHandle parentHdl = m_depNodes[depNodeId].hdl;
			if (SMsgSlot* pDep = GetSlotSafe(parentHdl))
			{
				if (pDep->liveness != eL_Sent && pDep->liveness != eL_Discarded)
				{
					isBlocked = true;
				}
				else if (GetState(parentHdl.id) == eMSS_Waiting || GetState(parentHdl.id) == eMSS_Limbo)
				{
					isBlocked = true;
				}
			}
		}
		if ((pEntSend->msg.flags & eSMF_TraditionalReliable) && m_reliableSeq)
		{
			isBlocked = true;
		}
		if (isBlocked)
		{
			continue;
		}

		return true;
	}

	return false;
}

void CMessageQueue::WriteMessages(IMessageOutput* pOut, const SSchedulingParams& params)
{
	CRY_PROFILE_FUNCTION(PROFILE_NETWORK);

	VerifyBlocking();
	ValidateHandles();

	uint32 newReliableSeq = m_reliableSeq;
	uint32 stopAfterBytes = params.targetBytes - 8;

	#if USE_ARITHSTREAM
	CCommOutputStream* pStm = pOut->GetStream();
	#else
	CNetOutputSerializeImpl* pStm = pOut->GetStream();
	#endif

	#if LOG_INCOMING_MESSAGES | LOG_OUTGOING_MESSAGES
	bool debug = (CNetCVars::Get().LogNetMessages & 128) != 0;
	#endif

	bool writtenHeader = false;
	CIncrementalSorter sorter(this);
	uint32 idxEntSend = 0;
	#if LOG_MESSAGE_QUEUE
	uint32 historyCount = 0;
	#endif // LOG_SEND_QUEUE
	while (true)
	{
		idxEntSend = sorter.Next();
		if (idxEntSend == uint32(-1))
			break;

		SMsgSlot* pEntSend = &m_slots[idxEntSend];

		if (pEntSend->ordering.latencyClass == eLC_CantSend)
			break;

		NET_ASSERT(pEntSend->childrenPatched == pEntSend->childCount);

		ELatencyClass lcSort = m_slots[pEntSend->sortOrderingSlot].ordering.latencyClass;

		switch (lcSort)
		{
		case eLC_DontBother:
			continue;
		case eLC_Discardable:
			if (!IsReliable(pEntSend->msg.pSendable->GetReliability()))
				pEntSend->liveness = eL_Discarded;
			break;
		}
		if (pEntSend->liveness != eL_Alive)
			continue;

		if ((m_flags & eMQF_IsAfterSpawning) == 0 && pEntSend->msg.pSendable->CheckParallelFlag(eMPF_AfterSpawning))
			continue;

		if (pEntSend->pAG && lcSort == eLC_DontCare && pEntSend->pAG->BandwidthExceeded(params.now, true))
			continue;

		bool isBlocked = false;
		for (uint32 depNodeId = pEntSend->msg.firstDepNodeId; depNodeId != InvalidDepNodeId; depNodeId = m_depNodes[depNodeId].next)
		{
			SSendableHandle parentHdl = m_depNodes[depNodeId].hdl;
			if (SMsgSlot* pDep = GetSlotSafe(parentHdl))
			{
				if (pDep->liveness != eL_Sent && pDep->liveness != eL_Discarded)
					isBlocked = true;
				else if (GetState(parentHdl.id) == eMSS_Waiting || GetState(parentHdl.id) == eMSS_Limbo)
					isBlocked = true;
			}
		}
		if (pEntSend->msg.flags & eSMF_TraditionalReliable)
		{
			if (m_reliableSeq)
				isBlocked = true;
			else
				newReliableSeq = params.nSeq;
		}
		if (isBlocked)
			continue;

		if (!writtenHeader)
		{
			switch (pOut->WriteHeader())
			{
			case eWMR_Delay:
				NET_ASSERT(false);
				break;
			case eWMR_Fail_Finish:
			case eWMR_Ok_Finish:
				return;
			}
			writtenHeader = true;
		}

	#if LOG_INCOMING_MESSAGES | LOG_OUTGOING_MESSAGES
		if (debug)
		{
			NetLog("Q: select %s hdl=%s", pEntSend->msg.pSendable->GetDescription(), HandleFromPointer(pEntSend).GetText());
			string buf;
			for (uint32 depNodeId = pEntSend->msg.firstDepNodeId; depNodeId != InvalidDepNodeId; depNodeId = m_depNodes[depNodeId].next)
				buf += string().Format("%s %s", depNodeId == pEntSend->msg.firstDepNodeId ? "  deps:" : ",", m_depNodes[depNodeId].hdl.GetText());
			if (!buf.empty())
				NetLog("%s", buf.c_str());
		}
	#endif

		uint32 sizeBefore = pStm->GetApproximateSize();
		EWriteMessageResult writeResult = eWMR_Fail_Finish;
		{
			//CRY_PROFILE_REGION(PROFILE_NETWORK, "CMessageQueue::WriteMessages.EncodeMessage");
			writeResult = pOut->WriteMessage(pEntSend->msg, HandleFromPointer(pEntSend));
			// need to recache pEntSend (it may have changed during the WriteMessage call)
			pEntSend = &m_slots[idxEntSend];
		}
		uint32 sizeAfter = pStm->GetApproximateSize();
		if (writeResult == eWMR_Delay)
			pEntSend->liveness = eL_Fresh;
		else if (writeResult == eWMR_Fail_Finish || writeResult == eWMR_Fail_Continue)
			pEntSend->liveness = eL_Rotten;
		else
		{
			pEntSend->liveness = eL_Sent;
	#if ENABLE_DEBUG_KIT
			if (CVARS.NetInspector)
			{
				NET_INSPECTOR.AddMessage(pEntSend->msg.pSendable->GetDescription(), (sizeAfter - sizeBefore) / 8.0f, (g_time - pEntSend->msg.inserted).GetMilliSeconds());
			}
	#endif
		}

	#if LOG_MESSAGE_QUEUE
		{
			if (historyCount < MESSAGE_HISTORY_SIZE)
			{
				g_messageHistory[historyCount].m_size = sizeAfter - sizeBefore;
				g_messageHistory[historyCount].m_index = idxEntSend;
			}
			++historyCount;
		}
	#endif // LOG_SEND_QUEUE

		if (pEntSend->pAG && sizeAfter > sizeBefore)
		{
			NET_ASSERT(sizeAfter - sizeBefore < MAX_TRANSMISSION_PACKET_SIZE);
			pEntSend->pAG->totBandwidthUsed += sizeAfter - sizeBefore;
			pEntSend->pAG->bandwidthThisFrame += sizeAfter - sizeBefore;
			if (pEntSend->pAG->sends.Full())
				pEntSend->pAG->PopSend();
			pEntSend->pAG->sends.Push(SSentEnt(params.now, (float)(sizeAfter - sizeBefore)));
		}
		if (writeResult == eWMR_Ok_Finish || writeResult == eWMR_Fail_Finish)
			break;
		if (pStm->GetApproximateSize() > stopAfterBytes)
			break;
	}

	#if LOG_MESSAGE_QUEUE
	// The assumption here is that we've sent at least 1 message but have unsent messages pending
	if (((CNetCVars::Get().net_logMessageQueue > 0) && (pStm->GetApproximateSize() > stopAfterBytes)) || (CNetCVars::Get().net_logMessageQueue > 1))
	{
		uint32 count = 0;
		uint32 sentCount = 0;

		const char* channelName = (m_pParent != NULL) ? m_pParent->GetName() : "<unknown>";

		NetLog("[Message]: [%s] approximate stream size %d (target was %d)", channelName, pStm->GetApproximateSize(), stopAfterBytes);
		idxEntSend = g_messageHistory[count].m_index;

		while (idxEntSend != uint32(-1))
		{
			SMsgSlot* pMsg = &m_slots[idxEntSend];
			ELatencyClass lcSort = m_slots[pMsg->sortOrderingSlot].ordering.latencyClass;
			char* latencyClass = "unknown";
			switch (lcSort)
			{
			case eLC_Discardable:
				latencyClass = "eLC_Discardable";
				break;
			case eLC_Expired:
				latencyClass = "eLC_Expired";
				break;
			case eLC_NearlyExpired:
				latencyClass = "eLC_NearlyExpired";
				break;
			case eLC_DontCare:
				latencyClass = "eLC_DontCare";
				break;
			case eLC_DontBother:
				latencyClass = "eLC_DontBother";
				break;
			case eLC_CantSend:
				latencyClass = "eLC_CantSend";
				break;
			}
			char* liveliness = "unknown";
			switch (pMsg->liveness)
			{
			case eL_Fresh:
				liveliness = "eL_Fresh";
				break;
			case eL_Alive:
				liveliness = "eL_Alive";
				break;
			case eL_Sent:
				liveliness = "eL_Sent";
				++sentCount;
				break;
			case eL_Rotten:
				liveliness = "eL_Rotten";
				break;
			case eL_Discarded:
				liveliness = "eL_Discarded";
				break;
			}
			uint32 calcSlot = pMsg - &*m_slots.begin();
			NetLog("[Message]: [%s]: [%s] idx %d  (desc/grp) %s:%s (time) %.3f (sched order) %.2f:%.2f (slot) (%d:%d) (latency class) %s (size) %d",
			       liveliness,
			       channelName,
			       idxEntSend,
			       pMsg->msg.pSendable->GetDescription(),
			       (pMsg->msg.pSendable->GetGroup()) ? (KeyToString(pMsg->msg.pSendable->GetGroup())).c_str() : "????",
			       (params.now - pMsg->msg.inserted).GetSeconds(),
			       m_slots[pMsg->sortOrderingSlot].ordering.schedulingOrder,
			       (pMsg->sortOrderingSlot != calcSlot) ? pMsg->ordering.schedulingOrder : 0,
			       pMsg->sortOrderingSlot,
			       calcSlot,
			       latencyClass,
			       (pMsg->liveness == eL_Sent) ? g_messageHistory[count].m_size : -1);

			++count;
			idxEntSend = (count < historyCount) ? g_messageHistory[count].m_index : sorter.Next();
		}

		NetLog("[Message]: [%s] sent %d (unsent %d) messages", channelName, sentCount, count - sentCount);
	}
	#endif // LOG_SEND_QUEUE

	if (writtenHeader || true)
	{
		switch (pOut->WriteFooter())
		{
		case eWMR_Delay:
			NET_ASSERT(false);
			break;
		case eWMR_Fail_Finish:
		case eWMR_Ok_Finish:
			break;
		}
	}
	m_reliableSeq = newReliableSeq;

	ValidateHandles();
	VerifyBlocking();
}

void CMessageQueue::FinishFrame(const SSchedulingParams* pParams)
{
	CRY_PROFILE_FUNCTION(PROFILE_NETWORK);

	ValidateHandles();
	VerifyBlocking();

	static std::vector<std::pair<uint32, EMsgSlotState>> changeStates;
	NET_ASSERT(changeStates.empty());

	CActiveElemIterator iter(this, eMSS_Active);
	while (SMsgSlot* pSlot = iter.Next())
	{
		if (pSlot->liveness == eL_Rotten || pSlot->liveness == eL_Discarded)
		{
	#if LOG_MESSAGE_DROPS
			if (CNetCVars::Get().LogDroppedMessages())
				NetLog("REJECTED MESSAGE FROM QUEUE: [%s] %s", HandleFromPointer(pSlot).GetText(), pSlot->msg.pSendable->GetDescription());
	#endif
			pSlot->msg.pSendable->UpdateState(pParams ? pParams->nSeq : ~0, eNSSU_Rejected);
			changeStates.push_back(std::make_pair(uint32(pSlot - &*m_slots.begin()), eMSS_Dead));
		}
		else if (pSlot->liveness == eL_Sent)
		{
			changeStates.push_back(std::make_pair(uint32(pSlot - &*m_slots.begin()), eMSS_Waiting));
		}
	}
	iter = CActiveElemIterator(this, eMSS_JustQueued);
	while (SMsgSlot* pSlot = iter.Next())
	{
		changeStates.push_back(std::make_pair(uint32(pSlot - &*m_slots.begin()), eMSS_Active));
	}

	while (!changeStates.empty())
	{
		SetState(changeStates.back().first, changeStates.back().second);
		changeStates.pop_back();
	}

	iter = CActiveElemIterator(this, eMSS_Dead);
	while (SMsgSlot* pSlot = iter.Next())
	{
		changeStates.push_back(std::make_pair(uint32(pSlot - &*m_slots.begin()), eMSS_Dead));
		m_nBlockingMessages -= pSlot->msg.pSendable->CheckParallelFlag(eMPF_BlocksStateChange);
		m_nAlertedMessages -= !pSlot->msg.pSendable->CheckParallelFlag(eMPF_DontAwake);
		TO_GAME(&CNetChannel::GC_SendableSink, pParams->pChannel, pSlot->msg.pSendable);
		pSlot->msg.pSendable = INetSendablePtr();
		FreeDependencies(pSlot);
		pSlot->Reset();
		m_nMessages--;
	}

	while (!changeStates.empty())
	{
		uint32 id = changeStates.back().first;
		SSlotState& slotState = m_slotState[id];
		m_slotState[slotState.prev].next = slotState.next;
		m_slotState[slotState.next].prev = slotState.prev;
		slotState.state = eMSS_Free;
		uint32 bin = id >> LOG2_SLOTS_PER_BIN;
		if (bin >= m_freeSlotNumElems.size())
		{
			m_freeSlotNumElems.resize(bin + 1, 0);
			m_freeSlots.resize(bin + 1);
		}
		NET_ASSERT(m_freeSlotNumElems[bin] < SLOTS_PER_BIN);
		m_freeSlots[bin].elems[m_freeSlotNumElems[bin]++] = id;
		changeStates.pop_back();
	}

	ValidateHandles();
	VerifyBlocking();
}

void CMessageQueue::SetConfig(CConfig* pConfig, int version)
{
	m_nAccountingGroups = 0;
	for (std::map<uint32, SAccountingGroupPolicy>::iterator iter = pConfig->m_policy.begin(); iter != pConfig->m_policy.end(); ++iter)
	{
		NET_ASSERT(iter->first > SNetObjectID::InvalidId);
		SAccountingGroup* pGrp = &m_accountingGroups[m_nAccountingGroups++];
		pGrp->policy = iter->second;
		pGrp->totBandwidthUsed = 0;
		pGrp->id = iter->first;

		if (pGrp->policy.maxBandwidth > 0)
		{
			pGrp->totBandwidthUsed = pGrp->policy.maxBandwidth;
			pGrp->sends.Push(SSentEnt(g_time, pGrp->policy.maxBandwidth));
		}
	}
	if (m_nAccountingGroups > 0)
	{
		std::sort(m_accountingGroups, m_accountingGroups + m_nAccountingGroups, CCompareAccountingGroupsById());
	}
	m_version = version;

	for (size_t i = 0; i < m_slots.size(); i++)
	{
		if (GetState(i) == eMSS_Active || GetState(i) == eMSS_Waiting)
			m_slots[i].pAG = GetAccountingGroup(m_slots[i].msg.pSendable->GetGroup());
	}
}

ILINE CMessageQueue::SAccountingGroup* CMessageQueue::GetAccountingGroup(uint32 grpId)
{
	int count = m_nAccountingGroups;
	SAccountingGroup* pFirst = m_accountingGroups;
	SAccountingGroup* pLast = m_accountingGroups + m_nAccountingGroups;
	for (; 0 < count; )
	{
		int count2 = count / 2;
		SAccountingGroup* pMid = pFirst + count2;

		if (pMid->id < grpId)
			pFirst = pMid + 1, count -= count2 + 1;
		else
			count = count2;
	}
	if (pFirst != pLast && pFirst->id == grpId)
		return pFirst;
	else
		return 0;
}

void CMessageQueue::RegularCleanup(const SSchedulingParams& params)
{
	CRY_PROFILE_FUNCTION(PROFILE_NETWORK);

	CTimeValue oldTime = params.now - 0.5f;

	int curVersion = CNetwork::Get()->GetMessageQueueConfigVersion();
	if (m_version != curVersion)
	{
		for (int i = 0; i < MAX_ACCOUNTING_GROUPS; i++)
			m_accountingGroups[i].id = ~uint32(0);

		CConfig* pConfig = CNetwork::Get()->GetMessageQueueConfig();
		if (pConfig)
		{
			SetConfig(CNetwork::Get()->GetMessageQueueConfig(), curVersion);
		}
		else
		{
			NetWarning("Network configuration not found!");
		}
	}

	for (int i = 0; i < m_nAccountingGroups; i++)
	{
		SAccountingGroup* pAG = &m_accountingGroups[i];
		float maxBW = pAG->policy.maxBandwidth;
		while (pAG->sends.Size() > 4 && pAG->totBandwidthUsed > 3.0f * maxBW && pAG->sends.Front().when < oldTime)
			pAG->PopSend();
	}
}

void CMessageQueue::FlushObjectHeads()
{
	if (!m_objectHeads.empty())
	{
		std::fill(m_objectHeads.begin(), m_objectHeads.end(), ~uint32(0));
	}
}

void CMessageQueue::PrepareMessageList(const SSchedulingParams& params)
{
	ValidateHandles();

	FlushObjectHeads();
	BeginAccountingFrame();
	PrepareLiveList();
	CalculatePerFrameData(params);
	PatchObjectGroupings();
	PatchOrderedPriorities();

	ValidateHandles();
}

bool CMessageQueue::BuildPacket(IMessageOutput* pOut, const SSchedulingParams& params)
{
	NET_ASSERT(!m_inWrite);
	m_inWrite = true;
	RegularCleanup(params);
	PrepareMessageList(params);
	WriteMessages(pOut, params);
	FinishFrame(&params);
	m_inWrite = false;
	for (std::vector<SSendableHandle>::iterator it = m_pendingRemovals.begin(); it != m_pendingRemovals.end(); ++it)
		RemoveSendable(*it);
	m_pendingRemovals.resize(0);
	return true;
}

void CMessageQueue::AckMessages(SSendableHandle* pMsgs, size_t nMsgs, uint32 nSeq, bool ack, bool clear)
{
	VerifyBlocking();
	ValidateHandles();

	if (nSeq == m_reliableSeq)
		m_reliableSeq = 0;

	if (!ack)
	{
		for (size_t i = 0; i < nMsgs; i++)
		{
			SMsgSlot* pEnt = GetSlot(pMsgs[i]);
			EMsgSlotState state = GetState(pMsgs[i].id);
			NET_ASSERT(state == eMSS_Waiting || state == eMSS_Limbo);

			ENetSendableStateUpdate newState = eNSSU_Requeue;
			if (state == eMSS_Limbo)
			{
				state = eMSS_Dead;
				newState = eNSSU_Nack;
			}
			else if (IsReliable(pEnt->msg.pSendable->GetReliability()))
			{
				if (!m_emptyMode)
					state = eMSS_Active;
				else
				{
					state = eMSS_Dead;
					newState = eNSSU_Nack;
				}
			}
			else
			{
				newState = eNSSU_Nack;
				state = eMSS_Dead;
			}
	#if LOG_MESSAGE_DROPS
			if (CNetCVars::Get().LogDroppedMessages() && newState != eNSSU_Requeue)
				NetLog("DROPPED MESSAGE: [%s] %s", pMsgs[i].GetText(), pEnt->msg.pSendable->GetDescription());
	#endif
			pEnt->msg.pSendable->UpdateState(nSeq, newState);
			SetState(pMsgs[i].id, state);
		}
	}
	else
	{
		for (size_t i = nMsgs; i != 0; )
		{
			--i;
			SMsgSlot* pEnt = GetSlot(pMsgs[i]);
			EMsgSlotState state = GetState(pMsgs[i].id);
			NET_ASSERT(state == eMSS_Waiting || state == eMSS_Limbo);
			SetState(pMsgs[i].id, eMSS_Dead);
			pEnt->msg.pSendable->UpdateState(nSeq, eNSSU_Ack);
			// WARNING: pEnt no longer valid (UpdateState may queue messages!!)
			if (pMsgs[i] == m_stateChangeHandle)
				m_stateChangeHandle = SSendableHandle();
		}
	}

	ValidateHandles();
	VerifyBlocking();
}

void CMessageQueue::Empty(bool includeRoots)
{
	std::vector<uint32> mySlots;
	for (int type = 0; type < eMSS_NUM_LIVE_SLOT_TYPES; type++)
	{
		CActiveElemIterator iter(this, (EMsgSlotState)type);
		while (SMsgSlot* pSlot = iter.Next())
			mySlots.push_back(pSlot - &*m_slots.begin());
		if (includeRoots)
			mySlots.push_back(m_rootSlots[type].id);
	}

	for (size_t i = 0; i < mySlots.size(); i++)
	{
		uint32 id = mySlots[i];

		FreeDependencies(&m_slots[id]);
		m_slots[id].Reset();
		m_slots[id].msg.pSendable = INetSendablePtr();
		SSlotState& state = m_slotState[id];
		state.prev = state.next = id;
		state.state = eMSS_Free;
	}

	if (!includeRoots)
	{
		for (int i = 0; i < eMSS_NUM_LIVE_SLOT_TYPES; i++)
		{
			uint32 id = m_rootSlots[i].id;
			m_slotState[id].next = m_slotState[id].prev = id;
		}
	}
}

bool CMessageQueue::IsEmpty() const
{
	return m_nMessages == 0;
}

static int GetIndentForMessage(int depth)
{
	/*
	   NET_ASSERT(depth <= 0);
	   depth = -depth;
	   return int(0.5f+100*expf(-0.10536f*depth));
	 */
	return 0;
}

void CMessageQueue::DrawLabel(float x, float y, float* clr, const char* msg, ...)
{
	va_list args;
	va_start(args, msg);
	char buffer[4096];
	cry_vsprintf(buffer, msg, args);

	IRenderAuxText::Draw2dLabel((float)x, (float)y, 1.f, clr, false, "%s", buffer);
	ITextModeConsole* t = gEnv->pSystem->GetITextModeConsole();
	if (t)
		t->PutText((int)(x / 4 - 2), (int)(y / 10 - 1), buffer);

	va_end(args);
}

const char* CMessageQueue::DebugMessageForHandle(SSendableHandle hdl)
{
	SMsgSlot* pSlot = GetSlotSafe(hdl);
	if (!pSlot)
		return "complete";
	SSlotState state = m_slotState[pSlot - &*m_slots.begin()];
	switch (state.state)
	{
	case eMSS_Free:
		return "complete";
	case eMSS_Active:
		return "queued";
	case eMSS_Waiting:
		return "inflight";
	case eMSS_Limbo:
		return "limbo";
	case eMSS_Dead:
		return "dead";
	default:
		NET_ASSERT(!"invalid state");
		return "broken";
	}
}

void CMessageQueue::SetState(uint32 slot, EMsgSlotState state)
{
	assert(state < eMSS_NUM_LIVE_SLOT_TYPES);

	SSlotState& slotState = m_slotState[slot];
	if (slotState.state >= eMSS_NUM_LIVE_SLOT_TYPES)
		CryFatalError("Cannot change state from the special states");
	else if (state >= eMSS_NUM_LIVE_SLOT_TYPES)
		CryFatalError("Cannot change state to the special states");
	if (slotState.state != state)
	{
		m_slotState[slotState.prev].next = slotState.next;
		m_slotState[slotState.next].prev = slotState.prev;

		uint32 rootId = m_rootSlots[state].id;
		slotState.next = rootId;
		slotState.prev = m_slotState[rootId].prev;
		m_slotState[slotState.prev].next = slot;
		m_slotState[slotState.next].prev = slot;
	}
	slotState.state = state;
}

void CMessageQueue::GetBandwidthStatistics(uint32 channelIndex, SBandwidthStats* pStats)
{
	int accountingGroupIndex = 0;

	while (accountingGroupIndex < m_nAccountingGroups)
	{
		SAccountingGroup& accountingGroup = m_accountingGroups[accountingGroupIndex];
		SAccountingGroupStats& stats = pStats->m_channel[channelIndex].m_messageQueue.m_accountingGroup[accountingGroupIndex];

		stats.m_inUse = true;
		cry_strcpy(stats.m_name, KeyToString(accountingGroup.id).c_str());
		stats.m_sends = accountingGroup.sends.Size();
		stats.m_bandwidthUsed = accountingGroup.GetBandwidthUsed(g_time);
		stats.m_totalBandwidthUsed = accountingGroup.totBandwidthUsed;
		stats.m_priority = static_cast<uint32>(accountingGroup.policy.priority);
		stats.m_maxLatency = accountingGroup.policy.maxLatency;
		stats.m_discardLatency = accountingGroup.policy.discardLatency;

		accountingGroupIndex++;
	}

	while (accountingGroupIndex < STATS_MAX_MESSAGEQUEUE_ACCOUNTING_GROUPS)
	{
		SAccountingGroupStats& stats = pStats->m_channel[channelIndex].m_messageQueue.m_accountingGroup[accountingGroupIndex];
		stats.m_inUse = false;

		accountingGroupIndex++;
	}
}

void CMessageQueue::DebugDraw(const SSchedulingParams& params)
{
	PrepareMessageList(params);

	int y = 10;

	float white[4] = { 1, 1, 1, 1 };
	float shade_white[4] = { 1, 1, 1, 0.5f };
	float red[4] = { 1, 0, 0, 1 };
	float yellow[4] = { 1, 1, 0, 1 };
	//	float green[4] = {0,1,0,1};

	for (int i = 0; i < m_nAccountingGroups; i++)
	{
		SAccountingGroup* pAG = &m_accountingGroups[i];
		float a = 1.0f - (pAG->GetBandwidthUsed(params.now) / pAG->policy.maxBandwidth);
		float clr[] = { 1, a, a, 1 };
		DrawLabel(10, y, clr, "Group %s prio:%.2f lat:%.2f maxbw:%.2f drawn:%d curbw:%.2f",
		          KeyToString(pAG->id).c_str(),
		          pAG->policy.priority,
		          pAG->policy.maxLatency,
		          pAG->policy.maxBandwidth,
		          pAG->policy.drawn,
		          pAG->GetBandwidthUsed(params.now));
		y += 10;
	}

	DrawLabel(10, y, white, "%d/%d messages blocking state change", m_nBlockingMessages, m_nMessages);
	y += 10;
	#if FULL_ON_SCHEDULING
	float drawDistanceScale = 1.0f;
	if (params.haveWitnessFov)
	{
		drawDistanceScale = 0.05f + 0.95f * (RAD2DEG(params.witnessFov) / 60.f); // from 3dengine
		drawDistanceScale = 1.2f / drawDistanceScale;
		DrawLabel(10, y, white, "Draw distance scale: %f", drawDistanceScale);
		y += 10;
	}
	#endif

	y += (y != 10) * 10;
	CIncrementalSorter sorter(this);
	while (true)
	{
		uint32 idxMsg = sorter.Next();
		if (idxMsg == uint32(-1))
			break;
		SMsgSlot* pMsg = &m_slots[idxMsg];

		ELatencyClass lcSort = m_slots[pMsg->sortOrderingSlot].ordering.latencyClass;

		float* clr = white;
		if (lcSort == eLC_Expired)
			clr = red;
		else if (lcSort == eLC_NearlyExpired)
			clr = yellow;
		else if (lcSort >= eLC_DontBother)
			clr = shade_white;

		int indent = GetIndentForMessage(pMsg->depth);

		DrawLabel(10, y, clr, "%.2f", (params.now - pMsg->msg.inserted).GetSeconds());
		DrawLabel(30, y, clr, "%.2f", m_slots[pMsg->sortOrderingSlot].ordering.schedulingOrder);
		if (pMsg->sortOrderingSlot != pMsg - &*m_slots.begin())
			DrawLabel(60, y, clr, "%.2f", pMsg->ordering.schedulingOrder);
		if (pMsg->msg.pSendable->GetGroup())
			DrawLabel(120, y, clr, "%s", (KeyToString(pMsg->msg.pSendable->GetGroup())).c_str());

		if (pMsg->pAG)
		{
			float bump = 0.0f;
			bool isWithinRenderedDistance = true;
			SMessagePositionInfo posInfo;
			pMsg->msg.pSendable->GetPositionInfo(posInfo);

	#if FULL_ON_SCHEDULING
			if (params.haveWitnessPosition && posInfo.havePosition)
			{
				float distanceFromWitness = params.witnessPosition.GetDistance(posInfo.position);
				if (posInfo.haveDrawDistance)
					isWithinRenderedDistance = distanceFromWitness < posInfo.drawDistance * drawDistanceScale;
				bump += pMsg->pAG->policy.distanceScaler.GetBump(distanceFromWitness);
				if (params.haveWitnessDirection)
				{
					Vec3 dirToMessage = (posInfo.position - params.witnessPosition).GetNormalizedSafe(params.witnessDirection);
					bump += pMsg->pAG->policy.dirScaler.GetBump(params.witnessDirection.Dot(dirToMessage));
				}
			}
	#endif

			const CPriorityPulseState* pPulseState = pMsg->msg.pSendable->GetPulses();
			if (pPulseState && pPulseState->GetNumPulses() && pMsg->pAG->policy.numPulses)
			{
				const CPriorityPulseState::SPulse* pP0 = pPulseState->GetPulses();
				const CPriorityPulseState::SPulse* pP0_End = pPulseState->GetPulses() + pPulseState->GetNumPulses();
				SAccountingGroupPulse* pP1 = pMsg->pAG->policy.pulses;
				SAccountingGroupPulse* pP1_End = pMsg->pAG->policy.pulses + pMsg->pAG->policy.numPulses;
				while (pP0 != pP0_End && pP1 != pP1_End)
				{
					if (pP0->key < pP1->name)
						pP0++;
					else if (pP0->key > pP1->name)
						pP1++;
					else
					{
						bump += pP1->scaler.GetBump((params.now - pP0->tm).GetSeconds());
						pP0++;
						pP1++;
					}
				}
			}

			if (pMsg->pAG->policy.drawn && !isWithinRenderedDistance)
				bump = -32;

			DrawLabel(150, y, clr, "%.2f", bump);
		}
		CryFixedStringT<512> msglbl;
		CryFixedStringT<512> tmpstr;
		msglbl += tmpstr.Format("%d:%d: %s", HandleFromPointer(pMsg).id, HandleFromPointer(pMsg).salt, pMsg->msg.pSendable->GetDescription());
		if (pMsg->msg.numDepNodes)
		{
			msglbl += " (";
			for (uint32 depNodeId = pMsg->msg.firstDepNodeId; depNodeId != InvalidDepNodeId; depNodeId = m_depNodes[depNodeId].next)
				msglbl += tmpstr.Format("%s:%s", DebugMessageForHandle(m_depNodes[depNodeId].hdl), m_depNodes[depNodeId].hdl.GetText());
		}
		if (msglbl.length() > 512)
		{
			msglbl.resize(512);
			msglbl += "...";
		}
		DrawLabel(180 + indent, y, clr, "%s", msglbl.c_str());
		y += 10;
	}
}

#endif // !NEW_BANDWIDTH_MANAGEMENT
