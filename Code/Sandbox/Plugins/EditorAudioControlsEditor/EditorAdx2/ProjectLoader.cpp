// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ProjectLoader.h"

#include "Impl.h"
#include "Utils.h"

#include <CryAudioImplAdx2/GlobalData.h>
#include <CrySystem/File/CryFile.h>
#include <CrySystem/ISystem.h>
#include <CrySystem/XML/IXml.h>

namespace ACE
{
namespace Impl
{
namespace Adx2
{
constexpr char const* g_nameAttrib = "OrcaName";
constexpr char const* g_typeAttrib = "OrcaType";

constexpr uint32 g_globalSettingsAttribId = CryAudio::StringToId("GlobalSettings");
constexpr uint32 g_cueSheetFolderAttribId = CryAudio::StringToId("CueSheetFolder");

constexpr uint32 g_aisacControlsAttribId = CryAudio::StringToId("AISAC-Controls");
constexpr uint32 g_gameVariablesAttribId = CryAudio::StringToId("GameVariables");
constexpr uint32 g_selectorFolderAttribId = CryAudio::StringToId("SelectorFolder");
constexpr uint32 g_categoriesAttribId = CryAudio::StringToId("Categories");
constexpr uint32 g_dspBusSettingsAttribId = CryAudio::StringToId("DspBusSettings");

constexpr uint32 g_coreDspBusAttribId = CryAudio::StringToId("CriMw.CriAtomCraft.AcCore.AcOoDspBus");
constexpr uint32 g_coreDspSettingSnapshotAttribId = CryAudio::StringToId("CriMw.CriAtomCraft.AcCore.AcOoDspSettingSnapshot");
constexpr uint32 g_coreCueSheetSubFolderAttribId = CryAudio::StringToId("CriMw.CriAtomCraft.AcCore.AcOoCueSheetSubFolder");
constexpr uint32 g_coreCueFolderAttribId = CryAudio::StringToId("CriMw.CriAtomCraft.AcCore.AcOoCueFolder");

//////////////////////////////////////////////////////////////////////////
XmlNodeRef FindNodeByAttributeValue(XmlNodeRef const& rootNode, uint32 const valueId)
{
	XmlNodeRef node;

	if (rootNode.isValid())
	{
		int const numChildren = rootNode->getChildCount();

		for (int i = 0; i < numChildren; ++i)
		{
			XmlNodeRef const childNode = rootNode->getChild(i);

			if (childNode.isValid())
			{
				uint32 const nameValueId = CryAudio::StringToId(childNode->getAttr(g_nameAttrib));

				if (nameValueId == valueId)
				{
					node = childNode;
					break;
				}
				else if (childNode->getChildCount() > 0)
				{
					node = FindNodeByAttributeValue(childNode, valueId);

					if (node.isValid())
					{
						break;
					}
				}
			}
		}
	}

	return node;
}

//////////////////////////////////////////////////////////////////////////
CProjectLoader::CProjectLoader(
	string const& projectPath,
	string const& assetsPath,
	string const& localizedAssetsPath,
	CItem& rootItem,
	ItemCache& itemCache,
	CImpl const& impl)
	: m_rootItem(rootItem)
	, m_itemCache(itemCache)
	, m_impl(impl)
	, m_pBusesFolder(nullptr)
	, m_pSnapShotsFolder(nullptr)
{
	LoadGlobalSettings(projectPath, rootItem);

	CItem* const pWorkUnitsFolder = CreateItem(s_workUnitsFolderName, EItemType::FolderCueSheet, &m_rootItem, EItemFlags::IsContainer);
	LoadWorkUnits(projectPath + "/WorkUnits/", *pWorkUnitsFolder);

	CItem* const pBinariesFolder = CreateItem(s_binariesFolderName, EItemType::FolderCueSheet, &m_rootItem, EItemFlags::IsContainer);
	LoadBinaries(assetsPath, false, *pBinariesFolder);
	LoadBinaries(localizedAssetsPath, true, *pBinariesFolder);

	RemoveEmptyFolder(pWorkUnitsFolder);
	RemoveEmptyFolder(pBinariesFolder);
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
		if (type == EItemType::Cue)
		{
			pItem = new CItem(name, id, type, flags, pakStatus, filePath, Utils::FindCueSheetName(pParent, m_rootItem));
		}
		else
		{
			pItem = new CItem(name, id, type, flags, pakStatus, filePath);
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
void CProjectLoader::LoadGlobalSettings(string const& folderPath, CItem& parent)
{
	_finddata_t fd;
	ICryPak* const pCryPak = gEnv->pCryPak;
	intptr_t const handle = pCryPak->FindFirst(folderPath + "/*.atmcglobal", &fd);

	if (handle != -1)
	{
		do
		{
			string const fileName = fd.name;
			XmlNodeRef const rootNode = GetISystem()->LoadXmlFromFile(folderPath + "/" + fileName);

			if (rootNode.isValid())
			{
				XmlNodeRef const globalSettingsNode = FindNodeByAttributeValue(rootNode, g_globalSettingsAttribId);

				if (globalSettingsNode.isValid())
				{
					CItem* const pGlobalSettingsFolder = CreateItem(s_globalSettingsFolderName, EItemType::FolderGlobal, &m_rootItem, EItemFlags::IsContainer);
					int const numChildren = globalSettingsNode->getChildCount();

					for (int i = 0; i < numChildren; ++i)
					{
						XmlNodeRef const childNode = globalSettingsNode->getChild(i);

						if (childNode.isValid() && (childNode->getChildCount() > 0))
						{
							uint32 const nameValueId = CryAudio::StringToId(childNode->getAttr(g_nameAttrib));

							switch (nameValueId)
							{
							case g_aisacControlsAttribId:
								{
									CItem* const pFolder = CreateItem(s_aisacControlsFolderName, EItemType::FolderGlobal, pGlobalSettingsFolder, EItemFlags::IsContainer);
									ParseGlobalSettingsFile(childNode, *pFolder, EItemType::AisacControl);

									break;
								}
							case g_gameVariablesAttribId:
								{
									CItem* const pFolder = CreateItem(s_gameVariablesFolderName, EItemType::FolderGlobal, pGlobalSettingsFolder, EItemFlags::IsContainer);
									ParseGlobalSettingsFile(childNode, *pFolder, EItemType::GameVariable);

									break;
								}
							case g_selectorFolderAttribId:
								{
									CItem* const pFolder = CreateItem(s_selectorsFolderName, EItemType::FolderGlobal, pGlobalSettingsFolder, EItemFlags::IsContainer);
									ParseGlobalSettingsFile(childNode, *pFolder, EItemType::Selector);

									break;
								}
							case g_categoriesAttribId:
								{
									CItem* const pFolder = CreateItem(s_categoriesFolderName, EItemType::FolderGlobal, pGlobalSettingsFolder, EItemFlags::IsContainer);
									ParseGlobalSettingsFile(childNode, *pFolder, EItemType::CategoryGroup);

									break;
								}
							case g_dspBusSettingsAttribId:
								{
									m_pBusesFolder = CreateItem(s_dspBusesFolderName, EItemType::FolderGlobal, pGlobalSettingsFolder, EItemFlags::IsContainer);
									m_pSnapShotsFolder = CreateItem(s_snapshotsFolderName, EItemType::FolderGlobal, pGlobalSettingsFolder, EItemFlags::IsContainer);

									CItem* const pFolder = CreateItem(s_dspBusSettingsFolderName, EItemType::FolderGlobal, pGlobalSettingsFolder, EItemFlags::IsContainer);
									ParseBusSettings(childNode, *pFolder);

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

			break;
		}
		while (pCryPak->FindNext(handle, &fd) >= 0);

		pCryPak->FindClose(handle);
	}
}

//////////////////////////////////////////////////////////////////////////
void CProjectLoader::ParseGlobalSettingsFile(XmlNodeRef const& node, CItem& parent, EItemType const type)
{
	int const numChildren = node->getChildCount();

	for (int i = 0; i < numChildren; ++i)
	{
		XmlNodeRef const childNode = node->getChild(i);

		if (childNode.isValid())
		{
			char const* const szNameValue = childNode->getAttr(g_nameAttrib);
			CItem* const pItem = CreateItem(szNameValue, type, &parent, EItemFlags::None);

			switch (type)
			{
			case EItemType::Selector:
				{
					pItem->SetFlags(pItem->GetFlags() | EItemFlags::IsContainer);
					ParseGlobalSettingsFile(childNode, *pItem, EItemType::SelectorLabel);

					break;
				}
			case EItemType::CategoryGroup:
				{
					pItem->SetFlags(pItem->GetFlags() | EItemFlags::IsContainer);
					ParseGlobalSettingsFile(childNode, *pItem, EItemType::Category);

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

//////////////////////////////////////////////////////////////////////////
void CProjectLoader::ParseBusSettings(XmlNodeRef const& node, CItem& parent)
{
	int const numChildren = node->getChildCount();

	for (int i = 0; i < numChildren; ++i)
	{
		XmlNodeRef const childNode = node->getChild(i);

		if (childNode.isValid())
		{
			char const* const szNameValue = childNode->getAttr(g_nameAttrib);
			CreateItem(szNameValue, EItemType::DspBusSetting, &parent, EItemFlags::None);
			ParseBusesAndSnapshots(childNode);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CProjectLoader::ParseBusesAndSnapshots(XmlNodeRef const& node)
{
	int const numChildren = node->getChildCount();

	for (int i = 0; i < numChildren; ++i)
	{
		XmlNodeRef const childNode = node->getChild(i);

		if (childNode.isValid())
		{
			uint32 const typeAttribId = CryAudio::StringToId(childNode->getAttr(g_typeAttrib));

			switch (typeAttribId)
			{
			case g_coreDspBusAttribId:
				{
					char const* const szNameValue = childNode->getAttr(g_nameAttrib);
					CreateItem(szNameValue, EItemType::Bus, m_pBusesFolder, EItemFlags::None);

					break;
				}
			case g_coreDspSettingSnapshotAttribId:
				{
					char const* const szNameValue = childNode->getAttr(g_nameAttrib);
					CreateItem(szNameValue, EItemType::Snapshot, m_pSnapShotsFolder, EItemFlags::None);

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

//////////////////////////////////////////////////////////////////////////
void CProjectLoader::LoadWorkUnits(string const& folderPath, CItem& parent)
{
	_finddata_t fd;
	ICryPak* const pCryPak = gEnv->pCryPak;
	intptr_t const handle = pCryPak->FindFirst(folderPath + "*", &fd);

	if (handle != -1)
	{
		do
		{
			string const fileName = fd.name;

			if ((fileName != ".") && (fileName != "..") && !fileName.empty())
			{
				string const filePath = folderPath + "/" + fileName;
				LoadWorkUnitFile(filePath, parent);

			}
		}
		while (pCryPak->FindNext(handle, &fd) >= 0);

		pCryPak->FindClose(handle);
	}
}

//////////////////////////////////////////////////////////////////////////
void CProjectLoader::LoadWorkUnitFile(string const& folderPath, CItem& parent)
{
	_finddata_t fd;
	ICryPak* const pCryPak = gEnv->pCryPak;
	intptr_t const handle = pCryPak->FindFirst(folderPath + "/*.atmcunit", &fd);

	if (handle != -1)
	{
		do
		{
			string fileName = fd.name;
			XmlNodeRef const rootNode = GetISystem()->LoadXmlFromFile(folderPath + "/" + fileName);

			if (rootNode.isValid())
			{
				PathUtil::RemoveExtension(fileName);
				XmlNodeRef const workUnitNode = FindNodeByAttributeValue(rootNode, CryAudio::StringToId(fileName.c_str()));

				if (workUnitNode.isValid())
				{
					CItem* const pWorkUnit = CreateItem(fileName, EItemType::WorkUnit, &parent, EItemFlags::IsContainer);
					XmlNodeRef const cueSheetFolderNode = FindNodeByAttributeValue(rootNode, g_cueSheetFolderAttribId);

					if (cueSheetFolderNode.isValid())
					{
						CItem* const pCueSheetFolder = CreateItem("CueSheetFolder", EItemType::FolderCueSheet, pWorkUnit, EItemFlags::IsContainer);
						ParseWorkUnitFile(cueSheetFolderNode, *pCueSheetFolder);
					}
				}
			}

			break;
		}
		while (pCryPak->FindNext(handle, &fd) >= 0);

		pCryPak->FindClose(handle);
	}
}

//////////////////////////////////////////////////////////////////////////
void CProjectLoader::ParseWorkUnitFile(XmlNodeRef const& node, CItem& parent)
{
	int const numChildren = node->getChildCount();

	for (int i = 0; i < numChildren; ++i)
	{
		XmlNodeRef const childNode = node->getChild(i);

		if (childNode.isValid())
		{
			char const* const szNameValue = childNode->getAttr(g_nameAttrib);

			if (childNode->haveAttr("CueID"))
			{
				CreateItem(szNameValue, EItemType::Cue, &parent, EItemFlags::None);
			}
			else if (childNode->haveAttr("AwbHash"))
			{
				CItem* const pItem = CreateItem(szNameValue, EItemType::CueSheet, &parent, EItemFlags::IsContainer);
				ParseWorkUnitFile(childNode, *pItem);
			}
			else
			{
				uint32 const typeAttribId = CryAudio::StringToId(childNode->getAttr(g_typeAttrib));

				switch (typeAttribId)
				{
				case g_coreCueSheetSubFolderAttribId:
					{
						CItem* const pItem = CreateItem(szNameValue, EItemType::FolderCueSheet, &parent, EItemFlags::IsContainer);
						ParseWorkUnitFile(childNode, *pItem);

						break;
					}
				case g_coreCueFolderAttribId:
					{
						CItem* const pItem = CreateItem(szNameValue, EItemType::FolderCue, &parent, EItemFlags::IsContainer);
						ParseWorkUnitFile(childNode, *pItem);

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

//////////////////////////////////////////////////////////////////////////
void CProjectLoader::LoadBinaries(string const& folderPath, bool const isLocalized, CItem& parent)
{
	_finddata_t fd;
	ICryPak* const pCryPak = gEnv->pCryPak;
	intptr_t const handle = pCryPak->FindFirst(folderPath + "/*.acb", &fd);

	if (handle != -1)
	{
		EItemFlags const flags = isLocalized ? EItemFlags::IsLocalized : EItemFlags::None;

		do
		{
			string const fileName = fd.name;

			if (_stricmp(PathUtil::GetExt(fileName), "acb") == 0)
			{
				string const filePath = folderPath + "/" + fileName;
				EPakStatus const pakStatus = pCryPak->IsFileExist(filePath.c_str(), ICryPak::eFileLocation_OnDisk) ? EPakStatus::OnDisk : EPakStatus::None;

				CreateItem(fileName, EItemType::Binary, &parent, flags, pakStatus, filePath);
			}

		}
		while (pCryPak->FindNext(handle, &fd) >= 0);

		pCryPak->FindClose(handle);
	}
}

//////////////////////////////////////////////////////////////////////////
void CProjectLoader::RemoveEmptyFolder(CItem* const pFolder)
{
	if (pFolder->GetNumChildren() == 0)
	{
		m_rootItem.RemoveChild(pFolder);
		ItemCache::const_iterator const it(m_itemCache.find(pFolder->GetId()));

		if (it != m_itemCache.end())
		{
			m_itemCache.erase(it);
		}

		delete pFolder;
	}
}
} // namespace Adx2
} // namespace Impl
} // namespace ACE
