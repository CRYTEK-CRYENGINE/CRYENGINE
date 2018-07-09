// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "EntityScript.h"
#include "EntitySystem.h"
#include "ScriptBind_Entity.h"

#define SCRIPT_SERVER_STATE                     "Server"
#define SCRIPT_CLIENT_STATE                     "Client"
#define SCRIPT_STATES_DEF_TABLE                 "States"

#define SCRIPT_SPAWN                            "OnSpawn"
#define SCRIPT_INIT                             "OnInit"
#define SCRIPT_DESTROY                          "OnDestroy"
#define SCRIPT_SHUTDOWN                         "OnShutDown"
//! On reset callback, mostly called when Editor wants to Reset the state
#define SCRIPT_ONRESET                          "OnReset"
#define SCRIPT_ONTRANSFORMATION_FROMEDITOR_DONE "OnTransformFromEditorDone"

//////////////////////////////////////////////////////////////////////////
//! This structure must 100% match EScriptStateFunctions enum.
static const char* s_ScriptStateFunctions[] =
{
	"OnBeginState",
	"OnEndState",
	"OnUpdate",
	"OnTimer",
	"OnEvent",
	"OnDamage",

	// Used by all other entities!
	"OnEnterArea",
	"OnLeaveArea",
	"OnEnterNearArea",
	"OnLeaveNearArea",
	"OnProceedFadeArea",
	"OnMoveNearArea",

	// Used only by the local client entities!
	"OnLocalClientEnterArea",
	"OnLocalClientLeaveArea",
	"OnLocalClientEnterNearArea",
	"OnLocalClientLeaveNearArea",
	"OnLocalClientProceedFadeArea",
	"OnLocalClientMoveNearArea",

	// Used only by audio listener entities!
	"OnAudioListenerEnterArea",
	"OnAudioListenerLeaveArea",
	"OnAudioListenerEnterNearArea",
	"OnAudioListenerLeaveNearArea",
	"OnAudioListenerProceedFadeArea",
	"OnAudioListenerMoveNearArea",

	"OnBind",
	"OnBindThis",
	"OnUnBind",
	"OnUnBindThis",
	"OnMove",
	"OnCollision",
	"OnAnimationEvent",
	"OnPhysicsBreak",
	"OnSoundDone",
	"OnLevelLoaded",
	"OnStartLevel",
	"OnStartGame",
	"OnHidden",
	"OnUnHidden",
	"OnPreparedFromPool",
	// reserved.
	"",
};

//////////////////////////////////////////////////////////////////////////
SScriptState::SScriptState()
{
	for (int i = 0; i < NUM_STATES; i++)
		pStateFuns[i] = NULL;
}

//////////////////////////////////////////////////////////////////////////
SScriptState::~SScriptState()
{
	for (int i = 0; i < NUM_STATES; i++)
		delete pStateFuns[i];
}

