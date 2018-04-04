// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AudioControlsLoader.h"
#include <CryAudio/IAudioSystem.h>
#include <CryString/StringUtils.h>
#include <CrySystem/File/CryFile.h>
#include <CrySystem/ISystem.h>
#include "AudioAssetsManager.h"
#include <CryString/CryPath.h>
#include <IAudioSystemEditor.h>
#include <IAudioSystemItem.h>
#include "QtUtil.h"
#include "IUndoObject.h"
#include "AudioAssetsExplorerModel.h"
#include "IEditor.h"
#include <ConfigurationManager.h>
#include "AudioControlsEditorPlugin.h"

using namespace PathUtil;

namespace ACE
{
const string CAudioControlsLoader::ms_levelsFolder = "levels" CRY_NATIVE_PATH_SEPSTR;
const string CAudioControlsLoader::ms_controlsLevelsFolder = "levels" CRY_NATIVE_PATH_SEPSTR;

EItemType TagToType(const string& tag)
{
	if (tag == "ATLSwitch")
	{
		return eItemType_Switch;
	}
	else if (tag == "ATLSwitchState")
	{
		return eItemType_State;
	}
	else if (tag == "ATLEnvironment")
	{
		return eItemType_Environment;
	}
	else if (tag == "ATLRtpc")
	{
		return eItemType_RTPC;
	}
	else if (tag == "ATLTrigger")
	{
		return eItemType_Trigger;
	}
	else if (tag == "ATLPreloadRequest")
	{
		return eItemType_Preload;
	}
	return eItemType_NumTypes;
}

CAudioControlsLoader::CAudioControlsLoader(CAudioAssetsManager* pAssetsManager)
	: m_pAssetsManager(pAssetsManager)
	, m_errorCodeMask(EErrorCode::eErrorCode_NoError)
{}

void CAudioControlsLoader::LoadAll()
{
	LoadScopes();
	LoadControls();
}

void CAudioControlsLoader::LoadControls()
{
	const CScopedSuspendUndo suspendUndo;

	// load the global controls
	LoadAllLibrariesInFolder(Utils::GetAssetFolder(), "");

	// load the level specific controls
	_finddata_t fd;
	ICryPak* pCryPak = gEnv->pCryPak;
	intptr_t handle = pCryPak->FindFirst(Utils::GetAssetFolder() + ms_controlsLevelsFolder + "*.*", &fd);
	if (handle != -1)
	{
		do
		{
			if (fd.attrib & _A_SUBDIR)
			{
				string name = fd.name;
				if (name != "." && name != "..")
				{
					LoadAllLibrariesInFolder(Utils::GetAssetFolder(), name);
					if (!m_pAssetsManager->ScopeExists(fd.name))
					{
						// if the control doesn't exist it
						// means it is not a real level in the
						// project so it is flagged as LocalOnly
						m_pAssetsManager->AddScope(fd.name, true);
					}
				}
			}
		}
		while (pCryPak->FindNext(handle, &fd) >= 0);
		pCryPak->FindClose(handle);
	}

	CreateDefaultControls();
}

void CAudioControlsLoader::LoadAllLibrariesInFolder(const string& folderPath, const string& level)
{
	string path = folderPath;
	if (!level.empty())
	{
		path = path + ms_controlsLevelsFolder + level + CRY_NATIVE_PATH_SEPSTR;
	}

	string searchPath = path + "*.xml";
	ICryPak* pCryPak = gEnv->pCryPak;
	_finddata_t fd;
	intptr_t handle = pCryPak->FindFirst(searchPath, &fd);
	if (handle != -1)
	{
		do
		{
			string filename = path + fd.name;
			XmlNodeRef root = GetISystem()->LoadXmlFromFile(filename);
			if (root)
			{
				string tag = root->getTag();
				if (tag == "ATLConfig")
				{
					m_loadedFilenames.insert(filename.MakeLower());
					string file = fd.name;
					if (root->haveAttr("atl_name"))
					{
						file = root->getAttr("atl_name");
					}

					int atlVersion = 1;
					root->getAttr("atl_version", atlVersion);
					RemoveExtension(file);
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

IAudioAsset* CAudioControlsLoader::AddUniqueFolderPath(IAudioAsset* pParent, const QString& path)
{
	QStringList folderNames = path.split(QRegExp(R"((\\|\/))"), QString::SkipEmptyParts);
	const int size = folderNames.length();
	for (int i = 0; i < size; ++i)
	{
		if (!folderNames[i].isEmpty())
		{
			IAudioAsset* pChild = m_pAssetsManager->CreateFolder(QtUtil::ToString(folderNames[i]), pParent);
			if (pChild)
			{
				pParent = pChild;
			}
		}
	}
	return pParent;
}

void CAudioControlsLoader::LoadControlsLibrary(XmlNodeRef pRoot, const string& filepath, const string& level, const string& filename, uint version)
{
  // Always create a library file, even if no proper formatting is present.
	CAudioLibrary* const pLibrary = m_pAssetsManager->CreateLibrary(filename);

	if (pRoot)
	{
		const int controlTypeCount = pRoot->getChildCount();
		for (int i = 0; i < controlTypeCount; ++i)
		{
			XmlNodeRef pNode = pRoot->getChild(i);
			if (pNode)
			{
				if (pNode->isTag("EditorData"))
				{
					LoadEditorData(pNode, pLibrary);
				}
				else
				{
					Scope scope = level.empty() ? Utils::GetGlobalScope() : m_pAssetsManager->GetScope(level);
					const int numControls = pNode->getChildCount();
					for (int j = 0; j < numControls; ++j)
					{
						LoadControl(pNode->getChild(j), scope, version, pLibrary);
					}
				}
			}
		}
	}
}

CAudioControl* CAudioControlsLoader::LoadControl(XmlNodeRef pNode, Scope scope, uint version, IAudioAsset* pParentItem)
{
	CAudioControl* pControl = nullptr;
	if (pNode)
	{
		IAudioAsset* pFolderItem = AddUniqueFolderPath(pParentItem, QtUtil::ToQString(pNode->getAttr("path")));
		if (pFolderItem)
		{
			const string name = pNode->getAttr("atl_name");
			const EItemType controlType = TagToType(pNode->getTag());
			pControl = m_pAssetsManager->CreateControl(name, controlType, pFolderItem);
			if (pControl)
			{
				switch (controlType)
				{
				case eItemType_Trigger:
					{
						float radius = 0.0f;
						pNode->getAttr("atl_radius", radius);
						pControl->SetRadius(radius);

						float occlusionFadeOutDistance = 0.0f;
						pNode->getAttr("atl_occlusion_fadeout_distance", occlusionFadeOutDistance);
						pControl->SetOcclusionFadeOutDistance(occlusionFadeOutDistance);

						bool bMatchRadiusToAttenuation = true;
						pNode->getAttr("atl_match_radius_attenuation", bMatchRadiusToAttenuation);
						pControl->SetMatchRadiusToAttenuationEnabled(bMatchRadiusToAttenuation);

						LoadConnections(pNode, pControl);
					}
					break;
				case eItemType_Switch:
					{
						const int stateCount = pNode->getChildCount();
						for (int i = 0; i < stateCount; ++i)
						{
							LoadControl(pNode->getChild(i), scope, version, pControl);
						}
					}
					break;
				case eItemType_Preload:
					LoadPreloadConnections(pNode, pControl, version);
					break;
				default:
					LoadConnections(pNode, pControl);
				}
				pControl->SetScope(scope);
			}
		}
	}
	return pControl;
}

void CAudioControlsLoader::LoadScopes()
{
	LoadScopesImpl(ms_levelsFolder);
}

void CAudioControlsLoader::LoadScopesImpl(const string& sLevelsFolder)
{
	_finddata_t fd;
	ICryPak* pCryPak = gEnv->pCryPak;
	intptr_t handle = pCryPak->FindFirst(sLevelsFolder + CRY_NATIVE_PATH_SEPSTR "*.*", &fd);
	if (handle != -1)
	{
		do
		{
			string name = fd.name;
			if (name != "." && name != ".." && !name.empty())
			{
				if (fd.attrib & _A_SUBDIR)
				{
					LoadScopesImpl(sLevelsFolder + CRY_NATIVE_PATH_SEPSTR + name);
				}
				else
				{
					string extension = GetExt(name);
					if (extension == "cry")
					{
						RemoveExtension(name);
						m_pAssetsManager->AddScope(name);
					}
				}
			}
		}
		while (pCryPak->FindNext(handle, &fd) >= 0);
		pCryPak->FindClose(handle);
	}
}

std::set<string> CAudioControlsLoader::GetLoadedFilenamesList()
{
	return m_loadedFilenames;
}

void CAudioControlsLoader::CreateDefaultControls()
{
	// Create default controls if they don't exist.
	// These controls need to always exist in your project!
	IAudioAsset* const pLibrary = static_cast<IAudioAsset*>(m_pAssetsManager->CreateLibrary("default_controls"));

	if (pLibrary != nullptr)
	{
		CAudioControl* pControl = m_pAssetsManager->FindControl(CryAudio::GetFocusTriggerName, eItemType_Trigger);
		if (pControl == nullptr)
		{
			m_pAssetsManager->CreateControl(CryAudio::GetFocusTriggerName, eItemType_Trigger, pLibrary);
		}

		pControl = m_pAssetsManager->FindControl(CryAudio::LoseFocusTriggerName, eItemType_Trigger);
		if (pControl == nullptr)
		{
			m_pAssetsManager->CreateControl(CryAudio::LoseFocusTriggerName, eItemType_Trigger, pLibrary);
		}

		pControl = m_pAssetsManager->FindControl(CryAudio::MuteAllTriggerName, eItemType_Trigger);
		if (pControl == nullptr)
		{
			m_pAssetsManager->CreateControl(CryAudio::MuteAllTriggerName, eItemType_Trigger, pLibrary);
		}

		pControl = m_pAssetsManager->FindControl(CryAudio::UnmuteAllTriggerName, eItemType_Trigger);
		if (pControl == nullptr)
		{
			m_pAssetsManager->CreateControl(CryAudio::UnmuteAllTriggerName, eItemType_Trigger, pLibrary);
		}

		pControl = m_pAssetsManager->FindControl(CryAudio::DoNothingTriggerName, eItemType_Trigger);
		if (pControl == nullptr)
		{
			m_pAssetsManager->CreateControl(CryAudio::DoNothingTriggerName, eItemType_Trigger, pLibrary);
		}

		/*
		   Following audio controls

		   object_doppler_tracking
		   object_velocity_tracking
		   object_doppler
		   object_speed

		   are handled for backwards compatibility reasons.
		   Introduced in March 2017, remove this code at a feasible point in the future.
		 */
		pControl = m_pAssetsManager->FindControl(CryAudio::AbsoluteVelocityParameterName, eItemType_RTPC);
		if (pControl == nullptr)
		{
			pControl = m_pAssetsManager->FindControl("object_speed", eItemType_RTPC);
			if (pControl == nullptr)
			{
				m_pAssetsManager->CreateControl(CryAudio::AbsoluteVelocityParameterName, eItemType_RTPC, pLibrary);
			}
			else
			{
				pControl->SetName(CryAudio::AbsoluteVelocityParameterName);
			}
		}

		pControl = m_pAssetsManager->FindControl(CryAudio::RelativeVelocityParameterName, eItemType_RTPC);
		if (pControl == nullptr)
		{
			pControl = m_pAssetsManager->FindControl("object_doppler", eItemType_RTPC);
			if (pControl == nullptr)
			{
				m_pAssetsManager->CreateControl(CryAudio::RelativeVelocityParameterName, eItemType_RTPC, pLibrary);
			}
			else
			{
				pControl->SetName(CryAudio::RelativeVelocityParameterName);
			}
		}

		{
			char const* const arr[] = { CryAudio::IgnoreStateName, CryAudio::AdaptiveStateName, CryAudio::LowStateName, CryAudio::MediumStateName, CryAudio::HighStateName };
			SwitchStates const states(arr, arr + sizeof(arr) / sizeof(arr[0]));
			CreateDefaultSwitch(pLibrary, "ObstrOcclCalcType", CryAudio::OcclusionCalcSwitchName, states);
		}
		{
			char const* const arr[] = { CryAudio::OnStateName, CryAudio::OffStateName };
			SwitchStates const states(arr, arr + sizeof(arr) / sizeof(arr[0]));

			pControl = m_pAssetsManager->FindControl(CryAudio::AbsoluteVelocityTrackingSwitchName, eItemType_Switch);
			if (pControl == nullptr)
			{
				pControl = m_pAssetsManager->FindControl("object_velocity_tracking", eItemType_Switch);
				if (pControl == nullptr)
				{
					CreateDefaultSwitch(pLibrary, CryAudio::AbsoluteVelocityTrackingSwitchName, CryAudio::AbsoluteVelocityTrackingSwitchName, states);
				}
				else
				{
					pControl->SetName(CryAudio::AbsoluteVelocityTrackingSwitchName);

					size_t const numChildren = pControl->ChildCount();
					for (size_t i = 0; i < numChildren; ++i)
					{
						CAudioControl* const pChild = static_cast<CAudioControl*>(pControl->GetChild(i));
						XMLNodeList const& nodeList = pChild->GetRawXMLConnections();

						for (auto const& node : nodeList)
						{
							if (_stricmp(node.xmlNode->getTag(), "ATLSwitchRequest") == 0)
							{
								node.xmlNode->setAttr("atl_name", CryAudio::AbsoluteVelocityTrackingSwitchName);
							}
						}
					}
				}
			}

			pControl = m_pAssetsManager->FindControl(CryAudio::RelativeVelocityTrackingSwitchName, eItemType_Switch);
			if (pControl == nullptr)
			{
				pControl = m_pAssetsManager->FindControl("object_doppler_tracking", eItemType_Switch);
				if (pControl == nullptr)
				{
					CreateDefaultSwitch(pLibrary, CryAudio::RelativeVelocityTrackingSwitchName, CryAudio::RelativeVelocityTrackingSwitchName, states);
				}
				else
				{
					pControl->SetName(CryAudio::RelativeVelocityTrackingSwitchName);

					size_t const numChildren = pControl->ChildCount();
					for (size_t i = 0; i < numChildren; ++i)
					{
						CAudioControl* const pChild = static_cast<CAudioControl*>(pControl->GetChild(i));
						XMLNodeList const& nodeList = pChild->GetRawXMLConnections();

						for (auto const& node : nodeList)
						{
							if (_stricmp(node.xmlNode->getTag(), "ATLSwitchRequest") == 0)
							{
								node.xmlNode->setAttr("atl_name", CryAudio::RelativeVelocityTrackingSwitchName);
							}
						}
					}
				}
			}
		}

		if (pLibrary->ChildCount() == 0)
		{
			m_pAssetsManager->DeleteItem(pLibrary);
		}
	}
}

void CAudioControlsLoader::CreateDefaultSwitch(IAudioAsset* pLibrary, const char* szExternalName, const char* szInternalName, const SwitchStates& states)
{
	CAudioControl* pSwitch = m_pAssetsManager->FindControl(szExternalName, eItemType_Switch);
	if (pSwitch)
	{
		// Remove any states that shouldn't be part of the default control
		size_t childIndex = 0;
		size_t childCount = pSwitch->ChildCount();
		while (childIndex < childCount)
		{
			IAudioAsset* pChild = pSwitch->GetChild(childIndex);
			if (pChild)
			{
				bool bRemove = true;
				for (const auto& szStateName : states)
				{
					if (strcmp(pChild->GetName().c_str(), szStateName) == 0)
					{
						++childIndex;
						bRemove = false;
						break;
					}
				}
				if (bRemove)
				{
					pSwitch->RemoveChild(pChild);
					childCount = pSwitch->ChildCount();
				}
			}
		}
	}
	else
	{
		pSwitch = m_pAssetsManager->CreateControl(szExternalName, eItemType_Switch, pLibrary);
	}

	for (const auto& szStateName : states)
	{
		CAudioControl* pState = nullptr;

		const size_t stateCount = pSwitch->ChildCount();
		for (size_t i = 0; i < stateCount; ++i)
		{
			CAudioControl* pChild = static_cast<CAudioControl*>(pSwitch->GetChild(i));
			if (pChild && (strcmp(pChild->GetName().c_str(), szStateName) == 0) && pChild->GetType() == eItemType_State)
			{
				pState = pChild;
				break;
			}
		}

		if (pState == nullptr)
		{
			pState = m_pAssetsManager->CreateControl(szStateName, eItemType_State, pSwitch);

			XmlNodeRef pRequestNode = GetISystem()->CreateXmlNode("ATLSwitchRequest");
			pRequestNode->setAttr("atl_name", szInternalName);
			XmlNodeRef pValueNode = pRequestNode->createNode("ATLValue");
			pValueNode->setAttr("atl_name", szStateName);
			pRequestNode->addChild(pValueNode);

			pState->AddRawXMLConnection(pRequestNode, false);
		}
	}
}

void CAudioControlsLoader::LoadConnections(XmlNodeRef pRoot, CAudioControl* pControl)
{
	if (pControl != nullptr)
	{
		// The radius might change because of the attenuation matching option
		// so we check here to inform the user if their data is outdated.
		float radius = pControl->GetRadius();

		const int numChildren = pRoot->getChildCount();
		for (int i = 0; i < numChildren; ++i)
		{
			XmlNodeRef pNode = pRoot->getChild(i);
			pControl->LoadConnectionFromXML(pNode);
		}

		if (radius != pControl->GetRadius())
		{
			m_errorCodeMask |= eErrorCode_NonMatchedActivityRadius;
			pControl->SetModified(true, true);
		}
	}
}

void CAudioControlsLoader::LoadPreloadConnections(XmlNodeRef pNode, CAudioControl* pControl, uint version)
{
	if (pControl)
	{
		string type = pNode->getAttr("atl_type");
		if (type.compare("AutoLoad") == 0)
		{
			pControl->SetAutoLoad(true);
		}
		else
		{
			pControl->SetAutoLoad(false);
		}

		// Read the connection information for each of the platform groups
		const std::vector<dll_string>& platforms = GetIEditor()->GetConfigurationManager()->GetPlatformNames();
		const int numChildren = pNode->getChildCount();
		for (int i = 0; i < numChildren; ++i)
		{
			// Skip unused data from previous format
			XmlNodeRef pGroupNode = pNode->getChild(i);
			const string tag = pGroupNode->getTag();
			if (version == 1 && tag.compare("ATLConfigGroup") != 0)
			{
				continue;
			}
			// Get the index for that platform name
			int platformIndex = -1;
			const string platformName = pGroupNode->getAttr("atl_name");
			const size_t size = platforms.size();
			for (size_t j = 0; j < size; ++j)
			{
				if (strcmp(platformName.c_str(), platforms[j].c_str()) == 0)
				{
					platformIndex = j;
					break;
				}
			}

			if (platformIndex == -1)
			{
				m_errorCodeMask |= static_cast<uint>(EErrorCode::eErrorCode_UnkownPlatform);
				pControl->SetModified(true, true);
			}

			const int numConnections = pGroupNode->getChildCount();
			for (int j = 0; j < numConnections; ++j)
			{
				XmlNodeRef pConnectionNode = pGroupNode->getChild(j);
				if (pConnectionNode)
				{
					pControl->LoadConnectionFromXML(pConnectionNode, platformIndex);
				}
			}
		}
	}
}

void CAudioControlsLoader::LoadEditorData(XmlNodeRef pEditorDataNode, IAudioAsset* pRootItem)
{
	if (pEditorDataNode && pRootItem)
	{
		const int size = pEditorDataNode->getChildCount();
		for (int i = 0; i < size; ++i)
		{
			XmlNodeRef pChild = pEditorDataNode->getChild(i);
			if (pChild->isTag("Folders"))
			{
				LoadAllFolders(pChild, pRootItem);
			}
		}
	}
}

void CAudioControlsLoader::LoadAllFolders(XmlNodeRef pRootFoldersNode, IAudioAsset* pParentItem)
{
	if (pRootFoldersNode && pParentItem)
	{
		const int size = pRootFoldersNode->getChildCount();
		for (int i = 0; i < size; ++i)
		{
			LoadFolderData(pRootFoldersNode->getChild(i), pParentItem);
		}
	}
}

void CAudioControlsLoader::LoadFolderData(XmlNodeRef pFolderNode, IAudioAsset* pParentItem)
{
	IAudioAsset* pItem = AddUniqueFolderPath(pParentItem, pFolderNode->getAttr("name"));
	if (pItem)
	{
		const int size = pFolderNode->getChildCount();
		for (int i = 0; i < size; ++i)
		{
			LoadFolderData(pFolderNode->getChild(i), pItem);
		}
	}
}
}
