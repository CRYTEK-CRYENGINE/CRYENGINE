// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "GameStatistics.h"
#include <CryGame/GameUtils.h>
#include "ScriptBind_GameStatistics.h"
#include "IGameRulesSystem.h"
#include "CryAction.h"
#include "CryActionCVars.h"
#include "StatsSizer.h"

static const int MAX_SAVE_ATTEMPTS = 100;

//////////////////////////////////////////////////////////////////////////
// CGameStatistics
//////////////////////////////////////////////////////////////////////////

CGameStatistics::CGameStatistics()
	: m_gsCallback(0)
	, m_storageFactory(&m_defaultStorageFactory)
	, m_memoryLimit(MEMORY_LIMIT_MAXIMUM)
	, m_currTimeStamp(0)
	, m_cachedDeadMemory(0)
	, m_cachedLiveUnloadableMemory(0)
{
	// Register common game events
	SGameStatDesc commonEvents[eSE_Num] = {
		GAME_STAT_DESC(eSE_Kill,         "kill"),
		GAME_STAT_DESC(eSE_Score,        "score"),
		GAME_STAT_DESC(eSE_Shot,         "shot"),
		GAME_STAT_DESC(eSE_Throw,        "throw"),
		GAME_STAT_DESC(eSE_Hit,          "hit"),
		GAME_STAT_DESC(eSE_Activate,     "activate"),
		GAME_STAT_DESC(eSE_Explode,      "explode"),
		GAME_STAT_DESC(eSE_Death,        "death"),
		GAME_STAT_DESC(eSE_Reload,       "reload"),
		GAME_STAT_DESC(eSE_Position,     "position"),
		GAME_STAT_DESC(eSE_Health,       "health"),
		GAME_STAT_DESC(eSE_Stamina,      "stamina"),
		GAME_STAT_DESC(eSE_LookDir,      "lookdir"),
		GAME_STAT_DESC(eSE_LookRotation, "lookrotation"),
		GAME_STAT_DESC(eSE_Weapon,       "weapon"),
		GAME_STAT_DESC(eSE_Consume,      "consume"),
		GAME_STAT_DESC(eSE_Connect,      "connect"),
		GAME_STAT_DESC(eSE_Disconnect,   "disconnect"),
		GAME_STAT_DESC(eSE_TeamChange,   "team_change"),
		GAME_STAT_DESC(eSE_Lifetime,     "lifetime"),
		GAME_STAT_DESC(eSE_Resurrect,    "resurrect"),
		GAME_STAT_DESC(eSE_Damage,       "damage"),
		GAME_STAT_DESC(eSE_Action,       "action"),
		GAME_STAT_DESC(eSE_Enable,       "enable"),
	};
	RegisterGameEvents(commonEvents, eSE_Num);

	SGameStatDesc commonStates[eSS_Num] = {
		GAME_STAT_DESC(eSS_GameSettings,  "game_settings"),
		GAME_STAT_DESC(eSS_Map,           "map"),
		GAME_STAT_DESC(eSS_Gamemode,      "gamemode"),
		GAME_STAT_DESC(eSS_Team,          "team"),
		GAME_STAT_DESC(eSS_Winner,        "winner"),
		GAME_STAT_DESC(eSS_Weapons,       "weapons"),
		GAME_STAT_DESC(eSS_Ammos,         "ammos"),
		GAME_STAT_DESC(eSS_PlayerName,    "name"),
		GAME_STAT_DESC(eSS_ProfileId,     "profile_id"),
		GAME_STAT_DESC(eSS_PlayerInfo,    "player_info"),
		GAME_STAT_DESC(eSS_EntityId,      "entity_id"),
		GAME_STAT_DESC(eSS_Kind,          "kind"),
		GAME_STAT_DESC(eSS_TriggerParams, "trigger_params"),
		GAME_STAT_DESC(eSS_Score,         "score"),
	};
	RegisterGameStates(commonStates, eSS_Num);
}

