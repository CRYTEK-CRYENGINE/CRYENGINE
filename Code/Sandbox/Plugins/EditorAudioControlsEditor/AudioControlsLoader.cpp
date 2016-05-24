// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AudioControlsLoader.h"
#include <CryString/StringUtils.h>
#include <CrySystem/File/CryFile.h>
#include <CrySystem/ISystem.h>
#include "ATLControlsModel.h"
#include <CryString/CryPath.h>
#include <IAudioSystemEditor.h>
#include <IAudioSystemItem.h>
#include "QAudioControlTreeWidget.h"
#include "QtUtil.h"

#include <QStandardItem>
#include "QAudioControlTreeWidget.h"
#include "QATLControlsTreeModel.h"
#include "IEditor.h"
#include <ConfigurationManager.h>
#include <QMessageBox>
#include "AudioControlsEditorPlugin.h"

using namespace PathUtil;

namespace ACE
{
const string CAudioControlsLoader::ms_controlsPath = AUDIO_SYSTEM_DATA_ROOT CRY_NATIVE_PATH_SEPSTR "ace" CRY_NATIVE_PATH_SEPSTR;
const string CAudioControlsLoader::ms_levelsFolder = "levels" CRY_NATIVE_PATH_SEPSTR;
const string CAudioControlsLoader::ms_controlsLevelsFolder = "levels" CRY_NATIVE_PATH_SEPSTR;

EACEControlType TagToType(const string& tag)
{
	if (tag == "ATLSwitch")
	{
		return eACEControlType_Switch;
	}
	else if (tag == "ATLSwitchState")
	{
		return eACEControlType_State;
	}
	else if (tag == "ATLEnvironment")
	{
		return eACEControlType_Environment;
	}
	else if (tag == "ATLRtpc")
	{
		return eACEControlType_RTPC;
	}
	else if (tag == "ATLTrigger")
	{
		return eACEControlType_Trigger;
	}
	else if (tag == "ATLPreloadRequest")
	{
		return eACEControlType_Preload;
	}
	return eACEControlType_NumTypes;
}

CAudioControlsLoader::CAudioControlsLoader(CATLControlsModel* pATLModel, QATLTreeModel* pLayoutModel, IAudioSystemEditor* pAudioSystemImpl)
	: m_pModel(pATLModel)
	, m_pLayout(pLayoutModel)
	, m_pAudioSystemImpl(pAudioSystemImpl)
	, m_errorCodeMask(EErrorCode::eErrorCode_NoError)
{}

void CAudioControlsLoader::LoadAll()
{
	LoadScopes();
	LoadControls();
}

void CAudioControlsLoader::LoadControls()
{
	const CUndoSuspend suspendUndo;

	// load the global controls
	LoadAllLibrariesInFolder(ms_controlsPath, "");

	// load the level specific controls
	_finddata_t fd;
	ICryPak* pCryPak = gEnv->pCryPak;
	intptr_t handle = pCryPak->FindFirst(ms_controlsPath + ms_controlsLevelsFolder + "*.*", &fd);
	if (handle != -1)
	{
		do
		{
			if (fd.attrib & _A_SUBDIR)
			{
				string name = fd.name;
				if (name != "." && name != "..")
				{
					LoadAllLibrariesInFolder(ms_controlsPath, name);
					if (!m_pModel->ScopeExists(fd.name))
					{
						// if the control doesn't exist it
						// means it is not a real level in the
						// project so it is flagged as LocalOnly
						m_pModel->AddScope(fd.name, true);
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

QStandardItem* CAudioControlsLoader::AddFolder(QStandardItem* pParent, const QString& sName)
{
	if (pParent && !sName.isEmpty())
	{
		const int size = pParent->rowCount();
		for (int i = 0; i < size; ++i)
		{
			QStandardItem* pItem = pParent->child(i);
			if (pItem && (pItem->data(eDataRole_Type) == eItemType_Folder) && (QString::compare(sName, pItem->text(), Qt::CaseInsensitive) == 0))
			{
				return pItem;
			}
		}

		QStandardItem* pItem = new QFolderItem(sName);
		if (pParent && pItem)
		{
			pParent->appendRow(pItem);
			return pItem;
		}
	}
	return nullptr;
}

QStandardItem* CAudioControlsLoader::AddUniqueFolderPath(QStandardItem* pParent, const QString& sPath)
{
	QStringList folderNames = sPath.split(QRegExp("(\\\\|\\/)"), QString::SkipEmptyParts);
	const int size = folderNames.length();
	for (int i = 0; i < size; ++i)
	{
		if (!folderNames[i].isEmpty())
		{
			QStandardItem* pChild = AddFolder(pParent, folderNames[i]);
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
	QStandardItem* pRootFolder = AddUniqueFolderPath(m_pLayout->ControlsRootItem(), QtUtil::ToQString(filename));
	if (pRootFolder && pRoot)
	{
		const int nControlTypeCount = pRoot->getChildCount();
		for (int i = 0; i < nControlTypeCount; ++i)
		{
			XmlNodeRef pNode = pRoot->getChild(i);
			if (pNode)
			{
				if (pNode->isTag("EditorData"))
				{
					LoadEditorData(pNode, pRootFolder);
				}
				else
				{
					Scope scope = level.empty() ? m_pModel->GetGlobalScope() : m_pModel->GetScope(level);
					const int nControlCount = pNode->getChildCount();
					for (int j = 0; j < nControlCount; ++j)
					{
						LoadControl(pNode->getChild(j), pRootFolder, scope, version);
					}
				}
			}
		}
	}
}

CATLControl* CAudioControlsLoader::LoadControl(XmlNodeRef pNode, QStandardItem* pFolder, Scope scope, uint version)
{
	CATLControl* pControl = nullptr;
	if (pNode)
	{
		QStandardItem* pParent = AddUniqueFolderPath(pFolder, QtUtil::ToQString(pNode->getAttr("path")));
		if (pParent)
		{
			const string sName = pNode->getAttr("atl_name");
			const EACEControlType eControlType = TagToType(pNode->getTag());
			pControl = m_pModel->CreateControl(sName, eControlType);
			if (pControl)
			{
				QStandardItem* pItem = new QAudioControlItem(QtUtil::ToQString(pControl->GetName()), pControl);
				if (pItem)
				{
					pParent->appendRow(pItem);
				}

				switch (eControlType)
				{
				case eACEControlType_Switch:
					{
						const int nStateCount = pNode->getChildCount();
						for (int i = 0; i < nStateCount; ++i)
						{
							CATLControl* pStateControl = LoadControl(pNode->getChild(i), pItem, scope, version);
							if (pStateControl)
							{
								pStateControl->SetParent(pControl);
								pControl->AddChild(pStateControl);
							}
						}
					}
					break;
				case eACEControlType_Preload:
					LoadPreloadConnections(pNode, pControl, pItem, version);
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
						m_pModel->AddScope(name);
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
	// Load default controls if the don't exist.
	// These controls need to always exist in your project

	QStandardItem* pFolder = AddFolder(m_pLayout->ControlsRootItem(), "default_controls");
	if (pFolder)
	{
		CATLControl* pControl = m_pModel->FindControl("get_focus", eACEControlType_Trigger, m_pModel->GetGlobalScope());
		if (pControl == nullptr)
		{
			AddControl(m_pModel->CreateControl("get_focus", eACEControlType_Trigger), pFolder);
		}

		pControl = m_pModel->FindControl("lose_focus", eACEControlType_Trigger, m_pModel->GetGlobalScope());
		if (pControl == nullptr)
		{
			AddControl(m_pModel->CreateControl("lose_focus", eACEControlType_Trigger), pFolder);
		}

		pControl = m_pModel->FindControl("mute_all", eACEControlType_Trigger, m_pModel->GetGlobalScope());
		if (pControl == nullptr)
		{
			AddControl(m_pModel->CreateControl("mute_all", eACEControlType_Trigger), pFolder);
		}

		pControl = m_pModel->FindControl("unmute_all", eACEControlType_Trigger, m_pModel->GetGlobalScope());
		if (pControl == nullptr)
		{
			AddControl(m_pModel->CreateControl("unmute_all", eACEControlType_Trigger), pFolder);
		}

		pControl = m_pModel->FindControl("do_nothing", eACEControlType_Trigger, m_pModel->GetGlobalScope());
		if (pControl == nullptr)
		{
			AddControl(m_pModel->CreateControl("do_nothing", eACEControlType_Trigger), pFolder);
		}

		pControl = m_pModel->FindControl("object_speed", eACEControlType_RTPC, m_pModel->GetGlobalScope());
		if (pControl == nullptr)
		{
			AddControl(m_pModel->CreateControl("object_speed", eACEControlType_RTPC), pFolder);
		}

		pControl = m_pModel->FindControl("object_doppler", eACEControlType_RTPC, m_pModel->GetGlobalScope());
		if (pControl == nullptr)
		{
			AddControl(m_pModel->CreateControl("object_doppler", eACEControlType_RTPC), pFolder);
		}

		{
			const char* arr[] = { "Ignore", "SingleRay", "MultiRay" };
			std::vector<const char*> states(arr, arr + sizeof(arr) / sizeof(arr[0]));
			CreateDefaultSwitch(pFolder, "ObstrOcclCalcType", "ObstructionOcclusionCalculationType", states);
		}
		{
			const char* arr[] = { "on", "off" };
			std::vector<const char*> states(arr, arr + sizeof(arr) / sizeof(arr[0]));
			CreateDefaultSwitch(pFolder, "object_velocity_tracking", "object_velocity_tracking", states);
			CreateDefaultSwitch(pFolder, "object_doppler_tracking", "object_doppler_tracking", states);
		}

		if (!pFolder->hasChildren())
		{
			m_pLayout->removeRow(pFolder->row(), m_pLayout->indexFromItem(pFolder->parent()));
		}
	}
}

void CAudioControlsLoader::CreateDefaultSwitch(QStandardItem* pFolder, const char* szExternalName, const char* szInternalName, const std::vector<const char*>& states)
{
	CATLControl* pControl = m_pModel->FindControl(szExternalName, eACEControlType_Switch, m_pModel->GetGlobalScope());
	QStandardItem* pSwitch = nullptr;
	if (pControl)
	{
		QModelIndexList indexes = m_pLayout->match(m_pLayout->index(0, 0, QModelIndex()), eDataRole_Id, pControl->GetId(), 1, Qt::MatchRecursive);
		if (!indexes.empty())
		{
			pSwitch = m_pLayout->itemFromIndex(indexes.at(0));
		}
	}
	else
	{
		pControl = m_pModel->CreateControl(szExternalName, eACEControlType_Switch);
		pSwitch = AddControl(pControl, pFolder);
	}
	if (pSwitch)
	{
		for (auto szStateName : states)
		{
			CATLControl* pChild = m_pModel->FindControl(szStateName, eACEControlType_State, m_pModel->GetGlobalScope(), pControl);
			if (pChild == nullptr)
			{
				pChild = m_pModel->CreateControl(szStateName, eACEControlType_State, pControl);

				XmlNodeRef pRequestNode = GetISystem()->CreateXmlNode("ATLSwitchRequest");
				pRequestNode->setAttr("atl_name", szInternalName);
				XmlNodeRef pValueNode = pRequestNode->createNode("ATLValue");
				pValueNode->setAttr("atl_name", szStateName);
				pRequestNode->addChild(pValueNode);

				pChild->AddRawXMLConnection(pRequestNode, false);
				AddControl(pChild, pSwitch);
			}
		}
	}
}

void CAudioControlsLoader::LoadConnections(XmlNodeRef pRoot, CATLControl* pControl)
{
	if (pControl)
	{
		const int nSize = pRoot->getChildCount();
		for (int i = 0; i < nSize; ++i)
		{
			XmlNodeRef pNode = pRoot->getChild(i);
			pControl->LoadConnectionFromXML(pNode);
		}
	}
}

void CAudioControlsLoader::LoadPreloadConnections(XmlNodeRef pNode, CATLControl* pControl, QStandardItem* pItem, uint version)
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
				pItem->setData(true, eDataRole_Modified);
			}

			const int numConnections = pGroupNode->getChildCount();
			for (int j = 0; j < numConnections; ++j)
			{
				XmlNodeRef pConnectionNode = pGroupNode->getChild(j);
				if (pConnectionNode && m_pAudioSystemImpl)
				{
					pControl->LoadConnectionFromXML(pConnectionNode, platformIndex);
				}
			}
		}
	}
}

QStandardItem* CAudioControlsLoader::AddControl(CATLControl* pControl, QStandardItem* pFolder)
{
	QStandardItem* pItem = new QAudioControlItem(QtUtil::ToQString(pControl->GetName()), pControl);
	if (pItem)
	{
		pItem->setData(true, eDataRole_Modified);
		pFolder->appendRow(pItem);
	}
	return pItem;
}

void CAudioControlsLoader::LoadEditorData(XmlNodeRef pEditorDataNode, QStandardItem* pRootItem)
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

void CAudioControlsLoader::LoadAllFolders(XmlNodeRef pRootFoldersNode, QStandardItem* pParentItem)
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

void CAudioControlsLoader::LoadFolderData(XmlNodeRef pFolderNode, QStandardItem* pParentItem)
{
	QStandardItem* pItem = AddUniqueFolderPath(pParentItem, pFolderNode->getAttr("name"));
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
