// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AudioSystemEditor_fmod.h"
#include <CryAudio/IAudioSystem.h>
#include <CryAudio/IProfileData.h>
#include <CrySystem/File/CryFile.h>
#include <CrySystem/ISystem.h>
#include <CryString/CryPath.h>
#include <CryCore/StlUtils.h>
#include <CrySerialization/IArchiveHost.h>
#include <CrySerialization/Enum.h>

namespace ACE
{

const string g_userSettingsFile = "%USER%/audiocontrolseditor_fmod.user";

// Paths
const string g_foldersPath = "/metadata/eventfolder/";
const string g_groupsPath = "/metadata/group/";
const string g_eventsPath = "/metadata/event/";
const string g_snapshotsPath = "/metadata/snapshot/";
const string g_returnsPath = "/metadata/return/";

// XML tags
const string g_eventTag = "FmodEvent";
const string g_eventParameterTag = "FmodEventParameter";
const string g_snapshotTag = "FmodSnapshot";
const string g_snapshotParameterTag = "FmodSnapshotParameter";
const string g_bankTag = "FmodFile";
const string g_returnTag = "FmodBus";
const string g_nameAttribute = "fmod_name";
const string g_pathAttribute = "fmod_path";
const string g_valueAttribute = "fmod_value";
const string g_multAttribute = "fmod_value_multiplier";
const string g_shiftAttribute = "fmod_value_shift";
const string g_eventTypeAttribute = "fmod_event_type";

EFmodItemType TagToType(const string& tag)
{
	if (tag == g_eventTag)
	{
		return eFmodItemType_Event;
	}
	else if (tag == g_eventParameterTag)
	{
		return eFmodItemType_EventParameter;
	}
	else if (tag == g_snapshotTag)
	{
		return eFmodItemType_Snapshot;
	}
	else if (tag == g_snapshotParameterTag)
	{
		return eFmodItemType_SnapshotParameter;
	}
	else if (tag == g_bankTag)
	{
		return eFmodItemType_Bank;
	}
	else if (tag == g_returnTag)
	{
		return eFmodItemType_Return;
	}

	return eFmodItemType_Invalid;
}

string TypeToTag(ItemType type)
{
	switch (type)
	{
	case eFmodItemType_Event:
		return g_eventTag;
	case eFmodItemType_EventParameter:
		return g_eventParameterTag;
	case eFmodItemType_Snapshot:
		return g_snapshotTag;
	case eFmodItemType_SnapshotParameter:
		return g_snapshotParameterTag;
	case eFmodItemType_Bank:
		return g_bankTag;
	case eFmodItemType_Return:
		return g_returnTag;
	}
	return "";
}

enum EFmodEventType
{
	eFmodEventType_Start = 0,
	eFmodEventType_Stop,
};

class CEventConnection : public IAudioConnection
{
public:
	explicit CEventConnection(CID nID) : IAudioConnection(nID) {}

	virtual bool HasProperties() override
	{
		return true;
	}

	virtual void Serialize(Serialization::IArchive& ar) override
	{
		ar(type, "action", "Action");
		if (ar.isInput())
		{
			signalConnectionChanged();
		}
	}

	EFmodEventType type = eFmodEventType_Start;
};

class CParamConnection : public IAudioConnection
{
public:
	explicit CParamConnection(CID nID) : IAudioConnection(nID) {}

	virtual bool HasProperties() override { return true; }

	virtual void Serialize(Serialization::IArchive& ar) override
	{
		ar(mult, "mult", "Multiply");
		ar(shift, "shift", "Shift");
		if (ar.isInput())
		{
			signalConnectionChanged();
		}
	}

	float mult = 1.0f;
	float shift = 0.0f;
};

class CParamToStateConnection : public IAudioConnection
{
public:
	explicit CParamToStateConnection(CID id) : IAudioConnection(id) {}

	virtual bool HasProperties() override
	{
		return true;
	}

	virtual void Serialize(Serialization::IArchive& ar) override
	{
		ar(m_value, "value", "Value");
		if (ar.isInput())
		{
			signalConnectionChanged();
		}
	}

