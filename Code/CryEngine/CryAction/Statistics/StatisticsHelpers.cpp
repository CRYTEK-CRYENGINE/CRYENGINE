// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   StatisticsHelpers.cpp
//  Version:     v1.00
//  Created:     04/03/2010 by Sergey Mikhtonyuk
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////
#include "StdAfx.h"
#include "StatisticsHelpers.h"
#include "GameStatistics.h"
#include "CryActionCVars.h"
#include "StatsSizer.h"

//////////////////////////////////////////////////////////////////////////
// CStatsTracker
//////////////////////////////////////////////////////////////////////////

CStatsTracker::CStatsTracker(const SNodeLocator& locator, CGameStatistics* pGameStats, IScriptTable* pTable)
	: m_pGameStats(pGameStats)
	, m_locator(locator)
	, m_scriptTable(pTable)
{
	int eventCount = pGameStats->GetEventCount();
	int stateCount = pGameStats->GetStateCount();
	m_container = m_pGameStats->GetStorageFactory()->CreateContainer();
	m_container->Init(eventCount, stateCount);
}

//////////////////////////////////////////////////////////////////////////

CStatsTracker::~CStatsTracker()
{
}

//////////////////////////////////////////////////////////////////////////

SNodeLocator CStatsTracker::GetLocator() const
{
	return m_locator;
}

//////////////////////////////////////////////////////////////////////////

IScriptTable* CStatsTracker::GetScriptTable() const
{
	return m_scriptTable.GetPtr();
}

//////////////////////////////////////////////////////////////////////////

IStatsContainer* CStatsTracker::GetStatsContainer()
{
	return m_container;
}

//////////////////////////////////////////////////////////////////////////

void CStatsTracker::StateValue(size_t stateID, const SStatAnyValue& value)
{
#if STATS_MODE_CVAR
	if (CCryActionCVars::Get().g_statisticsMode != 2)
		return;
#endif

	m_container->AddState(stateID, value);

	m_pGameStats->OnTrackedState(m_locator, stateID, value);
}

//////////////////////////////////////////////////////////////////////////

void CStatsTracker::Event(size_t eventID, const SStatAnyValue& value)
{
#if STATS_MODE_CVAR
	if (CCryActionCVars::Get().g_statisticsMode != 2)
		return;
#endif

	CTimeValue timeVal = gEnv->pTimer->GetFrameStartTime(ITimer::ETIMER_UI);
	m_container->AddEvent(eventID, timeVal, value);

	m_pGameStats->OnTrackedEvent(m_locator, eventID, timeVal, value);
}

//////////////////////////////////////////////////////////////////////////

void CStatsTracker::GetMemoryStatistics(ICrySizer* pSizer)
{
	pSizer->Add(*this);
	m_container->GetMemoryStatistics(pSizer);
}

//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
// CStatsContainer
//////////////////////////////////////////////////////////////////////////

void CStatsContainer::Init(size_t numEvents, size_t numStates)
{
	m_events.reserve(numEvents);
	m_states.reserve(numStates);
}

//////////////////////////////////////////////////////////////////////////

void CStatsContainer::AddEvent(size_t eventID, const CTimeValue& time, const SStatAnyValue& val)
{
	// Do we have a track for this event?
	if (eventID >= m_events.size())
	{
		// Events must be obsolete, extending event set
		m_events.resize(eventID + 1);
	}

	m_events[eventID].push_back(std::make_pair(time, val));

	CStatsSizer sizer;
	val.GetMemoryUsage(&sizer);
	m_unloadableMemUsage += sizer.GetTotalSize();
}

//////////////////////////////////////////////////////////////////////////

void CStatsContainer::AddState(size_t stateID, const SStatAnyValue& val)
{
	// Do we have a track for this event?
	if (stateID >= m_states.size())
	{
		// Events must be obsolete, extending event set
		m_states.resize(stateID + 1);
	}

	m_states[stateID] = val;

	CStatsSizer sizer;
	val.GetMemoryUsage(&sizer);
	m_unloadableMemUsage += sizer.GetTotalSize();
}

