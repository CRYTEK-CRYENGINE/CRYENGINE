// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//! \cond INTERNAL

#pragma once

struct IFlowGraph;
struct ICustomAction;

#define CUSTOM_ACTIONS_PATH "Libs/CustomActions"

//! State used to represent current action state.
enum ECustomActionState
{
	CAS_Ended = 0,
	CAS_Started,
	CAS_Succeeded,
	CAS_SucceededWait,
	CAS_SucceededWaitComplete,
	CAS_Aborted,
};

//! Event passed to event handlers on state change.
enum ECustomActionEvent
{
	CAE_Terminated = 0,
	CAE_Started,
	CAE_Succeeded,
	CAE_SucceededWait,
	CAE_SucceededWaitComplete,
	CAE_Aborted,
	CAE_EndedSuccess,
	CAE_EndedFailure,
};

//! Event listeners for ICustomAction states.
struct ICustomActionListener
{
	// <interfuscator:shuffle>
	virtual ~ICustomActionListener(){}
	virtual void OnCustomActionEvent(ECustomActionEvent event, ICustomAction& customAction) = 0;
	// </interfuscator:shuffle>
};

//! ICustomAction references an Action Flow Graph.
struct ICustomAction
{
	// <interfuscator:shuffle>
	virtual ~ICustomAction(){}

	//! \return Name of the custom action graph.
	virtual const char* GetCustomActionGraphName() const = 0;

	//! \return The Flow Graph associated to this Action.
	virtual IFlowGraph* GetFlowGraph() const = 0;

	//! \return The Object entity associated to this Action.
	virtual IEntity* GetObjectEntity() const = 0;

	//! Execute start section of action.
	virtual bool StartAction() = 0;

	//! Execute succeed section of action.
	virtual bool SucceedAction() = 0;

	//! Execute succeed wait section of action (Alternative to calling EndSuccess from Succeed).
	virtual bool SucceedWaitAction() = 0;

	//! Execute succeed complete section of action (Alternative to calling EndSuccess from Succeed).
	virtual bool SucceedWaitCompleteAction() = 0;

	//! Ends action with success.
	virtual bool EndActionSuccess() = 0;

	//! Ends action with failure.
	virtual bool EndActionFailure() = 0;

	//! Execute abort section of action.
	virtual bool AbortAction() = 0;

	//! Terminates execution of this Action.
	virtual void TerminateAction() = 0;

	//! Marks this Action as modified.
	virtual void               Invalidate() = 0;

	virtual ECustomActionState GetCurrentState() const = 0;

	virtual void               RegisterListener(ICustomActionListener* pEventListener, const char* name) = 0;
	virtual void               UnregisterListener(ICustomActionListener* pEventListener) = 0;
	// </interfuscator:shuffle>
};

struct ICustomActionManager
{
	// <interfuscator:shuffle>
	virtual ~ICustomActionManager(){}

	//! Execute start section of action.
	virtual bool StartAction(IEntity* pObject, const char* szCustomActionGraphName, ICustomActionListener* pListener = NULL) = 0;

	//! Execute succeed section of action.
	virtual bool SucceedAction(IEntity* pObject, const char* szCustomActionGraphName, ICustomActionListener* pListener = NULL) = 0;

	//! Execute succeed wait section of action (Alternative to calling EndSuccess from Succeed).
	virtual bool SucceedWaitAction(IEntity* pObject) = 0;

	//! Execute succeed complete section of action (Alternative to calling EndSuccess from Succeed).
	virtual bool SucceedWaitCompleteAction(IEntity* pObject) = 0;

	//! Execute abort section of action.
	virtual bool AbortAction(IEntity* pObject) = 0;

	//! Ends action.
	virtual bool EndAction(IEntity* pObject, bool bSuccess) = 0;

	//! Loads the library of Custom Action Flow Graphs.
	virtual void LoadLibraryActions(const char* sPath) = 0;

	//! Clears all library actions.
	virtual void ClearLibraryActions() = 0;

	//! Clears all active actions.
	virtual void ClearActiveActions() = 0;

	//! Gets number of actions in library.
	virtual size_t GetNumberOfCustomActionsFromLibrary() const = 0;

	//! \return An action by name if exists in library, NULL if not found.
	virtual ICustomAction* GetCustomActionFromLibrary(const char* szCustomActionGraphName) = 0;

	//! Returns a action by index if exists in library, NULL if not found.
	virtual ICustomAction* GetCustomActionFromLibrary(const size_t index) = 0;

	//! Gets number of currently active actions.
	virtual size_t GetNumberOfActiveCustomActions() const = 0;

	//! Gets active action by name, NULL if not found.
	virtual ICustomAction* GetActiveCustomAction(const IEntity* pObject) = 0;

	//! Gets active action by index, NULL if not found.
	virtual ICustomAction* GetActiveCustomAction(const size_t index) = 0;

	//! Unregisters listener from all actions if found, return false if none found.
	virtual bool UnregisterListener(ICustomActionListener* pEventListener) = 0;

	virtual void Serialize(TSerialize ser) = 0;
	// </interfuscator:shuffle>
};

//! \endcond