CGameStatistics::~CGameStatistics()
{
	CRY_ASSERT_MESSAGE(!m_gameScopes.GetStackSize(), "Stack should be empty now");
	if (m_gameScopes.GetStackSize())
	{
		CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_ERROR, "CGameStatistics::~CGameStatistics() Stack is not empty");
	}

	// Remove all data left on the stack and do clean up
	while (m_gameScopes.GetStackSize())
		PopGameScope(INVALID_STAT_ID);
}

//////////////////////////////////////////////////////////////////////////
// Events
//////////////////////////////////////////////////////////////////////////

bool CGameStatistics::RegisterGameEvents(const SGameStatDesc* eventDescs, size_t numEvents)
{
	return m_eventRegistry.Register(eventDescs, numEvents);
}

//////////////////////////////////////////////////////////////////////////

size_t CGameStatistics::RegisterGameEvent(const char* scriptName, const char* serializeName)
{
	return m_eventRegistry.Register(scriptName, serializeName);
}

//////////////////////////////////////////////////////////////////////////

size_t CGameStatistics::GetEventCount() const
{
	return m_eventRegistry.Count();
}

//////////////////////////////////////////////////////////////////////////

size_t CGameStatistics::GetEventID(const char* scriptName) const
{
	return m_eventRegistry.GetID(scriptName);
}

//////////////////////////////////////////////////////////////////////////

size_t CGameStatistics::GetEventIDBySerializeName(const char* serializeName) const
{
	for (size_t index = 0; index < m_eventRegistry.Count(); ++index)
	{
		const SGameStatDesc* pDesc = m_eventRegistry.GetDesc(index);
		if (pDesc && !strcmp(serializeName, pDesc->serializeName))
			return index;
	}

	return INVALID_STAT_ID;
}

//////////////////////////////////////////////////////////////////////////

const SGameStatDesc* CGameStatistics::GetEventDesc(size_t eventID) const
{
	return m_eventRegistry.GetDesc(eventID);
}

//////////////////////////////////////////////////////////////////////////
// States
//////////////////////////////////////////////////////////////////////////

bool CGameStatistics::RegisterGameStates(const SGameStatDesc* stateDescs, size_t numStates)
{
	return m_stateRegistry.Register(stateDescs, numStates);
}

//////////////////////////////////////////////////////////////////////////

size_t CGameStatistics::RegisterGameState(const char* scriptName, const char* serializeName)
{
	return m_stateRegistry.Register(scriptName, serializeName);
}

//////////////////////////////////////////////////////////////////////////

size_t CGameStatistics::GetStateCount() const
{
	return m_stateRegistry.Count();
}

//////////////////////////////////////////////////////////////////////////

size_t CGameStatistics::GetStateID(const char* scriptName) const
{
	return m_stateRegistry.GetID(scriptName);
}

//////////////////////////////////////////////////////////////////////////

const SGameStatDesc* CGameStatistics::GetStateDesc(size_t stateID) const
{
	return m_stateRegistry.GetDesc(stateID);
}

//////////////////////////////////////////////////////////////////////////
// Scopes
//////////////////////////////////////////////////////////////////////////

bool CGameStatistics::RegisterGameScopes(const SGameScopeDesc* scopeDescs, size_t numScopes)
{
	m_gameScopes.RegisterGameScopes(scopeDescs, numScopes);

	return true;
}

//////////////////////////////////////////////////////////////////////////

size_t CGameStatistics::GetScopeCount() const
{
	return m_gameScopes.GetRegisteredCount();
}

//////////////////////////////////////////////////////////////////////////

IStatsTracker* CGameStatistics::PushGameScope(size_t scopeID)
{
	CRY_PROFILE_FUNCTION(PROFILE_ACTION);

	CStatsTracker* tracker = m_gameScopes.PushGameScope(scopeID, ++m_currTimeStamp, this);
	if (tracker == 0)
		return 0;

	if (m_gameScopes.GetStackSize() == 1)
	{
		CRY_ASSERT(!m_scriptBind.get());
		m_scriptBind.reset(new CScriptBind_GameStatistics(this));
	}

	if (m_gsCallback)
		m_gsCallback->OnNodeAdded(tracker->GetLocator());

	m_scriptBind->BindTracker(GetGameRulesTable(), GetScopeDesc(scopeID)->trackerName, tracker);

	return tracker;
}

