// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryString/CryString.h>
#include "AudioControl.h"
#include <IAudioConnection.h>
#include "ACETypes.h"

namespace ACE
{

struct IATLControlModelListener
{
	virtual void OnControlAdded(CATLControl* pControl)                                            {}
	virtual void OnControlModified(CATLControl* pControl)                                         {}
	virtual void OnControlRemoved(CATLControl* pControl)                                          {}
	virtual void OnConnectionAdded(CATLControl* pControl, IAudioSystemItem* pMiddlewareControl)   {}
	virtual void OnConnectionRemoved(CATLControl* pControl, IAudioSystemItem* pMiddlewareControl) {}
};

class CATLControlsModel
{
	friend class IUndoControlOperation;
	friend class CUndoControlModified;
	friend class CATLControl;

public:
	CATLControlsModel();
	~CATLControlsModel();

	void         Clear();
	CATLControl* CreateControl(const string& sControlName, EACEControlType type, CATLControl* pParent = nullptr);
	void         RemoveControl(CID id);

	CATLControl* GetControlByID(CID id) const;
	CATLControl* FindControl(const string& sControlName, EACEControlType eType, Scope scope, CATLControl* pParent = nullptr) const;

	// Scope
	void       ClearScopes();
	Scope      GetGlobalScope() const;
	void       AddScope(const string& name, bool bLocalOnly = false);
	bool       ScopeExists(const string& name) const;
	Scope      GetScope(const string& name) const;
	SScopeInfo GetScopeInfo(Scope id) const;
	void       GetScopeInfoList(ScopeInfoList& scopeList) const;

	// Helper functions
	bool   IsChangeValid(const CATLControl* const pControlToChange, const string& newName, Scope newScope) const;
	string GenerateUniqueName(const CATLControl* const pControlToChange, const string& newName) const;
	void   ClearAllConnections();
	void   ReloadAllConnections();

	void   AddListener(IATLControlModelListener* pListener);
	void   RemoveListener(IATLControlModelListener* pListener);
	void   SetSuppressMessages(bool bSuppressMessages);
	bool   IsTypeDirty(EACEControlType eType);
	bool   IsDirty();
	void   ClearDirtyFlags();

private:
	void                         OnControlAdded(CATLControl* pControl);
	void                         OnControlModified(CATLControl* pControl);
	void                         OnConnectionAdded(CATLControl* pControl, IAudioSystemItem* pMiddlewareControl);
	void                         OnConnectionRemoved(CATLControl* pControl, IAudioSystemItem* pMiddlewareControl);
	void                         OnControlRemoved(CATLControl* pControl);

	CID                          GenerateUniqueId() { return m_nextId++; }

	std::shared_ptr<CATLControl> TakeControl(CID nID);
	void                         InsertControl(std::shared_ptr<CATLControl> pControl);

	static CID m_nextId;
	std::vector<std::shared_ptr<CATLControl>> m_controls;
	std::map<Scope, SScopeInfo>               m_scopeMap;

	std::vector<IATLControlModelListener*>    m_listeners;
	bool m_bSuppressMessages;
	bool m_bControlTypeModified[eACEControlType_NumTypes];
};
}
