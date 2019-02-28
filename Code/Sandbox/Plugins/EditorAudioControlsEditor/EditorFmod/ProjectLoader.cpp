// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ProjectLoader.h"

#include "Impl.h"
#include "Utils.h"

#include <CrySystem/File/CryFile.h>
#include <CrySystem/ISystem.h>
#include <CrySystem/ILocalizationManager.h>

namespace ACE
{
namespace Impl
{
namespace Fmod
{
// Paths
string const g_eventFoldersPath = "/metadata/eventfolder/";
string const g_parametersFoldersPath = "/metadata/parameterpresetfolder/";
string const g_snapshotGroupsPath = "/metadata/snapshotgroup/";
string const g_mixerGroupsPath = "/metadata/group/";
string const g_eventsPath = "/metadata/event/";
string const g_parametersPath = "/metadata/parameterpreset/";
string const g_snapshotsPath = "/metadata/snapshot/";
string const g_returnsPath = "/metadata/return/";
string const g_vcasPath = "/metadata/vca/";

//////////////////////////////////////////////////////////////////////////
void AddNonStreamsBank(AssetNames& banks, string const& fileName)
{
	size_t const pos = fileName.rfind(".streams.bank");

	if (pos == string::npos)
	{
		banks.emplace_back(fileName);
	}
}

//////////////////////////////////////////////////////////////////////////
CProjectLoader::CProjectLoader(string const& projectPath, string const& soundbanksPath, CItem& rootItem, ItemCache& itemCache, CImpl const& impl)
	: m_rootItem(rootItem)
	, m_itemCache(itemCache)
	, m_projectPath(projectPath)
	, m_impl(impl)
{
	CItem* const pSoundBanksFolder = CreateItem(s_soundBanksFolderName, EItemType::EditorFolder, &m_rootItem);
	LoadBanks(soundbanksPath, false, *pSoundBanksFolder);

	CItem* const pEventsFolder = CreateItem(s_eventsFolderName, EItemType::EditorFolder, &m_rootItem);
	CItem* const pParametersFolder = CreateItem(s_parametersFolderName, EItemType::EditorFolder, &m_rootItem);
	CItem* const pSnapshotsFolder = CreateItem(s_snapshotsFolderName, EItemType::EditorFolder, &m_rootItem);
	CItem* const pReturnsFolder = CreateItem(s_returnsFolderName, EItemType::EditorFolder, &m_rootItem);
	CItem* const pVcasFolder = CreateItem(s_vcasFolderName, EItemType::EditorFolder, &m_rootItem);

	ParseFolder(projectPath + g_eventFoldersPath, *pEventsFolder, rootItem);          // Event folders
	ParseFolder(projectPath + g_parametersFoldersPath, *pParametersFolder, rootItem); // Parameter folders
	ParseFolder(projectPath + g_snapshotGroupsPath, *pSnapshotsFolder, rootItem);     // Snapshot groups
	ParseFolder(projectPath + g_mixerGroupsPath, *pReturnsFolder, rootItem);          // Mixer groups
	ParseFolder(projectPath + g_eventsPath, *pEventsFolder, rootItem);                // Events
	ParseFolder(projectPath + g_parametersPath, *pParametersFolder, rootItem);        // Parameters
	ParseFolder(projectPath + g_snapshotsPath, *pSnapshotsFolder, rootItem);          // Snapshots
	ParseFolder(projectPath + g_returnsPath, *pReturnsFolder, rootItem);              // Returns
	ParseFolder(projectPath + g_vcasPath, *pVcasFolder, rootItem);                    // VCAs

	RemoveEmptyMixerGroups();

	RemoveEmptyEditorFolders(pSoundBanksFolder);
	RemoveEmptyEditorFolders(pEventsFolder);
	RemoveEmptyEditorFolders(pParametersFolder);
	RemoveEmptyEditorFolders(pSnapshotsFolder);
	RemoveEmptyEditorFolders(pReturnsFolder);
	RemoveEmptyEditorFolders(pVcasFolder);
}

//////////////////////////////////////////////////////////////////////////
void CProjectLoader::LoadBanks(string const& folderPath, bool const isLocalized, CItem& parent)
{
	_finddata_t fd;
	ICryPak* const pCryPak = gEnv->pCryPak;
	intptr_t const handle = pCryPak->FindFirst(folderPath + "/*.bank", &fd);

	if (handle != -1)
	{
		// We have to exclude the Master Bank, for this we look
		// for the file that ends with "strings.bank" as it is guaranteed
		// to have the same name as the Master Bank and there should be unique
		AssetNames banks;
		string masterBankName = "";

		do
		{
			string const fileName = fd.name;

			if ((fileName != ".") && (fileName != "..") && !fileName.empty())
			{
				size_t const pos = fileName.rfind(".strings.bank");

				if (pos != string::npos)
				{
					masterBankName = fileName.substr(0, pos);
				}
				else
				{
					AddNonStreamsBank(banks, fileName);
				}
			}
		}
		while (pCryPak->FindNext(handle, &fd) >= 0);

		for (string const& bankName : banks)
		{
			if (bankName.compareNoCase(0, masterBankName.length(), masterBankName) != 0)
			{
				string const filePath = folderPath + "/" + bankName;
				EPakStatus const pakStatus = gEnv->pCryPak->IsFileExist(filePath.c_str(), ICryPak::eFileLocation_OnDisk) ? EPakStatus::OnDisk : EPakStatus::None;

				CreateItem(bankName, EItemType::Bank, &parent, pakStatus, filePath);
			}
		}

		pCryPak->FindClose(handle);
	}
}

//////////////////////////////////////////////////////////////////////////
void CProjectLoader::ParseFolder(string const& folderPath, CItem& editorFolder, CItem& parent)
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
				string const filePath = folderPath + filename;
				EPakStatus const pakStatus = gEnv->pCryPak->IsFileExist(filePath.c_str(), ICryPak::eFileLocation_OnDisk) ? EPakStatus::OnDisk : EPakStatus::None;