//////////////////////////////////////////////////////////////////////////

size_t CStatsContainer::GetEventTrackLength(size_t eventID) const
{
	return eventID >= m_events.size() ? 0 : m_events[eventID].size();
}

//////////////////////////////////////////////////////////////////////////

void CStatsContainer::GetEventInfo(size_t eventID, size_t idx, CTimeValue& outTime, SStatAnyValue& outParam) const
{
	if (eventID < m_events.size() && idx < m_events[eventID].size())
	{
		outTime = m_events[eventID][idx].first;
		outParam = m_events[eventID][idx].second;
	}
	else
	{
		outTime = CTimeValue();
		outParam = SStatAnyValue();
	}
}

//////////////////////////////////////////////////////////////////////////

void CStatsContainer::GetStateInfo(size_t stateID, SStatAnyValue& outValue) const
{
	if (stateID < m_states.size())
		outValue = m_states[stateID];
	else
		outValue = SStatAnyValue();
}

//////////////////////////////////////////////////////////////////////////

void CStatsContainer::Clear()
{
	for (size_t i = 0; i != m_events.size(); ++i)
		m_events[i].clear();

	for (size_t i = 0; i != m_states.size(); ++i)
	{
		SStatAnyValue none;
		m_states[i] = none;
	}

	m_unloadableMemUsage = 0;
}

//////////////////////////////////////////////////////////////////////////

bool CStatsContainer::IsEmpty() const
{
	return m_unloadableMemUsage == 0;
}

//////////////////////////////////////////////////////////////////////////

void CStatsContainer::GetMemoryStatistics(ICrySizer* pSizer)
{
	pSizer->Add(*this);
	pSizer->AddContainer(m_events);
	pSizer->AddContainer(m_states);

	for (size_t i = 0; i != m_events.size(); ++i)
	{
		const TEventTrack& track = m_events[i];
		pSizer->AddContainer(track);
	}

	pSizer->AddObject(this, m_unloadableMemUsage);
}

//////////////////////////////////////////////////////////////////////////

void CStatsContainer::AddRef()
{
	++m_refCount;
}

//////////////////////////////////////////////////////////////////////////

void CStatsContainer::Release()
{
	--m_refCount;
	if (m_refCount <= 0)
	{
		delete this;
	}
}

//////////////////////////////////////////////////////////////////////////

IStatsContainer* CDefaultStorageFactory::CreateContainer()
{
	return new CStatsContainer();
}

//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
// SDeadStatNode
//////////////////////////////////////////////////////////////////////////

SDeadStatNode::SDeadStatNode(const SNodeLocator& loc, CStatsTracker* track)
	: locator(loc)
	, tracker(track)
	, cachedMemoryStats(MEMORY_LIMIT_MAXIMUM)
{}

//////////////////////////////////////////////////////////////////////////

SDeadStatNode::~SDeadStatNode()
{
	delete tracker;
}

//////////////////////////////////////////////////////////////////////////

size_t SDeadStatNode::GetMemoryStatistics() const
{
	// The sub-tree is static, so we can calculate memory once and cache it
	if (cachedMemoryStats == MEMORY_LIMIT_MAXIMUM)
	{
		CStatsSizer sizer;

		sizer.Add(*this);
		sizer.AddContainer(children);
		tracker->GetMemoryStatistics(&sizer);

		for (size_t i = 0; i != children.size(); ++i)
			children[i]->GetMemoryStatistics(&sizer);

		cachedMemoryStats = sizer.GetTotalSize();
	}

	return cachedMemoryStats;
}

//////////////////////////////////////////////////////////////////////////

void SDeadStatNode::GetMemoryStatistics(ICrySizer* pSizer) const
{
	pSizer->AddObject(this, GetMemoryStatistics());
}

//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
// SScopeData
//////////////////////////////////////////////////////////////////////////

SScopeData::SScopeData(SNodeLocator _locator, CStatsTracker* _tracker)
	: locator(_locator)
	, tracker(_tracker)
{}

//////////////////////////////////////////////////////////////////////////

