// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ProjectLoader.h"

#include "EditorImpl.h"

#include <IEditorImpl.h>
#include <ImplItem.h>

#include <CrySystem/File/CryFile.h>
#include <CrySystem/ISystem.h>
#include <CrySystem/ILocalizationManager.h>
#include <CryString/CryPath.h>

namespace ACE
{
namespace Fmod
{
// Paths
string const g_foldersPath = "/metadata/eventfolder/";
string const g_groupsPath = "/metadata/group/";
string const g_eventsPath = "/metadata/event/";
string const g_snapshotsPath = "/metadata/snapshot/";
string const g_returnsPath = "/metadata/return/";

//////////////////////////////////////////////////////////////////////////
CProjectLoader::CProjectLoader(string const& projectPath, string const& soundbanksPath, CImplItem& root)
	: m_root(root)
	, m_projectPath(projectPath)
{
	LoadBanks(soundbanksPath);

	ParseFolder(projectPath + g_foldersPath);    // folders
	ParseFolder(projectPath + g_groupsPath);     // groups
	ParseFolder(projectPath + g_eventsPath);     // events
	ParseFolder(projectPath + g_snapshotsPath);  // snapshots
	ParseFolder(projectPath + g_returnsPath);    // returns
}

//////////////////////////////////////////////////////////////////////////
void CProjectLoader::LoadBanks(string const& folderPath)
{
	_finddata_t fd;
	ICryPak* const pCryPak = gEnv->pCryPak;
	intptr_t const handle = pCryPak->FindFirst(folderPath + "/*.bank", &fd);

	if (handle != -1)
	{
		// We have to exclude the Master Bank, for this we look
		// for the file that ends with "strings.bank" as it is guaranteed
		// to have the same name as the Master Bank and there should be unique
		std::vector<string> banks;
		string masterBankName = "";

		do
		{
			string const filename = fd.name;

			if ((filename != ".") && (filename != "..") && !filename.empty())
			{
				int const pos = filename.rfind(".strings.bank");

				if (pos != string::npos)
				{
					masterBankName = filename.substr(0, pos);
				}
				else
				{
					banks.emplace_back(filename);
				}
			}
		}
		while (pCryPak->FindNext(handle, &fd) >= 0);

		for (string const& filename : banks)
		{
			if (filename.compare(0, masterBankName.length(), masterBankName) != 0)
			{
				CreateItem(EImpltemType::Bank, nullptr, filename);
			}
		}

		pCryPak->FindClose(handle);
	}
}

//////////////////////////////////////////////////////////////////////////
void CProjectLoader::ParseFolder(string const& folderPath)
{
	_finddata_t fd;
	ICryPak* const pCryPak = gEnv->pCryPak;
	intptr_t const handle = pCryPak->FindFirst(folderPath + "*.xml", &fd);

	if (handle != -1)
	{
		do
		{
			string const filename = fd.name;

			if ((filename != ".") && (filename != "..") && !filename.empty())
			{
				ParseFile(folderPath + filename);
			}
		}
		while (pCryPak->FindNext(handle, &fd) >= 0);

		pCryPak->FindClose(handle);
	}
}

//////////////////////////////////////////////////////////////////////////
void CProjectLoader::ParseFile(string const& filepath)
{
	if (GetISystem()->GetIPak()->IsFileExist(filepath))
	{
		XmlNodeRef const pRoot = GetISystem()->LoadXmlFromFile(filepath);
		if (pRoot != nullptr)
		{
			CImplItem* pImplItem = nullptr;
			int const size = pRoot->getChildCount();

			for (int i = 0; i < size; ++i)
			{
				XmlNodeRef const pChild = pRoot->getChild(i);

				if (pChild != nullptr)
				{
					string const className = pChild->getAttr("class");

					if (className == "EventFolder")
					{
						pImplItem = LoadFolder(pChild);
					}
					else if (className == "Event")
					{
						pImplItem = LoadEvent(pChild);
					}
					else if (className == "Snapshot")
					{
						pImplItem = LoadSnapshot(pChild);
					}
					else if (className == "GameParameter")
					{
						if (pImplItem != nullptr)
						{
							if (static_cast<EImpltemType>(pImplItem->GetType()) == EImpltemType::Event)
							{
								LoadParameter(pChild, EImpltemType::EventParameter, *pImplItem);
							}
							else
							{
								LoadParameter(pChild, EImpltemType::SnapshotParameter, *pImplItem);
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

//////////////////////////////////////////////////////////////////////////
CImplItem* CProjectLoader::GetContainer(string const& id, EImpltemType const type)
{
	auto folder = m_containerIdMap.find(id);

	if (folder != m_containerIdMap.end())
	{
		return (*folder).second;
	}

	// If folder not found parse the file corresponding to it and try looking for it again
	if (type == EImpltemType::Folder)
	{
		ParseFile(m_projectPath + g_foldersPath + id + ".xml");
	}
	else if (type == EImpltemType::Group)
	{
		ParseFile(m_projectPath + g_groupsPath + id + ".xml");
	}

	folder = m_containerIdMap.find(id);

	if (folder != m_containerIdMap.end())
	{
		return (*folder).second;
	}

	return nullptr;
}

//////////////////////////////////////////////////////////////////////////
CImplItem* CProjectLoader::LoadContainer(XmlNodeRef const pNode, EImpltemType const type, string const& relationshipParamName)
{
	if (pNode != nullptr)
	{
		string containerName = "";
		CImplItem* pParent = nullptr;

		int const size = pNode->getChildCount();

		for (int i = 0; i < size; ++i)
		{
			XmlNodeRef const pChild = pNode->getChild(i);
			string const name = pChild->getAttr("name");

			if (name == "name")
			{
				// Get the container name
				XmlNodeRef const pContainerNameNode = pChild->getChild(0);

				if (pContainerNameNode != nullptr)
				{
					containerName = pContainerNameNode->getContent();
				}
			}
			else if (name == relationshipParamName)
			{
				// Get the container parent
				XmlNodeRef const pParentContainerNode = pChild->getChild(0);
				if (pParentContainerNode != nullptr)
				{
					string const parentContainerId = pParentContainerNode->getContent();
					pParent = GetContainer(parentContainerId, type);
				}
			}
		}

		CImplItem* const pContainer = CreateItem(type, pParent, containerName);
		m_containerIdMap[pNode->getAttr("id")] = pContainer;
		return pContainer;
	}

	return nullptr;
}

//////////////////////////////////////////////////////////////////////////
CImplItem* CProjectLoader::LoadFolder(XmlNodeRef const pNode)
{
	return LoadContainer(pNode, EImpltemType::Folder, "folder");
}

//////////////////////////////////////////////////////////////////////////
CImplItem* CProjectLoader::LoadGroup(XmlNodeRef const pNode)
{
	return LoadContainer(pNode, EImpltemType::Group, "output");
}

//////////////////////////////////////////////////////////////////////////
CImplItem* CProjectLoader::LoadItem(XmlNodeRef const pNode, EImpltemType const type)
{
	if (pNode != nullptr)
	{
		string name = "";
		CImplItem* pParent = nullptr;
		int const size = pNode->getChildCount();

		for (int i = 0; i < size; ++i)
		{
			XmlNodeRef const pChild = pNode->getChild(i);

			if (pChild != nullptr)
			{
				string const tag = pChild->getTag();

				if (tag == "property")
				{
					string const paramName = pChild->getAttr("name");

					if (paramName == "name")
					{
						XmlNodeRef const pValue = pChild->getChild(0);

						if (pValue != nullptr)
						{
							name = pValue->getContent();
						}
					}
				}
				else if (tag == "relationship")
				{
					string const name = pChild->getAttr("name");

					if (name == "folder" || name == "output")
					{
						XmlNodeRef const pValue = pChild->getChild(0);

						if (pValue != nullptr)
						{
							string const parentContainerId = pValue->getContent();
							auto const folder = m_containerIdMap.find(parentContainerId);

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

//////////////////////////////////////////////////////////////////////////
CImplItem* CProjectLoader::LoadEvent(XmlNodeRef const pNode)
{
	return LoadItem(pNode, EImpltemType::Event);
}

//////////////////////////////////////////////////////////////////////////
CImplItem* CProjectLoader::LoadSnapshot(XmlNodeRef const pNode)
{
	return LoadItem(pNode, EImpltemType::Snapshot);
}

//////////////////////////////////////////////////////////////////////////
CImplItem* CProjectLoader::LoadReturn(XmlNodeRef const pNode)
{
	return LoadItem(pNode, EImpltemType::Return);
}

//////////////////////////////////////////////////////////////////////////
CImplItem* CProjectLoader::LoadParameter(XmlNodeRef const pNode, EImpltemType const type, CImplItem& parentEvent)
{
	if (pNode != nullptr)
	{
		int const size = pNode->getChildCount();

		for (int i = 0; i < size; ++i)
		{
			XmlNodeRef const pChild = pNode->getChild(i);

			if (pChild != nullptr)
			{
				string const name = pChild->getAttr("name");

				if (name == "name")
				{
					XmlNodeRef const pValue = pChild->getChild(0);

					if (pValue != nullptr)
					{
						string const value = pValue->getContent();
						CImplItem* const pImplItem = CreateItem(type, &parentEvent, value);
						return pImplItem;
					}
				}
			}
		}
	}

	return nullptr;
}

//////////////////////////////////////////////////////////////////////////
CImplItem* CProjectLoader::CreateItem(EImpltemType const type, CImplItem* const pParent, string const& name)
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
			m_root.AddChild(pImplItem);
			pImplItem->SetParent(&m_root);
		}

		m_controlsCache[id] = pImplItem;

	}

	return pImplItem;
}

//////////////////////////////////////////////////////////////////////////
CID CProjectLoader::GetId(EImpltemType const type, string const& name, CImplItem* const pParent) const
{
	string const fullname = GetTypeName(type) + GetPathName(pParent) + CRY_NATIVE_PATH_SEPSTR + name;
	return CCrc32::Compute(fullname);
}

//////////////////////////////////////////////////////////////////////////
CImplItem* CProjectLoader::GetControl(CID const id) const
{
	for (auto const controlPair : m_controlsCache)
	{
		CImplItem* const pImplControl = controlPair.second;

		if (pImplControl->GetId() == id)
		{
			return pImplControl;
		}
	}

	return nullptr;
}

//////////////////////////////////////////////////////////////////////////
string CProjectLoader::GetPathName(CImplItem const* const pImplItem) const
{
	if (pImplItem != nullptr)
	{
		string fullname = pImplItem->GetName();
		CImplItem const* pParent = pImplItem->GetParent();

		while ((pParent != nullptr) && (pParent != &m_root))
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
string CProjectLoader::GetTypeName(EImpltemType const type) const
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
} // namespace Fmod
} // namespace ACE
