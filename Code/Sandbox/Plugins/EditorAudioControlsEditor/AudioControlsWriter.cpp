// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AudioControlsWriter.h"
#include <CryString/StringUtils.h>
#include <CrySystem/File/CryFile.h>
#include <CrySystem/ISystem.h>
#include "ATLControlsModel.h"
#include <ISourceControl.h>
#include <IEditor.h>
#include <IAudioSystemEditor.h>
#include <IAudioSystemItem.h>
#include "QtUtil.h"
#include <ConfigurationManager.h>

#include <QModelIndex>
#include <QStandardItemModel>

using namespace PathUtil;

namespace ACE
{

const string CAudioControlsWriter::ms_sControlsPath = AUDIO_SYSTEM_DATA_ROOT CRY_NATIVE_PATH_SEPSTR "ace" CRY_NATIVE_PATH_SEPSTR;
const string CAudioControlsWriter::ms_sLevelsFolder = "levels" CRY_NATIVE_PATH_SEPSTR;
const uint CAudioControlsWriter::ms_currentFileVersion = 2;

string TypeToTag(EACEControlType eType)
{
	switch (eType)
	{
	case eACEControlType_RTPC:
		return "ATLRtpc";
	case eACEControlType_Trigger:
		return "ATLTrigger";
	case eACEControlType_Switch:
		return "ATLSwitch";
	case eACEControlType_State:
		return "ATLSwitchState";
	case eACEControlType_Preload:
		return "ATLPreloadRequest";
	case eACEControlType_Environment:
		return "ATLEnvironment";
	}
	return "";
}

CAudioControlsWriter::CAudioControlsWriter(CATLControlsModel* pATLModel, QStandardItemModel* pLayoutModel, IAudioSystemEditor* pAudioSystemImpl, std::set<string>& previousLibraryPaths)
	: m_pATLModel(pATLModel)
	, m_pLayoutModel(pLayoutModel)
	, m_pAudioSystemImpl(pAudioSystemImpl)
{
	if (pATLModel && pLayoutModel && pAudioSystemImpl)
	{
		m_pLayoutModel->blockSignals(true);

		int i = 0;
		QModelIndex root = pLayoutModel->index(0, 0);
		if (root.isValid())
		{
			QModelIndex index = root.child(0, 0);
			while (index.isValid())
			{
				WriteLibrary(QtUtil::ToString(index.data(Qt::DisplayRole).toString()), index);
				++i;
				index = index.sibling(i, 0);
			}

			// Delete libraries that don't exist anymore from disk
			std::set<string> librariesToDelete;
			std::set_difference(previousLibraryPaths.begin(), previousLibraryPaths.end(), m_foundLibraryPaths.begin(), m_foundLibraryPaths.end(),
			                    std::inserter(librariesToDelete, librariesToDelete.begin()));

			for (auto it = librariesToDelete.begin(); it != librariesToDelete.end(); ++it)
			{
				string sFullFilePath = PathUtil::GetGameFolder() + CRY_NATIVE_PATH_SEPSTR + *it;
				DeleteLibraryFile(sFullFilePath);
			}
			previousLibraryPaths = m_foundLibraryPaths;
		}

		m_pLayoutModel->blockSignals(false);
	}
}

void CAudioControlsWriter::WriteLibrary(const string& sLibraryName, QModelIndex root)
{
	if (root.isValid())
	{
		TLibraryStorage library;
		int i = 0;
		QModelIndex child = root.child(0, 0);
		while (child.isValid())
		{
			WriteItem(child, "", library, root.data(eDataRole_Modified).toBool());
			child = root.child(++i, 0);
		}

		// If empty, force it to write an empty library at the root
		if (library.empty())
		{
			library[m_pATLModel->GetGlobalScope()].bDirty = true;
		}

		TLibraryStorage::const_iterator it = library.begin();
		TLibraryStorage::const_iterator end = library.end();
		for (; it != end; ++it)
		{
			string sLibraryPath;
			const Scope scope = it->first;
			if (scope == m_pATLModel->GetGlobalScope())
			{
				// no scope, file at the root level
				sLibraryPath = ms_sControlsPath + sLibraryName;
			}
			else
			{
				// with scope, inside level folder
				sLibraryPath = ms_sControlsPath + ms_sLevelsFolder + m_pATLModel->GetScopeInfo(scope).name + CRY_NATIVE_PATH_SEPSTR + sLibraryName;
			}

			m_foundLibraryPaths.insert(sLibraryPath.MakeLower() + ".xml");

			const SLibraryScope& libScope = it->second;
			if (libScope.bDirty)
			{
				XmlNodeRef pFileNode = GetISystem()->CreateXmlNode("ATLConfig");
				pFileNode->setAttr("atl_name", sLibraryName);
				pFileNode->setAttr("atl_version", ms_currentFileVersion);

				for (int i = 0; i < eACEControlType_NumTypes; ++i)
				{
					if (i != eACEControlType_State)   // switch_states are written inside the switches
					{
						if (libScope.pNodes[i]->getChildCount() > 0)
						{
							pFileNode->addChild(libScope.pNodes[i]);
						}
					}
				}

				// Editor data
				XmlNodeRef pEditorData = pFileNode->createNode("EditorData");
				if (pEditorData)
				{
					XmlNodeRef pFoldersNode = pEditorData->createNode("Folders");
					if (pFoldersNode)
					{
						WriteEditorData(root, pFoldersNode);
						pEditorData->addChild(pFoldersNode);
					}
					pFileNode->addChild(pEditorData);
				}

				const string sFullFilePath = PathUtil::GetGameFolder() + CRY_NATIVE_PATH_SEPSTR + sLibraryPath + ".xml";

				const DWORD fileAttributes = GetFileAttributesA(sFullFilePath.c_str());
				if (fileAttributes & FILE_ATTRIBUTE_READONLY)
				{
					// file is read-only
					SetFileAttributesA(sFullFilePath.c_str(), FILE_ATTRIBUTE_NORMAL);
				}
				pFileNode->saveToFile(sFullFilePath);
				CheckOutFile(sFullFilePath);
			}
		}
	}
}

void CAudioControlsWriter::WriteItem(QModelIndex index, const string& path, TLibraryStorage& library, bool bParentModified)
{
	if (index.isValid())
	{
		if (index.data(eDataRole_Type) == eItemType_Folder)
		{
			int i = 0;
			QModelIndex child = index.child(0, 0);
			while (child.isValid())
			{
				// Use forward slash only to ensure cross platform compatibility.
				string sNewPath = path.empty() ? "" : path + "/";
				sNewPath += QtUtil::ToString(index.data(Qt::DisplayRole).toString());
				WriteItem(child, sNewPath, library, index.data(eDataRole_Modified).toBool() || bParentModified);
				child = index.child(++i, 0);
			}
			QStandardItem* pItem = m_pLayoutModel->itemFromIndex(index);
			if (pItem)
			{
				pItem->setData(false, eDataRole_Modified);
			}
		}
		else
		{
			CATLControl* pControl = m_pATLModel->GetControlByID(index.data(eDataRole_Id).toUInt());
			if (pControl)
			{
				SLibraryScope& scope = library[pControl->GetScope()];
				if (IsItemModified(index) || bParentModified)
				{
					scope.bDirty = true;
					QStandardItem* pItem = m_pLayoutModel->itemFromIndex(index);
					if (pItem)
					{
						pItem->setData(false, eDataRole_Modified);
					}
				}
				WriteControlToXML(scope.pNodes[pControl->GetType()], pControl, path);
			}
		}
	}
}

bool CAudioControlsWriter::IsItemModified(QModelIndex index) const
{
	if (index.data(eDataRole_Modified).toBool() == true)
	{
		return true;
	}

	int i = 0;
	QModelIndex child = index.child(0, 0);
	while (child.isValid())
	{
		if (IsItemModified(child))
		{
			return true;
		}
		child = index.child(++i, 0);
	}
	return false;
}

void CAudioControlsWriter::WriteControlToXML(XmlNodeRef pNode, CATLControl* pControl, const string& sPath)
{
	const EACEControlType type = pControl->GetType();
	XmlNodeRef pChildNode = pNode->createNode(TypeToTag(type));
	pChildNode->setAttr("atl_name", pControl->GetName());
	if (!sPath.empty())
	{
		pChildNode->setAttr("path", sPath);
	}

	if (type == eACEControlType_Switch)
	{
		const size_t size = pControl->ChildCount();
		for (size_t i = 0; i < size; ++i)
		{
			WriteControlToXML(pChildNode, pControl->GetChild(i), "");
		}
	}
	else if (type == eACEControlType_Preload)
	{
		if (pControl->IsAutoLoad())
		{
			pChildNode->setAttr("atl_type", "AutoLoad");
		}

		const std::vector<dll_string>& platforms = GetIEditor()->GetConfigurationManager()->GetPlatformNames();
		const size_t count = platforms.size();
		for (size_t i = 0; i < count; ++i)
		{
			XmlNodeRef pGroupNode = pChildNode->createNode("ATLPlatform");
			pGroupNode->setAttr("atl_name", platforms[i].c_str());
			WriteConnectionsToXML(pGroupNode, pControl, i);
			if (pGroupNode->getChildCount() > 0)
			{
				pChildNode->addChild(pGroupNode);
			}
		}
	}
	else
	{
		WriteConnectionsToXML(pChildNode, pControl);
	}

	pNode->addChild(pChildNode);
}

void CAudioControlsWriter::WriteConnectionsToXML(XmlNodeRef pNode, CATLControl* pControl, const int platformIndex)
{
	if (pControl && m_pAudioSystemImpl)
	{
		XMLNodeList& otherNodes = pControl->GetRawXMLConnections(platformIndex);

		XMLNodeList::const_iterator end = std::remove_if(otherNodes.begin(), otherNodes.end(), [](const SRawConnectionData& node) { return node.bValid; });
		otherNodes.erase(end, otherNodes.end());

		for (auto& node : otherNodes)
		{
			pNode->addChild(node.xmlNode);
		}

		const int size = pControl->GetConnectionCount();
		for (int i = 0; i < size; ++i)
		{
			ConnectionPtr pConnection = pControl->GetConnectionAt(i);
			if (pConnection)
			{
				if (pControl->GetType() != eACEControlType_Preload || pConnection->IsPlatformEnabled(platformIndex))
				{
					XmlNodeRef pChild = m_pAudioSystemImpl->CreateXMLNodeFromConnection(pConnection, pControl->GetType());
					if (pChild)
					{
						pNode->addChild(pChild);
						pControl->AddRawXMLConnection(pChild, true, platformIndex);
					}
				}
			}
		}
	}
}

void CAudioControlsWriter::CheckOutFile(const string& filepath)
{
	ISourceControl* pSourceControl = GetIEditor()->GetSourceControl();
	if (pSourceControl)
	{
		uint32 fileAttributes = pSourceControl->GetFileAttributes(filepath.c_str());
		if (fileAttributes & SCC_FILE_ATTRIBUTE_MANAGED)
		{
			pSourceControl->CheckOut(filepath);
		}
		else if ((fileAttributes == SCC_FILE_ATTRIBUTE_INVALID) || (fileAttributes & SCC_FILE_ATTRIBUTE_NORMAL))
		{
			pSourceControl->Add(filepath, "(ACE Changelist)", ADD_WITHOUT_SUBMIT | ADD_CHANGELIST);
		}
	}
}

void CAudioControlsWriter::DeleteLibraryFile(const string& filepath)
{
	ISourceControl* pSourceControl = GetIEditor()->GetSourceControl();
	if (pSourceControl && pSourceControl->GetFileAttributes(filepath.c_str()) & SCC_FILE_ATTRIBUTE_MANAGED)
	{
		// if source control is connected, let it handle the delete
		pSourceControl->Delete(filepath, "(ACE Changelist)", DELETE_WITHOUT_SUBMIT | ADD_CHANGELIST);
		DeleteFile(filepath.c_str());
	}
	else
	{
		DWORD fileAttributes = GetFileAttributesA(filepath.c_str());
		if (fileAttributes == INVALID_FILE_ATTRIBUTES || !DeleteFile(filepath.c_str()))
		{
			CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, "[Audio Controls Editor] Failed to delete file %s", filepath);
		}
	}
}

void CAudioControlsWriter::WriteEditorData(const QModelIndex& parentIndex, XmlNodeRef pParentNode) const
{
	if (pParentNode && parentIndex.isValid())
	{
		int i = 0;
		QModelIndex child = parentIndex.child(i, 0);
		while (child.isValid())
		{
			if (child.data(eDataRole_Type) == eItemType_Folder)
			{
				XmlNodeRef pFolderNode = pParentNode->createNode("Folder");
				if (pFolderNode)
				{
					pFolderNode->setAttr("name", QtUtil::ToString(child.data(Qt::DisplayRole).toString()));
					WriteEditorData(child, pFolderNode);
					pParentNode->addChild(pFolderNode);
				}
			}
			child = parentIndex.child(++i, 0);
		}
	}
}
}
