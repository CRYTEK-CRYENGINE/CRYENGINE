// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ProjectLoader.h"

#include "Common.h"
#include "Impl.h"
#include "Utils.h"

#include <CryAudioImplFmod/GlobalData.h>
#include <CrySystem/File/CryFile.h>
#include <CrySystem/ISystem.h>
#include <CrySystem/XML/IXml.h>

namespace ACE
{
namespace Impl
{
namespace Fmod
{
// Paths
constexpr char const* g_szEventFoldersPath = "/metadata/eventfolder/";
constexpr char const* g_szParametersFoldersPath = "/metadata/parameterpresetfolder/";
constexpr char const* g_szSnapshotGroupsPath = "/metadata/snapshotgroup/";
constexpr char const* g_szMixerGroupsPath = "/metadata/group/";
constexpr char const* g_szEventsPath = "/metadata/event/";
constexpr char const* g_szParametersPath = "/metadata/parameterpreset/";
constexpr char const* g_szSnapshotsPath = "/metadata/snapshot/";
constexpr char const* g_szReturnsPath = "/metadata/return/";
constexpr char const* g_szVcasPath = "/metadata/vca/";
constexpr char const* g_szBankPath = "/metadata/bank/";

constexpr char const* g_szAttribName = "name";
constexpr char const* g_szAttribId = "id";
constexpr char const* g_szAttribTrue = "true";

constexpr uint32 g_classIdEventFolder = CryAudio::StringToId("EventFolder");
constexpr uint32 g_classIdParameterPresetFolder = CryAudio::StringToId("ParameterPresetFolder");
constexpr uint32 g_classIdSnapshotGroup = CryAudio::StringToId("SnapshotGroup");
constexpr uint32 g_classIdEvent = CryAudio::StringToId("Event");
constexpr uint32 g_classIdSnapshot = CryAudio::StringToId("Snapshot");
constexpr uint32 g_classIdParameterPreset = CryAudio::StringToId("ParameterPreset");
constexpr uint32 g_classIdMixerReturn = CryAudio::StringToId("MixerReturn");
constexpr uint32 g_classIdMixerGroup = CryAudio::StringToId("MixerGroup");
constexpr uint32 g_classIdMixerVCA = CryAudio::StringToId("MixerVCA");
constexpr uint32 g_classIdAudioTable = CryAudio::StringToId("AudioTable");
constexpr uint32 g_classIdProgrammerSound = CryAudio::StringToId("ProgrammerSound");

constexpr uint32 g_attribIdName = CryAudio::StringToId(g_szAttribName);
constexpr uint32 g_attribIdItems = CryAudio::StringToId("items");
constexpr uint32 g_attribIdSourceDirectory = CryAudio::StringToId("sourceDirectory");
constexpr uint32 g_attribIdIsLocalized = CryAudio::StringToId("isLocalized");
constexpr uint32 g_attribIdIncludeSubDirectories = CryAudio::StringToId("includeSubDirectories");

constexpr uint32 g_tagIdIdProperty = CryAudio::StringToId("property");
constexpr uint32 g_tagIdIdRelationship = CryAudio::StringToId("relationship");

constexpr uint32 g_relationshipIdFolder = CryAudio::StringToId("folder");
constexpr uint32 g_relationshipIdOutput = CryAudio::StringToId("output");

constexpr uint32 g_fileExtensionIdAiff = CryAudio::StringToId(".aiff");
constexpr uint32 g_fileExtensionIdFlac = CryAudio::StringToId(".flac");
constexpr uint32 g_fileExtensionIdMp2 = CryAudio::StringToId(".mp2");
constexpr uint32 g_fileExtensionIdMp3 = CryAudio::StringToId(".mp3");
constexpr uint32 g_fileExtensionIdOgg = CryAudio::StringToId(".ogg");
constexpr uint32 g_fileExtensionIdWav = CryAudio::StringToId(".wav");

using ItemNames = std::vector<string>;

//////////////////////////////////////////////////////////////////////////
void AddNonStreamsBank(ItemNames& banks, string const& fileName)
{
	size_t const pos = fileName.rfind(".streams.bank");

	if (pos == string::npos)
	{
		banks.emplace_back(fileName);
	}
}

//////////////////////////////////////////////////////////////////////////
CProjectLoader::CProjectLoader(
	string const& projectPath,
	string const& banksPath,
	string const& localizedBanksPath,
	CItem& rootItem,
	ItemCache& itemCache,
	CImpl const& impl)
	: m_rootItem(rootItem)
	, m_itemCache(itemCache)
	, m_projectPath(projectPath)
	, m_impl(impl)
{
	CItem* const pSoundBanksFolder = CreateItem(s_soundBanksFolderName, EItemType::EditorFolder, &m_rootItem, EItemFlags::IsContainer);
	LoadBanks(banksPath, false, *pSoundBanksFolder);
	LoadBanks(localizedBanksPath, true, *pSoundBanksFolder);

	CItem* const pEventsFolder = CreateItem(s_eventsFolderName, EItemType::EditorFolder, &m_rootItem, EItemFlags::IsContainer);
	CItem* const pParametersFolder = CreateItem(s_parametersFolderName, EItemType::EditorFolder, &m_rootItem, EItemFlags::IsContainer);
	CItem* const pSnapshotsFolder = CreateItem(s_snapshotsFolderName, EItemType::EditorFolder, &m_rootItem, EItemFlags::IsContainer);
	CItem* const pReturnsFolder = CreateItem(s_returnsFolderName, EItemType::EditorFolder, &m_rootItem, EItemFlags::IsContainer);
	CItem* const pVcasFolder = CreateItem(s_vcasFolderName, EItemType::EditorFolder, &m_rootItem, EItemFlags::IsContainer);

	ParseFolder(projectPath + g_szEventFoldersPath, *pEventsFolder, rootItem);          // Event folders
	ParseFolder(projectPath + g_szParametersFoldersPath, *pParametersFolder, rootItem); // Parameter folders
	ParseFolder(projectPath + g_szSnapshotGroupsPath, *pSnapshotsFolder, rootItem);     // Snapshot groups
	ParseFolder(projectPath + g_szMixerGroupsPath, *pReturnsFolder, rootItem);          // Mixer groups
	ParseFolder(projectPath + g_szEventsPath, *pEventsFolder, rootItem);                // Events
	ParseFolder(projectPath + g_szParametersPath, *pParametersFolder, rootItem);        // Parameters
	ParseFolder(projectPath + g_szSnapshotsPath, *pSnapshotsFolder, rootItem);          // Snapshots
	ParseFolder(projectPath + g_szReturnsPath, *pReturnsFolder, rootItem);              // Returns
	ParseFolder(projectPath + g_szVcasPath, *pVcasFolder, rootItem);                    // VCAs
	ParseFolder(projectPath + g_szBankPath, rootItem, rootItem);                        // Audio tables of banks

	if (!m_audioTableInfos.empty())
	{
		CItem* const pKeysFolder = CreateItem(s_keysFolderName, EItemType::EditorFolder, &m_rootItem, EItemFlags::IsContainer);
		LoadKeys(*pKeysFolder);
		RemoveEmptyEditorFolders(pKeysFolder);
	}

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
		EItemFlags const flags = isLocalized ? EItemFlags::IsLocalized : EItemFlags::None;

		// We have to exclude the Master Bank, for this we look
		// for the file that ends with "strings.bank" as it is guaranteed
		// to have the same name as the Master Bank and there should be unique.
		ItemNames banks;
		ItemNames masterBankNames;

		do
		{
			string const fileName = fd.name;

			if ((fileName != ".") && (fileName != "..") && !fileName.empty())
			{
				if (isLocalized)
				{
					AddNonStreamsBank(banks, fileName);
				}
				else
				{
					size_t const pos = fileName.rfind(".strings.bank");

					if (pos != string::npos)
					{
						masterBankNames.emplace_back(fileName.substr(0, pos));
					}
					else
					{
						AddNonStreamsBank(banks, fileName);
					}
				}
			}
		}
		while (pCryPak->FindNext(handle, &fd) >= 0);

		for (string const& bankName : banks)
		{
			bool canCreateItem = isLocalized;

			if (!canCreateItem)
			{
				canCreateItem = true;

				for (auto const& masterBankName : masterBankNames)
				{
					if ((bankName.compareNoCase(0, masterBankName.length(), masterBankName) == 0))
					{
						canCreateItem = false;
						break;
					}
				}
			}

			if (canCreateItem)
			{
				string const filePath = folderPath + "/" + bankName;
				EPakStatus const pakStatus = pCryPak->IsFileExist(filePath.c_str(), ICryPak::eFileLocation_OnDisk) ? EPakStatus::OnDisk : EPakStatus::None;

				CreateItem(bankName, EItemType::Bank, &parent, flags, pakStatus, filePath);
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
				EPakStatus const pakStatus = pCryPak->IsFileExist(filePath.c_str(), ICryPak::eFileLocation_OnDisk) ? EPakStatus::OnDisk : EPakStatus::None;

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
		XmlNodeRef const rootNode = GetISystem()->LoadXmlFromFile(filepath);

		if (rootNode.isValid())
		{
			CItem* pItem = nullptr;
			int const numChildren = rootNode->getChildCount();

			for (int i = 0; i < numChildren; ++i)
			{
				XmlNodeRef const childNode = rootNode->getChild(i);

				if (childNode.isValid())
				{
					uint32 const classNameId = CryAudio::StringToId(childNode->getAttr("class"));

					switch (classNameId)
					{
					case g_classIdEventFolder: // Intentional fall-through.
					case g_classIdParameterPresetFolder:
						{
							LoadFolder(childNode, parent, pakStatus);
							break;
						}
					case g_classIdSnapshotGroup:
						{
							LoadSnapshotGroup(childNode, parent, pakStatus);
							break;
						}
					case g_classIdEvent:
						{
							pItem = LoadEvent(childNode, parent, pakStatus);
							break;
						}
					case g_classIdSnapshot:
						{
							LoadSnapshot(childNode, parent, pakStatus);
							break;
						}
					case g_classIdParameterPreset:
						{
							LoadParameter(childNode, parent, pakStatus);
							break;
						}
					case g_classIdMixerReturn:
						{
							LoadReturn(childNode, parent, pakStatus);
							break;
						}
					case g_classIdMixerGroup:
						{
							LoadMixerGroup(childNode, parent, pakStatus);
							break;
						}
					case g_classIdMixerVCA:
						{
							LoadVca(childNode, parent, pakStatus);
							break;
						}
					case g_classIdAudioTable:
						{
							LoadAudioTable(childNode);
							break;
						}
					case g_classIdProgrammerSound:
						{
							if ((pItem != nullptr) && (pItem->GetType() == EItemType::Event))
							{
								string const pathName = Utils::GetPathName(pItem, m_rootItem);
								CImpl::s_programmerSoundEvents.emplace_back(pathName);
							}

							break;
						}
					default:
						{
							break;
						}
					}
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CProjectLoader::ParseKeysFile(string const& filePath, CItem& parent)
{
	CCryFile file;

	if (file.Open(filePath, "r"))
	{
		size_t const length = file.GetLength();
		string allText;

		if (length > 0)
		{
			std::vector<char> buffer;
			buffer.resize(length, '\n');
			file.ReadRaw(&buffer[0], length);
			allText.assign(&buffer[0], length);

			int linePos = 0;
			string line;

			while (!(line = allText.Tokenize("\r\n", linePos)).empty())
			{
				int keyPos = 0;
				string const key = line.Tokenize(",", keyPos).Trim();
				CreateItem(key, EItemType::Key, &parent, EItemFlags::None);
			}
		}

		file.Close();
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
		// If folder not found parse the file corresponding to it and try looking for it again.
		switch (type)
		{
		case EItemType::Folder:
			{
				ParseFile(m_projectPath + g_szEventFoldersPath + id + ".xml", parent, pakStatus);
				ParseFile(m_projectPath + g_szParametersFoldersPath + id + ".xml", parent, pakStatus);
				break;
			}
		case EItemType::MixerGroup:
			{
				ParseFile(m_projectPath + g_szMixerGroupsPath + id + ".xml", parent, pakStatus);
				break;
			}
		default:
			{
				break;
			}
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
void CProjectLoader::LoadContainer(XmlNodeRef const& node, EItemType const type, uint32 const relationshipParamId, CItem& parent, EPakStatus const pakStatus)
{
	string name = "";
	CItem* pParent = &parent;
	int const numChildren = node->getChildCount();

	for (int i = 0; i < numChildren; ++i)
	{
		XmlNodeRef const childNode = node->getChild(i);
		uint32 const attribId = CryAudio::StringToId(childNode->getAttr(g_szAttribName));

		if (attribId == g_attribIdName)
		{
			// Get the container name.
			XmlNodeRef const nameNode = childNode->getChild(0);

			if (nameNode.isValid())
			{
				name = nameNode->getContent();
			}
		}
		else if (attribId == relationshipParamId)
		{
			// Get the container parent.
			XmlNodeRef const parentContainerNode = childNode->getChild(0);

			if (parentContainerNode.isValid())
			{
				string const parentContainerId = parentContainerNode->getContent();
				pParent = GetContainer(parentContainerId, type, parent, pakStatus);
			}
		}
	}

	CItem* const pContainer = CreateItem(name, type, pParent, EItemFlags::IsContainer, pakStatus);
	m_containerIds[node->getAttr(g_szAttribId)] = pContainer;
}

//////////////////////////////////////////////////////////////////////////
void CProjectLoader::LoadSnapshotGroup(XmlNodeRef const& node, CItem& parent, EPakStatus const pakStatus)
{
	string name = "";
	ItemNames snapshotsItems;
	int const numChildren = node->getChildCount();

	for (int i = 0; i < numChildren; ++i)
	{
		XmlNodeRef const childNode = node->getChild(i);
		uint32 const attribId = CryAudio::StringToId(childNode->getAttr(g_szAttribName));

		switch (attribId)
		{
		case g_attribIdName:
			{
				XmlNodeRef const nameNode = childNode->getChild(0);

				if (nameNode.isValid())
				{
					name = nameNode->getContent();
				}

				break;
			}
		case g_attribIdItems:
			{
				int const itemCount = childNode->getChildCount();

				for (int j = 0; j < itemCount; ++j)
				{
					XmlNodeRef const itemNode = childNode->getChild(j);

					if (itemNode.isValid())
					{
						snapshotsItems.emplace_back(itemNode->getContent());
					}
				}

				break;
			}
		default:
			{
				break;
			}
		}
	}

	CItem* const pItem = CreateItem(name, EItemType::Folder, &parent, EItemFlags::IsContainer, pakStatus);

	if (!snapshotsItems.empty())
	{
		for (auto const& snapshotId : snapshotsItems)
		{
			m_snapshotGroupItems[snapshotId] = pItem;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CProjectLoader::LoadFolder(XmlNodeRef const& node, CItem& parent, EPakStatus const pakStatus)
{
	LoadContainer(node, EItemType::Folder, g_relationshipIdFolder, parent, pakStatus);
}

//////////////////////////////////////////////////////////////////////////
void CProjectLoader::LoadMixerGroup(XmlNodeRef const& node, CItem& parent, EPakStatus const pakStatus)
{
	LoadContainer(node, EItemType::MixerGroup, g_relationshipIdOutput, parent, pakStatus);
}

//////////////////////////////////////////////////////////////////////////
CItem* CProjectLoader::LoadItem(XmlNodeRef const& node, EItemType const type, CItem& parent, EPakStatus const pakStatus)
{
	CItem* pItem = nullptr;
	string itemName = "";
	CItem* pParent = &parent;
	int const numChildren = node->getChildCount();

	for (int i = 0; i < numChildren; ++i)
	{
		XmlNodeRef const childNode = node->getChild(i);

		if (childNode.isValid())
		{
			uint32 const tagId = CryAudio::StringToId(childNode->getTag());

			switch (tagId)
			{
			case g_tagIdIdProperty:
				{
					uint32 const paramNameId = CryAudio::StringToId(childNode->getAttr(g_szAttribName));

					if (paramNameId == g_attribIdName)
					{
						XmlNodeRef const valueNode = childNode->getChild(0);

						if (valueNode.isValid())
						{
							itemName = valueNode->getContent();
						}
					}

					break;
				}
			case g_tagIdIdRelationship:
				{
					uint32 const relationshipId = CryAudio::StringToId(childNode->getAttr(g_szAttribName));

					if ((relationshipId == g_relationshipIdFolder) || (relationshipId == g_relationshipIdOutput))
					{
						XmlNodeRef const valueNode = childNode->getChild(0);

						if (valueNode.isValid())
						{
							string const parentContainerId = valueNode->getContent();
							auto const folder = m_containerIds.find(parentContainerId);

							if (folder != m_containerIds.end())
							{
								pParent = (*folder).second;
							}
						}
					}

					break;
				}
			default:
				{
					break;
				}
			}
		}
	}

	if (type == EItemType::Snapshot)
	{
		string const id = node->getAttr(g_szAttribId);

		auto const snapshotGroupPair = m_snapshotGroupItems.find(id);

		if (snapshotGroupPair != m_snapshotGroupItems.end())
		{
			pParent = (*snapshotGroupPair).second;
		}
	}

	pItem = CreateItem(itemName, type, pParent, EItemFlags::None, pakStatus);

	return pItem;
}

//////////////////////////////////////////////////////////////////////////
CItem* CProjectLoader::LoadEvent(XmlNodeRef const& node, CItem& parent, EPakStatus const pakStatus)
{
	return LoadItem(node, EItemType::Event, parent, pakStatus);
}

//////////////////////////////////////////////////////////////////////////
void CProjectLoader::LoadSnapshot(XmlNodeRef const& node, CItem& parent, EPakStatus const pakStatus)
{
	LoadItem(node, EItemType::Snapshot, parent, pakStatus);
}

//////////////////////////////////////////////////////////////////////////
void CProjectLoader::LoadReturn(XmlNodeRef const& node, CItem& parent, EPakStatus const pakStatus)
{
	CItem* const pReturn = LoadItem(node, EItemType::Return, parent, pakStatus);

	if (pReturn != nullptr)
	{
		auto pParent = static_cast<CItem*>(pReturn->GetParent());

		while ((pParent != nullptr) && (pParent->GetType() == EItemType::MixerGroup))
		{
			m_emptyMixerGroups.erase(std::remove(m_emptyMixerGroups.begin(), m_emptyMixerGroups.end(), pParent), m_emptyMixerGroups.end());
			pParent = static_cast<CItem*>(pParent->GetParent());
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CProjectLoader::LoadParameter(XmlNodeRef const& node, CItem& parent, EPakStatus const pakStatus)
{
	LoadItem(node, EItemType::Parameter, parent, pakStatus);
}

//////////////////////////////////////////////////////////////////////////
void CProjectLoader::LoadVca(XmlNodeRef const& node, CItem& parent, EPakStatus const pakStatus)
{
	LoadItem(node, EItemType::VCA, parent, pakStatus);
}

//////////////////////////////////////////////////////////////////////////
void CProjectLoader::LoadAudioTable(XmlNodeRef const& node)
{
	string name = "";
	bool isLocalized = false;
	bool includeSubdirs = false;

	int const numChildren = node->getChildCount();

	for (int i = 0; i < numChildren; ++i)
	{
		XmlNodeRef const childNode = node->getChild(i);

		if (childNode.isValid())
		{
			uint32 const tagId = CryAudio::StringToId(childNode->getTag());

			if (tagId == g_tagIdIdProperty)
			{
				uint32 const paramId = CryAudio::StringToId(childNode->getAttr(g_szAttribName));

				switch (paramId)
				{
				case g_attribIdSourceDirectory:
					{
						XmlNodeRef const valueNode = childNode->getChild(0);

						if (valueNode.isValid())
						{
							name = m_projectPath + "/" + valueNode->getContent();
						}

						break;
					}
				case g_attribIdIsLocalized:
					{
						XmlNodeRef const valueNode = childNode->getChild(0);

						if ((valueNode.isValid()) && (_stricmp(valueNode->getContent(), g_szAttribTrue) == 0))
						{
							isLocalized = true;
						}

						break;
					}
				case g_attribIdIncludeSubDirectories:
					{
						XmlNodeRef const valueNode = childNode->getChild(0);

						if ((valueNode.isValid()) && (_stricmp(valueNode->getContent(), g_szAttribTrue) == 0))
						{
							includeSubdirs = true;
						}

						break;
					}
				default:
					{
						break;
					}
				}
			}
		}
	}

	if (!name.empty())
	{
		m_audioTableInfos.emplace(SAudioTableInfo(name, isLocalized, includeSubdirs));
	}
}

//////////////////////////////////////////////////////////////////////////
void CProjectLoader::LoadKeys(CItem& parent)
{
	for (auto const& tableInfo : m_audioTableInfos)
	{
		string const keysFilePath = tableInfo.isLocalized ? (tableInfo.name + "/" + g_language + "/keys.txt") : (tableInfo.name + "/keys.txt");
		ICryPak* const pCryPak = gEnv->pCryPak;

		if (pCryPak->IsFileExist(keysFilePath.c_str()))
		{
			ParseKeysFile(keysFilePath, parent);
		}
		else
		{
			string const filePath = tableInfo.isLocalized ? (tableInfo.name + "/" + g_language) : (tableInfo.name);
			LoadKeysFromFiles(parent, filePath);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CProjectLoader::LoadKeysFromFiles(CItem& parent, string const& filePath)
{
	ICryPak* const pCryPak = gEnv->pCryPak;
	_finddata_t fd;
	intptr_t const handle = pCryPak->FindFirst(filePath + "/*.*", &fd);

	if (handle != -1)
	{
		do
		{
			if (fd.attrib & _A_SUBDIR)
			{
				string const subFolderName = fd.name;

				if ((subFolderName != ".") && (subFolderName != ".."))
				{
					CItem* const pFolderItem = CreateItem(subFolderName, EItemType::Folder, &parent, EItemFlags::IsContainer);
					string const subFolderPath = filePath + "/" + fd.name;

					LoadKeysFromFiles(*pFolderItem, subFolderPath);
				}
			}
			else
			{
				string const fileName = fd.name;

				if ((fileName != ".") && (fileName != "..") && !fileName.empty())
				{
					string::size_type const posExtension = fileName.rfind('.');

					if (posExtension != string::npos)
					{
						uint32 const fileExtensionId = CryAudio::StringToId(fileName.data() + posExtension);

						switch (fileExtensionId)
						{
						case g_fileExtensionIdWav:  // Intentional fall-through.
						case g_fileExtensionIdOgg:  // Intentional fall-through.
						case g_fileExtensionIdMp3:  // Intentional fall-through.
						case g_fileExtensionIdMp2:  // Intentional fall-through.
						case g_fileExtensionIdFlac: // Intentional fall-through.
						case g_fileExtensionIdAiff:
							{
								CreateItem(PathUtil::RemoveExtension(fileName), EItemType::Key, &parent, EItemFlags::None);
								break;
							}
						default:
							{
								break;
							}
						}
					}
				}
			}
		}
		while (pCryPak->FindNext(handle, &fd) >= 0);

		pCryPak->FindClose(handle);
	}
}

//////////////////////////////////////////////////////////////////////////
CItem* CProjectLoader::CreateItem(
	string const& name,
	EItemType const type,
	CItem* const pParent,
	EItemFlags const flags,
	EPakStatus const pakStatus /*= EPakStatus::None*/,
	string const& filePath /*= ""*/)
{
	ControlId const id = Utils::GetId(type, name, pParent, m_rootItem);
	auto pItem = static_cast<CItem*>(m_impl.GetItem(id));

	if (pItem == nullptr)
	{
		pItem = new CItem(name, id, type, flags, pakStatus, filePath);

		if (pParent != nullptr)
		{
			pParent->AddChild(pItem);
		}
		else
		{
			m_rootItem.AddChild(pItem);
		}

		if (type == EItemType::Event)
		{
			pItem->SetPathName(Utils::GetPathName(pItem, m_rootItem));
		}
		else if (type == EItemType::MixerGroup)
		{
			m_emptyMixerGroups.push_back(pItem);
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
