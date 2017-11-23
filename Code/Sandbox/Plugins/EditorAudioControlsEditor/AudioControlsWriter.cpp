// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AudioControlsWriter.h"

#include "SystemAssetsManager.h"

#include <IEditorImpl.h>
#include <ImplItem.h>
#include <CryString/StringUtils.h>
#include <CrySystem/File/CryFile.h>
#include <CrySystem/ISystem.h>
#include <ISourceControl.h>
#include <IEditor.h>
#include <QtUtil.h>
#include <ConfigurationManager.h>

namespace ACE
{
string const CAudioControlsWriter::s_controlsPath = AUDIO_SYSTEM_DATA_ROOT CRY_NATIVE_PATH_SEPSTR "ace" CRY_NATIVE_PATH_SEPSTR;
string const CAudioControlsWriter::s_levelsFolder = "levels" CRY_NATIVE_PATH_SEPSTR;
uint const CAudioControlsWriter::s_currentFileVersion = 2;

//////////////////////////////////////////////////////////////////////////
string TypeToTag(ESystemItemType const eType)
{
	switch (eType)
	{
	case ESystemItemType::Parameter:
		return "ATLRtpc";
	case ESystemItemType::Trigger:
		return "ATLTrigger";
	case ESystemItemType::Switch:
		return "ATLSwitch";
	case ESystemItemType::State:
		return "ATLSwitchState";
	case ESystemItemType::Preload:
		return "ATLPreloadRequest";
	case ESystemItemType::Environment:
		return "ATLEnvironment";
	}
	return "";
}

//////////////////////////////////////////////////////////////////////////
CAudioControlsWriter::CAudioControlsWriter(CSystemAssetsManager* pAssetsManager, IEditorImpl* pEditorImpl, std::set<string>& previousLibraryPaths)
	: m_pAssetsManager(pAssetsManager)
	, m_pEditorImpl(pEditorImpl)
{
	if ((pAssetsManager != nullptr) && (pEditorImpl != nullptr))
	{
		size_t const libCount = pAssetsManager->GetLibraryCount();

		for (size_t i = 0; i < libCount; ++i)
		{
			CSystemLibrary& library = *pAssetsManager->GetLibrary(i);
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

//////////////////////////////////////////////////////////////////////////
void CAudioControlsWriter::WriteLibrary(CSystemLibrary& library)
{
	if (library.IsModified())
	{
		LibraryStorage libraryXmlNodes;

		size_t const itemCount = library.ChildCount();

		for (size_t i = 0; i < itemCount; ++i)
		{
			WriteItem(library.GetChild(i), "", libraryXmlNodes);
		}

		// If empty, force it to write an empty library at the root
		if (libraryXmlNodes.empty())
		{
			libraryXmlNodes[Utils::GetGlobalScope()].isDirty = true;
		}

		for (auto const& libraryPair : libraryXmlNodes)
		{
			string libraryPath = s_controlsPath;
			Scope const scope = libraryPair.first;

			if (scope == Utils::GetGlobalScope())
			{
				// no scope, file at the root level
				libraryPath += library.GetName();
			}
			else
			{
				// with scope, inside level folder
				libraryPath += s_levelsFolder + m_pAssetsManager->GetScopeInfo(scope).name + CRY_NATIVE_PATH_SEPSTR + library.GetName();
			}

			m_foundLibraryPaths.insert(libraryPath.MakeLower() + ".xml");

			SLibraryScope const& libScope = libraryPair.second;

			if (libScope.isDirty)
			{
				XmlNodeRef pFileNode = GetISystem()->CreateXmlNode("ATLConfig");
				pFileNode->setAttr("atl_name", library.GetName());
				pFileNode->setAttr("atl_version", s_currentFileVersion);

				int const numTypes = static_cast<int>(ESystemItemType::NumTypes);

				for (int i = 0; i < numTypes; ++i)
				{
					if (i != static_cast<int>(ESystemItemType::State))   // switch_states are written inside the switches
					{
						XmlNodeRef node = libScope.GetXmlNode((ESystemItemType)i);

						if ((node != nullptr) && (node->getChildCount() > 0))
						{
							pFileNode->addChild(node);
						}
					}
				}

				// Editor data
				XmlNodeRef const pEditorData = pFileNode->createNode("EditorData");

				if (pEditorData != nullptr)
				{
					XmlNodeRef const pLibraryNode = pEditorData->createNode("Library");

					if (pLibraryNode != nullptr)
					{
						WriteLibraryEditorData(library, pLibraryNode);
						pEditorData->addChild(pLibraryNode);
					}

					XmlNodeRef const pFoldersNode = pEditorData->createNode("Folders");

					if (pFoldersNode != nullptr)
					{
						WriteFolderEditorData(library, pFoldersNode);
						pEditorData->addChild(pFoldersNode);
					}

					XmlNodeRef const pControlsNode = pEditorData->createNode("Controls");

					if (pControlsNode != nullptr)
					{
						WriteControlsEditorData(library, pControlsNode);
						pEditorData->addChild(pControlsNode);
					}

					pFileNode->addChild(pEditorData);
				}

				string const fullFilePath = PathUtil::GetGameFolder() + CRY_NATIVE_PATH_SEPSTR + libraryPath + ".xml";

				DWORD const fileAttributes = GetFileAttributesA(fullFilePath.c_str());

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

		size_t const numChildren = library.ChildCount();

		for (size_t i = 0; i < numChildren; ++i)
		{
			CSystemAsset* const pItem = library.GetChild(i);
			GetScopes(pItem, scopes);
		}

		for (auto const scope : scopes)
		{
			string libraryPath = s_controlsPath;

			if (scope == Utils::GetGlobalScope())
			{
				// no scope, file at the root level
				libraryPath += library.GetName();
			}
			else
			{
				// with scope, inside level folder
				libraryPath += s_levelsFolder + m_pAssetsManager->GetScopeInfo(scope).name + CRY_NATIVE_PATH_SEPSTR + library.GetName();
			}

			m_foundLibraryPaths.insert(libraryPath.MakeLower() + ".xml");
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioControlsWriter::WriteItem(CSystemAsset* const pItem, string const& path, LibraryStorage& library)
{
	if (pItem != nullptr)
	{
		if (pItem->GetType() == ESystemItemType::Folder)
		{
			size_t const itemCount = pItem->ChildCount();

			for (size_t i = 0; i < itemCount; ++i)
			{
				// Use forward slash only to ensure cross platform compatibility.
				string newPath = path.empty() ? "" : path + "/";
				newPath += pItem->GetName();
				WriteItem(pItem->GetChild(i), newPath, library);
			}

			pItem->SetModified(false);
		}
		else
		{
			CSystemControl* pControl = static_cast<CSystemControl*>(pItem);

			if (pControl != nullptr)
			{
				SLibraryScope& scope = library[pControl->GetScope()];
				scope.isDirty = true;
				WriteControlToXML(scope.GetXmlNode(pControl->GetType()), pControl, path);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioControlsWriter::GetScopes(CSystemAsset const* const pItem, std::unordered_set<Scope>& scopes)
{
	if (pItem->GetType() == ESystemItemType::Folder)
	{
		size_t const numChildren = pItem->ChildCount();

		for (size_t i = 0; i < numChildren; ++i)
		{
			GetScopes(pItem->GetChild(i), scopes);
		}
	}
	else
	{
		CSystemControl const* const pControl = static_cast<CSystemControl const*>(pItem);

		if (pControl != nullptr)
		{
			scopes.insert(pControl->GetScope());
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioControlsWriter::WriteControlToXML(XmlNodeRef const pNode, CSystemControl* pControl, string const& path)
{
	ESystemItemType const type = pControl->GetType();
	XmlNodeRef const pChildNode = pNode->createNode(TypeToTag(type));
	pChildNode->setAttr("atl_name", pControl->GetName());

	if (!path.empty())
	{
		pChildNode->setAttr("path", path);
	}

	if (type == ESystemItemType::Trigger)
	{
		float const radius = pControl->GetRadius();

		if (radius > 0.0f)
		{
			pChildNode->setAttr("atl_radius", radius);
			float const fadeOutDistance = pControl->GetOcclusionFadeOutDistance();

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

	if (type == ESystemItemType::Switch)
	{
		size_t const size = pControl->ChildCount();

		for (size_t i = 0; i < size; ++i)
		{
			CSystemAsset* pItem = pControl->GetChild(i);

			if ((pItem != nullptr) && (pItem->GetType() == ESystemItemType::State))
			{
				WriteControlToXML(pChildNode, static_cast<CSystemControl*>(pItem), "");
			}
		}
	}
	else if (type == ESystemItemType::Preload)
	{
		if (pControl->IsAutoLoad())
		{
			pChildNode->setAttr("atl_type", "AutoLoad");
		}

		std::vector<dll_string> const& platforms = GetIEditor()->GetConfigurationManager()->GetPlatformNames();
		size_t const count = platforms.size();

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

	pControl->SetModified(false);
	pNode->addChild(pChildNode);
}

//////////////////////////////////////////////////////////////////////////
void CAudioControlsWriter::WriteConnectionsToXML(XmlNodeRef const pNode, CSystemControl* const pControl, const int platformIndex)
{
	if ((pControl != nullptr) && (m_pEditorImpl != nullptr))
	{
		XMLNodeList& otherNodes = pControl->GetRawXMLConnections(platformIndex);

		XMLNodeList::const_iterator end = std::remove_if(otherNodes.begin(), otherNodes.end(), [](const SRawConnectionData& node) { return node.isValid; });
		otherNodes.erase(end, otherNodes.end());

		for (auto const& node : otherNodes)
		{
			// Don't add identical nodes!
			bool shouldAddNode = true;
			XmlNodeRef const tempNode = pNode->findChild(node.xmlNode->getTag());

			if (tempNode)
			{
				int const numAttributes1 = tempNode->getNumAttributes();
				int const numAttributes2 = node.xmlNode->getNumAttributes();

				if (numAttributes1 == numAttributes2)
				{
					char const* key1 = nullptr, * val1 = nullptr, * key2 = nullptr, * val2 = nullptr;
					shouldAddNode = false;

					for (int i = 0; i < numAttributes1; ++i)
					{
						tempNode->getAttributeByIndex(i, &key1, &val1);
						node.xmlNode->getAttributeByIndex(i, &key2, &val2);

						if ((_stricmp(key1, key2) != 0) || (_stricmp(val1, val2) != 0))
						{
							shouldAddNode = true;
							break;
						}
					}
				}
			}

			if (shouldAddNode)
			{
				pNode->addChild(node.xmlNode);
			}
		}

		int const size = pControl->GetConnectionCount();

		for (int i = 0; i < size; ++i)
		{
			ConnectionPtr const pConnection = pControl->GetConnectionAt(i);

			if (pConnection != nullptr)
			{
				if ((pControl->GetType() != ESystemItemType::Preload) || (pConnection->IsPlatformEnabled(platformIndex)))
				{
					XmlNodeRef const pChild = m_pEditorImpl->CreateXMLNodeFromConnection(pConnection, pControl->GetType());

					if (pChild != nullptr)
					{
						pNode->addChild(pChild);
						pControl->AddRawXMLConnection(pChild, true, platformIndex);
					}
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioControlsWriter::CheckOutFile(string const& filepath)
{
	ISourceControl* const pSourceControl = GetIEditor()->GetSourceControl();

	if (pSourceControl != nullptr)
	{
		uint32 const fileAttributes = pSourceControl->GetFileAttributes(filepath.c_str());

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

//////////////////////////////////////////////////////////////////////////
void CAudioControlsWriter::DeleteLibraryFile(string const& filepath)
{
	ISourceControl* const pSourceControl = GetIEditor()->GetSourceControl();

	if ((pSourceControl != nullptr) && (pSourceControl->GetFileAttributes(filepath.c_str()) & SCC_FILE_ATTRIBUTE_MANAGED))
	{
		// if source control is connected, let it handle the delete
		pSourceControl->Delete(filepath, "(ACE Changelist)", DELETE_WITHOUT_SUBMIT | ADD_CHANGELIST);
		DeleteFile(filepath.c_str());
	}
	else
	{
		DWORD const fileAttributes = GetFileAttributesA(filepath.c_str());

		if ((fileAttributes == INVALID_FILE_ATTRIBUTES) || !DeleteFile(filepath.c_str()))
		{
			CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, "[Audio Controls Editor] Failed to delete file %s", filepath);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioControlsWriter::WriteLibraryEditorData(CSystemAsset const& library, XmlNodeRef const pParentNode) const
{
	string const description = library.GetDescription();

	if (!description.IsEmpty())
	{
		pParentNode->setAttr("description", description);
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioControlsWriter::WriteFolderEditorData(CSystemAsset const& library, XmlNodeRef const pParentNode) const
{
	size_t const itemCount = library.ChildCount();

	for (size_t i = 0; i < itemCount; ++i)
	{
		CSystemAsset const* const pAsset = library.GetChild(i);

		if (pAsset->GetType() == ESystemItemType::Folder)
		{
			XmlNodeRef const pFolderNode = pParentNode->createNode("Folder");

			if (pFolderNode != nullptr)
			{
				pFolderNode->setAttr("name", pAsset->GetName());
				string const description = pAsset->GetDescription();

				if (!description.IsEmpty())
				{
					pFolderNode->setAttr("description", description);
				}

				WriteFolderEditorData(*pAsset, pFolderNode);
				pParentNode->addChild(pFolderNode);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CAudioControlsWriter::WriteControlsEditorData(CSystemAsset const& parentAsset, XmlNodeRef const pParentNode) const
{
	size_t const itemCount = parentAsset.ChildCount();

	for (size_t i = 0; i < itemCount; ++i)
	{
		CSystemAsset const& asset = *parentAsset.GetChild(i);
		ESystemItemType const type = asset.GetType();
		string const& nodeName = TypeToTag(type);

		if (!nodeName.IsEmpty())
		{
			XmlNodeRef const pControlNode = pParentNode->createNode(nodeName);

			if (pControlNode != nullptr)
			{
				string const& description = asset.GetDescription();

				if (!description.IsEmpty())
				{
					pControlNode->setAttr("name", asset.GetName());
					pControlNode->setAttr("description", description);
					pParentNode->addChild(pControlNode);
				}
			}
		}

		if ((type == ESystemItemType::Folder) || (type == ESystemItemType::Switch))
		{
			WriteControlsEditorData(asset, pParentNode);
		}
	}
}
} // namespace ACE
