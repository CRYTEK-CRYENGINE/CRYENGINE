// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef _CUSTOMACTIONMANAGER_H_
#define _CUSTOMACTIONMANAGER_H_

#if _MSC_VER > 1000
	#pragma once
#endif

#include <CryFlowGraph/IFlowSystem.h>
#include <CryAction/ICustomActions.h>
#include <CryCore/Containers/CryListenerSet.h>
#include "CustomAction.h"

///////////////////////////////////////////////////
// CCustomActionManager keeps track of all CustomActions
///////////////////////////////////////////////////
class CCustomActionManager : public ICustomActionManager
{
public:
	CCustomActionManager();
	virtual ~CCustomActionManager();

public:
	// ICustomActionManager
	virtual bool           StartAction(IEntity* pObject, const char* szCustomActionGraphName, ICustomActionListener* pListener = NULL);
	virtual bool           SucceedAction(IEntity* pObject, const char* szCustomActionGraphName, ICustomActionListener* pListener = NULL);
	virtual bool           SucceedWaitAction(IEntity* pObject);
	virtual bool           SucceedWaitCompleteAction(IEntity* pObject);
	virtual bool           AbortAction(IEntity* pObject);
	virtual bool           EndAction(IEntity* pObject, bool bSuccess);
	virtual void           LoadLibraryActions(const char* sPath);
	virtual void           ClearActiveActions();
	virtual void           ClearLibraryActions();
	virtual size_t         GetNumberOfCustomActionsFromLibrary() const { return m_actionsLib.size(); }
	virtual ICustomAction* GetCustomActionFromLibrary(const char* szCustomActionGraphName);
	virtual ICustomAction* GetCustomActionFromLibrary(const size_t index);
	virtual size_t         GetNumberOfActiveCustomActions() const;
	virtual ICustomAction* GetActiveCustomAction(const IEntity* pObject);
	virtual ICustomAction* GetActiveCustomAction(const size_t index);
	virtual bool           UnregisterListener(ICustomActionListener* pEventListener);
	virtual void           Serialize(TSerialize ser);
	// ~ICustomActionManager

	// Removes deleted Action from the list of active actions
	void Update();

	void GetMemoryUsage(ICrySizer* pSizer) const
	{
		SIZER_COMPONENT_NAME(pSizer, "CustomActionManager");

		pSizer->AddObject(this, sizeof(*this));
		pSizer->AddObject(m_actionsLib);
		pSizer->AddObject(m_activeActions);
	}

protected:
	// Adds an Action in the list of active actions
	ICustomAction* AddActiveCustomAction(IEntity* pObject, const char* szCustomActionGraphName, ICustomActionListener* pListener = NULL);

	// Called when entity is removed
	void OnEntityRemove(IEntity* pEntity);

private:
	// Library of all defined Actions
	typedef std::map<string, CCustomAction> TCustomActionsLib;
	TCustomActionsLib m_actionsLib;

	// List of all active Actions (including suspended and to be deleted)
	typedef std::list<CCustomAction> TActiveActions;
	TActiveActions m_activeActions;
};

#endif
