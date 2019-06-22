// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AudioControlsLoader.h"

#include "AssetsManager.h"
#include "ContextManager.h"

#include <CrySystem/File/CryFile.h>
#include <CrySystem/ISystem.h>
#include <QtUtil.h>

#include <QRegularExpression>

#if defined (USE_BACKWARDS_COMPATIBILITY)
// This file is deprecated and only used for backwards compatibility. It will be removed with CE 5.7.
namespace ACE
{
string const CAudioControlsLoader::s_controlsLevelsFolder = "levels/";
string const CAudioControlsLoader::s_assetsFolderPath = CRY_AUDIO_DATA_ROOT "/ace/";

//////////////////////////////////////////////////////////////////////////
EAssetType TagToType_BackwardsComp(char const* const szTag)
{
	EAssetType type = EAssetType::None;

	if (_stricmp(szTag, "ATLSwitch") == 0)
	{
		type = EAssetType::Switch;
	}
	else if (_stricmp(szTag, "ATLSwitchState") == 0)
	{
		type = EAssetType::State;
	}
	else if (_stricmp(szTag, "ATLEnvironment") == 0)
	{
		type = EAssetType::Environment;
	}
	else if (_stricmp(szTag, "ATLRtpc") == 0)
	{
		type = EAssetType::Parameter;
	}
	else if (_stricmp(szTag, "ATLTrigger") == 0)
	{
		type = EAssetType::Trigger;
	}
	else if (_stricmp(szTag, "ATLPreloadRequest") == 0)
	{
		type = EAssetType::Preload;
	}
	else
	{
		type = EAssetType::None;
	}

	return type;
}

//////////////////////////////////////////////////////////////////////////
void CAudioControlsLoader::LoadAll(bool const loadOnlyDefaultControls /*= false*/)
{
	m_loadOnlyDefaultControls = loadOnlyDefaultControls;
	LoadControls(s_assetsFolderPath);
	LoadControls(g_assetsManager.GetConfigFolderPath());
}

//////////////////////////////////////////////////////////////////////////
void CAudioControlsLoader::LoadControls(string const& folderPath)
{
	// load the global controls
	LoadAllLibrariesInFolder(folderPath, "");

	// load the level specific controls
	_finddata_t fd;
	ICryPak* pCryPak = gEnv->pCryPak;
	intptr_t handle = pCryPak->FindFirst(folderPath + s_controlsLevelsFolder + "*.*", &fd);

	if (handle != -1)
	{
		do
		{
			if (fd.attrib & _A_SUBDIR)
			{
				string name = fd.name;

				if ((name != ".") && (name != ".."))
				{
					if (LoadAllLibrariesInFolder(folderPath, name))
					{
						g_contextManager.TryCreateContext(fd.name, true);
					}
				}
			}
		}
		while (pCryPak->FindNext(handle, &fd) >= 0);

		pCryPak->FindClose(handle);
	}
}

//////////////////////////////////////////////////////////////////////////
bool CAudioControlsLoader::LoadAllLibrariesInFolder(string const& folderPath, string const& level)
{
	bool libraryLoaded = false;
	string path = folderPath;

	if (!level.empty())
	{
		path = path + s_controlsLevelsFolder + level + "/";
	}

	string const searchPath = path + "*.xml";
	ICryPak* const pCryPak = gEnv->pCryPak;
	_finddata_t fd;
	intptr_t const handle = pCryPak->FindFirst(searchPath.c_str(), &fd);

	if (handle != -1)
	{
		do
		{
			string fileName = path + fd.name;

			if (_stricmp(PathUtil::GetExt(fileName), "xml") == 0)
			{
				XmlNodeRef const rootNode = GetISystem()->LoadXmlFromFile(fileName);

				if (rootNode.isValid())
				{
					if (_stricmp(rootNode->getTag(), "ATLConfig") == 0)
					{
						m_loadedFilenames.insert(fileName.MakeLower());
						string file = fd.name;

						if (rootNode->haveAttr("atl_name"))
						{
							file = rootNode->getAttr("atl_name");
						}

						int atlVersion = 1;
						rootNode->getAttr("atl_version", atlVersion);
						PathUtil::RemoveExtension(file);
						LoadControlsLibrary(rootNode, folderPath, level, file, static_cast<uint8>(atlVersion));

						libraryLoaded = true;
					}
				}
				else
				{
					CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, "[Audio Controls Editor] Failed parsing game sound file %s", fileName);
				}
			}
		}
		while (pCryPak->FindNext(handle, &fd) >= 0);

		pCryPak->FindClose(handle);
	}

	return libraryLoaded;
}

