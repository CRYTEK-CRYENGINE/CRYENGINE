// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryScriptSystem/IScriptSystem.h>
#include "EntityClass.h"

struct IEntityScript
{
	virtual ~IEntityScript(){}
	// Releases IEntityScript interface.
	virtual void Release() = 0;

	virtual void GetMemoryUsage(ICrySizer* pSizer) const {}
};

struct SEntityScriptEvent
{
	string                       name;
	HSCRIPTFUNCTION              func;
	IEntityClass::EventValueType valueType;
	unsigned char                bOutput   : 1;
	unsigned char                bOldEvent : 1;
};

#define SCRIPT_PROPERTIES_TABLE "Properties"

//! States's standard script functions.
enum EScriptStateFunctions
{
	ScriptState_OnBeginState,
	ScriptState_OnEndState,
	ScriptState_OnUpdate,
	ScriptState_OnTimer,
	ScriptState_OnEvent,
	ScriptState_OnDamage,

	// Used by all other entities!
	ScriptState_OnEnterArea,
	ScriptState_OnLeaveArea,
	ScriptState_OnEnterNearArea,
	ScriptState_OnLeaveNearArea,
	ScriptState_OnProceedFadeArea, // should be OnMoveInsideArea
	ScriptState_OnMoveNearArea,

	// Used only by the local client entities!
	ScriptState_OnLocalClientEnterArea,
	ScriptState_OnLocalClientLeaveArea,
	ScriptState_OnLocalClientEnterNearArea,
	ScriptState_OnLocalClientLeaveNearArea,
	ScriptState_OnLocalClientProceedFadeArea, // should be OnMoveInsideArea
	ScriptState_OnLocalClientMoveNearArea,

	// Used only by audio listener entities!
	ScriptState_OnAudioListenerEnterArea,
	ScriptState_OnAudioListenerLeaveArea,
	ScriptState_OnAudioListenerEnterNearArea,
	ScriptState_OnAudioListenerLeaveNearArea,
	ScriptState_OnAudioListenerProceedFadeArea, // should be OnMoveInsideArea
	ScriptState_OnAudioListenerMoveNearArea,

	ScriptState_OnBind,
	ScriptState_OnBindThis,
	ScriptState_OnUnBind,
	ScriptState_OnUnBindThis,
	ScriptState_OnMove, //!< OnMove script callback called when an entity moves.
	ScriptState_OnCollision,
	ScriptState_OnAnimationEvent,
	ScriptState_OnPhysicsBreak,
	ScriptState_OnSoundDone,
	ScriptState_OnLevelLoaded,
	ScriptState_OnStartLevel,
	ScriptState_OnStartGame,
	ScriptState_OnHidden,
	ScriptState_OnUnhidden,
	ScriptState_Last,
};

//////////////////////////////////////////////////////////////////////////
enum EScriptStates
{
	SERVER_STATE = 0,
	CLIENT_STATE = 1,
	NUM_STATES,
};

//! Structure that define current state of entity.
//! Contains pointer to script functions that implement state behaivor.
struct SScriptStateFunctions
{
	// Pointers to script state functions.
	HSCRIPTFUNCTION pFunction[ScriptState_Last];    //!< Called when entity is in contact with another entity.
};

//////////////////////////////////////////////////////////////////////////
struct SScriptState
{
	string                 name;
	SScriptStateFunctions* pStateFuns[NUM_STATES];

	//////////////////////////////////////////////////////////////////////////
	SScriptState();
	~SScriptState();
	void Free(IScriptSystem* pScriptSystem);
	// Checks if client or server function in this state is implemented.
	bool IsStateFunctionImplemented(EScriptStateFunctions function) const
	{
		if (pStateFuns[SERVER_STATE] && pStateFuns[SERVER_STATE]->pFunction[function])
			return true;
		else if (pStateFuns[CLIENT_STATE] && pStateFuns[CLIENT_STATE]->pFunction[function])
			return true;
		return false;
	}
};

//////////////////////////////////////////////////////////////////////////
// Description:
//    CScriptClass object represent single entity type in script.
//////////////////////////////////////////////////////////////////////////
class CEntityScript : public IEntityScript
{
public:
	CEntityScript();
	~CEntityScript();

	IScriptSystem* GetScriptSystem() const    { return m_pScriptSystem; };
	IScriptTable*  GetScriptTable() const     { return m_pEntityTable; };

	IScriptTable*  GetPropertiesTable() const { return m_pPropertiesTable; };

	// Initialize entity script, return true if success.
	// Init does not load the script. you must also call LoadScript before using it.
	virtual bool Init(const char* sTableName, const char* sScriptFilename);

	// Initialise entity script from a (code-created) script table.
	// Allows pure-C++ entities with no script file present.
	virtual bool Init(const char* sTableName, IScriptTable* pScriptTable);
	virtual void Release() { delete this; };

	// Description:
	//    Loads the script.
	//    It is safe to call LoadScript multiple times, only first time the script will be loaded, if bForceReload is not specified.
	virtual bool  LoadScript(bool bForceReload = false);

	int           GetStateId(const char* sStateName) const;
	const char*   GetStateName(int nStateId) const;
	SScriptState* GetState(const char* sStateName);
	SScriptState* GetState(int nStateId);