void SScopeData::GetMemoryStatistics(ICrySizer* pSizer) const
{
	pSizer->Add(*this);
	pSizer->AddContainer(elements);
	pSizer->AddContainer(deadNodes);

	for (TElements::const_iterator it = elements.begin(); it != elements.end(); ++it)
		it->second->GetMemoryStatistics(pSizer);

	for (size_t i = 0; i != deadNodes.size(); ++i)
		deadNodes[i]->GetMemoryStatistics(pSizer);
}

//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
// CGameScope
//////////////////////////////////////////////////////////////////////////

CGameScopes::CGameScopes()
{
	GetISystem()->GetXmlUtils()->InitStatsXmlNodePool(64 * 1024);
}

//////////////////////////////////////////////////////////////////////////

bool CGameScopes::RegisterGameScopes(const SGameScopeDesc scopeDescs[], size_t numScopes)
{
	CRY_ASSERT(scopeDescs);
	CRY_ASSERT(numScopes > 0);

	if (!ValidateScopes(scopeDescs, numScopes))
		return false;

	GrowRegistry(numScopes);
	InsertScopesInRegistry(scopeDescs, numScopes);

	return true;
}

//////////////////////////////////////////////////////////////////////////

const SGameScopeDesc* CGameScopes::GetScopeDesc(size_t id) const
{
	return id < m_scopeRegistry.size() ? &m_scopeRegistry[id] : 0;
}

//////////////////////////////////////////////////////////////////////////

const SGameScopeDesc* CGameScopes::GetScopeDesc(const char* scriptName) const
{
	TScopeMap::const_iterator it = m_scopeMap.find(scriptName);
	return it != m_scopeMap.end() ? GetScopeDesc(it->second) : 0;
}

//////////////////////////////////////////////////////////////////////////

CStatsTracker* CGameScopes::PushGameScope(size_t scopeID, uint32 timestamp, CGameStatistics* gameStatistics)
{
	CRY_ASSERT(scopeID < m_scopeRegistry.size());
	if (scopeID >= m_scopeRegistry.size())
		return 0;

	// Check that same scope isn't already on the stack
	size_t pos = FindScopePos(scopeID);
	CRY_ASSERT_MESSAGE(pos == m_scopeStack.size(), "Same scope can't be pushed twice");
	if (pos != m_scopeStack.size())
		return 0;

	SNodeLocator locator(scopeID);
	locator.timeStamp = timestamp;
	CStatsTracker* tracker = new CStatsTracker(locator, gameStatistics);
	m_scopeStack.push_back(SScopeData(locator, tracker));

	return tracker;
}

//////////////////////////////////////////////////////////////////////////

SDeadStatNode* CGameScopes::PopGameScope()
{
	SNodeLocator locator = m_scopeStack.back().locator;
	CStatsTracker* tracker = m_scopeStack.back().tracker;

	SDeadStatNode* deadScope = new SDeadStatNode(locator, tracker);
	std::swap(deadScope->children, m_scopeStack.back().deadNodes);

	m_scopeStack.pop_back();

	if (!m_scopeStack.empty())
		m_scopeStack.back().deadNodes.push_back(deadScope);
	else
		stl::free_container(m_scopeStack);

	return deadScope;
}

//////////////////////////////////////////////////////////////////////////

size_t CGameScopes::FindScopePos(size_t scopeID) const
{
	size_t size = m_scopeStack.size();
	for (size_t i = 0; i < size; ++i)
	{
		if (m_scopeStack[i].locator.scopeID == scopeID)
		{
			return i;
		}
	}
	return size;
}

//////////////////////////////////////////////////////////////////////////

size_t CGameScopes::GetRegisteredCount() const
{
	return m_scopeRegistry.size();
}

//////////////////////////////////////////////////////////////////////////

size_t CGameScopes::GetStackSize() const
{
	return m_scopeStack.size();
}

//////////////////////////////////////////////////////////////////////////

const SScopeData& CGameScopes::GetScopeAt(size_t pos) const
{
	return m_scopeStack[pos];
}

//////////////////////////////////////////////////////////////////////////