//////////////////////////////////////////////////////////////////////////
CAsset* CAudioControlsLoader::AddUniqueFolderPath(CAsset* pParent, QString const& path)
{
	QStringList folderNames = path.split(QRegularExpression(R"((\\|\/))"), QString::SkipEmptyParts);

	int const numFolders = folderNames.length();

	for (int i = 0; i < numFolders; ++i)
	{
		if (!folderNames[i].isEmpty())
		{
			CAsset* const pChild = g_assetsManager.CreateFolder(QtUtil::ToString(folderNames[i]), pParent);

			if (pChild != nullptr)
			{
				pParent = pChild;
			}
		}
	}

	return pParent;
}

//////////////////////////////////////////////////////////////////////////
void CAudioControlsLoader::LoadControlsLibrary(XmlNodeRef const& rootNode, string const& filepath, string const& level, string const& filename, uint8 const version)
{
	// Always create a library file, even if no proper formatting is present.
	CLibrary* const pLibrary = g_assetsManager.CreateLibrary(filename);

	if (pLibrary != nullptr)
	{
		int const controlTypeCount = rootNode->getChildCount();

		for (int i = 0; i < controlTypeCount; ++i)
		{
			XmlNodeRef const node = rootNode->getChild(i);

			if (node.isValid())
			{
				if (node->isTag("EditorData"))
				{
					LoadEditorData(node, *pLibrary);
				}
				else
				{
					CryAudio::ContextId const contextId = level.empty() ? CryAudio::GlobalContextId : g_contextManager.GenerateContextId(level);
					int const numControls = node->getChildCount();

					for (int j = 0; j < numControls; ++j)
					{
						if (m_loadOnlyDefaultControls)
						{
							LoadDefaultControl(node->getChild(j), contextId, pLibrary);
						}
						else
						{
							LoadControl(node->getChild(j), contextId, version, pLibrary);
						}
					}
				}
			}
		}

		if (version < g_currentFileVersion)
		{
			pLibrary->SetModified(true, true);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
CControl* CAudioControlsLoader::LoadControl(XmlNodeRef const& node, CryAudio::ContextId const contextId, uint8 const version, CAsset* const pParentItem)
{
	CControl* pControl = nullptr;

	if (node.isValid())
	{
		bool const isInDefaultLibrary = (pParentItem->GetName().compareNoCase(CryAudio::g_szDefaultLibraryName) == 0);
		QString pathName = "";

		if (!isInDefaultLibrary)
		{
			pathName = QtUtil::ToQString(node->getAttr("path"));
		}

		CAsset* const pFolderItem = AddUniqueFolderPath(pParentItem, pathName);

		if (pFolderItem != nullptr)
		{
			string const name = node->getAttr("atl_name");
			EAssetType const controlType = TagToType_BackwardsComp(node->getTag());

			if (!((controlType == EAssetType::Switch) && ((name.compareNoCase("ObstrOcclCalcType") == 0) ||
			                                              (name.compareNoCase("object_velocity_tracking") == 0) ||
			                                              (name.compareNoCase("object_doppler_tracking") == 0) ||
			                                              (name.compareNoCase("absolute_velocity_tracking") == 0) ||
			                                              (name.compareNoCase("relative_velocity_tracking") == 0))) &&
			    !((controlType == EAssetType::Trigger) && ((name.compareNoCase("do_nothing") == 0) ||
			                                               (m_defaultTriggerNames.find(name) != m_defaultTriggerNames.end()))) &&
			    !((controlType == EAssetType::Parameter) && (m_defaultParameterNames.find(name) != m_defaultParameterNames.end())))
			{
				pControl = g_assetsManager.FindControl(name, controlType);

				if (pControl == nullptr)
				{
					pControl = g_assetsManager.CreateControl(name, controlType, pFolderItem);

					if (pControl != nullptr)
					{
						switch (controlType)
						{
						case EAssetType::Switch:
							{
								int const stateCount = node->getChildCount();

								for (int i = 0; i < stateCount; ++i)
								{
									LoadControl(node->getChild(i), contextId, version, pControl);
								}

								break;
							}
						case EAssetType::Preload:
							{
								LoadPreloadConnections(node, pControl, version);
								break;
							}
						default:
							{
								LoadConnections(node, pControl);
								break;
							}
						}

						pControl->SetContextId(contextId);
						pControl->SetModified(true, true);
					}
				}

				if (isInDefaultLibrary &&
				    (!((controlType == EAssetType::Trigger) && (m_defaultTriggerNames.find(name) != m_defaultTriggerNames.end())) &&
				     !((controlType == EAssetType::Parameter) && (m_defaultParameterNames.find(name) != m_defaultParameterNames.end()))))
				{
					CLibrary* const pLibrary = g_assetsManager.CreateLibrary("_default_controls_old");
					pControl->GetParent()->RemoveChild(pControl);
					pLibrary->AddChild(pControl);
					pControl->SetParent(pLibrary);
					pLibrary->SetModified(true, true);
				}
			}
		}
	}

	return pControl;
}

//////////////////////////////////////////////////////////////////////////
CControl* CAudioControlsLoader::LoadDefaultControl(XmlNodeRef const& node, CryAudio::ContextId const contextId, CAsset* const pParentItem)
{
	CControl* pControl = nullptr;

	if (node.isValid())
	{
		string const name = node->getAttr("atl_name");
		EAssetType const controlType = TagToType_BackwardsComp(node->getTag());

		if ((controlType == EAssetType::Trigger) && (m_defaultTriggerNames.find(name) != m_defaultTriggerNames.end()))
		{
			pControl = g_assetsManager.FindControl(name, controlType);

			if (pControl == nullptr)
			{
				pControl = g_assetsManager.CreateControl(name, controlType, pParentItem);

				if (pControl != nullptr)
				{
					LoadConnections(node, pControl);
					pControl->SetModified(true, true);
				}
			}
		}
	}

	return pControl;
}

//////////////////////////////////////////////////////////////////////////
FileNames CAudioControlsLoader::GetLoadedFilenamesList()
{
	return m_loadedFilenames;
}

//////////////////////////////////////////////////////////////////////////
void CAudioControlsLoader::LoadConnections(XmlNodeRef const& rootNode, CControl* const pControl)
{
	int const numChildren = rootNode->getChildCount();

	for (int i = 0; i < numChildren; ++i)
	{
		XmlNodeRef const node = rootNode->getChild(i);
		pControl->LoadConnectionFromXML(node);
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioControlsLoader::LoadPreloadConnections(XmlNodeRef const& node, CControl* const pControl, uint8 const version)
{
	if (_stricmp(node->getAttr("atl_type"), "AutoLoad") == 0)
	{
		pControl->SetAutoLoad(true);
	}
	else
	{
		pControl->SetAutoLoad(false);
	}

	int const numChildren = node->getChildCount();

	for (int i = 0; i < numChildren; ++i)
	{
		// Skip unused data from previous format
		XmlNodeRef const groupNode = node->getChild(i);

		if ((version == 1) && (_stricmp(groupNode->getTag(), "ATLConfigGroup") != 0))
		{
			continue;
		}

		int const numConnections = groupNode->getChildCount();

		for (int j = 0; j < numConnections; ++j)
		{
			XmlNodeRef const connectionNode = groupNode->getChild(j);

			if (connectionNode.isValid())
			{
				pControl->LoadConnectionFromXML(connectionNode);
			}
		}

		pControl->SetModified(true, true);
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioControlsLoader::LoadEditorData(XmlNodeRef const& node, CAsset& library)
{
	int const numChildren = node->getChildCount();

	for (int i = 0; i < numChildren; ++i)
	{
		XmlNodeRef const childNode = node->getChild(i);

		if (childNode->isTag("Library"))
		{
			LoadLibraryEditorData(childNode, library);
		}
		else if (childNode->isTag("Folders"))
		{
			LoadAllFolders(childNode, library);
		}
		else if (childNode->isTag("Controls"))
		{
			LoadAllControlsEditorData(childNode);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioControlsLoader::LoadLibraryEditorData(XmlNodeRef const& node, CAsset& library)
{
	string description = "";
	node->getAttr("description", description);

	if (!description.IsEmpty())
	{
		library.SetDescription(description);
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioControlsLoader::LoadAllFolders(XmlNodeRef const& node, CAsset& library)
{
	if ((node.isValid()) && (library.GetName().compareNoCase(CryAudio::g_szDefaultLibraryName) != 0))
	{
		int const numChildren = node->getChildCount();

		for (int i = 0; i < numChildren; ++i)
		{
			LoadFolderData(node->getChild(i), library);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioControlsLoader::LoadFolderData(XmlNodeRef const& node, CAsset& parentAsset)
{
	CAsset* const pAsset = AddUniqueFolderPath(&parentAsset, node->getAttr("name"));

	if (pAsset != nullptr)
	{
		string description = "";
		node->getAttr("description", description);

		if (!description.IsEmpty())
		{
			pAsset->SetDescription(description);
		}

		int const numChildren = node->getChildCount();

		for (int i = 0; i < numChildren; ++i)
		{
			LoadFolderData(node->getChild(i), *pAsset);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioControlsLoader::LoadAllControlsEditorData(XmlNodeRef const& node)
{
	if (node.isValid())
	{
		int const numChildren = node->getChildCount();

		for (int i = 0; i < numChildren; ++i)
		{
			LoadControlsEditorData(node->getChild(i));
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioControlsLoader::LoadControlsEditorData(XmlNodeRef const& node)
{
	if (node.isValid())
	{
		EAssetType const controlType = TagToType_BackwardsComp(node->getTag());
		string description = "";
		node->getAttr("description", description);

		if ((controlType != EAssetType::None) && !description.IsEmpty())
		{
			CControl* const pControl = g_assetsManager.FindControl(node->getAttr("name"), controlType);

			if (pControl != nullptr)
			{
				pControl->SetDescription(description);
			}
		}
	}
}
}      // namespace ACE
#endif //  USE_BACKWARDS_COMPATIBILITY