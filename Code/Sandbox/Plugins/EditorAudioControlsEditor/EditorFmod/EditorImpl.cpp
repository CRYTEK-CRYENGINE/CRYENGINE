// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "EditorImpl.h"

#include "ImplConnections.h"
#include "ProjectLoader.h"

#include <CryAudio/IAudioSystem.h>
#include <CryAudio/IProfileData.h>
#include <CrySystem/ISystem.h>
#include <CryCore/StlUtils.h>
#include <CrySerialization/IArchiveHost.h>

namespace ACE
{
namespace Fmod
{
const string g_userSettingsFile = "%USER%/audiocontrolseditor_fmod.user";

// XML tags
string const g_eventTag = "FmodEvent";
string const g_eventParameterTag = "FmodEventParameter";
string const g_snapshotTag = "FmodSnapshot";
string const g_snapshotParameterTag = "FmodSnapshotParameter";
string const g_bankTag = "FmodFile";
string const g_returnTag = "FmodBus";

// XML attributes
string const g_nameAttribute = "fmod_name";
string const g_pathAttribute = "fmod_path";
string const g_valueAttribute = "fmod_value";
string const g_multAttribute = "fmod_value_multiplier";
string const g_shiftAttribute = "fmod_value_shift";
string const g_eventTypeAttribute = "fmod_event_type";
string const g_localizedAttribute = "fmod_localized";
string const g_trueAttribute = "true";

//////////////////////////////////////////////////////////////////////////
EImpltemType TagToType(string const& tag)
{
	if (tag == g_eventTag)
	{
		return EImpltemType::Event;
	}
	else if (tag == g_eventParameterTag)
	{
		return EImpltemType::EventParameter;
	}
	else if (tag == g_snapshotTag)
	{
		return EImpltemType::Snapshot;
	}
	else if (tag == g_snapshotParameterTag)
	{
		return EImpltemType::SnapshotParameter;
	}
	else if (tag == g_bankTag)
	{
		return EImpltemType::Bank;
	}
	else if (tag == g_returnTag)
	{
		return EImpltemType::Return;
	}

	return EImpltemType::Invalid;
}

//////////////////////////////////////////////////////////////////////////
string TypeToTag(EImpltemType const type)
{
	switch (type)
	{
	case EImpltemType::Event:
		return g_eventTag;
	case EImpltemType::EventParameter:
		return g_eventParameterTag;
	case EImpltemType::Snapshot:
		return g_snapshotTag;
	case EImpltemType::SnapshotParameter:
		return g_snapshotParameterTag;
	case EImpltemType::Bank:
		return g_bankTag;
	case EImpltemType::Return:
		return g_returnTag;
	}
	return "";
}

//////////////////////////////////////////////////////////////////////////
CImplItem* SearchForControl(CImplItem* const pImplItem, string const& name, ItemType const type)
{
	if ((pImplItem->GetName() == name) && (pImplItem->GetType() == type))
	{
		return pImplItem;
	}

	int const count = pImplItem->ChildCount();

	for (int i = 0; i < count; ++i)
	{
		CImplItem* const pFoundImplControl = SearchForControl(pImplItem->GetChildAt(i), name, type);

		if (pFoundImplControl != nullptr)
		{
			return pFoundImplControl;
		}
	}

	return nullptr;
}

//////////////////////////////////////////////////////////////////////////
void CImplSettings::SetProjectPath(char const* szPath)
{
	m_projectPath = szPath;
	Serialization::SaveJsonFile(g_userSettingsFile.c_str(), *this);
}

//////////////////////////////////////////////////////////////////////////
void CImplSettings::Serialize(Serialization::IArchive& ar)
{
	ar(m_projectPath, "projectPath", "Project Path");
}

//////////////////////////////////////////////////////////////////////////
CEditorImpl::CEditorImpl()
{
	Serialization::LoadJsonFile(m_implSettings, g_userSettingsFile.c_str());
}

//////////////////////////////////////////////////////////////////////////
CEditorImpl::~CEditorImpl()
{
	Clear();
}

//////////////////////////////////////////////////////////////////////////
void CEditorImpl::Reload(bool const preserveConnectionStatus)
{
	Clear();

	CProjectLoader(GetSettings()->GetProjectPath(), GetSettings()->GetSoundBanksPath(), m_rootControl);

	CreateControlCache(&m_rootControl);

	if (preserveConnectionStatus)
	{
		for (auto const& connection : m_connectionsByID)
		{
			if (connection.second > 0)
			{
				CImplItem* const pImplControl = GetControl(connection.first);

				if (pImplControl != nullptr)
				{
					pImplControl->SetConnected(true);
				}
			}
		}
	}
	else
	{
		m_connectionsByID.clear();
	}
}

//////////////////////////////////////////////////////////////////////////
CImplItem* CEditorImpl::GetControl(CID const id) const
{
	if (id >= 0)
	{
		return stl::find_in_map(m_controlsCache, id, nullptr);
	}

	return nullptr;
}

//////////////////////////////////////////////////////////////////////////
char const* CEditorImpl::GetTypeIcon(CImplItem const* const pImplItem) const
{
	auto const type = static_cast<EImpltemType>(pImplItem->GetType());

	switch (type)
	{
	case EImpltemType::Folder:
		return "icons:audio/fmod/folder_closed.ico";
	case EImpltemType::Event:
		return "icons:audio/fmod/event.ico";
	case EImpltemType::EventParameter:
		return "icons:audio/fmod/tag.ico";
	case EImpltemType::Snapshot:
		return "icons:audio/fmod/snapshot.ico";
	case EImpltemType::SnapshotParameter:
		return "icons:audio/fmod/tag.ico";
	case EImpltemType::Bank:
		return "icons:audio/fmod/bank.ico";
	case EImpltemType::Return:
		return "icons:audio/fmod/return.ico";
	case EImpltemType::Group:
		return "icons:audio/fmod/group.ico";
	}
	return "icons:Dialogs/dialog-error.ico";
}

//////////////////////////////////////////////////////////////////////////
string CEditorImpl::GetName() const
{
	return gEnv->pAudioSystem->GetProfileData()->GetImplName();
}

//////////////////////////////////////////////////////////////////////////
bool CEditorImpl::IsTypeCompatible(ESystemItemType const systemType, CImplItem const* const pImplItem) const
{
	auto const implType = static_cast<EImpltemType>(pImplItem->GetType());

	switch (systemType)
	{
	case ESystemItemType::Trigger:
		return (implType == EImpltemType::Event) || (implType == EImpltemType::Snapshot);
	case ESystemItemType::Parameter:
		return (implType == EImpltemType::EventParameter) || (implType == EImpltemType::SnapshotParameter);
	case ESystemItemType::Preload:
		return implType == EImpltemType::Bank;
	case ESystemItemType::State:
		return (implType == EImpltemType::EventParameter) || (implType == EImpltemType::SnapshotParameter);
	case ESystemItemType::Environment:
		return implType == EImpltemType::Return;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
ESystemItemType CEditorImpl::ImplTypeToSystemType(CImplItem const* const pImplItem) const
{
	auto const type = static_cast<EImpltemType>(pImplItem->GetType());

	switch (type)
	{
	case EImpltemType::Event:
		return ESystemItemType::Trigger;
	case EImpltemType::EventParameter:
		return ESystemItemType::Parameter;
	case EImpltemType::Snapshot:
		return ESystemItemType::Trigger;
	case EImpltemType::SnapshotParameter:
		return ESystemItemType::Parameter;
	case EImpltemType::Bank:
		return ESystemItemType::Preload;
	case EImpltemType::Return:
		return ESystemItemType::Environment;
	}
	return ESystemItemType::Invalid;
}

//////////////////////////////////////////////////////////////////////////
ConnectionPtr CEditorImpl::CreateConnectionToControl(ESystemItemType const controlType, CImplItem* const pImplItem)
{
	if (pImplItem != nullptr)
	{
		auto const type = static_cast<EImpltemType>(pImplItem->GetType());

		if ((type == EImpltemType::Event) || (type == EImpltemType::Snapshot))
		{
			return std::make_shared<CEventConnection>(pImplItem->GetId());
		}
		else if ((type == EImpltemType::EventParameter) || (type == EImpltemType::SnapshotParameter))
		{
			if (controlType == ESystemItemType::Parameter)
			{
				return std::make_shared<CParamConnection>(pImplItem->GetId());
			}
			else if (controlType == ESystemItemType::State)
			{
				return std::make_shared<CParamToStateConnection>(pImplItem->GetId());
			}
		}

		return std::make_shared<CImplConnection>(pImplItem->GetId());
	}

	return nullptr;
}

//////////////////////////////////////////////////////////////////////////
ConnectionPtr CEditorImpl::CreateConnectionFromXMLNode(XmlNodeRef pNode, ESystemItemType const controlType)
{
	if (pNode != nullptr)
	{
		auto const type = TagToType(pNode->getTag());

		if (type != EImpltemType::Invalid)
		{
			string name = pNode->getAttr(g_nameAttribute);
			string path = pNode->getAttr(g_pathAttribute);
			string const localizedAttribute = pNode->getAttr(g_localizedAttribute);
			bool const isLocalized = (localizedAttribute.compareNoCase(g_trueAttribute) == 0);

			if (!path.empty())
			{
				name = path + "/" + name;
			}

			CImplItem* pImplItem = GetItemFromPath(name);

			if ((pImplItem == nullptr) || (type != static_cast<EImpltemType>(pImplItem->GetType())))
			{
				// If control not found, create a placeholder.
				// We want to keep that connection even if it's not in the middleware as the user could
				// be using the engine without the fmod project

				path = "";
				int const pos = name.find_last_of("/");

				if (pos != string::npos)
				{
					path = name.substr(0, pos);
					name = name.substr(pos + 1, name.length() - pos);
				}

				pImplItem = CreateItem(type, CreatePlaceholderFolderPath(path), name);

				if (pImplItem != nullptr)
				{
					pImplItem->SetPlaceholder(true);
				}
			}

			switch (type)
			{
			case EImpltemType::Event:
			case EImpltemType::Snapshot:
				{
					string const eventType = pNode->getAttr(g_eventTypeAttribute);
					auto const pConnection = std::make_shared<CEventConnection>(pImplItem->GetId());
					pConnection->m_type = (eventType == "stop") ? EEventType::Stop : EEventType::Start;
					return pConnection;
				}
				break;
			case EImpltemType::EventParameter:
			case EImpltemType::SnapshotParameter:
				{
					if (controlType == ESystemItemType::Parameter)
					{
						std::shared_ptr<CParamConnection> const pConnection = std::make_shared<CParamConnection>(pImplItem->GetId());
						float mult = 1.0f;
						float shift = 0.0f;

						if (pNode->haveAttr(g_multAttribute))
						{
							string const value = pNode->getAttr(g_multAttribute);
							mult = (float)std::atof(value.c_str());
						}

						if (pNode->haveAttr(g_shiftAttribute))
						{
							string const value = pNode->getAttr(g_shiftAttribute);
							shift = (float)std::atof(value.c_str());
						}

						pConnection->m_mult = mult;
						pConnection->m_shift = shift;
						return pConnection;
					}
					else if (controlType == ESystemItemType::State)
					{
						std::shared_ptr<CParamToStateConnection> const pConnection = std::make_shared<CParamToStateConnection>(pImplItem->GetId());
						string const value = pNode->getAttr(g_valueAttribute);
						pConnection->m_value = (float)std::atof(value.c_str());
						return pConnection;
					}
				}
				break;
			case EImpltemType::Bank:
			case EImpltemType::Return:
				{
					return std::make_shared<CImplConnection>(pImplItem->GetId());
				}
				break;
			}
		}
	}

	return nullptr;
}

//////////////////////////////////////////////////////////////////////////
XmlNodeRef CEditorImpl::CreateXMLNodeFromConnection(ConnectionPtr const pConnection, ESystemItemType const controlType)
{
	CImplItem* const pImplControl = GetControl(pConnection->GetID());

	if (pImplControl != nullptr)
	{
		auto const type = static_cast<EImpltemType>(pImplControl->GetType());
		XmlNodeRef const pNode = GetISystem()->CreateXmlNode(TypeToTag(type));

		switch (type)
		{
		case EImpltemType::Event:
		case EImpltemType::Snapshot:
			{
				pNode->setAttr(g_nameAttribute, GetPathName(pImplControl));
				auto const pEventConnection = static_cast<const CEventConnection*>(pConnection.get());

				if ((pEventConnection != nullptr) && (pEventConnection->m_type == EEventType::Stop))
				{
					pNode->setAttr(g_eventTypeAttribute, "stop");
				}
			}
			break;
		case EImpltemType::Return:
			{
				pNode->setAttr(g_nameAttribute, GetPathName(pImplControl));
			}
			break;
		case EImpltemType::EventParameter:
		case EImpltemType::SnapshotParameter:
			{
				pNode->setAttr(g_nameAttribute, pImplControl->GetName());
				pNode->setAttr(g_pathAttribute, GetPathName(pImplControl->GetParent()));

				if (controlType == ESystemItemType::State)
				{
					auto const pStateConnection = static_cast<const CParamToStateConnection*>(pConnection.get());

					if (pStateConnection != nullptr)
					{
						pNode->setAttr(g_valueAttribute, pStateConnection->m_value);
					}
				}
				else if (controlType == ESystemItemType::Parameter)
				{
					auto const pParamConnection = static_cast<const CParamConnection*>(pConnection.get());

					if (pParamConnection->m_mult != 1.0f)
					{
						pNode->setAttr(g_multAttribute, pParamConnection->m_mult);
					}

					if (pParamConnection->m_shift != 0.0f)
					{
						pNode->setAttr(g_shiftAttribute, pParamConnection->m_shift);
					}
				}
			}
			break;
		case EImpltemType::Bank:
			{
				pNode->setAttr(g_nameAttribute, GetPathName(pImplControl));
			}
			break;
		}

		return pNode;
	}

	return nullptr;
}

//////////////////////////////////////////////////////////////////////////
void CEditorImpl::EnableConnection(ConnectionPtr const pConnection)
{
	CImplItem* const pImplControl = GetControl(pConnection->GetID());

	if (pImplControl != nullptr)
	{
		++m_connectionsByID[pImplControl->GetId()];
		pImplControl->SetConnected(true);
	}
}

//////////////////////////////////////////////////////////////////////////
void CEditorImpl::DisableConnection(ConnectionPtr const pConnection)
{
	CImplItem* const pImplControl = GetControl(pConnection->GetID());

	if (pImplControl != nullptr)
	{
		int connectionCount = m_connectionsByID[pImplControl->GetId()] - 1;

		if (connectionCount <= 0)
		{
			connectionCount = 0;
			pImplControl->SetConnected(false);
		}

		m_connectionsByID[pImplControl->GetId()] = connectionCount;
	}
}

//////////////////////////////////////////////////////////////////////////
void CEditorImpl::Clear()
{
	// Delete all the controls
	for (auto const& controlPair : m_controlsCache)
	{
		CImplItem const* const pImplControl = controlPair.second;

		if (pImplControl != nullptr)
		{
			delete pImplControl;
		}
	}

	m_controlsCache.clear();

	// Clean up the root control
	m_rootControl = CImplItem();
}

//////////////////////////////////////////////////////////////////////////
void CEditorImpl::CreateControlCache(CImplItem const* const pParent)
{
	if (pParent != nullptr)
	{
		size_t const count = pParent->ChildCount();

		for (size_t i = 0; i < count; ++i)
		{
			CImplItem* const pChild = pParent->GetChildAt(i);

			if (pChild != nullptr)
			{
				m_controlsCache[pChild->GetId()] = pChild;
				CreateControlCache(pChild);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
CImplItem* CEditorImpl::CreateItem(EImpltemType const type, CImplItem* const pParent, string const& name)
{
	CID const id = GetId(type, name, pParent);

	CImplItem* pImplItem = GetControl(id);

	if (pImplItem != nullptr)
	{
		if (pImplItem->IsPlaceholder())
		{
			pImplItem->SetPlaceholder(false);
			CImplItem* pParentItem = pParent;

			while (pParentItem != nullptr)
			{
				pParentItem->SetPlaceholder(false);
				pParentItem = pParentItem->GetParent();
			}
		}
	}
	else
	{
		if (type == EImpltemType::Folder)
		{
			pImplItem = new CImplFolder(name, id);
		}
		else if (type == EImpltemType::Group)
		{
			pImplItem = new CImplGroup(name, id);
		}
		else
		{
			pImplItem = new CImplItem(name, id, static_cast<ItemType>(type));
		}
		if (pParent != nullptr)
		{
			pParent->AddChild(pImplItem);
			pImplItem->SetParent(pParent);
		}
		else
		{
			m_rootControl.AddChild(pImplItem);
			pImplItem->SetParent(&m_rootControl);
		}

		m_controlsCache[id] = pImplItem;

	}

	return pImplItem;
}

//////////////////////////////////////////////////////////////////////////
CID CEditorImpl::GetId(EImpltemType const type, string const& name, CImplItem* const pParent) const
{
	string const fullname = GetTypeName(type) + GetPathName(pParent) + CRY_NATIVE_PATH_SEPSTR + name;
	return CCrc32::Compute(fullname);
}

//////////////////////////////////////////////////////////////////////////
string CEditorImpl::GetPathName(CImplItem const* const pImplItem) const
{
	if (pImplItem != nullptr)
	{
		string fullname = pImplItem->GetName();
		CImplItem const* pParent = pImplItem->GetParent();

		while ((pParent != nullptr) && (pParent != &m_rootControl))
		{
			// The id needs to represent the full path, as we can have items with the same name in different folders
			fullname = pParent->GetName() + "/" + fullname;
			pParent = pParent->GetParent();
		}

		return fullname;
	}

	return "";
}

//////////////////////////////////////////////////////////////////////////
string CEditorImpl::GetTypeName(EImpltemType const type) const
{
	switch (type)
	{
	case EImpltemType::Folder:
		return "folder:";
	case EImpltemType::Event:
		return "event:";
	case EImpltemType::EventParameter:
		return "eventparameter:";
	case EImpltemType::Snapshot:
		return "snapshot:";
	case EImpltemType::SnapshotParameter:
		return "snapshotparameter:";
	case EImpltemType::Bank:
		return "bank:";
	case EImpltemType::Return:
		return "return:";
	case EImpltemType::Group:
		return "group:";
	}
	return "";
}

//////////////////////////////////////////////////////////////////////////
CImplItem* CEditorImpl::GetItemFromPath(string const& fullpath)
{
	CImplItem* pImplItem = &m_rootControl;
	int start = 0;
	string token = fullpath.Tokenize("/", start);

	while (!token.empty())
	{
		token.Trim();
		CImplItem* pChild = nullptr;
		int const size = pImplItem->ChildCount();

		for (int i = 0; i < size; ++i)
		{
			CImplItem* const pNextChild = pImplItem->GetChildAt(i);

			if ((pNextChild != nullptr) && (pNextChild->GetName() == token))
			{
				pChild = pNextChild;
				break;
			}
		}

		if (pChild == nullptr)
		{
			return nullptr;
		}

		pImplItem = pChild;
		token = fullpath.Tokenize("/", start);
	}

	return pImplItem;
}

//////////////////////////////////////////////////////////////////////////
CImplItem* CEditorImpl::CreatePlaceholderFolderPath(string const& path)
{
	CImplItem* pImplItem = &m_rootControl;
	int start = 0;
	string token = path.Tokenize("/", start);

	while (!token.empty())
	{
		token.Trim();
		CImplItem* pFoundChild = nullptr;
		int const size = pImplItem->ChildCount();

		for (int i = 0; i < size; ++i)
		{
			CImplItem* const pChild = pImplItem->GetChildAt(i);

			if ((pChild != nullptr) && (pChild->GetName() == token))
			{
				pFoundChild = pChild;
				break;
			}
		}

		if (pFoundChild == nullptr)
		{
			pFoundChild = CreateItem(EImpltemType::Folder, pImplItem, token);
			pFoundChild->SetPlaceholder(true);
		}

		pImplItem = pFoundChild;
		token = path.Tokenize("/", start);
	}

	return pImplItem;
}
} // namespace Fmod
} // namespace ACE
