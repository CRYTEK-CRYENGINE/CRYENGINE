// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AudioControlsWriter.h"
#include <CryString/StringUtils.h>
#include <CrySystem/File/CryFile.h>
#include <CrySystem/ISystem.h>
#include "AudioAssetsManager.h"
#include <ISourceControl.h>
#include <IEditor.h>
#include <IAudioSystemEditor.h>
#include <IAudioSystemItem.h>
#include "QtUtil.h"
#include <ConfigurationManager.h>
#include <QModelIndex>

using namespace PathUtil;

namespace ACE
{

const string CAudioControlsWriter::ms_controlsPath = AUDIO_SYSTEM_DATA_ROOT CRY_NATIVE_PATH_SEPSTR "ace" CRY_NATIVE_PATH_SEPSTR;
const string CAudioControlsWriter::ms_levelsFolder = "levels" CRY_NATIVE_PATH_SEPSTR;
const uint CAudioControlsWriter::ms_currentFileVersion = 2;

string TypeToTag(EItemType eType)
{
	switch (eType)
	{
	case eItemType_RTPC:
		return "ATLRtpc";
	case eItemType_Trigger:
		return "ATLTrigger";
	case eItemType_Switch:
		return "ATLSwitch";
	case eItemType_State:
		return "ATLSwitchState";
	case eItemType_Preload:
		return "ATLPreloadRequest";
	case eItemType_Environment:
		return "ATLEnvironment";
	}
	return "";
}

CAudioControlsWriter::CAudioControlsWriter(CAudioAssetsManager* pAssetsManager, IAudioSystemEditor* pAudioSystemImpl, std::set<string>& previousLibraryPaths)
	: m_pAssetsManager(pAssetsManager)
	, m_pAudioSystemImpl(pAudioSystemImpl)
{
	if (pAssetsManager && pAudioSystemImpl)
	{
		const size_t libCount = pAssetsManager->GetLibraryCount();
		for (size_t i = 0; i < libCount; ++i)
		{
			CAudioLibrary& library = *pAssetsManager->GetLibrary(i);
			WriteLibrary(library);
			library.SetModified(false);
		}

		// Delete libraries that don't exist anymore from disk
		std::set<string> librariesToDelete;
		std::set_difference(previousLibraryPaths.begin(), previousLibraryPaths.end(), m_foundLibraryPaths.begin(), m_foundLibraryPaths.end(),
		                    std::inserter(librariesToDelete, librariesToDelete.begin()));

		for (auto const& name : librariesToDelete)
		{
			string const fullFilePath = PathUtil::GetGameFolder() + CRY_NATIVE_PATH_SEPSTR + name;
			DeleteLibraryFile(fullFilePath);
		}

		previousLibraryPaths = m_foundLibraryPaths;
	}
}

void CAudioControlsWriter::WriteLibrary(CAudioLibrary& library)
{
	if (library.IsModified())
	{
		LibraryStorage libraryXmlNodes;

		const size_t itemCount = library.ChildCount();
		for (size_t i = 0; i < itemCount; ++i)
		{
			WriteItem(library.GetChild(i), "", libraryXmlNodes);
		}

		// If empty, force it to write an empty library at the root
		if (libraryXmlNodes.empty())
		{
			libraryXmlNodes[Utils::GetGlobalScope()].bDirty = true;
		}

		for (auto const& libraryPair : libraryXmlNodes)
		{
			string libraryPath = ms_controlsPath;
			const Scope scope = libraryPair.first;
			if (scope == Utils::GetGlobalScope())
			{
				// no scope, file at the root level
				libraryPath += library.GetName();
			}
			else
			{
				// with scope, inside level folder
				libraryPath += ms_levelsFolder + m_pAssetsManager->GetScopeInfo(scope).name + CRY_NATIVE_PATH_SEPSTR + library.GetName();
			}

			m_foundLibraryPaths.insert(libraryPath.MakeLower() + ".xml");

			const SLibraryScope& libScope = libraryPair.second;
			if (libScope.bDirty)
			{
				XmlNodeRef pFileNode = GetISystem()->CreateXmlNode("ATLConfig");
				pFileNode->setAttr("atl_name", library.GetName());
				pFileNode->setAttr("atl_version", ms_currentFileVersion);

				for (int i = 0; i < eItemType_NumTypes; ++i)
				{
					if (i != eItemType_State)   // switch_states are written inside the switches
					{
						XmlNodeRef node = libScope.GetXmlNode((EItemType)i);
						if (node && node->getChildCount() > 0)
						{
							pFileNode->addChild(node);
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
						WriteEditorData(&library, pFoldersNode);
						pEditorData->addChild(pFoldersNode);
					}
					pFileNode->addChild(pEditorData);
				}

				const string fullFilePath = PathUtil::GetGameFolder() + CRY_NATIVE_PATH_SEPSTR + libraryPath + ".xml";

				const DWORD fileAttributes = GetFileAttributesA(fullFilePath.c_str());
				if (fileAttributes & FILE_ATTRIBUTE_READONLY)
				{
					// file is read-only
					SetFileAttributesA(fullFilePath.c_str(), FILE_ATTRIBUTE_NORMAL);
				}
				pFileNode->saveToFile(fullFilePath);
				CheckOutFile(fullFilePath);
			}
		}
	}
	else
	{
		std::unordered_set<Scope> scopes;

		const size_t numChildren = library.ChildCount();

		for (size_t i = 0; i < numChildren; ++i)
		{
			IAudioAsset* const pItem = library.GetChild(i);
			GetScopes(pItem, scopes);
		}

		for (auto const scope : scopes)
		{
			string libraryPath = ms_controlsPath;

			if (scope == Utils::GetGlobalScope())
			{
				// no scope, file at the root level
				libraryPath += library.GetName();
			}
			else
			{
				// with scope, inside level folder
				libraryPath += ms_levelsFolder + m_pAssetsManager->GetScopeInfo(scope).name + CRY_NATIVE_PATH_SEPSTR + library.GetName();
			}

			m_foundLibraryPaths.insert(libraryPath.MakeLower() + ".xml");
		}
	}
}

void CAudioControlsWriter::WriteItem(IAudioAsset* pItem, const string& path, LibraryStorage& library)
{
	if (pItem)
	{
		if (pItem->GetType() == EItemType::eItemType_Folder)
		{
			const size_t itemCount = pItem->ChildCount();
			for (size_t i = 0; i < itemCount; ++i)
			{
				// Use forward slash only to ensure cross platform compatibility.
				string newPath = path.empty() ? "" : path + "/";
				newPath += pItem->GetName();
				WriteItem(pItem->GetChild(i), newPath, library);
			}
		}
		else
		{
			CAudioControl* pControl = static_cast<CAudioControl*>(pItem);
			if (pControl)
			{
				SLibraryScope& scope = library[pControl->GetScope()];
				scope.bDirty = true;
				WriteControlToXML(scope.GetXmlNode(pControl->GetType()), pControl, path);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioControlsWriter::GetScopes(IAudioAsset const* const pItem, std::unordered_set<Scope>& scopes)
{
	if (pItem->GetType() == EItemType::eItemType_Folder)
	{
		size_t const numChildren = pItem->ChildCount();

		for (size_t i = 0; i < numChildren; ++i)
		{
			GetScopes(pItem->GetChild(i), scopes);
		}
	}
	else
	{
		CAudioControl const* const pControl = static_cast<CAudioControl const*>(pItem);

		if (pControl != nullptr)
		{
			scopes.insert(pControl->GetScope());
		}
	}
}

void CAudioControlsWriter::WriteControlToXML(XmlNodeRef pNode, CAudioControl* pControl, const string& path)
{
	const EItemType type = pControl->GetType();
	XmlNodeRef pChildNode = pNode->createNode(TypeToTag(type));
	pChildNode->setAttr("atl_name", pControl->GetName());
	if (!path.empty())
	{
		pChildNode->setAttr("path", path);
	}

	if (type == eItemType_Trigger)
	{
		const float radius = pControl->GetRadius();
		if (radius > 0.0f)
		{
			pChildNode->setAttr("atl_radius", radius);
			const float fadeOutDistance = pControl->GetOcclusionFadeOutDistance();
			if (fadeOutDistance > 0.0f)
			{
				pChildNode->setAttr("atl_occlusion_fadeout_distance", fadeOutDistance);
			}
		}
		if (!pControl->IsMatchRadiusToAttenuationEnabled())
		{
			pChildNode->setAttr("atl_match_radius_attenuation", "0");
		}

	}

	if (type == eItemType_Switch)
	{
		const size_t size = pControl->ChildCount();
		for (size_t i = 0; i < size; ++i)
		{
			IAudioAsset* pItem = pControl->GetChild(i);
			if (pItem && pItem->GetType() == EItemType::eItemType_State)
			{
				WriteControlToXML(pChildNode, static_cast<CAudioControl*>(pItem), "");
			}
		}
	}
	else if (type == eItemType_Preload)
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

void CAudioControlsWriter::WriteConnectionsToXML(XmlNodeRef pNode, CAudioControl* pControl, const int platformIndex)
{
	if (pControl && m_pAudioSystemImpl)
	{
		XMLNodeList& otherNodes = pControl->GetRawXMLConnections(platformIndex);

		XMLNodeList::const_iterator end = std::remove_if(otherNodes.begin(), otherNodes.end(), [](const SRawConnectionData& node) { return node.bValid; });
		otherNodes.erase(end, otherNodes.end());

		for (auto& node : otherNodes)
		{
			// Don't add identical nodes!
			bool bAdd = true;
			XmlNodeRef const tempNode = pNode->findChild(node.xmlNode->getTag());

			if (tempNode)
			{
				int const numAttributes1 = tempNode->getNumAttributes();
				int const numAttributes2 = node.xmlNode->getNumAttributes();

				if (numAttributes1 == numAttributes2)
				{
					const char* key1 = nullptr, * val1 = nullptr, * key2 = nullptr, * val2 = nullptr;
					bAdd = false;

					for (int i = 0; i < numAttributes1; ++i)
					{
						tempNode->getAttributeByIndex(i, &key1, &val1);
						node.xmlNode->getAttributeByIndex(i, &key2, &val2);

						if (_stricmp(key1, key2) != 0 || _stricmp(val1, val2) != 0)
						{
							bAdd = true;
							break;
						}
					}
				}
			}

			if (bAdd)
			{
				pNode->addChild(node.xmlNode);
			}
		}

		const int size = pControl->GetConnectionCount();
		for (int i = 0; i < size; ++i)
		{
			ConnectionPtr pConnection = pControl->GetConnectionAt(i);
			if (pConnection)
			{
				if (pControl->GetType() != eItemType_Preload || pConnection->IsPlatformEnabled(platformIndex))
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

void CAudioControlsWriter::WriteEditorData(IAudioAsset* pLibrary, XmlNodeRef pParentNode) const
{
	if (pParentNode && pLibrary)
	{
		const size_t itemCount = pLibrary->ChildCount();
		for (size_t i = 0; i < itemCount; ++i)
		{
			IAudioAsset* pItem = pLibrary->GetChild(i);
			if (pItem->GetType() == EItemType::eItemType_Folder)
			{
				XmlNodeRef pFolderNode = pParentNode->createNode("Folder");
				if (pFolderNode)
				{
					pFolderNode->setAttr("name", pItem->GetName());
					WriteEditorData(pItem, pFolderNode);
					pParentNode->addChild(pFolderNode);
				}
			}
		}
	}
}
}
