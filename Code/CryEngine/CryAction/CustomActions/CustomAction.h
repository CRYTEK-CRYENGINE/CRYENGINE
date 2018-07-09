// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef _CUSTOMACTION_H_
#define _CUSTOMACTION_H_

#if _MSC_VER > 1000
	#pragma once
#endif

#include <CryFlowGraph/IFlowSystem.h>
#include <CryAction/ICustomActions.h>
#include <CryCore/Containers/CryListenerSet.h>

// Forward declaration
class CCustomActionManager;

///////////////////////////////////////////////////
// CCustomAction references a Flow Graph - sequence of elementary actions
///////////////////////////////////////////////////
class CCustomAction : public ICustomAction
{
	friend class CCustomActionManager;  // CCustomActionManager needs right to access and modify these private data members

public:
	CCustomAction(const CCustomAction& src)
		: m_listeners(1)
		, m_pObjectEntity(NULL)
		, m_currentState(CAS_Ended)
	{
		m_customActionGraphName = src.m_customActionGraphName;

		// Doesn't make sense to copy state
	}

	CCustomAction()
		: m_listeners(1)
		, m_pObjectEntity(NULL)
		, m_currentState(CAS_Ended)
		, m_customActionGraphName("")
		, m_pFlowGraph(NULL)
	{}
	virtual ~CCustomAction() {}

	CCustomAction& operator=(const CCustomAction& src)
	{
		m_customActionGraphName = src.m_customActionGraphName;

		// Doesn't make sense to copy state

		return *this;
	}

	// Overloaded comparison
	bool operator==(const CCustomAction& src) const
	{
		return (m_pObjectEntity == src.m_pObjectEntity &&
		        m_customActionGraphName == src.m_customActionGraphName);   // Only needed for remove so ok to have duplicates after terminations
	}

	// ICustomAction
	virtual IEntity*           GetObjectEntity() const          { return m_pObjectEntity; }
	virtual const char*        GetCustomActionGraphName() const { return m_customActionGraphName; }
	virtual IFlowGraph*        GetFlowGraph() const             { return m_pFlowGraph; }
	virtual bool               StartAction();
	virtual bool               SucceedAction();
	virtual bool               SucceedWaitAction();
	virtual bool               SucceedWaitCompleteAction();
	virtual bool               EndActionSuccess();
	virtual bool               EndActionFailure();
	virtual bool               AbortAction();
	virtual void               TerminateAction();
	virtual void               Invalidate()                                                              { m_pFlowGraph = NULL; }
	virtual ECustomActionState GetCurrentState() const                                                   { return m_currentState; }
	virtual void               RegisterListener(ICustomActionListener* pEventListener, const char* name) { m_listeners.Add(pEventListener, name); }
	virtual void               UnregisterListener(ICustomActionListener* pEventListener)                 { m_listeners.Remove(pEventListener); }
	// ~ICustomAction

	// Called on serialize
	void Serialize(TSerialize ser);

	void GetMemoryUsage(ICrySizer* pSizer) const
	{
		pSizer->AddObject(m_customActionGraphName);
		pSizer->AddObject(m_pFlowGraph);
	}

protected:
	bool SwitchState(const ECustomActionState newState,
	                 const ECustomActionEvent event,
	                 const char* szNodeToCall,
	                 const char* szLuaFuncToCall);

	void NotifyListeners(ECustomActionEvent event, ICustomAction& customAction)
	{
		for (TCustomActionListeners::Notifier notifier(m_listeners); notifier.IsValid(); notifier.Next())
			notifier->OnCustomActionEvent(event, customAction);
	}

	IEntity*           m_pObjectEntity;         // Entities participants in this Action
	string             m_customActionGraphName; // Name of the custom action graph
	IFlowGraphPtr      m_pFlowGraph;            // Points to a flow graph which would be used to execute the Action
	ECustomActionState m_currentState;          // Current state of the action

	typedef CListenerSet<ICustomActionListener*> TCustomActionListeners;
	TCustomActionListeners m_listeners; // Actions listeners that listener to state change events
};

#endif