				ParseFile(filePath, editorFolder, pakStatus);
			}
		}
		while (pCryPak->FindNext(handle, &fd) >= 0);

		pCryPak->FindClose(handle);
	}
}

//////////////////////////////////////////////////////////////////////////
void CProjectLoader::ParseFile(string const& filepath, CItem& parent, EPakStatus const pakStatus)
{
	if (GetISystem()->GetIPak()->IsFileExist(filepath))
	{
		XmlNodeRef const pRoot = GetISystem()->LoadXmlFromFile(filepath);

		if (pRoot != nullptr)
		{
			CItem* pItem = nullptr;
			float maxDistance = 0.0f;
			int const size = pRoot->getChildCount();

			for (int i = 0; i < size; ++i)
			{
				XmlNodeRef const pChild = pRoot->getChild(i);

				if (pChild != nullptr)
				{
					char const* const szClassName = pChild->getAttr("class");

					if ((_stricmp(szClassName, "EventFolder") == 0) || (_stricmp(szClassName, "ParameterPresetFolder") == 0))
					{
						pItem = LoadFolder(pChild, parent, pakStatus);
					}
					else if (_stricmp(szClassName, "SnapshotGroup") == 0)
					{
						LoadSnapshotGroup(pChild, parent, pakStatus);
					}
					else if (_stricmp(szClassName, "Event") == 0)
					{
						pItem = LoadEvent(pChild, parent, pakStatus);
					}
					else if (_stricmp(szClassName, "Snapshot") == 0)
					{
						pItem = LoadSnapshot(pChild, parent, pakStatus);
					}
					else if (_stricmp(szClassName, "ParameterPreset") == 0)
					{
						pItem = LoadParameter(pChild, parent, pakStatus);
					}
					else if (_stricmp(szClassName, "MixerReturn") == 0)
					{
						LoadReturn(pChild, parent, pakStatus);
					}
					else if (_stricmp(szClassName, "MixerGroup") == 0)
					{
						LoadMixerGroup(pChild, parent, pakStatus);
					}
					else if (_stricmp(szClassName, "MixerVCA") == 0)
					{
						LoadVca(pChild, parent, pakStatus);
					}
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
CItem* CProjectLoader::GetContainer(string const& id, EItemType const type, CItem& parent, EPakStatus const pakStatus)
{
	CItem* pItem = &parent;
	auto folder = m_containerIds.find(id);

	if (folder != m_containerIds.end())
	{
		pItem = (*folder).second;
	}
	else
	{
		// If folder not found parse the file corresponding to it and try looking for it again
		if (type == EItemType::Folder)
		{
			ParseFile(m_projectPath + g_eventFoldersPath + id + ".xml", parent, pakStatus);
			ParseFile(m_projectPath + g_parametersFoldersPath + id + ".xml", parent, pakStatus);
		}
		else if (type == EItemType::MixerGroup)
		{
			ParseFile(m_projectPath + g_mixerGroupsPath + id + ".xml", parent, pakStatus);
		}

		folder = m_containerIds.find(id);

		if (folder != m_containerIds.end())
		{
			pItem = (*folder).second;
		}
	}

	return pItem;
}

//////////////////////////////////////////////////////////////////////////
CItem* CProjectLoader::LoadContainer(XmlNodeRef const pNode, EItemType const type, string const& relationshipParamName, CItem& parent, EPakStatus const pakStatus)
{
	CItem* pItem = nullptr;
	string name = "";
	CItem* pParent = &parent;
	int const size = pNode->getChildCount();

	for (int i = 0; i < size; ++i)
	{
		XmlNodeRef const pChild = pNode->getChild(i);
		string const attribName = pChild->getAttr("name");

		if (attribName.compareNoCase("name") == 0)
		{
			// Get the container name
			XmlNodeRef const pNameNode = pChild->getChild(0);

			if (pNameNode != nullptr)
			{
				name = pNameNode->getContent();
			}
		}
		else if (attribName.compareNoCase(relationshipParamName) == 0)
		{
			// Get the container parent
			XmlNodeRef const pParentContainerNode = pChild->getChild(0);

			if (pParentContainerNode != nullptr)
			{
				string const parentContainerId = pParentContainerNode->getContent();
				pParent = GetContainer(parentContainerId, type, parent, pakStatus);
			}
		}
	}

	CItem* const pContainer = CreateItem(name, type, pParent, pakStatus);
	m_containerIds[pNode->getAttr("id")] = pContainer;
	pItem = pContainer;

	return pItem;
}

//////////////////////////////////////////////////////////////////////////
CItem* CProjectLoader::LoadSnapshotGroup(XmlNodeRef const pNode, CItem& parent, EPakStatus const pakStatus)
{
	string name = "";
	AssetNames snapshotsItems;
	int const size = pNode->getChildCount();

	for (int i = 0; i < size; ++i)
	{
		XmlNodeRef const pChild = pNode->getChild(i);
		char const* const szAttribName = pChild->getAttr("name");

		if (_stricmp(szAttribName, "name") == 0)
		{
			XmlNodeRef const pNameNode = pChild->getChild(0);

			if (pNameNode != nullptr)
			{
				name = pNameNode->getContent();
			}
		}
		else if (_stricmp(szAttribName, "items") == 0)
		{
			int const itemCount = pChild->getChildCount();

			for (int j = 0; j < itemCount; ++j)
			{
				XmlNodeRef const itemNode = pChild->getChild(j);

				if (itemNode != nullptr)
				{
					snapshotsItems.emplace_back(itemNode->getContent());
				}
			}
		}
	}

	CItem* const pItem = CreateItem(name, EItemType::Folder, &parent, pakStatus);

	if (!snapshotsItems.empty())
	{
		for (auto const& snapshotId : snapshotsItems)
		{
			m_snapshotGroupItems[snapshotId] = pItem;
		}
	}

	return pItem;
}

//////////////////////////////////////////////////////////////////////////
CItem* CProjectLoader::LoadFolder(XmlNodeRef const pNode, CItem& parent, EPakStatus const pakStatus)
{
	return LoadContainer(pNode, EItemType::Folder, "folder", parent, pakStatus);
}

//////////////////////////////////////////////////////////////////////////
CItem* CProjectLoader::LoadMixerGroup(XmlNodeRef const pNode, CItem& parent, EPakStatus const pakStatus)
{
	return LoadContainer(pNode, EItemType::MixerGroup, "output", parent, pakStatus);
}

//////////////////////////////////////////////////////////////////////////
CItem* CProjectLoader::LoadItem(XmlNodeRef const pNode, EItemType const type, CItem& parent, EPakStatus const pakStatus)
{
	CItem* pItem = nullptr;
	string itemName = "";
	CItem* pParent = &parent;
	int const size = pNode->getChildCount();

	for (int i = 0; i < size; ++i)
	{
		XmlNodeRef const pChild = pNode->getChild(i);

		if (pChild != nullptr)
		{
			char const* const szTag = pChild->getTag();

			if (_stricmp(szTag, "property") == 0)
			{
				string const paramName = pChild->getAttr("name");

				if (paramName == "name")
				{
					XmlNodeRef const pValue = pChild->getChild(0);

					if (pValue != nullptr)
					{
						itemName = pValue->getContent();
					}
				}
			}
			else if (_stricmp(szTag, "relationship") == 0)
			{
				char const* const relationshipName = pChild->getAttr("name");

				if ((_stricmp(relationshipName, "folder") == 0) || (_stricmp(relationshipName, "output") == 0))
				{
					XmlNodeRef const pValue = pChild->getChild(0);

					if (pValue != nullptr)
					{
						string const parentContainerId = pValue->getContent();
						auto const folder = m_containerIds.find(parentContainerId);

						if (folder != m_containerIds.end())
						{
							pParent = (*folder).second;
						}
					}
				}
			}
		}
	}

	if (type == EItemType::Snapshot)
	{
		string const id = pNode->getAttr("id");

		auto const snapshotGroupPair = m_snapshotGroupItems.find(id);

		if (snapshotGroupPair != m_snapshotGroupItems.end())
		{
			pParent = (*snapshotGroupPair).second;
		}
	}

	pItem = CreateItem(itemName, type, pParent, pakStatus);

	return pItem;
}

//////////////////////////////////////////////////////////////////////////
CItem* CProjectLoader::LoadEvent(XmlNodeRef const pNode, CItem& parent, EPakStatus const pakStatus)
{
	return LoadItem(pNode, EItemType::Event, parent, pakStatus);
}

//////////////////////////////////////////////////////////////////////////
CItem* CProjectLoader::LoadSnapshot(XmlNodeRef const pNode, CItem& parent, EPakStatus const pakStatus)
{
	return LoadItem(pNode, EItemType::Snapshot, parent, pakStatus);
}

//////////////////////////////////////////////////////////////////////////
CItem* CProjectLoader::LoadReturn(XmlNodeRef const pNode, CItem& parent, EPakStatus const pakStatus)
{
	CItem* const pReturn = LoadItem(pNode, EItemType::Return, parent, pakStatus);

	if (pReturn != nullptr)
	{
		auto pParent = static_cast<CItem*>(pReturn->GetParent());
		EItemType const mixerGroupType = EItemType::MixerGroup;

		while ((pParent != nullptr) && (pParent->GetType() == mixerGroupType))
		{
			m_emptyMixerGroups.erase(std::remove(m_emptyMixerGroups.begin(), m_emptyMixerGroups.end(), pParent), m_emptyMixerGroups.end());
			pParent = static_cast<CItem*>(pParent->GetParent());
		}
	}

	return pReturn;
}

//////////////////////////////////////////////////////////////////////////
CItem* CProjectLoader::LoadParameter(XmlNodeRef const pNode, CItem& parent, EPakStatus const pakStatus)
{
	return LoadItem(pNode, EItemType::Parameter, parent, pakStatus);
}

//////////////////////////////////////////////////////////////////////////
CItem* CProjectLoader::LoadVca(XmlNodeRef const pNode, CItem& parent, EPakStatus const pakStatus)
{
	return LoadItem(pNode, EItemType::VCA, parent, pakStatus);
}

//////////////////////////////////////////////////////////////////////////
CItem* CProjectLoader::CreateItem(
  string const& name,
  EItemType const type,
  CItem* const pParent,
  EPakStatus const pakStatus /*= EPakStatus::None*/,
  string const& filePath /*= ""*/)
{
	ControlId const id = Utils::GetId(type, name, pParent, m_rootItem);
	auto pItem = static_cast<CItem*>(m_impl.GetItem(id));

	if (pItem == nullptr)
	{
		if (type == EItemType::Bank)
		{
			pItem = new CItem(name, id, type, EItemFlags::None, pakStatus, filePath);
		}
		else if ((type == EItemType::Folder) || (type == EItemType::EditorFolder))
		{
			pItem = new CItem(name, id, type, EItemFlags::IsContainer, pakStatus);
		}
		else if (type == EItemType::MixerGroup)
		{
			pItem = new CItem(name, id, type, EItemFlags::IsContainer, pakStatus);
			m_emptyMixerGroups.push_back(pItem);
		}
		else
		{
			pItem = new CItem(name, id, type, EItemFlags::None, pakStatus);
		}

		if (pParent != nullptr)
		{
			pParent->AddChild(pItem);
		}
		else
		{
			m_rootItem.AddChild(pItem);
		}

		m_itemCache[id] = pItem;

	}

	return pItem;
}

//////////////////////////////////////////////////////////////////////////
void CProjectLoader::RemoveEmptyMixerGroups()
{
	auto iter = m_emptyMixerGroups.begin();
	auto iterEnd = m_emptyMixerGroups.end();

	while (iter != iterEnd)
	{
		auto const pMixerGroup = *iter;

		if (pMixerGroup != nullptr)
		{
			auto const pParent = static_cast<CItem* const>(pMixerGroup->GetParent());

			if (pParent != nullptr)
			{
				pParent->RemoveChild(pMixerGroup);
			}

			size_t const numChildren = pMixerGroup->GetNumChildren();

			for (size_t i = 0; i < numChildren; ++i)
			{
				auto const pChild = static_cast<CItem* const>(pMixerGroup->GetChildAt(i));

				if (pChild != nullptr)
				{
					pMixerGroup->RemoveChild(pChild);
				}
			}

			auto const id = pMixerGroup->GetId();
			auto const cacheIter = m_itemCache.find(id);

			if (cacheIter != m_itemCache.end())
			{
				m_itemCache.erase(cacheIter);
			}

			delete pMixerGroup;
		}

		if (iter != (iterEnd - 1))
		{
			(*iter) = m_emptyMixerGroups.back();
		}

		m_emptyMixerGroups.pop_back();
		iter = m_emptyMixerGroups.begin();
		iterEnd = m_emptyMixerGroups.end();
	}

	m_emptyMixerGroups.clear();
}

//////////////////////////////////////////////////////////////////////////
void CProjectLoader::RemoveEmptyEditorFolders(CItem* const pEditorFolder)
{
	if (pEditorFolder->GetNumChildren() == 0)
	{
		m_rootItem.RemoveChild(pEditorFolder);
		ItemCache::const_iterator const it(m_itemCache.find(pEditorFolder->GetId()));

		if (it != m_itemCache.end())
		{
			m_itemCache.erase(it);
		}

		delete pEditorFolder;
	}
}
} // namespace Fmod
} // namespace Impl
} // namespace ACE
