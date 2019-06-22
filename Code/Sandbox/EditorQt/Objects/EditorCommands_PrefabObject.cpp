// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include <StdAfx.h>

// Sandbox
#include "IEditorImpl.h"
#include "IEditor.h"
#include "Objects/ObjectManager.h"
#include "Objects/PrefabObject.h"
#include "Prefabs/PrefabManager.h"

// EditorCommon
#include <Commands/ICommandManager.h>
#include <AssetSystem/AssetType.h>
#include <AssetSystem/AssetManager.h>

//////////////////////////////////////////////////////////////////////////
namespace Private_PrefabCommands
{
//Create a new prefab item
static CPrefabItem* CreateNewPrefabItem(const std::string& itemName)
{
	CPrefabItem* pItem = nullptr;
	string fullName;

	fullName.Format("%s.%s", itemName.c_str(), itemName.c_str());
	pItem = static_cast<CPrefabItem*>(GetIEditor()->GetPrefabManager()->FindItemByName(fullName));

	//This item already exist, we cannot create a new one
	if (pItem)
	{
		return nullptr;
	}

	//Find the prefab asset type
	const CAssetType* const pPrefabAssetType = GetIEditor()->GetAssetManager()->FindAssetType("Prefab");
	if (!pPrefabAssetType)
	{
		return nullptr;
	}

	//Generate the filename of the prefab asset
	const string prefabFilename = PathUtil::ReplaceExtension(itemName.c_str(), pPrefabAssetType->GetFileExtension());
	const string metadataFilename = string().Format("%s.%s", prefabFilename.c_str(), "cryasset");

	//nullptr selection group means that we don't want a prefab object to be created
	SPrefabCreateParams params(nullptr);
	//Create the prefab item and prefab asset (the metadata .cryasset that links to the xml representation of the prefab)
	if (pPrefabAssetType->Create(metadataFilename, &params))
	{
		//Get the newly generated prefab item
		pItem = static_cast<CPrefabItem*>(GetIEditor()->GetPrefabManager()->FindItemByName(fullName));
	}
	return pItem;
}

static void PyNewPrefabItem(const std::string& itemName)
{
	if (!CreateNewPrefabItem(itemName))
	{
		throw std::logic_error("Unable to create Prefab Item " + itemName);
	}
}

static const char* PyNewPrefab(const std::string& itemName, const std::string& prefabName, float x, float y, float z)
{
	//full name of prefab item is libraryName.itemName, since prefabs were ported to the new asset system libraryName == itemName
	string fullItemName;
	fullItemName.Format("%s.%s", itemName.c_str(), itemName.c_str());
	CPrefabItem* pItem = static_cast<CPrefabItem*>(GetIEditor()->GetPrefabManager()->FindItemByName(fullItemName));

	//if we don't have that item we need to create it from scratch
	if (!pItem)
	{
		pItem = CreateNewPrefabItem(itemName);
	}

	//We must create the prefab object with a valid item
	if (!pItem)
	{
		throw std::logic_error("Unable to create item " + itemName);
	}

	//Create the new prefab object, the name will be defaulted to SetUniqName(pItem->GetName()) so we'll have to change it later
	CPrefabObject* pPrefabObject = CPrefabObject::CreateFrom(std::vector<CBaseObject*>(), Vec3(x, y, z), pItem);
	if (!pPrefabObject)
	{
		throw std::logic_error("Unable to create a new Prefab object");
	}

	//Set the prefab name and make sure it is unique
	pPrefabObject->SetUniqName(prefabName.c_str());

	return pPrefabObject->GetName();
}

boost::python::tuple PyGetPrefabOfChild(const char* pObjName)
{
	boost::python::tuple result;
	CBaseObject* pObject;
	if (GetIEditor()->GetObjectManager()->FindObject(pObjName))
		pObject = GetIEditor()->GetObjectManager()->FindObject(pObjName);
	else if (GetIEditor()->GetObjectManager()->FindObject(CryGUID::FromString(pObjName)))
		pObject = GetIEditor()->GetObjectManager()->FindObject(CryGUID::FromString(pObjName));
	else
	{
		throw std::logic_error(string("\"") + pObjName + "\" is an invalid object.");
		return result;
	}

	result = boost::python::make_tuple(pObject->GetParent()->GetName(), pObject->GetParent()->GetId().ToString());
	return result;
}

static void PyNewPrefabFromSelection(const char* itemName)
{
	const CAssetType* const pPrefabAssetType = GetIEditor()->GetAssetManager()->FindAssetType("Prefab");
	if (!pPrefabAssetType)
	{
		return;
	}

	const string prefabFilename = PathUtil::ReplaceExtension(itemName, pPrefabAssetType->GetFileExtension());
	const string metadataFilename = string().Format("%s.%s", prefabFilename.c_str(), "cryasset");

	pPrefabAssetType->Create(metadataFilename);
}

static void PyDeletePrefabItem(const char* itemName)
{
	//Make sure the item actually exist
	string itemFullName;
	itemFullName.Format("%s.%s", itemName, itemName);
	CPrefabItem* pItem = static_cast<CPrefabItem*>(GetIEditor()->GetPrefabManager()->FindItemByName(itemFullName)); 

	if (!pItem)
	{
		throw std::logic_error(string("No prefab item named ") + itemName + " found");
	}
	
	//Delete item
	GetIEditor()->GetPrefabManager()->DeleteItem(pItem);

	//Then find the asset type
	CAssetManager* const pAssetManager = GetIEditor()->GetAssetManager();
	const CAssetType* const pPrefabAssetType = GetIEditor()->GetAssetManager()->FindAssetType("Prefab");
	if (!pPrefabAssetType)
	{
		return;
	}

	//Then delete all the asset files
	const string prefabFilename = PathUtil::ReplaceExtension(itemName, pPrefabAssetType->GetFileExtension());
	CAsset* pAsset = pAssetManager->FindAssetForFile(prefabFilename);
	if (!pAsset)
	{
		return;
	}

	std::future<void> deleteFuture = pAssetManager->DeleteAssetsWithFiles({ pAsset });
	deleteFuture.wait();
}

static std::vector<string> PyGetPrefabItems()
{
	CAssetManager* const pAssetManager = GetIEditor()->GetAssetManager();
	const CAssetType* const pPrefabAssetType = GetIEditor()->GetAssetManager()->FindAssetType("Prefab");
	if (!pPrefabAssetType)
	{
		return {};
	}

	std::vector<string> results;
	pAssetManager->ForeachAsset([&results, pPrefabAssetType](CAsset* pAsset)
		{
			if (pAsset->GetType() == pPrefabAssetType)
			{
			  results.push_back(pAsset->GetFile(0));
			}
		});

	return results;
}

boost::python::tuple PyGetPrefabChildWorldPos(const char* pObjName, const char* pChildName)
{
	CBaseObject* pObject;
	if (GetIEditor()->GetObjectManager()->FindObject(pObjName))
		pObject = GetIEditor()->GetObjectManager()->FindObject(pObjName);
	else if (GetIEditor()->GetObjectManager()->FindObject(CryGUID::FromString(pObjName)))
		pObject = GetIEditor()->GetObjectManager()->FindObject(CryGUID::FromString(pObjName));
	else
	{
		throw std::logic_error(string("\"") + pObjName + "\" is an invalid object.");
		return boost::python::make_tuple(false);
	}

	for (int i = 0, iChildCount(pObject->GetChildCount()); i < iChildCount; ++i)
	{
		CBaseObject* pChild = pObject->GetChild(i);
		if (pChild == NULL)
			continue;
		if (strcmp(pChild->GetName(), pChildName) == 0 || stricmp(pChild->GetId().ToString().c_str(), pChildName) == 0)
		{
			Vec3 childPos = pChild->GetPos();
			Vec3 parentPos = pChild->GetParent()->GetPos();
			return boost::python::make_tuple(parentPos.x - childPos.x, parentPos.y - childPos.y, parentPos.z - childPos.z);
		}
	}
	return boost::python::make_tuple(false);
}

static bool PyHasPrefabItem(const char* pItemName)
{
	const string prefabFilename = PathUtil::ReplaceExtension(pItemName, "Prefab");
	return GetIEditor()->GetAssetManager()->FindAssetForFile(prefabFilename) != nullptr;
}

static void PyCreateFromSelection()
{
	GetIEditorImpl()->GetLevelEditorSharedState()->SetEditTool(nullptr);
	GetIEditorImpl()->GetPrefabManager()->MakeFromSelection(GetIEditorImpl()->GetSelection());
}

static void PyAddSelection()
{
	GetIEditorImpl()->GetPrefabManager()->AddSelectionToPrefab();
}

static void PyExtractAllFromSelection()
{
	GetIEditorImpl()->GetPrefabManager()->ExtractAllFromSelection();
}

static void PyExtractAllFromPrefabs(const std::vector<std::string>& prefabNames)
{
	//Make sure that we have some prefabs to extract 
	if (!prefabNames.size())
	{
		throw std::logic_error("Prefab names list is empty");
	}

	std::vector<CPrefabObject*> prefabObjects;
	for (const std::string& prefabName : prefabNames)
	{
		//Find an object and make sure it's a prefab, if not log errors
		CBaseObject* pObject = GetIEditor()->GetObjectManager()->FindObject(prefabName.c_str());
		if (pObject && pObject->IsKindOf(RUNTIME_CLASS(CPrefabObject)))
		{
			CPrefabObject* pPrefabObject = static_cast<CPrefabObject*>(pObject);
			prefabObjects.push_back(pPrefabObject);
		}
		else if(pObject) 
		{
			throw std::logic_error(string("Object ") + prefabName.c_str() + " is not a prefab.");
		}
		else
		{
			throw std::logic_error(string("Object ") + prefabName.c_str() + " does not exist");

		}
	}

	GetIEditorImpl()->GetPrefabManager()->ExtractAllFromPrefabs(prefabObjects);
}

static void PyCloneAllFromSelection()
{
	GetIEditorImpl()->GetPrefabManager()->CloneAllFromSelection();
}

static void PyOpen()
{
	GetIEditorImpl()->GetPrefabManager()->OpenSelected();
}

static void PyClose()
{
	GetIEditorImpl()->GetPrefabManager()->CloseSelected();
}

static void PyOpenAll()
{
	std::vector<CPrefabObject*> prefabObjects;
	GetIEditorImpl()->GetPrefabManager()->GetPrefabObjects(prefabObjects);
	if (!prefabObjects.empty())
	{
		CUndo undo("Open all prefab objects");
		GetIEditorImpl()->GetPrefabManager()->OpenPrefabs(prefabObjects);
	}
}

static void PyCloseAll()
{
	std::vector<CPrefabObject*> prefabObjects;
	GetIEditorImpl()->GetPrefabManager()->GetPrefabObjects(prefabObjects);
	if (!prefabObjects.empty())
	{
		CUndo undo("Close all prefab objects");
		GetIEditorImpl()->GetPrefabManager()->ClosePrefabs(prefabObjects);
	}
}

static void PyReloadAll()
{
	GetIEditorImpl()->GetObjectManager()->SendEvent(EVENT_PREFAB_REMAKE);
}

static void PyUpdateAllPrefabs()
{
	GetIEditor()->GetPrefabManager()->UpdateAllPrefabsToLatestVersion();
}

static void PySelectAllInstancesOfSelectedType()
{
	CBaseObjectsArray objects;
	GetIEditorImpl()->GetSelection()->GetObjects(objects);
	std::vector<CPrefabItem*> items = GetIEditor()->GetPrefabManager()->GetAllPrefabItems(objects);
	//Select all instances only if we have one type of prefab in selection
	if (items.size() == 1)
	{
		GetIEditor()->GetPrefabManager()->SelectAllInstancesOfItem(items[0]);
	}
}
}