//////////////////////////////////////////////////////////////////////////

void CGameStatistics::PopGameScope(size_t checkScopeID)
{
	CRY_PROFILE_FUNCTION(PROFILE_ACTION);

	if (!ValidatePopScope(checkScopeID))
		return;

	SNodeLocator locator = m_gameScopes.GetLastScope().locator;
	CStatsTracker* tracker = m_gameScopes.GetLastScope().tracker;

	RemoveAllElements(m_gameScopes.GetStackSize() - 1);

	m_scriptBind->UnbindTracker(GetGameRulesTable(), GetScopeDesc(locator.scopeID)->trackerName, tracker);

	SDeadStatNode* deadScope = m_gameScopes.PopGameScope();
	m_cachedDeadMemory += deadScope->GetMemoryStatistics();

	if (m_gsCallback)
		m_gsCallback->OnNodeRemoved(locator, tracker);

	// Last scope removed from the stack?
	if (!m_gameScopes.GetStackSize())
	{
		SaveDeadNodesRec(deadScope);
		m_scriptBind.reset();
	}
	else
	{
		CheckMemoryOverflow();
	}
}

//////////////////////////////////////////////////////////////////////////

bool CGameStatistics::ValidatePopScope(size_t checkScopeID)
{
	CRY_ASSERT(m_gameScopes.GetStackSize());
	if (!m_gameScopes.GetStackSize())
	{
		CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_ERROR, "CGameStatistics::PopGameScope() called when no scopes on the stack, ignoring.");
		return false;
	}

	SNodeLocator locator = m_gameScopes.GetLastScope().locator;
	CStatsTracker* tracker = m_gameScopes.GetLastScope().tracker;

	CRY_ASSERT(checkScopeID == INVALID_STAT_ID || checkScopeID == locator.scopeID);
	if (checkScopeID != INVALID_STAT_ID && checkScopeID != locator.scopeID)
	{
		CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_ERROR,
		           "CGameStatistics::PopGameScope() scope validation failed, popping - %s, top scope - %s",
		           GetScopeDesc(checkScopeID)->scriptName.c_str(), GetScopeDesc(locator.scopeID)->scriptName.c_str());
		PopScopesUntil(checkScopeID);
		return false;
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////

void CGameStatistics::PopScopesUntil(size_t scopeID)
{
	size_t pos = m_gameScopes.FindScopePos(scopeID);
	if (pos == m_gameScopes.GetStackSize())
		return;

	while (pos < m_gameScopes.GetStackSize())
	{
		const SGameScopeDesc* desc = GetScopeDesc(m_gameScopes.GetLastScope().locator.scopeID);
		if (desc && desc->scriptName)
			CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_ERROR, "Normalizing scope state, popping %s", desc->scriptName.c_str());
		PopGameScope(INVALID_STAT_ID);
	}
}

//////////////////////////////////////////////////////////////////////////

size_t CGameStatistics::GetScopeStackSize() const
{
	return m_gameScopes.GetStackSize();
}

//////////////////////////////////////////////////////////////////////////

size_t CGameStatistics::GetScopeID(size_t depth) const
{
	CRY_ASSERT(depth < m_gameScopes.GetStackSize());
	if (depth < m_gameScopes.GetStackSize())
		return m_gameScopes.GetScopeAt(m_gameScopes.GetStackSize() - depth - 1).locator.scopeID;

	return INVALID_STAT_ID;
}

//////////////////////////////////////////////////////////////////////////

size_t CGameStatistics::GetScopeID(const char* scriptName) const
{
	CRY_ASSERT(scriptName);
	const SGameScopeDesc* desc = m_gameScopes.GetScopeDesc(scriptName);
	return desc != 0 ? desc->statID : INVALID_STAT_ID;
}

//////////////////////////////////////////////////////////////////////////

const SGameScopeDesc* CGameStatistics::GetScopeDesc(size_t scopeID) const
{
	return m_gameScopes.GetScopeDesc(scopeID);
}

//////////////////////////////////////////////////////////////////////////

XmlNodeRef CGameStatistics::CreateStatXMLNode(const char* tag)
{
	return GetISystem()->GetXmlUtils()->CreateStatsXmlNode(tag);
}