	float m_value = 1.0f;
};

CAudioSystemEditor_fmod::CAudioSystemEditor_fmod()
	: m_root("", ACE_INVALID_ID, AUDIO_SYSTEM_INVALID_TYPE)
{
	Serialization::LoadJsonFile(m_settings, g_userSettingsFile.c_str());
	Reload();
}

CAudioSystemEditor_fmod::~CAudioSystemEditor_fmod()
{
	m_containerIdMap.clear();
	std::for_each(m_items.begin(), m_items.end(), stl::container_object_deleter());
}

void CAudioSystemEditor_fmod::Reload(bool bPreserveConnectionStatus)
{
	// set all the controls as placeholder as we don't know if
	// any of them have been removed but still have connections to them
	for (IAudioSystemItem* pItem : m_items)
	{
		if (pItem)
		{
			pItem->SetPlaceholder(true);
			pItem->SetConnected(false);
		}
	}

	// Load banks
	{
		_finddata_t fd;
		ICryPak* pCryPak = gEnv->pCryPak;
		string path = m_settings.GetSoundBanksPath();
		intptr_t handle = pCryPak->FindFirst(path + "/*.bank", &fd);
		if (handle != -1)
		{
			// We have to exclude the Master Bank, for this we look
			// for the file that ends with "strings.bank" as it is guaranteed
			// to have the same name as the Master Bank and there should be unique
			std::vector<string> banks;
			string masterBankName = "";
			do
			{
				string filename = fd.name;
				if (filename != "." && filename != ".." && !filename.empty())
				{
					int pos = filename.rfind(".strings.bank");
					if (pos != string::npos)
					{
						masterBankName = filename.substr(0, pos);
					}
					else
					{
						banks.push_back(filename);
					}
				}
			}
			while (pCryPak->FindNext(handle, &fd) >= 0);

			for (string& filename : banks)
			{
				if (filename.compare(0, masterBankName.length(), masterBankName) != 0)
				{
					CreateItem(eFmodItemType_Bank, nullptr, filename);
				}
			}
			pCryPak->FindClose(handle);
		}
	}

	ParseFolder(m_settings.GetProjectPath() + g_foldersPath);    // folders
	ParseFolder(m_settings.GetProjectPath() + g_groupsPath);     // groups
	ParseFolder(m_settings.GetProjectPath() + g_eventsPath);     // events
	ParseFolder(m_settings.GetProjectPath() + g_snapshotsPath);  // snapshots
	ParseFolder(m_settings.GetProjectPath() + g_returnsPath);    // returns

	if (bPreserveConnectionStatus)
	{
		for (auto connection : m_connectionsByID)
		{
			if (connection.second > 0)
			{
				IAudioSystemItem* pControl = GetControl(connection.first);
				if (pControl)
				{
					pControl->SetConnected(true);
				}
			}
		}
	}
	else
	{
		m_connectionsByID.clear();
	}
}

IAudioSystemItem* CAudioSystemEditor_fmod::CreateItem(EFmodItemType type, IAudioSystemItem* pParent, const string& name)
{
	CID id = GetId(type, name, pParent);

	IAudioSystemItem* pItem = GetControl(id);
	if (pItem)
	{
		if (pItem->IsPlaceholder())
		{
			pItem->SetPlaceholder(false);
			IAudioSystemItem* pParentItem = pParent;
			while (pParentItem)
			{
				pParentItem->SetPlaceholder(false);
				pParentItem = pParentItem->GetParent();
			}
		}
	}
	else
	{
		if (type == eFmodItemType_Folder)
		{
			pItem = new CFmodFolder(name, id);
		}
		else if (type == eFmodItemType_Group)
		{
			pItem = new CFmodGroup(name, id);
		}
		else
		{
			pItem = new IAudioSystemItem(name, id, type);
		}
		if (pParent)
		{
			pParent->AddChild(pItem);
			pItem->SetParent(pParent);
		}
		else
		{
			m_root.AddChild(pItem);
			pItem->SetParent(&m_root);
		}
		m_items.push_back(pItem);

	}
	return pItem;
}

IAudioSystemItem* CAudioSystemEditor_fmod::GetControl(CID id) const
{
	for (IAudioSystemItem* pItem : m_items)
	{
		ItemType type = pItem->GetType();
		if (pItem->GetId() == id)
		{
			return pItem;
		}
	}
	return nullptr;
}

ConnectionPtr CAudioSystemEditor_fmod::CreateConnectionToControl(EItemType controlType, IAudioSystemItem* pMiddlewareControl)
{
	if (pMiddlewareControl)
	{
		ItemType type = pMiddlewareControl->GetType();
		if (type == eFmodItemType_Event || type == eFmodItemType_Snapshot)
		{
			return std::make_shared<CEventConnection>(pMiddlewareControl->GetId());
		}
		else if (type == eFmodItemType_EventParameter || type == eFmodItemType_SnapshotParameter)
		{
			if (controlType == eItemType_RTPC)
			{
				return std::make_shared<CParamConnection>(pMiddlewareControl->GetId());
			}
			else if (controlType == eItemType_State)
			{
				return std::make_shared<CParamToStateConnection>(pMiddlewareControl->GetId());
			}
		}
		return std::make_shared<IAudioConnection>(pMiddlewareControl->GetId());
	}
	return nullptr;
}

ConnectionPtr CAudioSystemEditor_fmod::CreateConnectionFromXMLNode(XmlNodeRef pNode, EItemType controlType)
{
	if (pNode)
	{
		EFmodItemType type = TagToType(pNode->getTag());
		if (type != eFmodItemType_Invalid)
		{

			string name = pNode->getAttr(g_nameAttribute);
			string path = pNode->getAttr(g_pathAttribute);
			if (!path.empty())
			{
				name = path + "/" + name;
			}

			IAudioSystemItem* pItem = GetItemFromPath(name);

			if (!pItem || pItem->GetType() != type)
			{
				// If control not found, create a placeholder.
				// We want to keep that connection even if it's not in the middleware as the user could
				// be using the engine without the fmod project

				path = "";
				int pos = name.find_last_of("/");
				if (pos != string::npos)
				{
					path = name.substr(0, pos);
					name = name.substr(pos + 1, name.length() - pos);
				}

				pItem = CreateItem(type, CreatePlaceholderFolderPath(path), name);
				if (pItem)
				{
					pItem->SetPlaceholder(true);
				}
			}

			switch (type)
			{
			case eFmodItemType_Event:
			case eFmodItemType_Snapshot:
				{
					string eventType = pNode->getAttr(g_eventTypeAttribute);
					auto pConnection = std::make_shared<CEventConnection>(pItem->GetId());
					pConnection->type = (eventType == "stop") ? eFmodEventType_Stop : eFmodEventType_Start;
					return pConnection;
				}
				break;
			case eFmodItemType_EventParameter:
			case eFmodItemType_SnapshotParameter:
				{
					if (controlType == eItemType_RTPC)
					{
						std::shared_ptr<CParamConnection> pConnection = std::make_shared<CParamConnection>(pItem->GetId());
						float mult = 1.0f;
						float shift = 0.0f;
						if (pNode->haveAttr(g_multAttribute))
						{
							const string value = pNode->getAttr(g_multAttribute);
							mult = (float)std::atof(value.c_str());
						}
						if (pNode->haveAttr(g_shiftAttribute))
						{
							const string value = pNode->getAttr(g_shiftAttribute);
							shift = (float)std::atof(value.c_str());
						}
						pConnection->mult = mult;
						pConnection->shift = shift;
						return pConnection;
					}
					else if (controlType == eItemType_State)
					{
						std::shared_ptr<CParamToStateConnection> pConnection = std::make_shared<CParamToStateConnection>(pItem->GetId());
						string value = pNode->getAttr(g_valueAttribute);
						pConnection->m_value = (float)std::atof(value.c_str());
						return pConnection;
					}
				}
				break;
			case eFmodItemType_Bank:
			case eFmodItemType_Return:
				{
					return std::make_shared<IAudioConnection>(pItem->GetId());
				}
				break;
			}
		}
	}
	return nullptr;
}

XmlNodeRef CAudioSystemEditor_fmod::CreateXMLNodeFromConnection(const ConnectionPtr pConnection, const EItemType controlType)
{
	IAudioSystemItem* pItem = GetControl(pConnection->GetID());
	if (pItem)
	{
		XmlNodeRef pNode = GetISystem()->CreateXmlNode(TypeToTag(pItem->GetType()));

		switch (pItem->GetType())
		{
		case eFmodItemType_Event:
		case eFmodItemType_Snapshot:
			{
				pNode->setAttr(g_nameAttribute, GetFullPathName(pItem));
				auto pEventConnection = static_cast<const CEventConnection*>(pConnection.get());
				if (pEventConnection && pEventConnection->type == eFmodEventType_Stop)
				{
					pNode->setAttr(g_eventTypeAttribute, "stop");
				}
			}
			break;
		case eFmodItemType_Return:
			{
				pNode->setAttr(g_nameAttribute, GetFullPathName(pItem));
			}
			break;
		case eFmodItemType_EventParameter:
		case eFmodItemType_SnapshotParameter:
			{
				pNode->setAttr(g_nameAttribute, pItem->GetName());
				pNode->setAttr(g_pathAttribute, GetFullPathName(pItem->GetParent()));
				if (controlType == eItemType_State)
				{
					auto pRtpcConnection = static_cast<const CParamToStateConnection*>(pConnection.get());
					if (pRtpcConnection)
					{
						pNode->setAttr(g_valueAttribute, pRtpcConnection->m_value);
					}
				}
				else if (controlType == eItemType_RTPC)
				{
					auto pParamConnection = static_cast<const CParamConnection*>(pConnection.get());
					if (pParamConnection->mult != 1.0f)
					{
						pNode->setAttr(g_multAttribute, pParamConnection->mult);
					}
					if (pParamConnection->shift != 0.0f)
					{
						pNode->setAttr(g_shiftAttribute, pParamConnection->shift);
					}
				}
			}
			break;
		case eFmodItemType_Bank:
			{
				pNode->setAttr(g_nameAttribute, GetFullPathName(pItem));
			}
			break;
		}
		return pNode;
	}
	return nullptr;
}

const char* CAudioSystemEditor_fmod::GetTypeIcon(ItemType type) const
{
	switch ((EFmodItemType)type)
	{
	case eFmodItemType_Folder:
		return "Editor/Icons/audio/fmod/folder_closed.png";
	case eFmodItemType_Event:
		return "Editor/Icons/audio/fmod/event.png";
	case eFmodItemType_EventParameter:
		return "Editor/Icons/audio/fmod/tag_icon.png";
	case eFmodItemType_Snapshot:
		return "Editor/Icons/audio/fmod/snapshot.png";
	case eFmodItemType_SnapshotParameter:
		return "Editor/Icons/audio/fmod/tag_icon.png";
	case eFmodItemType_Bank:
		return "Editor/Icons/audio/fmod/bank.png";
	case eFmodItemType_Return:
		return "Editor/Icons/audio/fmod/return.png";
	case eFmodItemType_Group:
		return "Editor/Icons/audio/fmod/group.png";
	}
	return "";
}

ACE::EItemType CAudioSystemEditor_fmod::ImplTypeToATLType(ItemType type) const
{
	switch (type)
	{
	case eFmodItemType_Event:
		return eItemType_Trigger;
	case eFmodItemType_EventParameter:
		return eItemType_RTPC;
	case eFmodItemType_Snapshot:
		return eItemType_Trigger;
	case eFmodItemType_SnapshotParameter:
		return eItemType_RTPC;
	case eFmodItemType_Bank:
		return eItemType_Preload;
	case eFmodItemType_Return:
		return eItemType_Environment;
	}
	return eItemType_Invalid;
}

ACE::TImplControlTypeMask CAudioSystemEditor_fmod::GetCompatibleTypes(EItemType controlType) const
{
	switch (controlType)
	{
	case eItemType_Trigger:
		return eFmodItemType_Event | eFmodItemType_Snapshot;
		break;
	case eItemType_RTPC:
		return eFmodItemType_EventParameter | eFmodItemType_SnapshotParameter;
		break;
	case eItemType_Preload:
		return eFmodItemType_Bank;
		break;
	case eItemType_State:
		return eFmodItemType_EventParameter | eFmodItemType_SnapshotParameter;
		break;
	case eItemType_Environment:
		return eFmodItemType_Return;
		break;
	}
	return AUDIO_SYSTEM_INVALID_TYPE;
}

string CAudioSystemEditor_fmod::GetName() const
{
	return gEnv->pAudioSystem->GetProfileData()->GetImplName();
}

void CAudioSystemEditor_fmod::EnableConnection(ConnectionPtr pConnection)
{
	IAudioSystemItem* pControl = GetControl(pConnection->GetID());
	if (pControl)
	{
		++m_connectionsByID[pControl->GetId()];
		pControl->SetConnected(true);
	}
}

void CAudioSystemEditor_fmod::DisableConnection(ConnectionPtr pConnection)
{
	IAudioSystemItem* pControl = GetControl(pConnection->GetID());
	if (pControl)
	{
		int nConnectionCount = m_connectionsByID[pControl->GetId()] - 1;
		if (nConnectionCount <= 0)
		{
			nConnectionCount = 0;
			pControl->SetConnected(false);
		}
		m_connectionsByID[pControl->GetId()] = nConnectionCount;
	}
}

CID CAudioSystemEditor_fmod::GetId(EFmodItemType type, const string& name, IAudioSystemItem* pParent) const
{
	string fullname = GetTypeName(type) + GetFullPathName(pParent) + CRY_NATIVE_PATH_SEPSTR + name;
	return CCrc32::Compute(fullname);
}

string CAudioSystemEditor_fmod::GetFullPathName(IAudioSystemItem* pItem) const
{
	if (pItem)
	{
		string fullname = pItem->GetName();
		IAudioSystemItem* pParent = pItem->GetParent();
		while (pParent && pParent != &m_root)
		{
			// The id needs to represent the full path, as we can have items with the same name in different folders
			fullname = pParent->GetName() + "/" + fullname;
			pParent = pParent->GetParent();
		}
		return fullname;
	}
	return "";
}

IAudioSystemItem* CAudioSystemEditor_fmod::LoadContainer(XmlNodeRef pNode, EFmodItemType type, const string& relationshipParamName)
{
	if (pNode)
	{
		string containerName = "";
		IAudioSystemItem* pParent = nullptr;

		const int size = pNode->getChildCount();
		for (int i = 0; i < size; ++i)
		{
			XmlNodeRef pChild = pNode->getChild(i);
			const string name = pChild->getAttr("name");
			if (name == "name")
			{
				// Get the container name
				XmlNodeRef pContainerNameNode = pChild->getChild(0);
				if (pContainerNameNode)
				{
					containerName = pContainerNameNode->getContent();
				}
			}
			else if (name == relationshipParamName)
			{
				// Get the container parent
				XmlNodeRef pParentContainerNode = pChild->getChild(0);
				if (pParentContainerNode)
				{
					string parentContainerId = pParentContainerNode->getContent();
					pParent = GetContainer(parentContainerId, type);
				}
			}
		}

		IAudioSystemItem* pContainer = CreateItem(type, pParent, containerName);
		m_containerIdMap[pNode->getAttr("id")] = pContainer;
		return pContainer;
	}
	return nullptr;
}

IAudioSystemItem* CAudioSystemEditor_fmod::LoadFolder(XmlNodeRef pNode)
{
	return LoadContainer(pNode, eFmodItemType_Folder, "folder");
}

IAudioSystemItem* CAudioSystemEditor_fmod::LoadGroup(XmlNodeRef pNode)
{
	return LoadContainer(pNode, eFmodItemType_Group, "output");
}

IAudioSystemItem* CAudioSystemEditor_fmod::LoadItem(XmlNodeRef pNode, EFmodItemType type)
{
	if (pNode)
	{
		string name = "";
		IAudioSystemItem* pParent = nullptr;
		const int size = pNode->getChildCount();
		for (int i = 0; i < size; ++i)
		{
			XmlNodeRef pChild = pNode->getChild(i);
			if (pChild)
			{
				const string tag = pChild->getTag();
				if (tag == "property")
				{
					const string paramName = pChild->getAttr("name");
					if (paramName == "name")
					{
						XmlNodeRef pValue = pChild->getChild(0);
						if (pValue)
						{
							name = pValue->getContent();
						}
					}
				}
				else if (tag == "relationship")
				{
					const string name = pChild->getAttr("name");
					if (name == "folder" || name == "output")
					{
						XmlNodeRef pValue = pChild->getChild(0);
						if (pValue)
						{
							string parentContainerId = pValue->getContent();
							auto folder = m_containerIdMap.find(parentContainerId);
							if (folder != m_containerIdMap.end())
							{
								pParent = (*folder).second;
							}
						}
					}
				}
			}
		}
		return CreateItem(type, pParent, name);
	}
	return nullptr;
}

IAudioSystemItem* CAudioSystemEditor_fmod::LoadEvent(XmlNodeRef pNode)
{
	return LoadItem(pNode, eFmodItemType_Event);
}

IAudioSystemItem* CAudioSystemEditor_fmod::LoadSnapshot(XmlNodeRef pNode)
{
	return LoadItem(pNode, eFmodItemType_Snapshot);
}

IAudioSystemItem* CAudioSystemEditor_fmod::LoadReturn(XmlNodeRef pNode)
{
	return LoadItem(pNode, eFmodItemType_Return);
}

IAudioSystemItem* CAudioSystemEditor_fmod::LoadParameter(XmlNodeRef pNode, EFmodItemType type, IAudioSystemItem& parentEvent)
{
	if (pNode)
	{
		const int size = pNode->getChildCount();
		for (int i = 0; i < size; ++i)
		{
			XmlNodeRef pChild = pNode->getChild(i);
			if (pChild)
			{
				const string name = pChild->getAttr("name");
				if (name == "name")
				{
					XmlNodeRef pValue = pChild->getChild(0);
					if (pValue)
					{
						string value = pValue->getContent();
						IAudioSystemItem* pItem = CreateItem(type, &parentEvent, value);
						return pItem;
					}
				}
			}
		}
	}
	return nullptr;
}

IAudioSystemItem* CAudioSystemEditor_fmod::GetItemFromPath(const string& fullpath)
{
	IAudioSystemItem* pItem = &m_root;
	int start = 0;
	string token = fullpath.Tokenize("/", start);
	while (!token.empty())
	{
		token.Trim();
		IAudioSystemItem* pChild = nullptr;
		const int size = pItem->ChildCount();
		for (int i = 0; i < size; ++i)
		{
			IAudioSystemItem* pNextChild = pItem->GetChildAt(i);
			if (pNextChild && pNextChild->GetName() == token)
			{
				pChild = pNextChild;
				break;
			}
		}
		if (pChild == nullptr)
		{
			return nullptr;
		}
		pItem = pChild;
		token = fullpath.Tokenize("/", start);
	}
	return pItem;
}

IAudioSystemItem* CAudioSystemEditor_fmod::CreatePlaceholderFolderPath(const string& path)
{
	IAudioSystemItem* pItem = &m_root;
	int start = 0;
	string token = path.Tokenize("/", start);
	while (!token.empty())
	{
		token.Trim();
		IAudioSystemItem* pFoundChild = nullptr;
		const int size = pItem->ChildCount();
		for (int i = 0; i < size; ++i)
		{
			IAudioSystemItem* pChild = pItem->GetChildAt(i);
			if (pChild && pChild->GetName() == token)
			{
				pFoundChild = pChild;
				break;
			}
		}
		if (!pFoundChild)
		{
			pFoundChild = CreateItem(eFmodItemType_Folder, pItem, token);
			pFoundChild->SetPlaceholder(true);
		}
		pItem = pFoundChild;
		token = path.Tokenize("/", start);
	}
	return pItem;
}

void CAudioSystemEditor_fmod::ParseFolder(const string& folderPath)
{
	_finddata_t fd;
	ICryPak* pCryPak = gEnv->pCryPak;
	intptr_t handle = pCryPak->FindFirst(folderPath + "*.xml", &fd);
	if (handle != -1)
	{
		do
		{
			string filename = fd.name;
			if (filename != "." && filename != ".." && !filename.empty())
			{
				ParseFile(folderPath + filename);
			}
		}
		while (pCryPak->FindNext(handle, &fd) >= 0);
		pCryPak->FindClose(handle);
	}
}

void CAudioSystemEditor_fmod::ParseFile(const string& filepath)
{
	if (GetISystem()->GetIPak()->IsFileExist(filepath))
	{
		XmlNodeRef pRoot = GetISystem()->LoadXmlFromFile(filepath);
		if (pRoot)
		{
			IAudioSystemItem* pItem = nullptr;
			const int size = pRoot->getChildCount();
			for (int i = 0; i < size; ++i)
			{
				XmlNodeRef pChild = pRoot->getChild(i);
				if (pChild)
				{
					const string className = pChild->getAttr("class");
					if (className == "EventFolder")
					{
						pItem = LoadFolder(pChild);
					}
					else if (className == "Event")
					{
						pItem = LoadEvent(pChild);
					}
					else if (className == "Snapshot")
					{
						pItem = LoadSnapshot(pChild);
					}
					else if (className == "GameParameter")
					{
						if (pItem)
						{
							if (pItem->GetType() == eFmodItemType_Event)
							{
								LoadParameter(pChild, eFmodItemType_EventParameter, *pItem);
							}
							else
							{
								LoadParameter(pChild, eFmodItemType_SnapshotParameter, *pItem);
							}
						}
						else
						{
							CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "[Audio Controls Editor] [Fmod] Found GameParameter tag before Event in file %s", filepath.c_str());
						}
					}
					else if (className == "MixerReturn")
					{
						LoadReturn(pChild);
					}
					else if (className == "MixerGroup")
					{
						LoadGroup(pChild);
					}
				}
			}
		}
	}
}

