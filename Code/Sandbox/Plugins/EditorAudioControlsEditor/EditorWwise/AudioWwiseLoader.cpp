// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AudioWwiseLoader.h"
#include <CrySystem/File/CryFile.h>
#include <CrySystem/ISystem.h>
#include <CrySystem/ILocalizationManager.h>
#include <CryString/CryPath.h>
#include "IAudioSystemEditor.h"
#include <IAudioSystemItem.h>
#include "AudioSystemEditor_wwise.h"

using namespace PathUtil;

namespace ACE
{
const string g_gameParametersFolder = "Game Parameters";
const string g_gameStatesPath = "States";
const string g_switchesFolder = "Switches";
const string g_eventsFolder = "Events";
const string g_environmentsFolder = "Master-Mixer Hierarchy";
const string g_soundBanksPath = AUDIO_SYSTEM_DATA_ROOT CRY_NATIVE_PATH_SEPSTR "wwise";
const string g_soundBanksInfoFilename = "SoundbanksInfo.xml";

EWwiseItemTypes TagToItemType(const string& tag)
{
	if (tag == "GameParameter")
	{
		return eWwiseItemTypes_Rtpc;
	}
	else if (tag == "Event")
	{
		return eWwiseItemTypes_Event;
	}
	else if (tag == "AuxBus")
	{
		return eWwiseItemTypes_AuxBus;
	}
	else if (tag == "WorkUnit")
	{
		return eWwiseItemTypes_WorkUnit;
	}
	else if (tag == "StateGroup")
	{
		return eWwiseItemTypes_StateGroup;
	}
	else if (tag == "SwitchGroup")
	{
		return eWwiseItemTypes_SwitchGroup;
	}
	else if (tag == "Switch")
	{
		return eWwiseItemTypes_Switch;
	}
	else if (tag == "State")
	{
		return eWwiseItemTypes_State;
	}
	else if (tag == "Folder")
	{
		return eWwiseItemTypes_VirtualFolder;
	}
	return eWwiseItemTypes_Invalid;
}

string BuildPath(IAudioSystemItem* pItem)
{
	if (pItem)
	{
		if (pItem->GetParent())
		{
			return BuildPath(pItem->GetParent()) + CRY_NATIVE_PATH_SEPSTR + pItem->GetName();
		}
		return pItem->GetName();
	}
	return "";
}

CAudioWwiseLoader::CAudioWwiseLoader(const string& projectPath, const string& soundbanksPath, IAudioSystemItem& root)
	: m_root(root)
	, m_projectRoot(projectPath)
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

