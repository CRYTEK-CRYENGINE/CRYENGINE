// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// -------------------------------------------------------------------------
//  File name:   StatisticsHelpers.h
//  Version:     v1.00
//  Created:     04/03/2010 by Sergey Mikhtonyuk
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////
#ifndef   _STATISTICSHELPERS_H__
#define   _STATISTICSHELPERS_H__

#pragma once

#define STATS_MODE_CVAR 1

#include <CryScriptSystem/IScriptSystem.h>
#include <CryGame/IGameStatistics.h>

//////////////////////////////////////////////////////////////////////////

class CGameStatistics;
class CScriptBind_GameStatistics;

//////////////////////////////////////////////////////////////////////////

class CStatsContainer : public IStatsContainer
{
	typedef std::vector<std::pair<CTimeValue, SStatAnyValue>> TEventTrack;
	typedef std::vector<TEventTrack>                          TEventTracks;
	typedef std::vector<SStatAnyValue>                        TStateTrack;

public:
	CStatsContainer() : m_refCount(0), m_unloadableMemUsage(0) {}
	virtual void   Init(size_t numEvents, size_t numStates);
	virtual void   AddEvent(size_t eventID, const CTimeValue& time, const SStatAnyValue& val);
	virtual void   AddState(size_t stateID, const SStatAnyValue& val);
	virtual size_t GetEventTrackLength(size_t eventID) const;
	virtual void   GetEventInfo(size_t eventID, size_t idx, CTimeValue& outTime, SStatAnyValue& outParam) const;
	virtual void   GetStateInfo(size_t stateID, SStatAnyValue& outValue) const;
	virtual void   Clear();
	virtual bool   IsEmpty() const;
	virtual void   GetMemoryStatistics(ICrySizer* pSizer);
	virtual void   AddRef();
	virtual void   Release();

private:
	CStatsContainer(const CStatsContainer& other);
	CStatsContainer& operator=(const CStatsContainer& rhs);

private:
	TEventTracks m_events;
	TStateTrack  m_states;
	int          m_refCount;
	size_t       m_unloadableMemUsage;
};

//////////////////////////////////////////////////////////////////////////

class CDefaultStorageFactory : public IStatsStorageFactory
{
public:
	virtual IStatsContainer* CreateContainer();
};

//////////////////////////////////////////////////////////////////////////

class CStatsTracker : public IStatsTracker
{
public:
	CStatsTracker(const SNodeLocator& locator, CGameStatistics* pGameStats, IScriptTable* pTable = 0);
	~CStatsTracker();
	virtual void             StateValue(size_t stateID, const SStatAnyValue& value);
	virtual void             Event(size_t eventID, const SStatAnyValue& value);
	virtual IStatsContainer* GetStatsContainer();
	virtual void             GetMemoryStatistics(ICrySizer* pSizer);

	SNodeLocator             GetLocator() const;
	IScriptTable*            GetScriptTable() const;

	void                     GetMemoryUsage(ICrySizer* pSizer) const {}
private:
	CStatsTracker(const CStatsTracker& other);
	CStatsTracker& operator=(const CStatsTracker& rhs);

private:
	IStatsContainerPtr m_container;
	SNodeLocator       m_locator;
	SmartScriptTable   m_scriptTable;
	CGameStatistics*   m_pGameStats;
};

//////////////////////////////////////////////////////////////////////////

struct SDeadStatNode
{
	typedef std::vector<SDeadStatNode*> TChildren;

	SNodeLocator   locator;
	CStatsTracker* tracker;
	TChildren      children;
	mutable size_t cachedMemoryStats;

	SDeadStatNode(const SNodeLocator& loc, CStatsTracker* track);
	~SDeadStatNode();
	size_t GetMemoryStatistics() const;
	void   GetMemoryStatistics(ICrySizer* pSizer) const;

private:
	SDeadStatNode(const SDeadStatNode& other);
	SDeadStatNode& operator=(const SDeadStatNode& rhs);
};

//////////////////////////////////////////////////////////////////////////