SScopeData& CGameScopes::GetScopeAt(size_t pos)
{
	return m_scopeStack[pos];
}

//////////////////////////////////////////////////////////////////////////

const SScopeData& CGameScopes::GetLastScope() const
{
	return m_scopeStack.back();
}

//////////////////////////////////////////////////////////////////////////

SScopeData& CGameScopes::GetLastScope()
{
	return m_scopeStack.back();
}

//////////////////////////////////////////////////////////////////////////

bool CGameScopes::ValidateScopes(const SGameScopeDesc scopeDescs[], size_t numScopes)
{
	const size_t firstFreeID = m_scopeRegistry.size();

	std::set<const char*, stl::less_strcmp<const char*>> scriptNames;

	for (size_t i = 0; i != numScopes; ++i)
	{
		if (!scopeDescs[i].IsValid())
			return false;

		if (scopeDescs[i].statID < firstFreeID || scopeDescs[i].statID >= (firstFreeID + numScopes))
			return false;

		if (m_scopeMap.find(scopeDescs[i].scriptName) != m_scopeMap.end()) // name collision
			return false;

		if (!scriptNames.insert(scopeDescs[i].scriptName.c_str()).second)
			return false;
	}
	return true;
}

//////////////////////////////////////////////////////////////////////////

void CGameScopes::GrowRegistry(size_t numScopes)
{
	m_scopeRegistry.resize(m_scopeRegistry.size() + numScopes);
}

//////////////////////////////////////////////////////////////////////////

void CGameScopes::InsertScopesInRegistry(const SGameScopeDesc scopeDescs[], size_t numScopes)
{
	for (size_t i = 0; i != numScopes; ++i)
	{
		CRY_ASSERT(m_scopeRegistry[scopeDescs[i].statID].scriptName.empty());

		m_scopeRegistry[scopeDescs[i].statID] = scopeDescs[i];

		m_scopeMap.insert(std::make_pair(scopeDescs[i].scriptName, scopeDescs[i].statID));
	}
}

//////////////////////////////////////////////////////////////////////////

void CGameScopes::GetMemoryStatistics(ICrySizer* pSizer) const
{
	pSizer->AddContainer(m_scopeRegistry);
	pSizer->AddContainer(m_scopeMap);
	pSizer->AddContainer(m_scopeStack);

	for (size_t i = 0; i != m_scopeStack.size(); ++i)
		m_scopeStack[i].GetMemoryStatistics(pSizer);
}

//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
// CStatRegistry
//////////////////////////////////////////////////////////////////////////

