// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ATLControlsModel.h"
#include <CryString/StringUtils.h>
#include "AudioControlsEditorUndo.h"
#include <IEditor.h>

namespace ACE
{
CID CATLControlsModel::m_nextId = 1;

CATLControlsModel::CATLControlsModel()
	: m_bSuppressMessages(false)
{
	ClearDirtyFlags();
	m_scopeMap[GetGlobalScope()] = SScopeInfo("global", false);
}

CATLControlsModel::~CATLControlsModel()
{
	Clear();
}

CATLControl* CATLControlsModel::CreateControl(const string& sControlName, EACEControlType eType, CATLControl* pParent)
{
	std::shared_ptr<CATLControl> pControl = std::make_shared<CATLControl>(sControlName, GenerateUniqueId(), eType, this);
	if (pControl)
	{
		if (pParent)
		{
			pControl->SetParent(pParent);
		}

		InsertControl(pControl);

		if (!CUndo::IsSuspended())
		{
			CUndo undo("Audio Control Created");
			CUndo::Record(new CUndoControlAdd(pControl->GetId()));
		}
	}
	return pControl.get();
}

void CATLControlsModel::RemoveControl(CID id)
{
	if (id >= 0)
	{
		for (auto it = m_controls.begin(); it != m_controls.end(); ++it)
		{
			std::shared_ptr<CATLControl>& pControl = *it;
			if (pControl && pControl->GetId() == id)
			{

				OnControlRemoved(pControl.get());
				pControl->ClearConnections();

				// Remove control from parent
				CATLControl* pParent = pControl->GetParent();
				if (pParent)
				{
					pParent->RemoveChild(pControl.get());
				}

				if (!CUndo::IsSuspended())
				{
					CUndo::Record(new CUndoControlRemove(pControl));
				}
				m_controls.erase(it, it + 1);
				break;
			}
		}
	}
}

CATLControl* CATLControlsModel::GetControlByID(CID id) const
{
	if (id >= 0)
	{
		size_t size = m_controls.size();
		for (size_t i = 0; i < size; ++i)
		{
			if (m_controls[i]->GetId() == id)
			{
				return m_controls[i].get();
			}
		}
	}
	return nullptr;
}

bool CATLControlsModel::IsChangeValid(const CATLControl* const pControlToChange, const string& newName, Scope newScope) const
{
	const EACEControlType type = pControlToChange->GetType();
	const CATLControl* const pParent = pControlToChange->GetParent();
	const bool bIsGlobal = newScope == GetGlobalScope();
	for (auto pControl : m_controls)
	{
		if (pControl.get() != pControlToChange)
		{
			if (pControl->GetType() == type &&
			    (pControl->GetName().compareNoCase(newName) == 0) &&
			    (bIsGlobal || pControl->GetScope() == GetGlobalScope() || pControl->GetScope() == newScope) &&
			    (pControl->GetParent() == pParent))
			{
				return false;
			}
		}
	}
	return true;
}

string CATLControlsModel::GenerateUniqueName(const CATLControl* const pControlToChange, const string& newName) const
{
	string finalName = newName;
	int number = 1;
	while (!IsChangeValid(pControlToChange, finalName, pControlToChange->GetScope()))
	{
		finalName = newName + "_" + CryStringUtils::toString(number);
		++number;
	}

	return finalName;
}

void CATLControlsModel::ClearScopes()
{
	m_scopeMap.clear();

	// The global scope must always exist
	m_scopeMap[GetGlobalScope()] = SScopeInfo("global", false);
}

void CATLControlsModel::AddScope(const string& name, bool bLocalOnly)
{
	string scopeName = name;
	m_scopeMap[CCrc32::Compute(scopeName.MakeLower())] = SScopeInfo(scopeName, bLocalOnly);
}

bool CATLControlsModel::ScopeExists(const string& name) const
{
	string scopeName = name;
	return m_scopeMap.find(CCrc32::Compute(scopeName.MakeLower())) != m_scopeMap.end();
}

void CATLControlsModel::GetScopeInfoList(ScopeInfoList& scopeList) const
{
	stl::map_to_vector(m_scopeMap, scopeList);
}

Scope CATLControlsModel::GetGlobalScope() const
{
	static const Scope globalScopeId = CCrc32::Compute("global");
	return globalScopeId;
}

Scope CATLControlsModel::GetScope(const string& name) const
{
	string scopeName = name;
	scopeName.MakeLower();
	return CCrc32::Compute(scopeName);
}

SScopeInfo CATLControlsModel::GetScopeInfo(Scope id) const
{
	return stl::find_in_map(m_scopeMap, id, SScopeInfo());
}

void CATLControlsModel::Clear()
{
	m_controls.clear();
	ClearScopes();
	ClearDirtyFlags();
}

void CATLControlsModel::AddListener(IATLControlModelListener* pListener)
{
	stl::push_back_unique(m_listeners, pListener);
}

void CATLControlsModel::RemoveListener(IATLControlModelListener* pListener)
{
	stl::find_and_erase(m_listeners, pListener);
}

void CATLControlsModel::OnControlAdded(CATLControl* pControl)
{
	if (!m_bSuppressMessages)
	{
		for (auto iter = m_listeners.begin(); iter != m_listeners.end(); ++iter)
		{
			(*iter)->OnControlAdded(pControl);
		}
		m_bControlTypeModified[pControl->GetType()] = true;
	}
}

void CATLControlsModel::OnControlRemoved(CATLControl* pControl)
{
	if (!m_bSuppressMessages)
	{
		for (auto iter = m_listeners.begin(); iter != m_listeners.end(); ++iter)
		{
			(*iter)->OnControlRemoved(pControl);
		}
		m_bControlTypeModified[pControl->GetType()] = true;
	}
}

void CATLControlsModel::OnConnectionAdded(CATLControl* pControl, IAudioSystemItem* pMiddlewareControl)
{
	if (!m_bSuppressMessages)
	{
		for (auto iter = m_listeners.begin(); iter != m_listeners.end(); ++iter)
		{
			(*iter)->OnConnectionAdded(pControl, pMiddlewareControl);
		}
	}
}

void CATLControlsModel::OnConnectionRemoved(CATLControl* pControl, IAudioSystemItem* pMiddlewareControl)
{
	if (!m_bSuppressMessages)
	{
		for (auto iter = m_listeners.begin(); iter != m_listeners.end(); ++iter)
		{
			(*iter)->OnConnectionRemoved(pControl, pMiddlewareControl);
		}
	}
}

void CATLControlsModel::OnControlModified(CATLControl* pControl)
{
	if (!m_bSuppressMessages)
	{
		for (auto iter = m_listeners.begin(); iter != m_listeners.end(); ++iter)
		{
			(*iter)->OnControlModified(pControl);
		}
		m_bControlTypeModified[pControl->GetType()] = true;
	}
}

void CATLControlsModel::SetSuppressMessages(bool bSuppressMessages)
{
	m_bSuppressMessages = bSuppressMessages;
}

bool CATLControlsModel::IsDirty()
{
	for (int i = 0; i < eACEControlType_NumTypes; ++i)
	{
		if (m_bControlTypeModified[i])
		{
			return true;
		}
	}
	return false;
}

bool CATLControlsModel::IsTypeDirty(EACEControlType eType)
{
	if (eType != eACEControlType_NumTypes)
	{
		return m_bControlTypeModified[eType];
	}
	return true;
}

void CATLControlsModel::ClearDirtyFlags()
{
	for (int i = 0; i < eACEControlType_NumTypes; ++i)
	{
		m_bControlTypeModified[i] = false;
	}
}

CATLControl* CATLControlsModel::FindControl(const string& sControlName, EACEControlType eType, Scope scope, CATLControl* pParent) const
{
	if (pParent)
	{
		const size_t size = pParent->ChildCount();
		for (size_t i = 0; i < size; ++i)
		{
			CATLControl* pControl = pParent->GetChild(i);
			if (pControl &&
			    pControl->GetName() == sControlName &&
			    pControl->GetType() == eType &&
			    pControl->GetScope() == scope)
			{
				return pControl;
			}
		}
	}
	else
	{
		const size_t size = m_controls.size();
		for (size_t i = 0; i < size; ++i)
		{
			CATLControl* pControl = m_controls[i].get();
			if (pControl &&
			    pControl->GetName() == sControlName &&
			    pControl->GetType() == eType &&
			    pControl->GetScope() == scope)
			{
				return pControl;
			}
		}
	}
	return nullptr;
}

std::shared_ptr<CATLControl> CATLControlsModel::TakeControl(CID nID)
{
	const size_t size = m_controls.size();
	for (size_t i = 0; i < size; ++i)
	{
		if (m_controls[i]->GetId() == nID)
		{
			std::shared_ptr<CATLControl> pControl = m_controls[i];
			RemoveControl(nID);
			return pControl;
		}
	}
	return nullptr;
}

void CATLControlsModel::InsertControl(std::shared_ptr<CATLControl> pControl)
{
	if (pControl)
	{
		m_controls.push_back(pControl);

		CATLControl* pParent = pControl->GetParent();
		if (pParent)
		{
			pParent->AddChild(pControl.get());
		}

		OnControlAdded(pControl.get());
	}
}

void CATLControlsModel::ClearAllConnections()
{
	const size_t size = m_controls.size();
	for (size_t i = 0; i < size; ++i)
	{
		CATLControl* pControl = m_controls[i].get();
		if (pControl)
		{
			pControl->ClearConnections();
		}
	}
}

void CATLControlsModel::ReloadAllConnections()
{
	const size_t size = m_controls.size();
	for (size_t i = 0; i < size; ++i)
	{
		CATLControl* pControl = m_controls[i].get();
		if (pControl)
		{
			pControl->ReloadConnections();
		}
	}
}

}
