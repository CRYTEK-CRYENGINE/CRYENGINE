// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AudioControl.h"
#include "ATLControlsModel.h"
#include "AudioControlsEditorUndo.h"
#include "IEditor.h"
#include <IAudioSystemItem.h>
#include <ACETypes.h>
#include "AudioControlsEditorPlugin.h"
#include "ImplementationManager.h"
#include <CrySerialization/StringList.h>

namespace ACE
{

CATLControl::CATLControl()
	: m_id(ACE_INVALID_ID)
	, m_type(eACEControlType_RTPC)
	, m_name("")
	, m_scope(0)
	, m_pParent(nullptr)
	, m_bAutoLoad(true)
	, m_bModified(false)
	, m_pModel(nullptr)
{
}

CATLControl::CATLControl(const string& controlName, CID nID, EACEControlType eType, CATLControlsModel* pModel)
	: m_id(nID)
	, m_type(eType)
	, m_name("")
	, m_scope(0)
	, m_pParent(nullptr)
	, m_bAutoLoad(true)
	, m_bModified(false)
	, m_pModel(pModel)
{
	SetName(controlName);
	m_scope = pModel->GetGlobalScope();
}

CATLControl::~CATLControl()
{
	m_connectedControls.clear();
}

CID CATLControl::GetId() const
{
	return m_id;
}

EACEControlType CATLControl::GetType() const
{
	return m_type;
}

string CATLControl::GetName() const
{
	return m_name;
}

void CATLControl::SetId(CID id)
{
	if (id != m_id)
	{
		SignalControlAboutToBeModified();
		m_id = id;
		SignalControlModified();
	}
}

void CATLControl::SetType(EACEControlType type)
{
	if (type != m_type)
	{
		SignalControlAboutToBeModified();
		m_type = type;
		SignalControlModified();
	}
}

void CATLControl::SetName(const string& name)
{
	if (name != m_name)
	{
		SignalControlAboutToBeModified();
		m_name = m_pModel->GenerateUniqueName(this, name);
		SignalControlModified();
	}
}

Scope CATLControl::GetScope() const
{
	return m_scope;
}

void CATLControl::SetScope(Scope scope)
{
	if (m_scope != scope)
	{
		if (m_pModel->IsChangeValid(this, GetName(), scope))
		{
			SignalControlAboutToBeModified();
			m_scope = scope;
			SignalControlModified();
		}
	}
}

bool CATLControl::IsAutoLoad() const
{
	return m_bAutoLoad;
}

void CATLControl::SetAutoLoad(bool bAutoLoad)
{
	if (bAutoLoad != m_bAutoLoad)
	{
		SignalControlAboutToBeModified();
		m_bAutoLoad = bAutoLoad;
		SignalControlModified();
	}
}

size_t CATLControl::GetConnectionCount()
{
	return m_connectedControls.size();
}

ConnectionPtr CATLControl::GetConnectionAt(int index)
{
	if (index < m_connectedControls.size())
	{
		return m_connectedControls[index];
	}
	return nullptr;
}

ConnectionPtr CATLControl::GetConnection(CID id)
{
	if (id >= 0)
	{
		const size_t size = m_connectedControls.size();
		for (int i = 0; i < size; ++i)
		{
			ConnectionPtr pConnection = m_connectedControls[i];
			if (pConnection && pConnection->GetID() == id)
			{
				return pConnection;
			}
		}
	}
	return nullptr;
}

ConnectionPtr CATLControl::GetConnection(IAudioSystemItem* pAudioSystemControl)
{
	return GetConnection(pAudioSystemControl->GetId());
}

void CATLControl::AddConnection(ConnectionPtr pConnection)
{
	if (pConnection)
	{
		SignalControlAboutToBeModified();
		m_connectedControls.push_back(pConnection);
		IAudioSystemEditor* pAudioSystemImpl = CAudioControlsEditorPlugin::GetImplementationManger()->GetImplementation();
		if (pAudioSystemImpl)
		{
			IAudioSystemItem* pAudioSystemControl = pAudioSystemImpl->GetControl(pConnection->GetID());
			if (pAudioSystemControl)
			{
				pAudioSystemImpl->EnableConnection(pConnection);
				SignalConnectionAdded(pAudioSystemControl);
			}
		}
		SignalControlModified();
	}
}

void CATLControl::RemoveConnection(ConnectionPtr pConnection)
{
	if (pConnection)
	{
		auto it = std::find(m_connectedControls.begin(), m_connectedControls.end(), pConnection);
		if (it != m_connectedControls.end())
		{
			SignalControlAboutToBeModified();
			IAudioSystemEditor* pAudioSystemImpl = CAudioControlsEditorPlugin::GetImplementationManger()->GetImplementation();
			if (pAudioSystemImpl)
			{
				IAudioSystemItem* pAudioSystemControl = pAudioSystemImpl->GetControl(pConnection->GetID());
				if (pAudioSystemControl)
				{
					pAudioSystemImpl->DisableConnection(pConnection);
					m_connectedControls.erase(it);
					SignalConnectionRemoved(pAudioSystemControl);
				}
			}
			SignalControlModified();
		}
	}
}

void CATLControl::ClearConnections()
{
	SignalControlAboutToBeModified();
	IAudioSystemEditor* pAudioSystemImpl = CAudioControlsEditorPlugin::GetImplementationManger()->GetImplementation();
	if (pAudioSystemImpl)
	{
		for (ConnectionPtr& c : m_connectedControls)
		{
			pAudioSystemImpl->DisableConnection(c);
			IAudioSystemItem* pAudioSystemControl = pAudioSystemImpl->GetControl(c->GetID());
			if (pAudioSystemControl)
			{
				SignalConnectionRemoved(pAudioSystemControl);
			}
		}
	}
	m_connectedControls.clear();
	SignalControlModified();
}

void CATLControl::RemoveConnection(IAudioSystemItem* pAudioSystemControl)
{
	if (pAudioSystemControl)
	{
		const CID nID = pAudioSystemControl->GetId();
		auto it = m_connectedControls.begin();
		auto end = m_connectedControls.end();
		for (; it != end; ++it)
		{
			if ((*it)->GetID() == nID)
			{
				SignalControlAboutToBeModified();
				IAudioSystemEditor* pAudioSystemImpl = CAudioControlsEditorPlugin::GetImplementationManger()->GetImplementation();
				if (pAudioSystemImpl)
				{
					pAudioSystemImpl->DisableConnection(*it);
				}
				m_connectedControls.erase(it);

				SignalConnectionRemoved(pAudioSystemControl);
				SignalControlModified();
				return;
			}
		}
	}
}

void CATLControl::SignalControlModified()
{
	if (m_pModel)
	{
		m_pModel->OnControlModified(this);
	}
}

void CATLControl::SignalControlAboutToBeModified()
{
	if (!CUndo::IsSuspended())
	{
		CUndo undo("ATL Control Modified");
		CUndo::Record(new CUndoControlModified(GetId()));
	}
}

void CATLControl::SignalConnectionAdded(IAudioSystemItem* pMiddlewareControl)
{
	if (m_pModel)
	{
		m_pModel->OnConnectionAdded(this, pMiddlewareControl);
	}
}

void CATLControl::SignalConnectionRemoved(IAudioSystemItem* pMiddlewareControl)
{
	if (m_pModel)
	{
		m_pModel->OnConnectionRemoved(this, pMiddlewareControl);
	}
}

void CATLControl::ReloadConnections()
{
	IAudioSystemEditor* pAudioSystemImpl = CAudioControlsEditorPlugin::GetImplementationManger()->GetImplementation();
	if (pAudioSystemImpl)
	{
		std::map<int, XMLNodeList> connectionNodes;
		std::swap(connectionNodes, m_connectionNodes);
		for (auto& connectionPair : connectionNodes)
		{
			for (auto& connection : connectionPair.second)
			{
				LoadConnectionFromXML(connection.xmlNode, connectionPair.first);
			}
		}
	}
}

void CATLControl::LoadConnectionFromXML(XmlNodeRef xmlNode, int platformIndex)
{
	IAudioSystemEditor* pAudioSystemImpl = CAudioControlsEditorPlugin::GetImplementationManger()->GetImplementation();
	if (pAudioSystemImpl)
	{
		ConnectionPtr pConnection = pAudioSystemImpl->CreateConnectionFromXMLNode(xmlNode, GetType());
		if (pConnection)
		{
			if (GetType() == eACEControlType_Preload)
			{
				// The connection could already exist but using a different platform
				ConnectionPtr pPreviousConnection = GetConnection(pConnection->GetID());
				if (pPreviousConnection == nullptr)
				{
					if (platformIndex != -1)
					{
						pConnection->ClearPlatforms();
					}
					AddConnection(pConnection);
				}
				else
				{
					pConnection = pPreviousConnection;
				}
				if (platformIndex != -1)
				{
					pConnection->EnableForPlatform(platformIndex, true);
				}
			}
			else
			{
				AddConnection(pConnection);
			}
		}
		AddRawXMLConnection(xmlNode, pConnection != nullptr, platformIndex);
	}
}

void CATLControl::Serialize(Serialization::IArchive& ar)
{
	// Name
	string newName = m_name;
	ar(newName, "name", "Name");

	// Scope
	Serialization::StringList scopeList;
	ScopeInfoList scopeInfoList;
	m_pModel->GetScopeInfoList(scopeInfoList);
	for (auto& scope : scopeInfoList)
	{
		scopeList.push_back(scope.name);
	}
	Serialization::StringListValue selectedScope(scopeList, m_pModel->GetScopeInfo(m_scope).name);
	Scope newScope = m_scope;
	if (m_type != eACEControlType_State)
	{
		ar(selectedScope, "scope", "Scope");
		newScope = m_pModel->GetScope(scopeList[selectedScope.index()]);
	}

	// Auto Load
	bool bAutoLoad = m_bAutoLoad;
	if (m_type == eACEControlType_Preload)
	{
		ar(bAutoLoad, "auto_load", "Auto Load");
	}

	if (ar.isInput())
	{
		SetName(newName);
		SetScope(newScope);
		SetAutoLoad(bAutoLoad);
	}
}

void CATLControl::SetParent(CATLControl* pParent)
{
	m_pParent = pParent;
	if (pParent)
	{
		SetScope(pParent->GetScope());
	}
}

void CATLControl::AddRawXMLConnection(XmlNodeRef xmlNode, bool bValid, int platformIndex /*= -1*/)
{
	m_connectionNodes[platformIndex].push_back(SRawConnectionData(xmlNode, bValid));
}

XMLNodeList& CATLControl::GetRawXMLConnections(int platformIndex /*= -1*/)
{
	return m_connectionNodes[platformIndex];
}

}