//////////////////////////////////////////////////////////////////////////
void SScriptState::Free(IScriptSystem* pScriptSystem)
{
	for (int nState = 0; nState < NUM_STATES; nState++)
	{
		if (pStateFuns[nState])
		{
			SScriptStateFunctions* state = pStateFuns[nState];
			for (int k = 0; k < sizeof(state->pFunction) / sizeof(state->pFunction[0]); k++)
			{
				if (state->pFunction[k])
				{
					pScriptSystem->ReleaseFunc(state->pFunction[k]);
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// CEntityScript implementation.
//////////////////////////////////////////////////////////////////////////
CEntityScript::CEntityScript()
{
	m_pScriptSystem = gEnv->pScriptSystem;
	m_pEntityTable = NULL;
	m_pPropertiesTable = NULL;
	ZeroStruct(m_pOnInitFunc);
	ZeroStruct(m_pOnShutDown);
	m_pOnReset = NULL;
	m_pOnTransformFromEditorDone = NULL;
	m_pOnSpawnFunc = NULL;
	m_pOnDestroyFunc = NULL;
	m_bScriptLoaded = false;
	m_bDefaultOnly = false;
}

//////////////////////////////////////////////////////////////////////////
CEntityScript::~CEntityScript()
{
	Clear();
}

void CEntityScript::Clear()
{
	if (!m_bScriptLoaded)
		return;
	unsigned int i;

	for (i = 0; i < NUM_STATES; i++)
	{
		if (m_pOnInitFunc[i])
			m_pScriptSystem->ReleaseFunc(m_pOnInitFunc[i]);
		if (m_pOnShutDown[i])
			m_pScriptSystem->ReleaseFunc(m_pOnShutDown[i]);
	}
	if (m_pOnReset)
	{
		m_pScriptSystem->ReleaseFunc(m_pOnReset);
		m_pOnReset = 0;
	}

	if (m_pOnTransformFromEditorDone)
	{
		m_pScriptSystem->ReleaseFunc(m_pOnTransformFromEditorDone);
		m_pOnTransformFromEditorDone = 0;
	}

	if (m_pOnSpawnFunc)
	{
		m_pScriptSystem->ReleaseFunc(m_pOnSpawnFunc);
		m_pOnSpawnFunc = 0;
	}

	if (m_pOnDestroyFunc)
	{
		m_pScriptSystem->ReleaseFunc(m_pOnDestroyFunc);
		m_pOnDestroyFunc = 0;
	}

	//////////////////////////////////////////////////////////////////////////
	// Release states.
	//////////////////////////////////////////////////////////////////////////
	for (i = 0; i < m_states.size(); i++)
	{
		m_states[i].Free(m_pScriptSystem);
	}
	m_defaultState.Free(m_pScriptSystem);
	m_states.clear();

	//////////////////////////////////////////////////////////////////////////
	// Release events.
	//////////////////////////////////////////////////////////////////////////
	for (i = 0; i < m_events.size(); i++)
	{
		if (m_events[i].func)
			m_pScriptSystem->ReleaseFunc(m_events[i].func);
	}

	m_pPropertiesTable = NULL;
	m_pEntityTable = NULL;
}

//////////////////////////////////////////////////////////////////////////
bool CEntityScript::Init(const char* sTableName, const char* sScriptFilename)
{
	m_sTableName = sTableName;
	m_sScriptFilename = sScriptFilename;
	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CEntityScript::Init(const char* sTableName, IScriptTable* pScriptTable)
{
	m_sTableName = sTableName;
	m_pEntityTable = pScriptTable;

	LoadEvents();

	DelegateProperties();

	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CEntityScript::LoadScript(bool bForceReload)
{
	if (m_pEntityTable && !bForceReload)
		return true;

	MEMSTAT_CONTEXT(EMemStatContextTypes::MSC_Other, 0, "Entity Scripts");

	Clear();

	if (!m_pScriptSystem->ExecuteFile(m_sScriptFilename, true, bForceReload))
		return false;

	if (!m_pScriptSystem->GetGlobalValue(m_sTableName, m_pEntityTable))
	{
		return false;
	}

	DelegateProperties();

	// Precache of OnReset function.
	m_pEntityTable->GetValue(SCRIPT_ONRESET, m_pOnReset);

	// Precache of OnTransformFromEditorDone function.
	m_pEntityTable->GetValue(SCRIPT_ONTRANSFORMATION_FROMEDITOR_DONE, m_pOnTransformFromEditorDone);

	// Enumerate all states.
	EnumStates();

	// Load all events in entity script.
	LoadEvents();

	m_bScriptLoaded = true;
	return true;
}

//////////////////////////////////////////////////////////////////////////
void CEntityScript::DelegateProperties()
{
	//////////////////////////////////////////////////////////////////////////
	// Delegate call from entity script table to the global entity binded methods.
	//////////////////////////////////////////////////////////////////////////
	GetIEntitySystem()->GetScriptBindEntity()->DelegateCalls(m_pEntityTable);

	m_pPropertiesTable = m_pScriptSystem->CreateTable(true);
	m_pPropertiesTable->AddRef();
	if (!m_pEntityTable->GetValue(SCRIPT_PROPERTIES_TABLE, m_pPropertiesTable))
	{
		SAFE_RELEASE(m_pPropertiesTable);
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntityScript::EnumStates()
{
	SmartScriptTable pServerTable;
	SmartScriptTable pClientTable;

	// Get Server table if exist.
	bool bServerTable = m_pEntityTable->GetValue(SCRIPT_SERVER_STATE, pServerTable);

	// Get Client table if exist.
	bool bClientTable = m_pEntityTable->GetValue(SCRIPT_CLIENT_STATE, pClientTable);

	//////////////////////////////////////////////////////////////////////////
	// Initialize Default State.
	//////////////////////////////////////////////////////////////////////////

	// common client/server initialization
	m_pEntityTable->GetValue(SCRIPT_SPAWN, m_pOnSpawnFunc);
	m_pEntityTable->GetValue(SCRIPT_DESTROY, m_pOnDestroyFunc);

	// Analyze script object for Client/Server states.
	if (bClientTable)
	{
		// Client state exist only on client and only if have Client table.
		m_defaultState.pStateFuns[CLIENT_STATE] = new SScriptStateFunctions;
		InitializeStateTable(pClientTable, m_defaultState.pStateFuns[CLIENT_STATE]);

		pClientTable->GetValue(SCRIPT_INIT, m_pOnInitFunc[CLIENT_STATE]);
		pClientTable->GetValue(SCRIPT_SHUTDOWN, m_pOnShutDown[CLIENT_STATE]);
	}

	// If Neither Client neither Server states exist, fallback to single server state.
	// This provided for backward compatability support and for classes that do not care about Client/Server.

	m_defaultState.pStateFuns[SERVER_STATE] = new SScriptStateFunctions;
	if (bServerTable)
	{
		InitializeStateTable(pServerTable, m_defaultState.pStateFuns[SERVER_STATE]);
		pServerTable->GetValue(SCRIPT_INIT, m_pOnInitFunc[SERVER_STATE]);
		pServerTable->GetValue(SCRIPT_SHUTDOWN, m_pOnShutDown[SERVER_STATE]);
	}
	else if (!bServerTable && !bClientTable)
	{
		InitializeStateTable(m_pEntityTable, m_defaultState.pStateFuns[SERVER_STATE]);
		m_pEntityTable->GetValue(SCRIPT_INIT, m_pOnInitFunc[SERVER_STATE]);
		m_pEntityTable->GetValue(SCRIPT_SHUTDOWN, m_pOnShutDown[SERVER_STATE]);
	}

	m_bDefaultOnly = (!bServerTable && !bClientTable);

	//////////////////////////////////////////////////////////////////////////
	// Enumerate Named States.
	//////////////////////////////////////////////////////////////////////////

	SmartScriptTable pStatesDefTable;
	if (m_pEntityTable->GetValue(SCRIPT_STATES_DEF_TABLE, pStatesDefTable))
	{
		const char* sStateName = "";
		SScriptState sstate;

		// Count the states
		size_t nStates = 0;
		IScriptTable::Iterator iter = pStatesDefTable->BeginIteration();
		while (pStatesDefTable->MoveNext(iter))
		{
			if (iter.value.GetType() != EScriptAnyType::String)
				continue;
			++nStates;
		}
		pStatesDefTable->EndIteration(iter);

		m_states.reserve(nStates);

		// Enumerate thru all known states.
		iter = pStatesDefTable->BeginIteration();
		while (pStatesDefTable->MoveNext(iter))
		{
			if (iter.value.GetType() != EScriptAnyType::String)
				continue;
			sStateName = iter.value.GetString();

			// Add a new state to array of states and make a name to id mapping.
			sstate.name = sStateName;
			m_states.push_back(sstate);
		}
		pStatesDefTable->EndIteration(iter);
	}

	//////////////////////////////////////////////////////////////////////////
	// Initialize Named States function tables.
	//////////////////////////////////////////////////////////////////////////

	if (bClientTable)
		InitializeNamedStates(pClientTable, CLIENT_STATE);
	if (bServerTable)
		InitializeNamedStates(pServerTable, SERVER_STATE);

	if (!bServerTable && !bClientTable)
		InitializeNamedStates(m_pEntityTable, SERVER_STATE);
}

//////////////////////////////////////////////////////////////////////////
void CEntityScript::InitializeNamedStates(IScriptTable* pTable, int nStateNum)
{
	// Initialize Server states.
	for (int i = 0; i < (int)m_states.size(); i++)
	{
		SScriptState& state = m_states[i];
		// Find named state table inside Client table.
		SmartScriptTable pStateTable;
		if (pTable->GetValue(state.name.c_str(), pStateTable))
		{
			// If state table found initialize it
			state.pStateFuns[nStateNum] = new SScriptStateFunctions;
			InitializeStateTable(pStateTable, state.pStateFuns[nStateNum]);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntityScript::InitializeStateTable(IScriptTable* pStateTable, SScriptStateFunctions* scriptState)
{
	// Query state table for supported functions.
	for (int stateFunc = 0; stateFunc < sizeof(scriptState->pFunction) / sizeof(scriptState->pFunction[0]); stateFunc++)
	{
		assert(stateFunc < CRY_ARRAY_COUNT(s_ScriptStateFunctions));
		scriptState->pFunction[stateFunc] = 0;
		pStateTable->GetValue(s_ScriptStateFunctions[stateFunc], scriptState->pFunction[stateFunc]);
	}
}

//////////////////////////////////////////////////////////////////////////
SScriptState* CEntityScript::GetState(const char* sStateName)
{
	int nStateId = GetStateId(sStateName);
	if (nStateId >= 0)
	{
		return GetState(nStateId);
	}
	return &m_defaultState;
}

//////////////////////////////////////////////////////////////////////////
SScriptState* CEntityScript::GetState(int nStateId)
{
	if (nStateId > 0 && nStateId <= (int)m_states.size())
	{
		return &m_states[nStateId - 1];
	}
	return &m_defaultState;
}

//////////////////////////////////////////////////////////////////////////
int CEntityScript::GetStateId(const char* sStateName) const
{
	for (int i = 0, num = m_states.size(); i < num; i++)
	{
		if (strcmp(m_states[i].name.c_str(), sStateName) == 0)
		{
			return i + 1;
		}
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////
const char* CEntityScript::GetStateName(int nStateId) const
{
	if (nStateId > 0 && nStateId <= (int)m_states.size())
	{
		return m_states[nStateId - 1].name.c_str();
	}
	return "";
}

//////////////////////////////////////////////////////////////////////////
void CEntityScript::Call_OnInit(IScriptTable* pThis, bool isReload)
{
	LOADING_TIME_PROFILE_SECTION(gEnv->pSystem);

	if (m_pOnSpawnFunc)
		Script::Call(m_pScriptSystem, m_pOnSpawnFunc, pThis, isReload);

	for (int i = 0; i < NUM_STATES; i++)
	{
		if (ShouldExecuteCall(i) && m_pOnInitFunc[i])
			Script::Call(m_pScriptSystem, m_pOnInitFunc[i], pThis, isReload);
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntityScript::Call_OnShutDown(IScriptTable* pThis)
{
	for (int i = 0; i < NUM_STATES; i++)
	{
		if (ShouldExecuteCall(i) && m_pOnShutDown[i])
			Script::Call(m_pScriptSystem, m_pOnShutDown[i], pThis);
	}
	if (m_pOnDestroyFunc)
		Script::Call(m_pScriptSystem, m_pOnDestroyFunc, pThis);
}

//////////////////////////////////////////////////////////////////////////
void CEntityScript::Call_OnReset(IScriptTable* pThis, bool toGame)
{
	if (m_pOnReset)
		Script::Call(m_pScriptSystem, m_pOnReset, pThis, toGame);
}

//////////////////////////////////////////////////////////////////////////
void CEntityScript::Call_OnTransformFromEditorDone(IScriptTable* pThis)
{
	if (m_pOnTransformFromEditorDone)
		Script::Call(m_pScriptSystem, m_pOnTransformFromEditorDone, pThis);
}

//////////////////////////////////////////////////////////////////////////
inline bool EntityEventLess(const SEntityScriptEvent& e1, const SEntityScriptEvent& e2)
{
	return e1.name < e2.name;
}

//////////////////////////////////////////////////////////////////////////
size_t CEntityScript::CountInOutEvents(IScriptTable* pEventsTable, std::vector<SEntityScriptEvent>& events, bool bOutput)
{
	size_t nEvents = 0;
	IScriptTable::Iterator it = pEventsTable->BeginIteration();
	while (pEventsTable->MoveNext(it))
	{
		HSCRIPTFUNCTION func = 0;
		if (!bOutput)
		{
			if (it.value.GetType() != EScriptAnyType::Table)
				continue;
		}
		else
		{
			if (it.value.GetType() != EScriptAnyType::String)
				continue;
		}

		++nEvents;
	}
	pEventsTable->EndIteration(it);
	return nEvents;
}

//////////////////////////////////////////////////////////////////////////
void CEntityScript::ParseInOutEvents(IScriptTable* pEventsTable, std::vector<SEntityScriptEvent>& events, bool bOutput)
{
	IScriptTable::Iterator it = pEventsTable->BeginIteration();
	while (pEventsTable->MoveNext(it))
	{
		//		const char *sEvent = it.sKey;
		const char* sTypeName = "bool";
		HSCRIPTFUNCTION func = 0;
		if (!bOutput)
		{
			if (it.value.GetType() != EScriptAnyType::Table)
				continue;

			IScriptTable* pEventTable = it.value.GetScriptTable();
			if (!pEventTable->GetAt(1, func))
				continue;
			pEventTable->GetAt(2, sTypeName);
		}
		else
		{
			if (it.value.GetType() != EScriptAnyType::String)
				continue;
			sTypeName = it.value.GetString();
		}

		// Event.
		SEntityScriptEvent event;
		event.bOutput = bOutput;
		event.bOldEvent = 0;
		event.name = it.sKey;
		event.func = func;
		event.valueType = IEntityClass::EVT_BOOL;

		if (stricmp(sTypeName, "float") == 0)
			event.valueType = IEntityClass::EVT_FLOAT;
		else if (stricmp(sTypeName, "int") == 0)
			event.valueType = IEntityClass::EVT_INT;
		else if (stricmp(sTypeName, "bool") == 0)
			event.valueType = IEntityClass::EVT_BOOL;
		else if (stricmp(sTypeName, "vec3") == 0)
			event.valueType = IEntityClass::EVT_VECTOR;
		else if (stricmp(sTypeName, "string") == 0)
			event.valueType = IEntityClass::EVT_STRING;
		else if (stricmp(sTypeName, "entity") == 0)
			event.valueType = IEntityClass::EVT_ENTITY;

		events.push_back(event);
	}
	pEventsTable->EndIteration(it);
}

//////////////////////////////////////////////////////////////////////////
void CEntityScript::LoadEvents()
{
	m_events.clear();

	SmartScriptTable eventsTable;
	if (m_pEntityTable->GetValue("FlowEvents", eventsTable))
	{
		SmartScriptTable inputsTable;
		SmartScriptTable outputsTable;
		size_t nEvents = 0;
		bool hasInputs = false, hasOutputs = false;
		if (eventsTable->GetValue("Inputs", inputsTable))
		{
			hasInputs = true;
			nEvents += CountInOutEvents(inputsTable, m_events, false);
		}
		if (eventsTable->GetValue("Outputs", outputsTable))
		{
			hasOutputs = true;
			nEvents += CountInOutEvents(outputsTable, m_events, true);
		}
		m_events.reserve(nEvents);
		if (hasInputs)
		{
			ParseInOutEvents(inputsTable, m_events, false);
		}
		if (hasOutputs)
		{
			ParseInOutEvents(outputsTable, m_events, true);
		}
	}
	else
	{
		//////////////////////////////////////////////////////////////////////////
		// Enumerate all entity class table functions and find the ones that start from the Event_
		//////////////////////////////////////////////////////////////////////////
		int eventLen = strlen("Event_");

		// First count the events to reserve space
		size_t nEvents = 0;
		IScriptTable::Iterator it = m_pEntityTable->BeginIteration();
		while (m_pEntityTable->MoveNext(it))
			if (it.value.GetType() == EScriptAnyType::Function)
				if (strncmp(it.sKey, "Event_", eventLen) == 0)
					++nEvents;
		m_pEntityTable->EndIteration(it);

		m_events.reserve(2 * nEvents);

		// Now do the actual work
		it = m_pEntityTable->BeginIteration();
		while (m_pEntityTable->MoveNext(it))
		{
			if (it.value.GetType() == EScriptAnyType::Function)
			{
				if (strncmp(it.sKey, "Event_", eventLen) == 0)
				{
					// Event.
					SEntityScriptEvent event;
					event.bOutput = true;
					event.bOldEvent = true;
					event.name = it.sKey + eventLen;
					event.func = 0;
					event.valueType = IEntityClass::EVT_BOOL;
					m_events.push_back(event);

					// Add also same input.
					event.bOutput = false;
					m_events.push_back(event);
				}
			}
		}
		m_pEntityTable->EndIteration(it);
	}

	// Sort events by name.
	std::sort(m_events.begin(), m_events.end(), EntityEventLess);
}

//////////////////////////////////////////////////////////////////////////
void CEntityScript::CallEvent(IScriptTable* pThis, const char* sEvent, float fValue)
{
	for (uint32 i = 0; i < m_events.size(); i++)
	{
		// Find inputs with matching name.
		if (!m_events[i].bOutput && strcmp(m_events[i].name, sEvent) == 0)
		{
			Script::CallMethod(pThis, m_events[i].func, ScriptAnyValue(EScriptAnyType::Nil), fValue);
			break;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntityScript::CallEvent(IScriptTable* pThis, const char* sEvent, bool bValue)
{
	for (uint32 i = 0; i < m_events.size(); i++)
	{
		// Find inputs with matching name.
		if (!m_events[i].bOutput && strcmp(m_events[i].name, sEvent) == 0)
		{
			if (m_events[i].bOldEvent && !bValue)
				break;

			Script::CallMethod(pThis, m_events[i].func, ScriptAnyValue(EScriptAnyType::Nil), bValue);
			break;

			/*
			      if (m_events[i].bOldEvent)
			      {
			        char temp[1024];
			        assert(6 + strlen(sEvent) < sizeof(temp));
			        cry_strcpy(temp, "Event_");
			        cry_strcat(temp, sEvent);
			        Script::CallMethod( pThis, temp );
			      }
			      else
			      {
			        Script::CallMethod( pThis,m_events[i].func, bValue );
			      }
			      break;
			 */
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntityScript::CallEvent(IScriptTable* pThis, const char* sEvent, const char* sValue)
{
	for (uint32 i = 0; i < m_events.size(); i++)
	{
		// Find inputs with matching name.
		if (!m_events[i].bOutput && strcmp(m_events[i].name, sEvent) == 0)
		{
			if (sValue)
				Script::CallMethod(pThis, m_events[i].func, ScriptAnyValue(EScriptAnyType::Nil), sValue);
			else
				Script::CallMethod(pThis, m_events[i].func, ScriptAnyValue(EScriptAnyType::Nil));
			break;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntityScript::CallEvent(IScriptTable* pThis, const char* sEvent, IScriptTable* pTable)
{
	for (uint32 i = 0; i < m_events.size(); i++)
	{
		// Find inputs with matching name.
		if (!m_events[i].bOutput && strcmp(m_events[i].name, sEvent) == 0)
		{
			if (pTable)
				Script::CallMethod(pThis, m_events[i].func, ScriptAnyValue(EScriptAnyType::Nil), pTable);
			else
				Script::CallMethod(pThis, m_events[i].func, ScriptAnyValue(EScriptAnyType::Nil));
			break;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CEntityScript::CallEvent(IScriptTable* pThis, const char* sEvent, const Vec3& vValue)
{
	for (uint32 i = 0; i < m_events.size(); i++)
	{
		// Find inputs with matching name.
		if (!m_events[i].bOutput && strcmp(m_events[i].name, sEvent) == 0)
		{
			Script::CallMethod(pThis, m_events[i].func, ScriptAnyValue(EScriptAnyType::Nil), vValue);
			break;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
const SEntityScriptEvent* CEntityScript::FindEvent(const char* sEvent) const
{
	for (uint32 i = 0; i < m_events.size(); i++)
	{
		// Find inputs with matching name.
		if (m_events[i].bOutput && strcmp(m_events[i].name, sEvent) == 0)
		{
			return &m_events[i];
		}
	}
	/*
	   std::vector<SEntityScriptEvent>::const_iterator it = std::lower_bound( m_events.begin(),m_events.end(),sEvent,EntityEventLess );
	   if (it != m_events.end())
	   {
	   return &(*it);
	   }
	 */
	return 0;
}
