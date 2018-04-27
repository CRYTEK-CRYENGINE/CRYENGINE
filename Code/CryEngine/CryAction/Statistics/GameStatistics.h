// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef   __GAMESTATISTICS_H__
#define   __GAMESTATISTICS_H__

#if _MSC_VER > 1000
	#pragma once
#endif

#include "StatisticsHelpers.h"

//////////////////////////////////////////////////////////////////////////

class CGameStatistics : public IGameStatistics
{
	typedef std::vector<IStatsSerializer*> TSerializers;

public:
	CGameStatistics();
	~CGameStatistics();

	virtual bool                     RegisterGameEvents(const SGameStatDesc* eventDescs, size_t numEvents);
	virtual size_t                   RegisterGameEvent(const char* scriptName, const char* serializeName);
	virtual size_t                   GetEventCount() const;
	virtual size_t                   GetEventID(const char* scriptName) const;
	virtual size_t                   GetEventIDBySerializeName(const char* serializeName) const;
	virtual const SGameStatDesc*     GetEventDesc(size_t eventID) const;

	virtual bool                     RegisterGameStates(const SGameStatDesc* stateDescs, size_t numStates);
	virtual size_t                   RegisterGameState(const char* scriptName, const char* serializeName);
	virtual size_t                   GetStateCount() const;
	virtual size_t                   GetStateID(const char* scriptName) const;
	virtual const SGameStatDesc*     GetStateDesc(size_t stateID) const;

	virtual bool                     RegisterGameScopes(const SGameScopeDesc* scopeDescs, size_t numScopes);
	virtual IStatsTracker*           PushGameScope(size_t scopeID);
	virtual void                     PopGameScope(size_t scopeID);
	virtual size_t                   GetScopeStackSize() const;
	virtual size_t                   GetScopeID(size_t depth = 0) const;
	virtual size_t                   GetScopeCount() const;
	virtual size_t                   GetScopeID(const char* scriptName) const;
	virtual const SGameScopeDesc*    GetScopeDesc(size_t scopeID) const;

	virtual bool                     RegisterGameElements(const SGameElementDesc* elemDescs, size_t numElems);
	virtual IStatsTracker*           AddGameElement(const SNodeLocator& locator, IScriptTable* pTable);
	virtual void                     RemoveElement(const SNodeLocator& locator);
	virtual size_t                   GetElementCount() const;
	virtual size_t                   GetElementID(const char* scriptName) const;
	virtual const SGameElementDesc*  GetElementDesc(size_t elemID) const;

	virtual IStatsTracker*           GetTracker(const SNodeLocator& locator) const;
	virtual SNodeLocator             GetTrackedNode(IStatsTracker* tracker) const;

	virtual XmlNodeRef               CreateStatXMLNode(const char* tag = "root");
	virtual IXMLSerializable*        WrapXMLNode(const XmlNodeRef& node);

	virtual void                     PreprocessScriptedEventParameter(size_t eventID, SStatAnyValue& value);
	virtual void                     PreprocessScriptedStateParameter(size_t stateID, SStatAnyValue& value);

	virtual void                     SetStatisticsCallback(IGameStatisticsCallback* pCallback);
	virtual IGameStatisticsCallback* GetStatisticsCallback() const;

	virtual void                     SetStorageFactory(IStatsStorageFactory* pFactory);
	virtual IStatsStorageFactory*    GetStorageFactory() const;

	virtual void                     RegisterSerializer(IStatsSerializer* serializer);
	virtual void                     UnregisterSerializer(IStatsSerializer* serializer);
	virtual void                     SetMemoryLimit(size_t kb);

	virtual void                     GetMemoryStatistics(ICrySizer* pSizer) const;

	void                             OnTrackedEvent(const SNodeLocator& locator, size_t eventID, const CTimeValue& timeVal, const SStatAnyValue& value);
	void                             OnTrackedState(const SNodeLocator& locator, size_t stateID, const SStatAnyValue& value);

private:
	bool                 ValidatePopScope(size_t checkScopeID);
	bool                 ValidateAddElement(const SNodeLocator& locator);
	void                 PopScopesUntil(size_t scopeID);
	bool                 DoRemoveElement(size_t scopePos, const SNodeLocator& locator, CStatsTracker* tracker);
	void                 RemoveAllElements(size_t scopeID);
	void                 CheckMemoryOverflow();
	void                 GrowLiveMemUsage(const SStatAnyValue& value);
	void                 SaveDeadNodesRec(SDeadStatNode* node);
	void                 SaveScopesRec(size_t scopePos = 0);
	const char*          GetSerializeName(const SNodeLocator& locator) const;
	void                 NotifyVisitNode(const SNodeLocator& locator, const char* serializeName, IStatsContainer& container, EStatNodeState state);
	void                 NotifyLeaveNode(const SNodeLocator& locator, const char* serializeName, IStatsContainer& container, EStatNodeState state);
	static IScriptTable* GetGameRulesTable();

	std::unique_ptr<CScriptBind_GameStatistics> m_scriptBind;
	IGameStatisticsCallback*                    m_gsCallback;
	IStatsStorageFactory*                       m_storageFactory;
	CDefaultStorageFactory                      m_defaultStorageFactory;

	CStatRegistry                               m_eventRegistry;
	CStatRegistry                               m_stateRegistry;
	CElemRegistry                               m_elemRegistry;
	CGameScopes  m_gameScopes;

	TSerializers m_serializers;
	size_t       m_memoryLimit;
	size_t       m_cachedDeadMemory;
	size_t       m_cachedLiveUnloadableMemory;
	uint32       m_currTimeStamp;
};

//////////////////////////////////////////////////////////////////////////

#endif //__GAMESTATISTICS_H__
