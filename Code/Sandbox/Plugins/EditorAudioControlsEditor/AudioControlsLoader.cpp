// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AudioControlsLoader.h"

#include "AudioControlsEditorPlugin.h"

#include <CrySystem/File/CryFile.h>
#include <CrySystem/ISystem.h>
#include <QtUtil.h>

#include <QRegularExpression>

// This file is deprecated and only used for backwards compatibility. It will be removed before March 2019.
namespace ACE
{
string const CAudioControlsLoader::s_controlsLevelsFolder = "levels/";
string const CAudioControlsLoader::s_assetsFolderPath = AUDIO_SYSTEM_DATA_ROOT "/ace/";

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
CAudioControlsLoader::CAudioControlsLoader()
	: m_errorCodeMask(EErrorCode::None)
	, m_loadOnlyDefaultControls(false)
{}

//////////////////////////////////////////////////////////////////////////
void CAudioControlsLoader::LoadAll(bool const loadOnlyDefaultControls /*= false*/)
{
	m_loadOnlyDefaultControls = loadOnlyDefaultControls;
	LoadScopes();
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
					LoadAllLibrariesInFolder(folderPath, name);

					if (!g_assetsManager.ScopeExists(fd.name))
					{
						// if the control doesn't exist it
						// means it is not a real level in the
						// project so it is flagged as LocalOnly
						g_assetsManager.AddScope(fd.name, true);
					}
				}
			}
		}
		while (pCryPak->FindNext(handle, &fd) >= 0);

		pCryPak->FindClose(handle);
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioControlsLoader::LoadAllLibrariesInFolder(string const& folderPath, string const& level)
{
	string path = folderPath;

	if (!level.empty())
	{
		path = path + s_controlsLevelsFolder + level + "/";
	}

	string const searchPath = path + "*.xml";
	ICryPak* const pCryPak = gEnv->pCryPak;
	_finddata_t fd;
	intptr_t const handle = pCryPak->FindFirst(searchPath, &fd);

	if (handle != -1)
	{
		do
		{
			string filename = path + fd.name;
			XmlNodeRef const root = GetISystem()->LoadXmlFromFile(filename);

			if (root != nullptr)
			{
				if (_stricmp(root->getTag(), "ATLConfig") == 0)
				{
					m_loadedFilenames.insert(filename.MakeLower());
					string file = fd.name;

					if (root->haveAttr("atl_name"))
					{
						file = root->getAttr("atl_name");
					}

					int atlVersion = 1;
					root->getAttr("atl_version", atlVersion);
					PathUtil::RemoveExtension(file);
					LoadControlsLibrary(root, folderPath, level, file, atlVersion);
				}
			}
			else
			{
				CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, "[Audio Controls Editor] Failed parsing game sound file %s", filename);
			}
		}
		while (pCryPak->FindNext(handle, &fd) >= 0);

		pCryPak->FindClose(handle);
	}
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
void CAudioControlsLoader::LoadControlsLibrary(XmlNodeRef const pRoot, string const& filepath, string const& level, string const& filename, uint32 const version)
{
	// Always create a library file, even if no proper formatting is present.
	CLibrary* const pLibrary = g_assetsManager.CreateLibrary(filename);

	if (pLibrary != nullptr)
	{
		int const controlTypeCount = pRoot->getChildCount();

		for (int i = 0; i < controlTypeCount; ++i)
		{
			XmlNodeRef const pNode = pRoot->getChild(i);

			if (pNode != nullptr)
			{
				if (pNode->isTag("EditorData"))
				{
					LoadEditorData(pNode, *pLibrary);
				}
				else
				{
					Scope const scope = level.empty() ? GlobalScopeId : g_assetsManager.GetScope(level);
					int const numControls = pNode->getChildCount();

					for (int j = 0; j < numControls; ++j)
					{
						if (m_loadOnlyDefaultControls)
						{
							LoadDefaultControl(pNode->getChild(j), scope, version, pLibrary);
						}
						else
						{
							LoadControl(pNode->getChild(j), scope, version, pLibrary);
						}
					}
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
CControl* CAudioControlsLoader::LoadControl(XmlNodeRef const pNode, Scope const scope, uint32 const version, CAsset* const pParentItem)
{
	CControl* pControl = nullptr;

	if (pNode != nullptr)
	{
		bool const isInDefaultLibrary = (pParentItem->GetName().compareNoCase(CryAudio::s_szDefaultLibraryName) == 0);
		QString pathName = "";

		if (!isInDefaultLibrary)
		{
			pathName = QtUtil::ToQString(pNode->getAttr("path"));
		}

		CAsset* const pFolderItem = AddUniqueFolderPath(pParentItem, pathName);

		if (pFolderItem != nullptr)
		{
			string const name = pNode->getAttr("atl_name");
			EAssetType const controlType = TagToType_BackwardsComp(pNode->getTag());

			if (!((controlType == EAssetType::Switch) && ((name.compareNoCase("ObstrOcclCalcType") == 0) ||
			                                              (name.compareNoCase("object_velocity_tracking") == 0) ||
			                                              (name.compareNoCase("object_doppler_tracking") == 0) ||
			                                              (name.compareNoCase(CryAudio::s_szAbsoluteVelocityTrackingSwitchName) == 0) ||
			                                              (name.compareNoCase(CryAudio::s_szRelativeVelocityTrackingSwitchName) == 0))) &&
			    !((controlType == EAssetType::Trigger) && ((name.compareNoCase(CryAudio::s_szDoNothingTriggerName) == 0) ||
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
						case EAssetType::Trigger:
							{
								float radius = 0.0f;
								pNode->getAttr("atl_radius", radius);
								pControl->SetRadius(radius);
								LoadConnections(pNode, pControl);
							}
							break;
						case EAssetType::Switch:
							{
								int const stateCount = pNode->getChildCount();

								for (int i = 0; i < stateCount; ++i)
								{
									LoadControl(pNode->getChild(i), scope, version, pControl);
								}
							}
							break;
						case EAssetType::Preload:
							LoadPreloadConnections(pNode, pControl, version);
							break;
						default:
							LoadConnections(pNode, pControl);
							break;
						}

						pControl->SetScope(scope);
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
CControl* CAudioControlsLoader::LoadDefaultControl(XmlNodeRef const pNode, Scope const scope, uint32 const version, CAsset* const pParentItem)
{
	CControl* pControl = nullptr;

	if (pNode != nullptr)
	{
		string const name = pNode->getAttr("atl_name");
		EAssetType const controlType = TagToType_BackwardsComp(pNode->getTag());

		if ((controlType == EAssetType::Trigger) && (m_defaultTriggerNames.find(name) != m_defaultTriggerNames.end()))
		{
			pControl = g_assetsManager.FindControl(name, controlType);

			if (pControl == nullptr)
			{
				pControl = g_assetsManager.CreateControl(name, controlType, pParentItem);

				if (pControl != nullptr)
				{
					float radius = 0.0f;
					pNode->getAttr("atl_radius", radius);
					pControl->SetRadius(radius);
					LoadConnections(pNode, pControl);
					pControl->SetModified(true, true);
				}
			}
		}
		else if ((controlType == EAssetType::Parameter) && (m_defaultParameterNames.find(name) != m_defaultParameterNames.end()))
		{
			pControl = g_assetsManager.FindControl(name, controlType);

			if (pControl == nullptr)
			{
				pControl = g_assetsManager.CreateControl(name, controlType, pParentItem);

				if (pControl != nullptr)
				{
					LoadConnections(pNode, pControl);

					if (pControl->GetName().compareNoCase("object_speed") == 0)
					{
						pControl->SetName(CryAudio::s_szAbsoluteVelocityParameterName);
					}
					else if (pControl->GetName().compareNoCase("object_doppler") == 0)
					{
						pControl->SetName(CryAudio::s_szRelativeVelocityParameterName);
					}

					pControl->SetModified(true, true);
				}
			}
		}
	}

	return pControl;
}

//////////////////////////////////////////////////////////////////////////
void CAudioControlsLoader::LoadScopes()
{
	LoadScopesImpl(s_controlsLevelsFolder);
}

//////////////////////////////////////////////////////////////////////////
void CAudioControlsLoader::LoadScopesImpl(string const& sLevelsFolder)
{
	// TODO: consider moving the file enumeration to a background thread to speed up the editor startup time.

	_finddata_t fd;
	ICryPak* const pCryPak = gEnv->pCryPak;
	intptr_t const handle = pCryPak->FindFirst(sLevelsFolder + "/*.*", &fd);

	if (handle != -1)
	{
		do
		{
			string name = fd.name;

			if ((name != ".") && (name != "..") && !name.empty())
			{
				if (fd.attrib & _A_SUBDIR)
				{
					LoadScopesImpl(sLevelsFolder + "/" + name);
				}
				else
				{
					if (_stricmp(PathUtil::GetExt(name), "level") == 0)
					{
						PathUtil::RemoveExtension(name);
						g_assetsManager.AddScope(name);
					}
				}
			}
		}
		while (pCryPak->FindNext(handle, &fd) >= 0);

		pCryPak->FindClose(handle);
	}
}

//////////////////////////////////////////////////////////////////////////
FileNames CAudioControlsLoader::GetLoadedFilenamesList()
{
	return m_loadedFilenames;
}

//////////////////////////////////////////////////////////////////////////
void CAudioControlsLoader::LoadConnections(XmlNodeRef const pRoot, CControl* const pControl)
{
	// The radius might change because of the attenuation matching option
	// so we check here to inform the user if their data is outdated.
	float const radius = pControl->GetRadius();

	int const numChildren = pRoot->getChildCount();

	for (int i = 0; i < numChildren; ++i)
	{
		XmlNodeRef const pNode = pRoot->getChild(i);
		pControl->LoadConnectionFromXML(pNode);
	}

	if (radius != pControl->GetRadius())
	{
		m_errorCodeMask |= EErrorCode::NonMatchedActivityRadius;
		pControl->SetModified(true, true);
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioControlsLoader::LoadPreloadConnections(XmlNodeRef const pNode, CControl* const pControl, uint32 const version)
{
	if (_stricmp(pNode->getAttr("atl_type"), "AutoLoad") == 0)
	{
		pControl->SetAutoLoad(true);
	}
	else
	{
		pControl->SetAutoLoad(false);
	}

	int const numChildren = pNode->getChildCount();

	for (int i = 0; i < numChildren; ++i)
	{
		// Skip unused data from previous format
		XmlNodeRef const pGroupNode = pNode->getChild(i);

		if ((version == 1) && (_stricmp(pGroupNode->getTag(), "ATLConfigGroup") != 0))
		{
			continue;
		}

		// Get the index for that platform name
		int platformIndex = -1;
		bool foundPlatform = false;
		char const* const szPlatformName = pGroupNode->getAttr("atl_name");

		for (auto const szPlatform : g_platforms)
		{
			++platformIndex;

			if (_stricmp(szPlatformName, szPlatform) == 0)
			{
				foundPlatform = true;
				break;
			}
		}

		if (!foundPlatform)
		{
			m_errorCodeMask |= EErrorCode::UnkownPlatform;
			pControl->SetModified(true, true);
		}

		int const numConnections = pGroupNode->getChildCount();

		for (int j = 0; j < numConnections; ++j)
		{
			XmlNodeRef const pConnectionNode = pGroupNode->getChild(j);

			if (pConnectionNode != nullptr)
			{
				pControl->LoadConnectionFromXML(pConnectionNode, platformIndex);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioControlsLoader::LoadEditorData(XmlNodeRef const pEditorDataNode, CAsset& library)
{
	int const numChildren = pEditorDataNode->getChildCount();

	for (int i = 0; i < numChildren; ++i)
	{
		XmlNodeRef const pChild = pEditorDataNode->getChild(i);

		if (pChild->isTag("Library"))
		{
			LoadLibraryEditorData(pChild, library);
		}
		else if (pChild->isTag("Folders"))
		{
			LoadAllFolders(pChild, library);
		}
		else if (pChild->isTag("Controls"))
		{
			LoadAllControlsEditorData(pChild);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioControlsLoader::LoadLibraryEditorData(XmlNodeRef const pLibraryNode, CAsset& library)
{
	string description = "";
	pLibraryNode->getAttr("description", description);

	if (!description.IsEmpty())
	{
		library.SetDescription(description);
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioControlsLoader::LoadAllFolders(XmlNodeRef const pFoldersNode, CAsset& library)
{
	if ((pFoldersNode != nullptr) && (library.GetName().compareNoCase(CryAudio::s_szDefaultLibraryName) != 0))
	{
		int const numChildren = pFoldersNode->getChildCount();

		for (int i = 0; i < numChildren; ++i)
		{
			LoadFolderData(pFoldersNode->getChild(i), library);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioControlsLoader::LoadFolderData(XmlNodeRef const pFolderNode, CAsset& parentAsset)
{
	CAsset* const pAsset = AddUniqueFolderPath(&parentAsset, pFolderNode->getAttr("name"));

	if (pAsset != nullptr)
	{
		string description = "";
		pFolderNode->getAttr("description", description);

		if (!description.IsEmpty())
		{
			pAsset->SetDescription(description);
		}

		int const numChildren = pFolderNode->getChildCount();

		for (int i = 0; i < numChildren; ++i)
		{
			LoadFolderData(pFolderNode->getChild(i), *pAsset);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioControlsLoader::LoadAllControlsEditorData(XmlNodeRef const pControlsNode)
{
	if (pControlsNode != nullptr)
	{
		int const numChildren = pControlsNode->getChildCount();

		for (int i = 0; i < numChildren; ++i)
		{
			LoadControlsEditorData(pControlsNode->getChild(i));
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioControlsLoader::LoadControlsEditorData(XmlNodeRef const pParentNode)
{
	if (pParentNode != nullptr)
	{
		EAssetType const controlType = TagToType_BackwardsComp(pParentNode->getTag());
		string description = "";
		pParentNode->getAttr("description", description);

		if ((controlType != EAssetType::None) && !description.IsEmpty())
		{
			CControl* const pControl = g_assetsManager.FindControl(pParentNode->getAttr("name"), controlType);

			if (pControl != nullptr)
			{
				pControl->SetDescription(description);
			}
		}
	}
}
} // namespace ACE