IAudioSystemItem* CAudioSystemEditor_fmod::GetContainer(const string& id, EFmodItemType type)
{
	auto folder = m_containerIdMap.find(id);
	if (folder != m_containerIdMap.end())
	{
		return (*folder).second;
	}

	// If folder not found parse the file corresponding to it and try looking for it again
	if (type == eFmodItemType_Folder)
	{
		ParseFile(m_settings.GetProjectPath() + g_foldersPath + id + ".xml");
	}
	else if (type == eFmodItemType_Group)
	{
		ParseFile(m_settings.GetProjectPath() + g_groupsPath + id + ".xml");
	}

	folder = m_containerIdMap.find(id);
	if (folder != m_containerIdMap.end())
	{
		return (*folder).second;
	}

	return nullptr;
}

string CAudioSystemEditor_fmod::GetTypeName(EFmodItemType type) const
{
	switch (type)
	{
	case eFmodItemType_Folder:
		return "folder:";
	case eFmodItemType_Event:
		return "event:";
	case eFmodItemType_EventParameter:
		return "eventparameter:";
	case eFmodItemType_Snapshot:
		return "snapshot:";
	case eFmodItemType_SnapshotParameter:
		return "snapshotparameter:";
	case eFmodItemType_Bank:
		return "bank:";
	case eFmodItemType_Return:
		return "return:";
	case eFmodItemType_Group:
		return "group:";
	}
	return "";
}

void CImplementationSettings_fmod::SetProjectPath(const char* szPath)
{
	m_projectPath = szPath;
	Serialization::SaveJsonFile(g_userSettingsFile.c_str(), *this);
}

SERIALIZATION_ENUM_BEGIN(EFmodEventType, "Event Type")
SERIALIZATION_ENUM(eFmodEventType_Start, "start", "Start")
SERIALIZATION_ENUM(eFmodEventType_Stop, "stop", "Stop")
SERIALIZATION_ENUM_END()

}
