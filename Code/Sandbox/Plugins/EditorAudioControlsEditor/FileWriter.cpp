// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "FileWriter.h"

#include "AssetsManager.h"
#include "ContextManager.h"
#include "LibraryScope.h"
#include "Common/IConnection.h"
#include "Common/IImpl.h"
#include "Common/IItem.h"

#include <CryString/StringUtils.h>
#include <CrySystem/File/CryFile.h>

namespace ACE
{
using LibraryStorage = std::map<CryAudio::ContextId, SLibraryScope>;

//////////////////////////////////////////////////////////////////////////
char const* TypeToTag(EAssetType const assetType)
{
	char const* szTag = nullptr;

	switch (assetType)
	{
	case EAssetType::Parameter:
		{
			szTag = CryAudio::g_szParameterTag;
			break;
		}
	case EAssetType::Trigger:
		{
			szTag = CryAudio::g_szTriggerTag;
			break;
		}
	case EAssetType::Switch:
		{
			szTag = CryAudio::g_szSwitchTag;
			break;
		}
	case EAssetType::State:
		{
			szTag = CryAudio::g_szStateTag;
			break;
		}
	case EAssetType::Preload:
		{
			szTag = CryAudio::g_szPreloadRequestTag;
			break;
		}
	case EAssetType::Environment:
		{
			szTag = CryAudio::g_szEnvironmentTag;
			break;
		}
	case EAssetType::Setting:
		{
			szTag = CryAudio::g_szSettingTag;
			break;
		}
	default:
		{
			szTag = nullptr;
			break;
		}
	}

	return szTag;
}

//////////////////////////////////////////////////////////////////////////
void CountControls(EAssetType const type, SLibraryScope& libScope)
{
	switch (type)
	{
	case EAssetType::Trigger:
		{
			++libScope.numTriggers;
			break;
		}
	case EAssetType::Parameter:
		{
			++libScope.numParameters;
			break;
		}
	case EAssetType::Switch:
		{
			++libScope.numSwitches;
			break;
		}
	case EAssetType::State:
		{
			++libScope.numStates;
			break;
		}
	case EAssetType::Environment:
		{
			++libScope.numEnvironments;
			break;
		}
	case EAssetType::Preload:
		{
			++libScope.numPreloads;
			break;
		}
	case EAssetType::Setting:
		{
			++libScope.numSettings;
			break;
		}
	default:
		{
			break;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void WriteConnectionsToXML(XmlNodeRef const& node, CControl* const pControl, SLibraryScope& libScope)
{
	EAssetType const type = pControl->GetType();
	size_t const numConnections = pControl->GetConnectionCount();

	for (size_t i = 0; i < numConnections; ++i)
	{
		IConnection const* const pIConnection = pControl->GetConnectionAt(i);

		if (pIConnection != nullptr)
		{
			XmlNodeRef const childNode = g_pIImpl->CreateXMLNodeFromConnection(pIConnection, type, pControl->GetContextId());

			if (childNode.isValid())
			{
				// Don't add identical nodes!
				bool shouldAddNode = true;
				int const numNodeChilds = node->getChildCount();

				for (int j = 0; j < numNodeChilds; ++j)
				{
					XmlNodeRef const tempNode = node->getChild(j);

					if ((tempNode.isValid()) && (_stricmp(tempNode->getTag(), childNode->getTag()) == 0))
					{
						int const numAttributes1 = tempNode->getNumAttributes();
						int const numAttributes2 = childNode->getNumAttributes();

						if (numAttributes1 == numAttributes2)
						{
							shouldAddNode = false;
							char const* key1 = nullptr;
							char const* val1 = nullptr;
							char const* key2 = nullptr;
							char const* val2 = nullptr;

							for (int k = 0; k < numAttributes1; ++k)
							{
								tempNode->getAttributeByIndex(k, &key1, &val1);
								childNode->getAttributeByIndex(k, &key2, &val2);

								if ((_stricmp(key1, key2) != 0) || (_stricmp(val1, val2) != 0))
								{
									shouldAddNode = true;
									break;
								}
							}

							if (!shouldAddNode)
							{
								break;
							}
						}
					}
				}

				if (shouldAddNode)
				{
					node->addChild(childNode);

					if (type == EAssetType::Preload)
					{
						++libScope.numFiles;
					}
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void WriteControlToXML(XmlNodeRef const& node, CControl* const pControl, string const& path, SLibraryScope& libScope)
{
	EAssetType const type = pControl->GetType();
	XmlNodeRef const childNode = node->createNode(TypeToTag(type));
	childNode->setAttr(CryAudio::g_szNameAttribute, pControl->GetName());
	CountControls(type, libScope);

	if (!path.empty())
	{
		childNode->setAttr(g_szPathAttribute, path);
	}

	if (type == EAssetType::Switch)
	{
		size_t const numChildren = pControl->ChildCount();

		for (size_t i = 0; i < numChildren; ++i)
		{
			CAsset* const pAsset = pControl->GetChild(i);

			if ((pAsset != nullptr) && (pAsset->GetType() == EAssetType::State))
			{
				WriteControlToXML(childNode, static_cast<CControl*>(pAsset), "", libScope);
			}
		}
	}
	else if ((type == EAssetType::Preload) || (type == EAssetType::Setting))
	{
		if (pControl->IsAutoLoad())
		{
			childNode->setAttr(CryAudio::g_szTypeAttribute, CryAudio::g_szDataLoadType);
		}

		WriteConnectionsToXML(childNode, pControl, libScope);
	}
	else
	{
		WriteConnectionsToXML(childNode, pControl, libScope);
	}

	pControl->SetModified(false);
	node->addChild(childNode);
}

//////////////////////////////////////////////////////////////////////////
void WriteItem(CAsset* const pAsset, string const& path, LibraryStorage& library)
{
	if (pAsset != nullptr)
	{
		if (pAsset->GetType() == EAssetType::Folder)
		{
			size_t const itemCount = pAsset->ChildCount();

			for (size_t i = 0; i < itemCount; ++i)
			{
				// Use forward slash only to ensure cross platform compatibility.
				string newPath = path.empty() ? "" : path + "/";
				newPath += pAsset->GetName();
				WriteItem(pAsset->GetChild(i), newPath, library);
			}

			pAsset->SetModified(false);
		}
		else
		{
			auto const pControl = static_cast<CControl*>(pAsset);

			if (pControl != nullptr)
			{
				SLibraryScope& libScope = library[pControl->GetContextId()];
				libScope.isDirty = true;
				WriteControlToXML(libScope.GetXmlNode(pControl->GetType()), pControl, path, libScope);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void WriteLibraryEditorData(CAsset const& library, XmlNodeRef const& node)
{
	string const& description = library.GetDescription();

	if (!description.IsEmpty() && (library.GetFlags() & EAssetFlags::IsDefaultControl) == 0)
	{
		node->setAttr(g_szDescriptionAttribute, description);
	}
}

//////////////////////////////////////////////////////////////////////////
void WriteFolderEditorData(CAsset const& library, XmlNodeRef const& node)
{
	size_t const itemCount = library.ChildCount();

	for (size_t i = 0; i < itemCount; ++i)
	{
		CAsset const* const pAsset = library.GetChild(i);

		if (pAsset->GetType() == EAssetType::Folder)
		{
			XmlNodeRef const folderNode = node->createNode(g_szFolderTag);

			if (folderNode.isValid())
			{
				folderNode->setAttr(CryAudio::g_szNameAttribute, pAsset->GetName());
				string const description = pAsset->GetDescription();

				if (!description.IsEmpty() && ((pAsset->GetFlags() & EAssetFlags::IsDefaultControl) == 0))
				{
					folderNode->setAttr(g_szDescriptionAttribute, description);
				}

				WriteFolderEditorData(*pAsset, folderNode);
				node->addChild(folderNode);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void WriteControlsEditorData(CAsset const& parentAsset, XmlNodeRef const& node)
{
	size_t const itemCount = parentAsset.ChildCount();

	for (size_t i = 0; i < itemCount; ++i)
	{
		CAsset const& asset = *parentAsset.GetChild(i);
		EAssetType const type = asset.GetType();
		char const* const szTag = TypeToTag(type);

		if (szTag != nullptr)
		{
			XmlNodeRef const controlNode = node->createNode(TypeToTag(type));

			if (controlNode.isValid())
			{
				string const& description = asset.GetDescription();

				if (!description.IsEmpty() && ((asset.GetFlags() & EAssetFlags::IsDefaultControl) == 0))
				{
					controlNode->setAttr(CryAudio::g_szNameAttribute, asset.GetName());
					controlNode->setAttr(g_szDescriptionAttribute, description);
					node->addChild(controlNode);
				}
			}
		}

		if ((type == EAssetType::Folder) || (type == EAssetType::Switch))
		{
			WriteControlsEditorData(asset, node);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void GetContexts(CAsset const* const pAsset, ContextIds& contextIds)
{
	if (pAsset->GetType() == EAssetType::Folder)
	{
		size_t const numChildren = pAsset->ChildCount();

		for (size_t i = 0; i < numChildren; ++i)
		{
			GetContexts(pAsset->GetChild(i), contextIds);
		}
	}
	else
	{
		auto const pControl = static_cast<CControl const*>(pAsset);

		if (pControl != nullptr)
		{
			contextIds.emplace_back(pControl->GetContextId());
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void DeleteLibraryFile(string const& filepath)
{
	// TODO: Mark for delete in source control.
	DWORD const fileAttributes = GetFileAttributesA(filepath.c_str());

	if ((fileAttributes & FILE_ATTRIBUTE_READONLY) != 0)
	{
		// file is read-only
		SetFileAttributesA(filepath.c_str(), FILE_ATTRIBUTE_NORMAL);
	}

	if ((fileAttributes == INVALID_FILE_ATTRIBUTES) || !DeleteFile(filepath.c_str()))
	{
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_ERROR, "[Audio Controls Editor] Failed to delete file %s", filepath);
	}
}

//////////////////////////////////////////////////////////////////////////
void CFileWriter::WriteAll()
{
	ContextIds contextIds;
	size_t const libCount = g_assetsManager.GetLibraryCount();

	for (size_t i = 0; i < libCount; ++i)
	{
		CLibrary& library = *g_assetsManager.GetLibrary(i);
		WriteLibrary(library, contextIds);
		library.SetModified(false);
	}

	// Delete libraries that don't exist anymore from disk
	FileNames librariesToDelete;
	std::set_difference(m_previousLibraryPaths.begin(), m_previousLibraryPaths.end(), m_foundLibraryPaths.begin(), m_foundLibraryPaths.end(),
	                    std::inserter(librariesToDelete, librariesToDelete.begin()));

	for (auto const& name : librariesToDelete)
	{
		string const fullFilePath = PathUtil::GetGameFolder() + "/" + name;
		DeleteLibraryFile(fullFilePath);
	}

	m_previousLibraryPaths = m_foundLibraryPaths;

	if (!contextIds.empty())
	{
		std::sort(contextIds.begin(), contextIds.end());
		auto const last = std::unique(contextIds.begin(), contextIds.end());
		contextIds.erase(last, contextIds.end());

		g_contextManager.TryRegisterContexts(contextIds);
	}
}

//////////////////////////////////////////////////////////////////////////
void CFileWriter::WriteLibrary(CLibrary& library, ContextIds& contextIds)
{
	if ((library.GetFlags() & EAssetFlags::IsModified) != 0)
	{
		g_pIImpl->OnBeforeWriteLibrary();

		LibraryStorage libraryXmlNodes;
		size_t const itemCount = library.ChildCount();

		if ((library.GetFlags() & EAssetFlags::IsDefaultControl) != 0)
		{
			for (size_t i = 0; i < itemCount; ++i)
			{
				CAsset* const pAsset = library.GetChild(i);

				if ((pAsset != nullptr) && ((pAsset->GetFlags() & EAssetFlags::IsInternalControl) == 0))
				{
					WriteItem(pAsset, "", libraryXmlNodes);
				}
			}
		}
		else
		{
			for (size_t i = 0; i < itemCount; ++i)
			{
				WriteItem(library.GetChild(i), "", libraryXmlNodes);
			}
		}

		// If empty, force it to write an empty library at the root
		if (libraryXmlNodes.empty())
		{
			libraryXmlNodes[CryAudio::GlobalContextId].isDirty = true;
		}

		for (auto const& libraryPair : libraryXmlNodes)
		{
			string libraryPath = g_assetsManager.GetConfigFolderPath();
			CryAudio::ContextId const contextId = libraryPair.first;

			if (contextId == CryAudio::GlobalContextId)
			{
				// Global context, file at the root level.
				libraryPath += library.GetName();
			}
			else
			{
				// User defined context, inside context folder.
				libraryPath += CryAudio::g_szContextsFolderName;
				libraryPath += "/" + g_contextManager.GetContextName(contextId) + "/" + library.GetName();
				contextIds.emplace_back(contextId);
			}

			m_foundLibraryPaths.insert(libraryPath.MakeLower() + ".xml");

			SLibraryScope const& libScope = libraryPair.second;

			if (libScope.isDirty)
			{
				XmlNodeRef fileNode = GetISystem()->CreateXmlNode(CryAudio::g_szRootNodeTag);
				fileNode->setAttr(CryAudio::g_szNameAttribute, library.GetName());
				fileNode->setAttr(CryAudio::g_szVersionAttribute, g_currentFileVersion);

				// Don't write control counts for default library, because default controls are not pooled.
				// But connections of default controls need to get written.
				if ((library.GetFlags() & EAssetFlags::IsDefaultControl) == 0)
				{
					if (libScope.numTriggers > 0)
					{
						fileNode->setAttr(CryAudio::g_szNumTriggersAttribute, libScope.numTriggers);
					}

					if (libScope.numParameters > 0)
					{
						fileNode->setAttr(CryAudio::g_szNumParametersAttribute, libScope.numParameters);
					}

					if (libScope.numSwitches > 0)
					{
						fileNode->setAttr(CryAudio::g_szNumSwitchesAttribute, libScope.numSwitches);
					}

					if (libScope.numStates > 0)
					{
						fileNode->setAttr(CryAudio::g_szNumStatesAttribute, libScope.numStates);
					}

					if (libScope.numEnvironments > 0)
					{
						fileNode->setAttr(CryAudio::g_szNumEnvironmentsAttribute, libScope.numEnvironments);
					}

					if (libScope.numPreloads > 0)
					{
						fileNode->setAttr(CryAudio::g_szNumPreloadsAttribute, libScope.numPreloads);
					}

					if (libScope.numSettings > 0)
					{
						fileNode->setAttr(CryAudio::g_szNumSettingsAttribute, libScope.numSettings);
					}
				}

				if (libScope.numFiles > 0)
				{
					fileNode->setAttr(CryAudio::g_szNumFilesAttribute, libScope.numFiles);
				}

				XmlNodeRef const implDataNode = g_pIImpl->SetDataNode(CryAudio::g_szImplDataNodeTag, contextId);

				if (implDataNode.isValid())
				{
					fileNode->addChild(implDataNode);
				}

				auto const numTypes = static_cast<int>(EAssetType::NumTypes);

				for (int i = 0; i < numTypes; ++i)
				{
					if (i != static_cast<int>(EAssetType::State))   // switch_states are written inside the switches
					{
						XmlNodeRef const node = libScope.GetXmlNode((EAssetType)i);

						if ((node.isValid()) && (node->getChildCount() > 0))
						{
							fileNode->addChild(node);
						}
					}
				}

				// Editor data
				XmlNodeRef const editorDataNode = fileNode->createNode(CryAudio::g_szEditorDataTag);

				if (editorDataNode.isValid())
				{
					XmlNodeRef const libraryNode = editorDataNode->createNode(g_szLibraryNodeTag);

					if (libraryNode.isValid())
					{
						WriteLibraryEditorData(library, libraryNode);
						editorDataNode->addChild(libraryNode);
					}

					XmlNodeRef const foldersNode = editorDataNode->createNode(g_szFoldersNodeTag);

					if (foldersNode.isValid())
					{
						WriteFolderEditorData(library, foldersNode);
						editorDataNode->addChild(foldersNode);
					}

					XmlNodeRef const controlsNode = editorDataNode->createNode(g_szControlsNodeTag);

					if (controlsNode.isValid())
					{
						WriteControlsEditorData(library, controlsNode);
						editorDataNode->addChild(controlsNode);
					}

					fileNode->addChild(editorDataNode);
				}

				string const fullFilePath = PathUtil::GetGameFolder() + "/" + libraryPath + ".xml";
				DWORD const fileAttributes = GetFileAttributesA(fullFilePath.c_str());

				if ((fileAttributes & FILE_ATTRIBUTE_READONLY) != 0)
				{
					// file is read-only
					SetFileAttributesA(fullFilePath.c_str(), FILE_ATTRIBUTE_NORMAL);
				}

				// TODO: Check out in source control.
				fileNode->saveToFile(fullFilePath);

				// Update pak status, because it will exist on disk, if writing doesn't fail.
				library.SetPakStatus(EPakStatus::OnDisk, gEnv->pCryPak->IsFileExist(fullFilePath.c_str(), ICryPak::eFileLocation_OnDisk));
			}
		}

		g_pIImpl->OnAfterWriteLibrary();
	}
	else
	{
		ContextIds ids;
		size_t const numChildren = library.ChildCount();

		for (size_t i = 0; i < numChildren; ++i)
		{
			CAsset* const pAsset = library.GetChild(i);
			GetContexts(pAsset, ids);
		}

		for (auto const id : ids)
		{
			string libraryPath = g_assetsManager.GetConfigFolderPath();

			if (id == CryAudio::GlobalContextId)
			{
				// Global context, file at the root level.
				libraryPath += library.GetName();
			}
			else
			{
				// User defined context, inside context folder.
				libraryPath += CryAudio::g_szContextsFolderName;
				libraryPath += "/" + g_contextManager.GetContextName(id) + "/" + library.GetName();
			}

			m_foundLibraryPaths.insert(libraryPath.MakeLower() + ".xml");
		}
	}
}
} // namespace ACE