	IAudioSystemItem* const pSoundBanks = CreateItem("SoundBanks", eWwiseItemTypes_PhysicalFolder, m_root);
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
void CAudioWwiseLoader::LoadSoundBanks(string const& folderPath, bool const bLocalized, IAudioSystemItem& parent)
{
	_finddata_t fd;
	intptr_t const handle = gEnv->pCryPak->FindFirst(folderPath + CRY_NATIVE_PATH_SEPSTR + "*.*", &fd);

	if (handle != -1)
	{
		do
		{
			string const name = fd.name;

			if (name != "." && name != ".." && !name.empty())
			{
				if ((fd.attrib & _A_SUBDIR) == 0)
				{
					if (name.find(".bnk") != string::npos && name.compareNoCase("Init.bnk") != 0)
					{
						string const fullname = folderPath + CRY_NATIVE_PATH_SEPSTR + name;
						CID const id = CCrc32::ComputeLowercase(fullname);
						IAudioSystemItem* const pControl = stl::find_in_map(m_controlsCache, id, nullptr);

						if (pControl == nullptr)
						{
							IAudioSystemControl_wwise* pNewControl = new IAudioSystemControl_wwise(name, id, eWwiseItemTypes_SoundBank);
							parent.AddChild(pNewControl);
							pNewControl->SetParent(&parent);
							pNewControl->SetLocalised(bLocalized);
							m_controlsCache[id] = pNewControl;
						}
					}
				}
				else
				{
					IAudioSystemItem* const pParent = CreateItem(name, eWwiseItemTypes_PhysicalFolder, parent);
					LoadSoundBanks(folderPath + CRY_NATIVE_PATH_SEPSTR + name, bLocalized, *pParent);
				}
			}
		}
		while (gEnv->pCryPak->FindNext(handle, &fd) >= 0);
		gEnv->pCryPak->FindClose(handle);
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioWwiseLoader::LoadFolder(string const& folderPath, string const& folderName, IAudioSystemItem& parent)
{
	_finddata_t fd;
	ICryPak* pCryPak = gEnv->pCryPak;
	intptr_t handle = pCryPak->FindFirst(m_projectRoot + CRY_NATIVE_PATH_SEPSTR + folderPath + CRY_NATIVE_PATH_SEPSTR + folderName + CRY_NATIVE_PATH_SEPSTR + "*.*", &fd);
	if (handle != -1)
	{
		IAudioSystemItem* const pParent = CreateItem(folderName, eWwiseItemTypes_PhysicalFolder, parent);

		do
		{
			string const name = fd.name;

			if (name != "." && name != ".." && !name.empty())
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
void CAudioWwiseLoader::LoadWorkUnitFile(const string& filePath, IAudioSystemItem& parent)
{
	XmlNodeRef const pRoot = GetISystem()->LoadXmlFromFile(m_projectRoot + CRY_NATIVE_PATH_SEPSTR + filePath);

	if (pRoot)
	{
		const uint32 fileId = CCrc32::ComputeLowercase(pRoot->getAttr("ID"));

		if (m_filesLoaded.count(fileId) == 0)
		{
			// Make sure we've loaded any work units we depend on before loading
			if (pRoot->haveAttr("RootDocumentID"))
			{
				const uint32 parentDocumentId = CCrc32::ComputeLowercase(pRoot->getAttr("RootDocumentID"));

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
			const int childCount = pRoot->getChildCount();

			for (int i = 0; i < childCount; ++i)
			{
				LoadXml(pRoot->getChild(i)->findChild("WorkUnit"), parent);
			}

			m_filesLoaded.insert(fileId);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioWwiseLoader::LoadXml(XmlNodeRef pRoot, IAudioSystemItem& parent)
{
	if (pRoot)
	{
		IAudioSystemItem* pControl = &parent;
		const EWwiseItemTypes type = TagToItemType(pRoot->getTag());
		if (type != eWwiseItemTypes_Invalid)
		{
			string name = pRoot->getAttr("Name");

			const uint32 itemId = CCrc32::ComputeLowercase(pRoot->getAttr("ID"));

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

		XmlNodeRef pChildren = pRoot->findChild("ChildrenList");
		if (pChildren)
		{
			const int childCount = pChildren->getChildCount();
			for (int i = 0; i < childCount; ++i)
			{
				LoadXml(pChildren->getChild(i), *pControl);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
IAudioSystemItem* CAudioWwiseLoader::CreateItem(const string& name, EWwiseItemTypes type, IAudioSystemItem& parent)
{
	const string path = BuildPath(&parent);
	const string fullPathName = path.empty() ? name : path + CRY_NATIVE_PATH_SEPSTR + name;

	// The id is always the path of the control from the root of the wwise project
	CID id = CCrc32::ComputeLowercase(fullPathName);

	IAudioSystemItem* pControl = stl::find_in_map(m_controlsCache, id, nullptr);
	if (pControl == nullptr)
	{
		pControl = new IAudioSystemControl_wwise(name, id, type);
		if (type == eWwiseItemTypes_Event)
		{
			pControl->SetRadius(m_eventsInfoMap[CCrc32::ComputeLowercase(name.c_str())].maxRadius);
		}
		parent.AddChild(pControl);
		pControl->SetParent(&parent);
		m_controlsCache[id] = pControl;
	}
	return pControl;
}

//////////////////////////////////////////////////////////////////////////
void CAudioWwiseLoader::LoadEventsMetadata(const string& soundbanksPath)
{
	m_eventsInfoMap.clear();
	string path = soundbanksPath;
	path += CRY_NATIVE_PATH_SEPSTR + g_soundBanksInfoFilename;
	XmlNodeRef pRootNode = GetISystem()->LoadXmlFromFile(path.c_str());
	if (pRootNode)
	{
		XmlNodeRef pSoundBanksNode = pRootNode->findChild("SoundBanks");
		if (pSoundBanksNode)
		{
			const int numSoundBankNodes = pSoundBanksNode->getChildCount();
			for (int i = 0; i < numSoundBankNodes; ++i)
			{
				XmlNodeRef pSoundBankNode = pSoundBanksNode->getChild(i);
				if (pSoundBankNode)
				{
					XmlNodeRef pIncludedEventsNode = pSoundBankNode->findChild("IncludedEvents");
					if (pIncludedEventsNode)
					{
						const int numEventNodes = pIncludedEventsNode->getChildCount();
						for (int j = 0; j < numEventNodes; ++j)
						{
							XmlNodeRef pEventNode = pIncludedEventsNode->getChild(j);
							if (pEventNode)
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
ACE::IAudioSystemItem* CAudioWwiseLoader::GetControlByName(const string& name, bool bIsLocalised, IAudioSystemItem* pParent) const
{
	string fullName = (pParent) ? pParent->GetName() + CRY_NATIVE_PATH_SEPSTR + name : name;
	if (bIsLocalised)
	{
		fullName = PathUtil::GetLocalizationFolder() + CRY_NATIVE_PATH_SEPSTR + fullName;
	}

	return stl::find_in_map(m_controlsCache, CCrc32::ComputeLowercase(fullName), nullptr);
}

//////////////////////////////////////////////////////////////////////////
void CAudioWwiseLoader::BuildFileCache(const string& folderPath)
{
	_finddata_t fd;
	ICryPak* pCryPak = gEnv->pCryPak;
	intptr_t handle = pCryPak->FindFirst(m_projectRoot + CRY_NATIVE_PATH_SEPSTR + folderPath + CRY_NATIVE_PATH_SEPSTR "*.*", &fd);
	if (handle != -1)
	{
		do
		{
			string name = fd.name;
			if (name != "." && name != ".." && !name.empty())
			{
				if (fd.attrib & _A_SUBDIR)
				{
					BuildFileCache(folderPath + CRY_NATIVE_PATH_SEPSTR + name);
				}
				else
				{
					string path = folderPath + CRY_NATIVE_PATH_SEPSTR + name;
					XmlNodeRef pRoot = GetISystem()->LoadXmlFromFile(m_projectRoot + CRY_NATIVE_PATH_SEPSTR + path);
					if (pRoot)
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
}