//////////////////////////////////////////////////////////////////////////
// Elements
//////////////////////////////////////////////////////////////////////////

bool CGameStatistics::RegisterGameElements(const SGameElementDesc* elemDescs, size_t numElems)
{
	return m_elemRegistry.Register(elemDescs, numElems);
}

//////////////////////////////////////////////////////////////////////////

IStatsTracker* CGameStatistics::AddGameElement(const SNodeLocator& locator, IScriptTable* pTable)
{
	CRY_PROFILE_FUNCTION(PROFILE_ACTION);

	if (!ValidateAddElement(locator))
		return 0;

	const SGameElementDesc* desc = m_elemRegistry.GetDesc(locator.elemID);
	size_t stackPos = m_gameScopes.FindScopePos(locator.scopeID);

	CStatsTracker* tracker = NULL;

	SScopeData& scope = m_gameScopes.GetScopeAt(stackPos);
	if (scope.elements.find(locator) == scope.elements.end())
	{
		SNodeLocator newLocator(locator);
		newLocator.timeStamp = ++m_currTimeStamp;

		tracker = new CStatsTracker(newLocator, this, pTable);

		scope.elements.insert(std::make_pair(newLocator, tracker));

		if (pTable)
			m_scriptBind->BindTracker(pTable, desc->trackerName, tracker);

		if (m_gsCallback)
			m_gsCallback->OnNodeAdded(newLocator);
	}

	return tracker;
}

//////////////////////////////////////////////////////////////////////////

bool CGameStatistics::ValidateAddElement(const SNodeLocator& locator)
{
	const SGameElementDesc* desc = m_elemRegistry.GetDesc(locator.elemID);
	CRY_ASSERT(desc && (desc->locatorID == locator.locatorType));
	CRY_ASSERT(!locator.isScope());

	if (!desc || locator.isScope() || desc->locatorID != locator.locatorType)
	{
		CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_ERROR, "CGameStatistics::AddGameElement() Invalid locator");
		return false;
	}

	size_t stackPos = m_gameScopes.FindScopePos(locator.scopeID);

	CRY_ASSERT_MESSAGE(stackPos != m_gameScopes.GetStackSize(), "Elements can be added only to the scopes on the stack");
	if (stackPos == m_gameScopes.GetStackSize())
	{
		CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_ERROR, "CGameStatistics::AddGameElement() attempt to add element to non existing scope");
		return false;
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////

void CGameStatistics::RemoveElement(const SNodeLocator& locator)
{
	CRY_PROFILE_FUNCTION(PROFILE_ACTION);

	size_t stackPos = m_gameScopes.FindScopePos(locator.scopeID);

	CRY_ASSERT_MESSAGE(stackPos != m_gameScopes.GetStackSize(), "No such scope on the stack");
	if (stackPos == m_gameScopes.GetStackSize())
	{
		CryWarning(VALIDATOR_MODULE_GAME, VALIDATOR_ERROR, "CGameStatistics::RemoveElement() attempt to remove element from non existing scope");
		return;
	}

	SScopeData::TElements& elems = m_gameScopes.GetScopeAt(stackPos).elements;

	SScopeData::TElements::iterator it = elems.find(locator);
	if (it != elems.end())
	{
		if (!DoRemoveElement(stackPos, it->first, it->second))
			delete it->second;

		elems.erase(it);
		CheckMemoryOverflow();
	}
}

//////////////////////////////////////////////////////////////////////////

bool CGameStatistics::DoRemoveElement(size_t scopePos, const SNodeLocator& locator, CStatsTracker* tracker)
{
	const SGameElementDesc* desc = m_elemRegistry.GetDesc(locator.elemID);
	CRY_ASSERT(desc);
	if (!desc)
		return false;

	SDeadStatNode* deadNode = new SDeadStatNode(locator, tracker);
	m_gameScopes.GetScopeAt(scopePos).deadNodes.push_back(deadNode);

	m_cachedDeadMemory += deadNode->GetMemoryStatistics();

	if (tracker->GetScriptTable())
		m_scriptBind->UnbindTracker(tracker->GetScriptTable(), desc->trackerName, tracker);

	if (m_gsCallback)
		m_gsCallback->OnNodeRemoved(locator, tracker);

	return true;
}