DECLARE_PYTHON_MODULE(prefab);

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_PrefabCommands::PyUpdateAllPrefabs, prefab, update_all_prefabs,
                                   CCommandDescription("Update all prefabs to latest version"));

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_PrefabCommands::PyCreateFromSelection, prefab, create_from_selection,
                                   CCommandDescription("Create prefab"));
REGISTER_EDITOR_COMMAND_ICON(prefab, create_from_selection, "icons:Tools/Create_Prefab.ico");

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_PrefabCommands::PyAddSelection, prefab, add_to_prefab,
                                   CCommandDescription("Add selection to prefab"));

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_PrefabCommands::PyExtractAllFromSelection, prefab, extract_all,
                                   CCommandDescription("Extract all objects"));

REGISTER_ONLY_PYTHON_COMMAND(Private_PrefabCommands::PyExtractAllFromPrefabs, prefab, extract_all_from_prefabs,
	CCommandDescription("Extract all object inside the prefabs indexed by name in the prefabNames list").Param("prefabNames", "The list of prefabs we have to extract the objects from"));

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_PrefabCommands::PyCloneAllFromSelection, prefab, clone_all,
                                   CCommandDescription("Clone all objects"));

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_PrefabCommands::PyOpen, prefab, open,
                                   CCommandDescription("Open prefab"));

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_PrefabCommands::PyClose, prefab, close,
                                   CCommandDescription("Close prefab"));

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_PrefabCommands::PyOpenAll, prefab, open_all,
                                   CCommandDescription("Open all prefabs"));

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_PrefabCommands::PyCloseAll, prefab, close_all,
                                   CCommandDescription("Close all prefabs"));

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_PrefabCommands::PyReloadAll, prefab, reload_all,
                                   CCommandDescription("Reload all prefabs"));

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_PrefabCommands::PySelectAllInstancesOfSelectedType, prefab, select_all_instances_of_type,
                                   CCommandDescription("Select all prefabs instances of selected type"));