	ILINE bool    ShouldExecuteCall(int state) const
	{
		return ((state == CLIENT_STATE && gEnv->IsClient())
		        || (state == SERVER_STATE && (m_bDefaultOnly || gEnv->bServer)));
	}

	void CallStateFunction(SScriptState* pState, IScriptTable* pThis, EScriptStateFunctions function)
	{
		for (int i = 0; i < NUM_STATES; i++)
		{
			if (ShouldExecuteCall(i) && pState->pStateFuns[i] && pState->pStateFuns[i]->pFunction[function])
				Script::Call(m_pScriptSystem, pState->pStateFuns[i]->pFunction[function], pThis);
		}
	}
	template<class P1>
	void CallStateFunction(SScriptState* pState, IScriptTable* pThis, EScriptStateFunctions function, const P1& p1)
	{
		for (int i = 0; i < NUM_STATES; i++)
		{
			if (ShouldExecuteCall(i) && pState->pStateFuns[i] && pState->pStateFuns[i]->pFunction[function])
				Script::Call(m_pScriptSystem, pState->pStateFuns[i]->pFunction[function], pThis, p1);
		}
	}
	template<class P1, class P2>
	void CallStateFunction(SScriptState* pState, IScriptTable* pThis, EScriptStateFunctions function, const P1& p1, const P2& p2)
	{
		for (int i = 0; i < NUM_STATES; i++)
		{
			if (ShouldExecuteCall(i) && pState->pStateFuns[i] && pState->pStateFuns[i]->pFunction[function])
				Script::Call(m_pScriptSystem, pState->pStateFuns[i]->pFunction[function], pThis, p1, p2);
		}
	}
	template<class P1, class P2, class P3>
	void CallStateFunction(SScriptState* pState, IScriptTable* pThis, EScriptStateFunctions function, const P1& p1, const P2& p2, const P3& p3)
	{
		for (int i = 0; i < NUM_STATES; i++)
		{
			if (ShouldExecuteCall(i) && pState->pStateFuns[i] && pState->pStateFuns[i]->pFunction[function])
				Script::Call(m_pScriptSystem, pState->pStateFuns[i]->pFunction[function], pThis, p1, p2, p3);
		}
	}
	template<class P1, class P2, class P3, class P4>
	void CallStateFunction(SScriptState* pState, IScriptTable* pThis, EScriptStateFunctions function, const P1& p1, const P2& p2, const P3& p3, const P4& p4)
	{
		for (int i = 0; i < NUM_STATES; i++)
		{
			if (ShouldExecuteCall(i) && pState->pStateFuns[i] && pState->pStateFuns[i]->pFunction[function])
				Script::Call(m_pScriptSystem, pState->pStateFuns[i]->pFunction[function], pThis, p1, p2, p3, p4);
		}
	}

	void Call_OnInit(IScriptTable* pThis, bool isReload);
	void Call_OnShutDown(IScriptTable* pThis);
	void Call_OnReset(IScriptTable* pThis, bool toGame);
	void Call_OnTransformFromEditorDone(IScriptTable* pThis);

	// Load events from the entity script.
	int                       GetEventCount() const      { return m_events.size(); };
	const SEntityScriptEvent& GetEvent(int nIndex) const { return m_events[nIndex]; };
	const SEntityScriptEvent* FindEvent(const char* sEvent) const;

	void                      CallEvent(IScriptTable* pThis, const char* sEvent, float fValue);
	void                      CallEvent(IScriptTable* pThis, const char* sEvent, bool bValue);
	void                      CallEvent(IScriptTable* pThis, const char* sEvent, const char* sValue);
	void                      CallEvent(IScriptTable* pThis, const char* sEvent, IScriptTable* pTable);
	void                      CallEvent(IScriptTable* pThis, const char* sEvent, const Vec3& vValue);

private:
	void   Clear();
	void   EnumStates();
	void   LoadEvents();
	void   DelegateProperties();
	void   InitializeStateTable(IScriptTable* pStateTable, SScriptStateFunctions* scriptState);
	void   InitializeNamedStates(IScriptTable* pTable, int nStateNum);

	size_t CountInOutEvents(IScriptTable* pEventsTable, std::vector<SEntityScriptEvent>& events, bool bOutput);
	void   ParseInOutEvents(IScriptTable* pEventsTable, std::vector<SEntityScriptEvent>& events, bool bOutput);

private:
	IScriptSystem*   m_pScriptSystem;
	// Original Entity table used as an entity prototype table.
	SmartScriptTable m_pEntityTable;
	SmartScriptTable m_pPropertiesTable;

	string           m_sTableName;
	string           m_sScriptFilename;

	HSCRIPTFUNCTION  m_pOnSpawnFunc;
	HSCRIPTFUNCTION  m_pOnDestroyFunc;
	HSCRIPTFUNCTION  m_pOnInitFunc[NUM_STATES];
	HSCRIPTFUNCTION  m_pOnShutDown[NUM_STATES];
	HSCRIPTFUNCTION  m_pOnReset;
	HSCRIPTFUNCTION  m_pOnTransformFromEditorDone;

	// Default state.
	SScriptState m_defaultState;

	// List of all available states in the script.
	std::vector<SScriptState>       m_states;

	std::vector<SEntityScriptEvent> m_events;

	bool                            m_bScriptLoaded;
	bool                            m_bDefaultOnly;
};