//////////////////////////////////////////////////////////////////////////

void CGameStatistics::RemoveAllElements(size_t stackPos)
{
	SScopeData::TElements& elems = m_gameScopes.GetScopeAt(stackPos).elements;

	for (SScopeData::TElements::iterator it = elems.begin(); it != elems.end(); ++it)
	{
		if (!DoRemoveElement(stackPos, it->first, it->second))
			delete it->second;
	}

	elems.clear();
}

//////////////////////////////////////////////////////////////////////////

size_t CGameStatistics::GetElementCount() const
{
	return m_elemRegistry.Count();
}

//////////////////////////////////////////////////////////////////////////

size_t CGameStatistics::GetElementID(const char* scriptName) const
{
	return m_elemRegistry.GetID(scriptName);
}

//////////////////////////////////////////////////////////////////////////

const SGameElementDesc* CGameStatistics::GetElementDesc(size_t elemID) const
{
	return m_elemRegistry.GetDesc(elemID);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

IStatsTracker* CGameStatistics::GetTracker(const SNodeLocator& locator) const
{
	CRY_ASSERT(locator.scopeID < m_gameScopes.GetRegisteredCount());

	if (locator.isScope())
	{
		CRY_ASSERT(locator.elemID == INVALID_STAT_ID);

		size_t pos = m_gameScopes.FindScopePos(locator.scopeID);

		if (pos != m_gameScopes.GetStackSize())
			return m_gameScopes.GetScopeAt(pos).tracker;
	}
	else
	{
		CRY_ASSERT(m_elemRegistry.GetDesc(locator.elemID));
		CRY_ASSERT(m_elemRegistry.GetDesc(locator.elemID)->locatorID == locator.locatorType);

		size_t pos = m_gameScopes.FindScopePos(locator.scopeID);

		if (pos != m_gameScopes.GetStackSize())
		{
			const SScopeData::TElements& elems = m_gameScopes.GetScopeAt(pos).elements;
			SScopeData::TElements::const_iterator it = elems.find(locator);
			if (it != elems.end())
				return it->second;
		}
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////

SNodeLocator CGameStatistics::GetTrackedNode(IStatsTracker* tracker) const
{
	CRY_ASSERT(tracker);
	if (!tracker)
		return SNodeLocator();

	CStatsTracker* track = static_cast<CStatsTracker*>(tracker);
	return track->GetLocator();
}

//////////////////////////////////////////////////////////////////////////
// Misc
//////////////////////////////////////////////////////////////////////////

// Wrapper for XML data for IXMLSerializable
class CXMLStatsWrapper : public CXMLSerializableBase
{
public:
	CXMLStatsWrapper(const XmlNodeRef& node) : m_node(node)
	{}

	virtual XmlNodeRef GetXML(IGameStatistics* pGS)
	{ return m_node; }

	virtual void GetMemoryStatistics(ICrySizer* pSizer) const
	{ pSizer->Add(*this); }

private:
	XmlNodeRef m_node;
};

//////////////////////////////////////////////////////////////////////////

IXMLSerializable* CGameStatistics::WrapXMLNode(const XmlNodeRef& node)
{
	return new CXMLStatsWrapper(node);
}

//////////////////////////////////////////////////////////////////////////

void CGameStatistics::PreprocessScriptedEventParameter(size_t eventID, SStatAnyValue& value)
{
	CRY_PROFILE_FUNCTION(PROFILE_ACTION);

	if (m_gsCallback)
		m_gsCallback->PreprocessScriptedEventParameter(eventID, value);
}

void CGameStatistics::PreprocessScriptedStateParameter(size_t stateID, SStatAnyValue& value)
{
	CRY_PROFILE_FUNCTION(PROFILE_ACTION);

	if (m_gsCallback)
		m_gsCallback->PreprocessScriptedStateParameter(stateID, value);
}

//////////////////////////////////////////////////////////////////////////

void CGameStatistics::SetStatisticsCallback(IGameStatisticsCallback* pCallback)
{
	// Allow reset only
	CRY_ASSERT(!m_gsCallback || !pCallback);

	m_gsCallback = pCallback;
}

//////////////////////////////////////////////////////////////////////////

IGameStatisticsCallback* CGameStatistics::GetStatisticsCallback() const
{
	return m_gsCallback;
}

//////////////////////////////////////////////////////////////////////////

void CGameStatistics::SetStorageFactory(IStatsStorageFactory* pFactory)
{
	m_storageFactory = pFactory ? pFactory : &m_defaultStorageFactory;
}

//////////////////////////////////////////////////////////////////////////

IStatsStorageFactory* CGameStatistics::GetStorageFactory() const
{
	return m_storageFactory;
}

//////////////////////////////////////////////////////////////////////////

void CGameStatistics::RegisterSerializer(IStatsSerializer* serializer)
{
	CRY_ASSERT(serializer);
	if (!serializer)
		return;

	m_serializers.push_back(serializer);
}

//////////////////////////////////////////////////////////////////////////

void CGameStatistics::UnregisterSerializer(IStatsSerializer* serializer)
{
	stl::find_and_erase(m_serializers, serializer);
	if (m_serializers.empty())
		stl::free_container(m_serializers);
}

//////////////////////////////////////////////////////////////////////////

void CGameStatistics::SetMemoryLimit(size_t kb)
{
	m_memoryLimit = kb << 10;
	CheckMemoryOverflow();
}

//////////////////////////////////////////////////////////////////////////

void CGameStatistics::GetMemoryStatistics(ICrySizer* pSizer) const
{
	SIZER_COMPONENT_NAME(pSizer, "Statistics");
	pSizer->Add(*this);

	m_eventRegistry.GetMemoryStatistics(pSizer);
	m_stateRegistry.GetMemoryStatistics(pSizer);
	m_elemRegistry.GetMemoryStatistics(pSizer);
	m_gameScopes.GetMemoryStatistics(pSizer);
}

//////////////////////////////////////////////////////////////////////////

void CGameStatistics::CheckMemoryOverflow()
{
	CRY_PROFILE_FUNCTION(PROFILE_ACTION);

	size_t totalMemory = m_cachedDeadMemory + m_cachedLiveUnloadableMemory;

	if (totalMemory >= m_memoryLimit)
	{
		m_cachedDeadMemory = 0;
		m_cachedLiveUnloadableMemory = 0;

		SaveScopesRec();
	}
}

//////////////////////////////////////////////////////////////////////////

const char* CGameStatistics::GetSerializeName(const SNodeLocator& locator) const
{
	return locator.isScope()
	       ? GetScopeDesc(locator.scopeID)->serializeName
	       : GetElementDesc(locator.elemID)->serializeName;
}

//////////////////////////////////////////////////////////////////////////

void CGameStatistics::SaveDeadNodesRec(SDeadStatNode* node)
{
	CRY_PROFILE_FUNCTION(PROFILE_ACTION);

	const char* serializeName = GetSerializeName(node->locator);

	NotifyVisitNode(node->locator, serializeName, *node->tracker->GetStatsContainer(), eSNS_Dead);

	for (size_t i = 0; i != node->children.size(); ++i)
		SaveDeadNodesRec(node->children[i]);

	node->children.clear();
	NotifyLeaveNode(node->locator, serializeName, *node->tracker->GetStatsContainer(), eSNS_Dead);

	delete node;
}

//////////////////////////////////////////////////////////////////////////

void CGameStatistics::SaveScopesRec(size_t scopePos)
{
	CRY_PROFILE_FUNCTION(PROFILE_ACTION);

	SScopeData& sd = m_gameScopes.GetScopeAt(scopePos);
	IStatsContainer& scopeCont = *sd.tracker->GetStatsContainer();

	const char* sname = GetSerializeName(sd.locator);
	NotifyVisitNode(sd.locator, sname, scopeCont, eSNS_Alive);

	CStatsSizer sizer;

	// Iterate over live elements and give a chance to save container data
	for (SScopeData::TElements::const_iterator it = sd.elements.begin();
	     it != sd.elements.end();
	     ++it)
	{
		IStatsContainer& cont = *it->second->GetStatsContainer();

		if (cont.IsEmpty())
			continue;

		const char* elName = GetSerializeName(it->first);
		NotifyVisitNode(it->first, elName, cont, eSNS_Alive);
		NotifyLeaveNode(it->first, elName, cont, eSNS_Alive);

		cont.GetMemoryStatistics(&sizer);
	}

	if ((scopePos + 1) < m_gameScopes.GetStackSize())
		SaveScopesRec(scopePos + 1);

	// Traverse dead elements
	for (size_t i = 0; i != sd.deadNodes.size(); ++i)
		SaveDeadNodesRec(sd.deadNodes[i]);
	sd.deadNodes.clear();

	NotifyLeaveNode(sd.locator, sname, scopeCont, eSNS_Alive);

	// Update memory
	scopeCont.GetMemoryStatistics(&sizer);
	m_cachedLiveUnloadableMemory += sizer.GetTotalSize();
}

//////////////////////////////////////////////////////////////////////////

void CGameStatistics::NotifyVisitNode(const SNodeLocator& locator, const char* serializeName, IStatsContainer& container, EStatNodeState state)
{
	CRY_PROFILE_FUNCTION(PROFILE_ACTION);

	for (size_t i = 0; i != m_serializers.size(); ++i)
		m_serializers[i]->VisitNode(locator, serializeName, container, state);
}

void CGameStatistics::NotifyLeaveNode(const SNodeLocator& locator, const char* serializeName, IStatsContainer& container, EStatNodeState state)
{
	CRY_PROFILE_FUNCTION(PROFILE_ACTION);

	for (size_t i = 0; i != m_serializers.size(); ++i)
		m_serializers[i]->LeaveNode(locator, serializeName, container, state);
}

//////////////////////////////////////////////////////////////////////////

void CGameStatistics::GrowLiveMemUsage(const SStatAnyValue& value)
{
	CStatsSizer sizer;
	value.GetMemoryUsage(&sizer);
	m_cachedLiveUnloadableMemory += sizer.GetTotalSize();
}

//////////////////////////////////////////////////////////////////////////

void CGameStatistics::OnTrackedEvent(const SNodeLocator& locator, size_t eventID, const CTimeValue& timeVal, const SStatAnyValue& value)
{
	GrowLiveMemUsage(value);

	if (!m_gsCallback)
		return;

	if (value.type == eSAT_TXML && value.pSerializable)
		value.pSerializable->DispatchEventToCallback(locator, eventID, timeVal, m_gsCallback);
	else
		m_gsCallback->OnEvent(locator, eventID, timeVal, value);

	CheckMemoryOverflow();
}

//////////////////////////////////////////////////////////////////////////

void CGameStatistics::OnTrackedState(const SNodeLocator& locator, size_t stateID, const SStatAnyValue& value)
{
	GrowLiveMemUsage(value);

	if (!m_gsCallback)
		return;

	if (value.type == eSAT_TXML && value.pSerializable)
		value.pSerializable->DispatchStateToCallback(locator, stateID, m_gsCallback);
	else
		m_gsCallback->OnState(locator, stateID, value);

	CheckMemoryOverflow();
}

//////////////////////////////////////////////////////////////////////////

IScriptTable* CGameStatistics::GetGameRulesTable()
{
	IGameRulesSystem* pGameRulesSystem = CCryAction::GetCryAction()->GetIGameRulesSystem();
	IGameRules* pGameRules = pGameRulesSystem ? pGameRulesSystem->GetCurrentGameRules() : NULL;
	IEntity* pRulesEntity = pGameRules ? pGameRules->GetEntity() : NULL;
	IScriptTable* gameRulesTable = pRulesEntity ? pRulesEntity->GetScriptTable() : NULL;

	//-- LEAVING THIS ASSERT HERE BECAUSE WE'RE USING THIS FUNCTION WRONG AT THE END OF A SESSION FOR TELEMTERY AND WE NEED TO FIX IT LATER
	CRY_ASSERT(gameRulesTable);
	return gameRulesTable;
}

//////////////////////////////////////////////////////////////////////////