struct SScopeData
{
	typedef std::map<SNodeLocator, CStatsTracker*> TElements;
	typedef std::vector<SDeadStatNode*>            TDeadNodes;

	SNodeLocator   locator;
	CStatsTracker* tracker;
	TElements      elements;
	TDeadNodes     deadNodes;

	SScopeData(SNodeLocator _locator, CStatsTracker* _tracker);
	void GetMemoryStatistics(ICrySizer* pSizer) const;
};

//////////////////////////////////////////////////////////////////////////

class CStatRegistry
{
	typedef std::vector<SGameStatDesc> TStatRegistry;
	typedef std::map<string, size_t>   TStatMap;

public:
	CStatRegistry() {}
	bool                 Register(const SGameStatDesc statDescs[], size_t numStats);
	size_t               Register(const char* scriptName, const char* serializeName);
	size_t               Count() const;
	size_t               GetID(const char* scriptName) const;
	const SGameStatDesc* GetDesc(size_t statID) const;
	void                 GetMemoryStatistics(ICrySizer* pSizer) const;

private:
	CStatRegistry(const CStatRegistry& other);
	CStatRegistry& operator=(const CStatRegistry& rhs);
	bool           ValidateRegistration(const SGameStatDesc* statDescs, size_t numStats);

	TStatRegistry m_statRegistry;
	TStatMap      m_statMap;
};

//////////////////////////////////////////////////////////////////////////

class CElemRegistry
{
	typedef std::vector<SGameElementDesc> TElemRegistry;
	typedef std::map<string, size_t>      TElemMap;

public:
	CElemRegistry() {}
	bool                    Register(const SGameElementDesc elemDescs[], size_t numElems);
	size_t                  Count() const;
	size_t                  GetID(const char* scriptName) const;
	const SGameElementDesc* GetDesc(size_t statID) const;
	void                    GetMemoryStatistics(ICrySizer* pSizer) const;

private:
	CElemRegistry(const CElemRegistry& other);
	CElemRegistry& operator=(const CElemRegistry& rhs);
	bool           ValidateRegistration(const SGameElementDesc* elemDescs, size_t numElems);

	TElemRegistry m_elemRegistry;
	TElemMap      m_elemMap;
};

//////////////////////////////////////////////////////////////////////////

class CGameScopes
{
public:
	typedef std::vector<SGameScopeDesc> TScopeRegistry;
	typedef std::map<string, size_t>    TScopeMap;
	typedef std::vector<SScopeData>     TScopeStack;

	CGameScopes();

	bool                  RegisterGameScopes(const SGameScopeDesc scopeDescs[], size_t numScopes);
	const SGameScopeDesc* GetScopeDesc(size_t id) const;
	const SGameScopeDesc* GetScopeDesc(const char* scriptName) const;

	CStatsTracker*        PushGameScope(size_t scopeID, uint32 timestamp, CGameStatistics* gameStatistics);
	SDeadStatNode*        PopGameScope();

	size_t                FindScopePos(size_t scopeID) const;
	size_t                GetRegisteredCount() const;
	size_t                GetStackSize() const;

	const SScopeData& GetScopeAt(size_t pos) const;
	SScopeData&       GetScopeAt(size_t pos);
	const SScopeData& GetLastScope() const;
	SScopeData&       GetLastScope();

	void              GetMemoryStatistics(ICrySizer* pSizer) const;

private:
	CGameScopes(const CGameScopes& other);
	CGameScopes& operator=(const CGameScopes& rhs);
	bool         ValidateScopes(const SGameScopeDesc scopeDescs[], size_t numScopes);
	void         GrowRegistry(size_t numScopes);
	void         InsertScopesInRegistry(const SGameScopeDesc* scopeDescs, size_t numScopes);

private:
	TScopeRegistry m_scopeRegistry;
	TScopeMap      m_scopeMap;
	TScopeStack    m_scopeStack;
};

//////////////////////////////////////////////////////////////////////////

#endif // __STATISTICSHELPERS_H__