bool CStatRegistry::Register(const SGameStatDesc statDescs[], size_t numStats)
{
	CRY_ASSERT(statDescs);
	if (!numStats) return true;

	const size_t firstFreeID = m_statRegistry.size();

	if (!ValidateRegistration(statDescs, numStats))
		return false;

	// Copy new events to registry and map
	m_statRegistry.resize(firstFreeID + numStats);

	for (size_t i = 0; i != numStats; ++i)
	{
		// Check for overwrite
		CRY_ASSERT(m_statRegistry[statDescs[i].statID].scriptName.empty());

		m_statRegistry[statDescs[i].statID] = statDescs[i];

		m_statMap.insert(std::make_pair(statDescs[i].scriptName, statDescs[i].statID));
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////

size_t CStatRegistry::Register(const char* scriptName, const char* serializeName)
{
	SGameStatDesc newStat(m_statRegistry.size(), scriptName, serializeName);

	if (!ValidateRegistration(&newStat, 1))
		return INVALID_STAT_ID;

	m_statRegistry.push_back(newStat);
	m_statMap.insert(std::make_pair(scriptName, newStat.statID));

	return m_statRegistry.back().statID;
}

//////////////////////////////////////////////////////////////////////////

size_t CStatRegistry::Count() const
{
	return m_statRegistry.size();
}

//////////////////////////////////////////////////////////////////////////

size_t CStatRegistry::GetID(const char* scriptName) const
{
	CRY_ASSERT(scriptName);

	TStatMap::const_iterator it = m_statMap.find(scriptName);
	return (it == m_statMap.end())
	       ? INVALID_STAT_ID
	       : it->second;
}

//////////////////////////////////////////////////////////////////////////

const SGameStatDesc* CStatRegistry::GetDesc(size_t statID) const
{
	return (statID >= m_statRegistry.size()) ?
	       0 :
	       &m_statRegistry[statID];
}

//////////////////////////////////////////////////////////////////////////

bool CStatRegistry::ValidateRegistration(const SGameStatDesc* statDescs, size_t numStats)
{
	const size_t firstFreeID = m_statRegistry.size();

	std::set<const char*, stl::less_strcmp<const char*>> scriptNames;

	for (size_t i = 0; i != numStats; ++i)
	{
		if (!statDescs[i].IsValid())
			return false;

		if (statDescs[i].statID < firstFreeID || statDescs[i].statID >= (firstFreeID + numStats)) // bad id
			return false;

		if ((m_statMap.find(statDescs[i].scriptName) != m_statMap.end())) // name collision
			return false;

		if (!scriptNames.insert(statDescs[i].scriptName.c_str()).second)
			return false;
	}
	return true;
}

//////////////////////////////////////////////////////////////////////////

void CStatRegistry::GetMemoryStatistics(ICrySizer* pSizer) const
{
	pSizer->AddContainer(m_statRegistry);
	pSizer->AddContainer(m_statMap);
}

//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
// CElemRegistry
//////////////////////////////////////////////////////////////////////////

bool CElemRegistry::Register(const SGameElementDesc elemDescs[], size_t numElems)
{
	CRY_ASSERT(elemDescs);
	if (!numElems) return true;

	const size_t firstFreeID = m_elemRegistry.size();

	if (!ValidateRegistration(elemDescs, numElems))
		return false;

	// Copy new events to registry and map
	m_elemRegistry.resize(firstFreeID + numElems);

	for (size_t i = 0; i != numElems; ++i)
	{
		// Check for overwrite
		CRY_ASSERT(m_elemRegistry[elemDescs[i].statID].scriptName.empty());

		m_elemRegistry[elemDescs[i].statID] = elemDescs[i];

		m_elemMap.insert(std::make_pair(elemDescs[i].scriptName, elemDescs[i].statID));
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////

size_t CElemRegistry::Count() const
{
	return m_elemRegistry.size();
}

//////////////////////////////////////////////////////////////////////////

size_t CElemRegistry::GetID(const char* scriptName) const
{
	CRY_ASSERT(scriptName);

	TElemMap::const_iterator it = m_elemMap.find(scriptName);
	return (it == m_elemMap.end())
	       ? INVALID_STAT_ID
	       : it->second;
}

//////////////////////////////////////////////////////////////////////////

const SGameElementDesc* CElemRegistry::GetDesc(size_t statID) const
{
	return (statID >= m_elemRegistry.size())
	       ? 0
	       : &m_elemRegistry[statID];
}

//////////////////////////////////////////////////////////////////////////

bool CElemRegistry::ValidateRegistration(const SGameElementDesc* elemDescs, size_t numElems)
{
	const size_t firstFreeID = m_elemRegistry.size();

	std::set<const char*, stl::less_strcmp<const char*>> scriptNames;

	for (size_t i = 0; i != numElems; ++i)
	{
		if (!elemDescs[i].IsValid())
			return false;

		if (elemDescs[i].statID < firstFreeID || elemDescs[i].statID >= (firstFreeID + numElems)) // bad id
			return false;

		if ((m_elemMap.find(elemDescs[i].scriptName) != m_elemMap.end())) // name collision
			return false;

		if (!scriptNames.insert(elemDescs[i].scriptName.c_str()).second)
			return false;
	}
	return true;
}

//////////////////////////////////////////////////////////////////////////

void CElemRegistry::GetMemoryStatistics(ICrySizer* pSizer) const
{
	pSizer->AddContainer(m_elemRegistry);
	pSizer->AddContainer(m_elemMap);
}

//////////////////////////////////////////////////////////////////////////