REGISTER_PYTHON_COMMAND(Private_PrefabCommands::PyNewPrefabFromSelection, prefab, new_prefab_from_selection, "Set the pivot position of a specified prefab.");

REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE(Private_PrefabCommands::PyGetPrefabOfChild, prefab, get_parent,
                                          "Get the parent prefab object of a given child object.",
                                          "prefab.get_parent(str childName)");

REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE(Private_PrefabCommands::PyGetPrefabItems, prefab, get_items,
                                          "Get the avalible prefab item of a specified library and group.",
                                          "prefab.get_items()");

REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE(Private_PrefabCommands::PyGetPrefabChildWorldPos, prefab, get_world_pos,
                                          "Get the absolute world position of the specified prefab object.",
                                          "prefab.get_world_pos()");

REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE(Private_PrefabCommands::PyHasPrefabItem, prefab, has_item,
                                          "Return true if in the specified prefab library, and in the specified group, the specified item exists.",
                                          "prefab.has_item()");

REGISTER_ONLY_PYTHON_COMMAND(Private_PrefabCommands::PyNewPrefabItem, prefab, new_item,
                             CCommandDescription("Creates a prefab item").Param("itemName", "The name of the item to create"));

REGISTER_ONLY_PYTHON_COMMAND(Private_PrefabCommands::PyNewPrefab, prefab, new_prefab,
                             CCommandDescription("Creates a new prefab from the item named itemName and sets its name to prefabName. Note that prefabName is modified to make it unique, so the returned name might be different from the one passed as argument").Param("itemName", "The name of the item to create").Param("prefabName", "The name of the prefab to create"));

REGISTER_PYTHON_COMMAND(Private_PrefabCommands::PyDeletePrefabItem, prefab, delete_item,
                        CCommandDescription("Delete a prefab item from a specified prefab library.").Param("itemName", "The name of the item to delete"));