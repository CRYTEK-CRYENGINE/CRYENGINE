// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AudioControlsLoader.h"

#include "SystemAssetsManager.h"
#include "SystemControlsModel.h"
#include "AudioControlsEditorPlugin.h"

#include <CryAudio/IAudioSystem.h>
#include <CryString/StringUtils.h>
#include <CrySystem/File/CryFile.h>
#include <CrySystem/ISystem.h>
#include <CryString/CryPath.h>
#include <QtUtil.h>
#include <IUndoObject.h>
#include <IEditor.h>
#include <ConfigurationManager.h>

#include <QRegularExpression>

namespace ACE
{
string const CAudioControlsLoader::s_levelsFolder = "levels" CRY_NATIVE_PATH_SEPSTR;
string const CAudioControlsLoader::s_controlsLevelsFolder = "levels" CRY_NATIVE_PATH_SEPSTR;

//////////////////////////////////////////////////////////////////////////
ESystemItemType TagToType(string const& tag)
{
	if (tag == "ATLSwitch")
	{
		return ESystemItemType::Switch;
	}
	else if (tag == "ATLSwitchState")
	{
		return ESystemItemType::State;
	}
	else if (tag == "ATLEnvironment")
	{
		return ESystemItemType::Environment;
	}
	else if (tag == "ATLRtpc")
	{
		return ESystemItemType::Parameter;
	}
	else if (tag == "ATLTrigger")
	{
		return ESystemItemType::Trigger;
	}
	else if (tag == "ATLPreloadRequest")
	{
		return ESystemItemType::Preload;
	}
	return ESystemItemType::Invalid;
}

//////////////////////////////////////////////////////////////////////////
CAudioControlsLoader::CAudioControlsLoader(CSystemAssetsManager* pAssetsManager)
	: m_pAssetsManager(pAssetsManager)
	, m_errorCodeMask(EErrorCode::NoError)
{}

//////////////////////////////////////////////////////////////////////////
void CAudioControlsLoader::LoadAll()
{
	LoadScopes();
	LoadControls();
}

//////////////////////////////////////////////////////////////////////////
void CAudioControlsLoader::LoadControls()
{
	CScopedSuspendUndo const suspendUndo;

	// load the global controls
	LoadAllLibrariesInFolder(Utils::GetAssetFolder(), "");

	// load the level specific controls
	_finddata_t fd;
	ICryPak* pCryPak = gEnv->pCryPak;
	intptr_t handle = pCryPak->FindFirst(Utils::GetAssetFolder() + s_controlsLevelsFolder + "*.*", &fd);

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

//////////////////////////////////////////////////////////////////////////
void CAudioControlsLoader::LoadAllLibrariesInFolder(string const& folderPath, string const& level)
{
	string path = folderPath;

	if (!level.empty())
	{
		path = path + s_controlsLevelsFolder + level + CRY_NATIVE_PATH_SEPSTR;
	}

	string const& searchPath = path + "*.xml";
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
				string const& tag = root->getTag();

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
CSystemAsset* CAudioControlsLoader::AddUniqueFolderPath(CSystemAsset* pParent, QString const& path)
{
	QStringList folderNames = path.split(QRegularExpression(R"((\\|\/))"), QString::SkipEmptyParts);

	int const size = folderNames.length();

	for (int i = 0; i < size; ++i)
	{
		if (!folderNames[i].isEmpty())
		{
			CSystemAsset* const pChild = m_pAssetsManager->CreateFolder(QtUtil::ToString(folderNames[i]), pParent);

			if (pChild != nullptr)
			{
				pParent = pChild;
			}
		}
	}

	return pParent;
}

//////////////////////////////////////////////////////////////////////////
void CAudioControlsLoader::LoadControlsLibrary(XmlNodeRef const pRoot, string const& filepath, string const& level, string const& filename, uint const version)
{
  // Always create a library file, even if no proper formatting is present.
	CSystemLibrary* const pLibrary = m_pAssetsManager->CreateLibrary(filename);

	if ((pRoot != nullptr) && (pLibrary != nullptr))
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
					Scope const scope = level.empty() ? Utils::GetGlobalScope() : m_pAssetsManager->GetScope(level);
					int const numControls = pNode->getChildCount();

					for (int j = 0; j < numControls; ++j)
					{
						LoadControl(pNode->getChild(j), scope, version, pLibrary);
					}
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
CSystemControl* CAudioControlsLoader::LoadControl(XmlNodeRef const pNode, Scope scope, uint version, CSystemAsset* const pParentItem)
{
	CSystemControl* pControl = nullptr;

	if (pNode != nullptr)
	{
		CSystemAsset* const pFolderItem = AddUniqueFolderPath(pParentItem, QtUtil::ToQString(pNode->getAttr("path")));

		if (pFolderItem != nullptr)
		{
			string const name = pNode->getAttr("atl_name");
			ESystemItemType const controlType = TagToType(pNode->getTag());
			pControl = m_pAssetsManager->CreateControl(name, controlType, pFolderItem);

			if (pControl != nullptr)
			{
				switch (controlType)
				{
				case ESystemItemType::Trigger:
					{
						float radius = 0.0f;
						pNode->getAttr("atl_radius", radius);
						pControl->SetRadius(radius);

						float occlusionFadeOutDistance = 0.0f;
						pNode->getAttr("atl_occlusion_fadeout_distance", occlusionFadeOutDistance);
						pControl->SetOcclusionFadeOutDistance(occlusionFadeOutDistance);

						bool matchRadiusToAttenuation = true;
						pNode->getAttr("atl_match_radius_attenuation", matchRadiusToAttenuation);
						pControl->SetMatchRadiusToAttenuationEnabled(matchRadiusToAttenuation);

						LoadConnections(pNode, pControl);
					}
					break;
				case ESystemItemType::Switch:
					{
						int const stateCount = pNode->getChildCount();

						for (int i = 0; i < stateCount; ++i)
						{
							LoadControl(pNode->getChild(i), scope, version, pControl);
						}
					}
					break;
				case ESystemItemType::Preload:
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

//////////////////////////////////////////////////////////////////////////
void CAudioControlsLoader::LoadScopes()
{
	LoadScopesImpl(s_levelsFolder);
}

//////////////////////////////////////////////////////////////////////////
void CAudioControlsLoader::LoadScopesImpl(string const& sLevelsFolder)
{
	// TODO: consider moving the file enumeration to a background thread to speed up the editor startup time.

	_finddata_t fd;
	ICryPak* const pCryPak = gEnv->pCryPak;
	intptr_t const handle = pCryPak->FindFirst(sLevelsFolder + CRY_NATIVE_PATH_SEPSTR "*.*", &fd);

	if (handle != -1)
	{
		do
		{
			string name = fd.name;

			if ((name != ".") && (name != "..") && !name.empty())
			{
				if (fd.attrib & _A_SUBDIR)
				{
					LoadScopesImpl(sLevelsFolder + CRY_NATIVE_PATH_SEPSTR + name);
				}
				else
				{
					if (strcmp(PathUtil::GetExt(name), "level") == 0)
					{
						PathUtil::RemoveExtension(name);
						m_pAssetsManager->AddScope(name);
					}
				}
			}
		}
		while (pCryPak->FindNext(handle, &fd) >= 0);

		pCryPak->FindClose(handle);
	}
}

//////////////////////////////////////////////////////////////////////////
std::set<string> CAudioControlsLoader::GetLoadedFilenamesList()
{
	return m_loadedFilenames;
}

//////////////////////////////////////////////////////////////////////////
void CAudioControlsLoader::CreateDefaultControls()
{
	// Create default controls if they don't exist.
	// These controls need to always exist in your project!
	bool wasModified = false;
	CSystemAsset* const pLibrary = static_cast<CSystemAsset*>(m_pAssetsManager->CreateLibrary("default_controls"));

	if (pLibrary != nullptr)
	{
		CSystemControl* pControl = m_pAssetsManager->FindControl(CryAudio::GetFocusTriggerName, ESystemItemType::Trigger);

		if (pControl == nullptr)
		{
			m_pAssetsManager->CreateControl(CryAudio::GetFocusTriggerName, ESystemItemType::Trigger, pLibrary);
			wasModified = true;
		}

		pControl = m_pAssetsManager->FindControl(CryAudio::LoseFocusTriggerName, ESystemItemType::Trigger);

		if (pControl == nullptr)
		{
			m_pAssetsManager->CreateControl(CryAudio::LoseFocusTriggerName, ESystemItemType::Trigger, pLibrary);
			wasModified = true;
		}

		pControl = m_pAssetsManager->FindControl(CryAudio::MuteAllTriggerName, ESystemItemType::Trigger);

		if (pControl == nullptr)
		{
			m_pAssetsManager->CreateControl(CryAudio::MuteAllTriggerName, ESystemItemType::Trigger, pLibrary);
			wasModified = true;
		}

		pControl = m_pAssetsManager->FindControl(CryAudio::UnmuteAllTriggerName, ESystemItemType::Trigger);

		if (pControl == nullptr)
		{
			m_pAssetsManager->CreateControl(CryAudio::UnmuteAllTriggerName, ESystemItemType::Trigger, pLibrary);
			wasModified = true;
		}

		pControl = m_pAssetsManager->FindControl(CryAudio::DoNothingTriggerName, ESystemItemType::Trigger);

		if (pControl == nullptr)
		{
			m_pAssetsManager->CreateControl(CryAudio::DoNothingTriggerName, ESystemItemType::Trigger, pLibrary);
			wasModified = true;
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
		pControl = m_pAssetsManager->FindControl(CryAudio::AbsoluteVelocityParameterName, ESystemItemType::Parameter);

		if (pControl == nullptr)
		{
			pControl = m_pAssetsManager->FindControl("object_speed", ESystemItemType::Parameter);

			if (pControl == nullptr)
			{
				m_pAssetsManager->CreateControl(CryAudio::AbsoluteVelocityParameterName, ESystemItemType::Parameter, pLibrary);
			}
			else
			{
				pControl->SetName(CryAudio::AbsoluteVelocityParameterName);
			}

			wasModified = true;
		}

		pControl = m_pAssetsManager->FindControl(CryAudio::RelativeVelocityParameterName, ESystemItemType::Parameter);

		if (pControl == nullptr)
		{
			pControl = m_pAssetsManager->FindControl("object_doppler", ESystemItemType::Parameter);

			if (pControl == nullptr)
			{
				m_pAssetsManager->CreateControl(CryAudio::RelativeVelocityParameterName, ESystemItemType::Parameter, pLibrary);
			}
			else
			{
				pControl->SetName(CryAudio::RelativeVelocityParameterName);
			}

			wasModified = true;
		}

		{
			char const* const arr[] = { CryAudio::IgnoreStateName, CryAudio::AdaptiveStateName, CryAudio::LowStateName, CryAudio::MediumStateName, CryAudio::HighStateName };
			SwitchStates const states(arr, arr + sizeof(arr) / sizeof(arr[0]));
			CreateDefaultSwitch(pLibrary, "ObstrOcclCalcType", CryAudio::OcclusionCalcSwitchName, states);
		}
		{
			char const* const arr[] = { CryAudio::OnStateName, CryAudio::OffStateName };
			SwitchStates const states(arr, arr + sizeof(arr) / sizeof(arr[0]));

			pControl = m_pAssetsManager->FindControl(CryAudio::AbsoluteVelocityTrackingSwitchName, ESystemItemType::Switch);

			if (pControl == nullptr)
			{
				pControl = m_pAssetsManager->FindControl("object_velocity_tracking", ESystemItemType::Switch);

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
						CSystemControl* const pChild = static_cast<CSystemControl*>(pControl->GetChild(i));
						XMLNodeList const& nodeList = pChild->GetRawXMLConnections();

						for (auto const& node : nodeList)
						{
							if (_stricmp(node.xmlNode->getTag(), "ATLSwitchRequest") == 0)
							{
								node.xmlNode->setAttr("atl_name", CryAudio::AbsoluteVelocityTrackingSwitchName);
							}
						}
					}

					pControl->SetHiddenDefault(true);
				}

				wasModified = true;
			}
			else
			{
				pControl->SetHiddenDefault(true);
			}

			pControl = m_pAssetsManager->FindControl(CryAudio::RelativeVelocityTrackingSwitchName, ESystemItemType::Switch);

			if (pControl == nullptr)
			{
				pControl = m_pAssetsManager->FindControl("object_doppler_tracking", ESystemItemType::Switch);

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
						CSystemControl* const pChild = static_cast<CSystemControl*>(pControl->GetChild(i));
						XMLNodeList const& nodeList = pChild->GetRawXMLConnections();

						for (auto const& node : nodeList)
						{
							if (_stricmp(node.xmlNode->getTag(), "ATLSwitchRequest") == 0)
							{
								node.xmlNode->setAttr("atl_name", CryAudio::RelativeVelocityTrackingSwitchName);
							}
						}
					}

					pControl->SetHiddenDefault(true);
				}

				wasModified = true;
			}
			else
			{
				pControl->SetHiddenDefault(true);
			}
		}

		if (pLibrary->ChildCount() == 0)
		{
			m_pAssetsManager->DeleteItem(pLibrary);
		}
		else if (wasModified)
		{
			pLibrary->SetModified(true, true);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioControlsLoader::CreateDefaultSwitch(CSystemAsset* const pLibrary, char const* const szExternalName, char const* const szInternalName, SwitchStates const& states)
{
	CSystemControl* pSwitch = m_pAssetsManager->FindControl(szExternalName, ESystemItemType::Switch);

	if (pSwitch != nullptr)
	{
		// Remove any states that shouldn't be part of the default control
		size_t childIndex = 0;
		size_t childCount = pSwitch->ChildCount();

		while (childIndex < childCount)
		{
			CSystemAsset const* const pChild = pSwitch->GetChild(childIndex);

			if (pChild != nullptr)
			{
				bool shouldRemoveChild = true;

				for (auto const& szStateName : states)
				{
					if (strcmp(pChild->GetName().c_str(), szStateName) == 0)
					{
						++childIndex;
						shouldRemoveChild = false;
						break;
					}
				}

				if (shouldRemoveChild)
				{
					pSwitch->RemoveChild(pChild);
					childCount = pSwitch->ChildCount();
				}
			}
		}
	}
	else
	{
		pSwitch = m_pAssetsManager->CreateControl(szExternalName, ESystemItemType::Switch, pLibrary);
	}

	for (auto const& szStateName : states)
	{
		CSystemControl* pState = nullptr;

		size_t const stateCount = pSwitch->ChildCount();

		for (size_t i = 0; i < stateCount; ++i)
		{
			CSystemControl* pChild = static_cast<CSystemControl*>(pSwitch->GetChild(i));
			if ((pChild != nullptr) && (strcmp(pChild->GetName().c_str(), szStateName) == 0) && (pChild->GetType() == ESystemItemType::State))
			{
				pState = pChild;
				break;
			}
		}

		if (pState == nullptr)
		{
			pState = m_pAssetsManager->CreateControl(szStateName, ESystemItemType::State, pSwitch);

			XmlNodeRef const pRequestNode = GetISystem()->CreateXmlNode("ATLSwitchRequest");
			pRequestNode->setAttr("atl_name", szInternalName);
			XmlNodeRef const pValueNode = pRequestNode->createNode("ATLValue");
			pValueNode->setAttr("atl_name", szStateName);
			pRequestNode->addChild(pValueNode);

			pState->AddRawXMLConnection(pRequestNode, false);
		}
	}

	pSwitch->SetHiddenDefault(true);
}

//////////////////////////////////////////////////////////////////////////
void CAudioControlsLoader::LoadConnections(XmlNodeRef const pRoot, CSystemControl* const pControl)
{
	if (pControl != nullptr)
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
}

//////////////////////////////////////////////////////////////////////////
void CAudioControlsLoader::LoadPreloadConnections(XmlNodeRef const pNode, CSystemControl* const pControl, uint const version)
{
	if (pControl != nullptr)
	{
		string const& type = pNode->getAttr("atl_type");

		if (type.compare("AutoLoad") == 0)
		{
			pControl->SetAutoLoad(true);
		}
		else
		{
			pControl->SetAutoLoad(false);
		}

		// Read the connection information for each of the platform groups
		std::vector<dll_string> const& platforms = GetIEditor()->GetConfigurationManager()->GetPlatformNames();
		int const numChildren = pNode->getChildCount();

		for (int i = 0; i < numChildren; ++i)
		{
			// Skip unused data from previous format
			XmlNodeRef const pGroupNode = pNode->getChild(i);
			string const& tag = pGroupNode->getTag();

			if ((version == 1) && (tag.compare("ATLConfigGroup") != 0))
			{
				continue;
			}

			// Get the index for that platform name
			int platformIndex = -1;
			string const platformName = pGroupNode->getAttr("atl_name");
			size_t const size = platforms.size();

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
}

//////////////////////////////////////////////////////////////////////////
void CAudioControlsLoader::LoadEditorData(XmlNodeRef const pEditorDataNode, CSystemAsset& library)
{
	int const size = pEditorDataNode->getChildCount();

	for (int i = 0; i < size; ++i)
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
void CAudioControlsLoader::LoadLibraryEditorData(XmlNodeRef const pLibraryNode, CSystemAsset& library)
{
	string description = "";
	pLibraryNode->getAttr("description", description);

	if (!description.IsEmpty())
	{
		library.SetDescription(description);
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioControlsLoader::LoadAllFolders(XmlNodeRef const pFoldersNode, CSystemAsset& library)
{
	if (pFoldersNode != nullptr)
	{
		int const size = pFoldersNode->getChildCount();

		for (int i = 0; i < size; ++i)
		{
			LoadFolderData(pFoldersNode->getChild(i), library);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioControlsLoader::LoadFolderData(XmlNodeRef const pFolderNode, CSystemAsset& parentAsset)
{
	CSystemAsset* const pItem = AddUniqueFolderPath(&parentAsset, pFolderNode->getAttr("name"));

	if (pItem != nullptr)
	{
		string description = "";
		pFolderNode->getAttr("description", description);

		if (!description.IsEmpty())
		{
			pItem->SetDescription(description);
		}

		int const size = pFolderNode->getChildCount();

		for (int i = 0; i < size; ++i)
		{
			LoadFolderData(pFolderNode->getChild(i), *pItem);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioControlsLoader::LoadAllControlsEditorData(XmlNodeRef const pControlsNode)
{
	if (pControlsNode != nullptr)
	{
		int const size = pControlsNode->getChildCount();

		for (int i = 0; i < size; ++i)
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
		ESystemItemType const controlType = TagToType(pParentNode->getTag());
		string description = "";
		pParentNode->getAttr("description", description);

		if ((controlType != ESystemItemType::Invalid) && !description.IsEmpty())
		{
			 CSystemControl* const pControl = m_pAssetsManager->FindControl(pParentNode->getAttr("name"), controlType);

			if (pControl != nullptr)
			{
				pControl->SetDescription(description);
			}
		}
	}
}
} // namespace ACE
