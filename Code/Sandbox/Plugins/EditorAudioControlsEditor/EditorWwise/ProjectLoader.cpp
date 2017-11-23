// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ProjectLoader.h"

#include "EditorImpl.h"

#include <CrySystem/File/CryFile.h>
#include <CrySystem/ISystem.h>
#include <CrySystem/ILocalizationManager.h>
#include <CryString/CryPath.h>

namespace ACE
{
namespace Wwise
{
string const g_gameParametersFolder = "Game Parameters";
string const g_gameStatesPath = "States";
string const g_switchesFolder = "Switches";
string const g_eventsFolder = "Events";
string const g_environmentsFolder = "Master-Mixer Hierarchy";
string const g_soundBanksPath = AUDIO_SYSTEM_DATA_ROOT CRY_NATIVE_PATH_SEPSTR "wwise";
string const g_soundBanksInfoFilename = "SoundbanksInfo.xml";

//////////////////////////////////////////////////////////////////////////
EImpltemType TagToItemType(string const& tag)
{
	if (tag == "GameParameter")
	{
		return EImpltemType::Parameter;
	}
	else if (tag == "Event")
	{
		return EImpltemType::Event;
	}
	else if (tag == "AuxBus")
	{
		return EImpltemType::AuxBus;
	}
	else if (tag == "WorkUnit")
	{
		return EImpltemType::WorkUnit;
	}
	else if (tag == "StateGroup")
	{
		return EImpltemType::StateGroup;
	}
	else if (tag == "SwitchGroup")
	{
		return EImpltemType::SwitchGroup;
	}
	else if (tag == "Switch")
	{
		return EImpltemType::Switch;
	}
	else if (tag == "State")
	{
		return EImpltemType::State;
	}
	else if (tag == "Folder")
	{
		return EImpltemType::VirtualFolder;
	}
	return EImpltemType::Invalid;
}

//////////////////////////////////////////////////////////////////////////
string BuildPath(CImplItem const* const pImplItem)
{
	if (pImplItem != nullptr)
	{
		if (pImplItem->GetParent())
		{
			return BuildPath(pImplItem->GetParent()) + CRY_NATIVE_PATH_SEPSTR + pImplItem->GetName();
		}

		return pImplItem->GetName();
	}
	return "";
}

//////////////////////////////////////////////////////////////////////////
CProjectLoader::CProjectLoader(string const& projectPath, string const& soundbanksPath, CImplItem& root)
	: m_root(root)
	, m_projectPath(projectPath)
{
	LoadEventsMetadata(soundbanksPath);

	// Wwise places all the Work Units in the same root physical folder but some of those WU can be nested
	// inside each other so to be able to reference them in the right order we build a cache with the path and UIDs
	// so that we can load them in the right order (i.e. the top most parent first)
	BuildFileCache(g_gameParametersFolder);
	BuildFileCache(g_gameStatesPath);
	BuildFileCache(g_switchesFolder);
	BuildFileCache(g_eventsFolder);
	BuildFileCache(g_environmentsFolder);

	LoadFolder("", g_gameParametersFolder, m_root);
	LoadFolder("", g_gameStatesPath, m_root);
	LoadFolder("", g_switchesFolder, m_root);
	LoadFolder("", g_eventsFolder, m_root);
	LoadFolder("", g_environmentsFolder, m_root);

	CImplItem* const pSoundBanks = CreateItem("SoundBanks", EImpltemType::PhysicalFolder, m_root);
	LoadSoundBanks(soundbanksPath, false, *pSoundBanks);

	char const* const szLanguage = gEnv->pSystem->GetLocalizationManager()->GetLanguage();
	string const locaFolder = PathUtil::GetLocalizationFolder() + CRY_NATIVE_PATH_SEPSTR + szLanguage + CRY_NATIVE_PATH_SEPSTR + g_soundBanksPath;
	LoadSoundBanks(locaFolder, true, *pSoundBanks);

	if (pSoundBanks->ChildCount() == 0)
	{
		m_root.RemoveChild(pSoundBanks);
		ControlsCache::const_iterator const it(m_controlsCache.find(pSoundBanks->GetId()));

		if (it != m_controlsCache.end())
		{
			m_controlsCache.erase(it);
		}

		delete pSoundBanks;
	}
}

//////////////////////////////////////////////////////////////////////////
void CProjectLoader::LoadSoundBanks(string const& folderPath, bool const isLocalized, CImplItem& parent)
{
	_finddata_t fd;
	intptr_t const handle = gEnv->pCryPak->FindFirst(folderPath + CRY_NATIVE_PATH_SEPSTR + "*.*", &fd);

	if (handle != -1)
	{
		do
		{
			string const name = fd.name;

			if ((name != ".") && (name != "..") && !name.empty())
			{
				if ((fd.attrib & _A_SUBDIR) == 0)
				{
					if ((name.find(".bnk") != string::npos) && (name.compareNoCase("Init.bnk") != 0))
					{
						string const fullname = folderPath + CRY_NATIVE_PATH_SEPSTR + name;
						CID const id = CCrc32::ComputeLowercase(fullname);
						CImplItem* const pImplControl = stl::find_in_map(m_controlsCache, id, nullptr);

						if (pImplControl == nullptr)
						{
							CImplControl* const pNewControl = new CImplControl(name, id, static_cast<ItemType>(EImpltemType::SoundBank));
							parent.AddChild(pNewControl);
							pNewControl->SetParent(&parent);
							pNewControl->SetLocalised(isLocalized);
							m_controlsCache[id] = pNewControl;
						}
					}
				}
				else
				{
					CImplItem* const pParent = CreateItem(name, EImpltemType::PhysicalFolder, parent);
					LoadSoundBanks(folderPath + CRY_NATIVE_PATH_SEPSTR + name, isLocalized, *pParent);
				}
			}
		}
		while (gEnv->pCryPak->FindNext(handle, &fd) >= 0);

		gEnv->pCryPak->FindClose(handle);
	}
}

//////////////////////////////////////////////////////////////////////////
void CProjectLoader::LoadFolder(string const& folderPath, string const& folderName, CImplItem& parent)
{
	_finddata_t fd;
	ICryPak* const pCryPak = gEnv->pCryPak;
	intptr_t const handle = pCryPak->FindFirst(m_projectPath + CRY_NATIVE_PATH_SEPSTR + folderPath + CRY_NATIVE_PATH_SEPSTR + folderName + CRY_NATIVE_PATH_SEPSTR + "*.*", &fd);

	if (handle != -1)
	{
		CImplItem* const pParent = CreateItem(folderName, EImpltemType::PhysicalFolder, parent);

		do
		{
			string const name = fd.name;

			if ((name != ".") && (name != "..") && !name.empty())
			{
				if ((fd.attrib & _A_SUBDIR) != 0)
				{
					LoadFolder(folderPath + CRY_NATIVE_PATH_SEPSTR + folderName, name, *pParent);
				}
				else
				{
					LoadWorkUnitFile(folderPath + CRY_NATIVE_PATH_SEPSTR + folderName + CRY_NATIVE_PATH_SEPSTR + name, *pParent);
				}
			}
		}
		while (pCryPak->FindNext(handle, &fd) >= 0);

		pCryPak->FindClose(handle);
	}
}

//////////////////////////////////////////////////////////////////////////
void CProjectLoader::LoadWorkUnitFile(const string& filePath, CImplItem& parent)
{
	XmlNodeRef const pRoot = GetISystem()->LoadXmlFromFile(m_projectPath + CRY_NATIVE_PATH_SEPSTR + filePath);

	if (pRoot != nullptr)
	{
		uint32 const fileId = CCrc32::ComputeLowercase(pRoot->getAttr("ID"));

		if (m_filesLoaded.count(fileId) == 0)
		{
			// Make sure we've loaded any work units we depend on before loading
			if (pRoot->haveAttr("RootDocumentID"))
			{
				uint32 const parentDocumentId = CCrc32::ComputeLowercase(pRoot->getAttr("RootDocumentID"));

				if (m_items.count(parentDocumentId) == 0)
				{
					// File hasn't been processed so we load it
					FilesCache::const_iterator const it(m_filesCache.find(parentDocumentId));

					if (it != m_filesCache.end())
					{
						LoadWorkUnitFile(it->second, parent);
					}
					else
					{
						CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, "[Audio Controls Editor] [Wwise] Couldn't find Work Unit which file %s depends on. Are you missing wwise project files?.", filePath.c_str());
					}
				}
			}

			// Each files starts with the type of controls and then the WorkUnit
			int const childCount = pRoot->getChildCount();

			for (int i = 0; i < childCount; ++i)
			{
				LoadXml(pRoot->getChild(i)->findChild("WorkUnit"), parent);
			}

			m_filesLoaded.insert(fileId);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CProjectLoader::LoadXml(XmlNodeRef const pRoot, CImplItem& parent)
{
	if (pRoot != nullptr)
	{
		CImplItem* pControl = &parent;
		EImpltemType const type = TagToItemType(pRoot->getTag());

		if (type != EImpltemType::Invalid)
		{
			string const name = pRoot->getAttr("Name");
			uint32 const itemId = CCrc32::ComputeLowercase(pRoot->getAttr("ID"));

			// Check if this item has not been created before. It could have been created in
			// a different Work Unit as a reference
			Items::const_iterator const it(m_items.find(itemId));

			if (it != m_items.end())
			{
				pControl = it->second;
			}
			else
			{
				pControl = CreateItem(name, type, parent);
				m_items[itemId] = pControl;
			}
		}

		XmlNodeRef const pChildren = pRoot->findChild("ChildrenList");

		if (pChildren != nullptr)
		{
			int const childCount = pChildren->getChildCount();
			for (int i = 0; i < childCount; ++i)
			{
				LoadXml(pChildren->getChild(i), *pControl);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
CImplItem* CProjectLoader::CreateItem(const string& name, EImpltemType const type, CImplItem& parent)
{
	string const path = BuildPath(&parent);
	string const fullPathName = path.empty() ? name : path + CRY_NATIVE_PATH_SEPSTR + name;

	// The id is always the path of the control from the root of the wwise project
	CID const id = CCrc32::ComputeLowercase(fullPathName);

	CImplItem* pImplControl = stl::find_in_map(m_controlsCache, id, nullptr);

	if (pImplControl == nullptr)
	{
		pImplControl = new CImplControl(name, id, static_cast<ItemType>(type));

		if (type == EImpltemType::Event)
		{
			pImplControl->SetRadius(m_eventsInfoMap[CCrc32::ComputeLowercase(name.c_str())].maxRadius);
		}
		else if ((type == EImpltemType::WorkUnit) || (type == EImpltemType::PhysicalFolder) || (type == EImpltemType::VirtualFolder))
		{
			pImplControl->SetContainer(true);
		}

		parent.AddChild(pImplControl);
		pImplControl->SetParent(&parent);
		m_controlsCache[id] = pImplControl;
	}

	return pImplControl;
}

//////////////////////////////////////////////////////////////////////////
void CProjectLoader::LoadEventsMetadata(const string& soundbanksPath)
{
	m_eventsInfoMap.clear();
	string path = soundbanksPath;
	path += CRY_NATIVE_PATH_SEPSTR + g_soundBanksInfoFilename;
	XmlNodeRef const pRootNode = GetISystem()->LoadXmlFromFile(path.c_str());

	if (pRootNode != nullptr)
	{
		XmlNodeRef const pSoundBanksNode = pRootNode->findChild("SoundBanks");

		if (pSoundBanksNode != nullptr)
		{
			int const numSoundBankNodes = pSoundBanksNode->getChildCount();

			for (int i = 0; i < numSoundBankNodes; ++i)
			{
				XmlNodeRef const pSoundBankNode = pSoundBanksNode->getChild(i);

				if (pSoundBankNode != nullptr)
				{
					XmlNodeRef const pIncludedEventsNode = pSoundBankNode->findChild("IncludedEvents");

					if (pIncludedEventsNode != nullptr)
					{
						int const numEventNodes = pIncludedEventsNode->getChildCount();

						for (int j = 0; j < numEventNodes; ++j)
						{
							XmlNodeRef const pEventNode = pIncludedEventsNode->getChild(j);

							if (pEventNode != nullptr)
							{
								SEventInfo info;

								if (!pEventNode->getAttr("MaxAttenuation", info.maxRadius))
								{
									info.maxRadius = 0.0f;
								}

								m_eventsInfoMap[CCrc32::ComputeLowercase(pEventNode->getAttr("Name"))] = info;
							}
						}
					}
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
CImplItem* CProjectLoader::GetControlByName(string const& name, bool const isLocalised, CImplItem const* const pParent) const
{
	string fullName = (pParent != nullptr) ? (pParent->GetName() + CRY_NATIVE_PATH_SEPSTR + name) : name;

	if (isLocalised)
	{
		fullName = PathUtil::GetLocalizationFolder() + CRY_NATIVE_PATH_SEPSTR + fullName;
	}

	return stl::find_in_map(m_controlsCache, CCrc32::ComputeLowercase(fullName), nullptr);
}

//////////////////////////////////////////////////////////////////////////
void CProjectLoader::BuildFileCache(string const& folderPath)
{
	_finddata_t fd;
	ICryPak* const pCryPak = gEnv->pCryPak;
	intptr_t const handle = pCryPak->FindFirst(m_projectPath + CRY_NATIVE_PATH_SEPSTR + folderPath + CRY_NATIVE_PATH_SEPSTR "*.*", &fd);

	if (handle != -1)
	{
		do
		{
			string const name = fd.name;

			if ((name != ".") && (name != "..") && !name.empty())
			{
				if (fd.attrib & _A_SUBDIR)
				{
					BuildFileCache(folderPath + CRY_NATIVE_PATH_SEPSTR + name);
				}
				else
				{
					string const path = folderPath + CRY_NATIVE_PATH_SEPSTR + name;
					XmlNodeRef const pRoot = GetISystem()->LoadXmlFromFile(m_projectPath + CRY_NATIVE_PATH_SEPSTR + path);

					if (pRoot != nullptr)
					{
						m_filesCache[CCrc32::ComputeLowercase(pRoot->getAttr("ID"))] = path;
					}
				}
			}
		}
		while (pCryPak->FindNext(handle, &fd) >= 0);

		pCryPak->FindClose(handle);
	}
}
} // namespace Wwise
} // namespace ACE
