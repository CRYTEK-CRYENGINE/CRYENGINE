// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "PrefabManager.h"

#include "GameEngine.h"
#include "LogFile.h"
#include "Objects/PrefabObject.h"
#include "Objects/SelectionGroup.h"
#include "PrefabEvents.h"
#include "PrefabItem.h"
#include "PrefabLibrary.h"

#include <AssetSystem/AssetImportContext.h>
#include <AssetSystem/AssetManager.h>
#include <AssetSystem/Browser/AssetBrowserDialog.h>
#include <Controls/QuestionDialog.h>
#include <FileUtils.h>
#include <PathUtils.h>
#include <Util/FileUtil.h>

namespace Private_PrefabManager
{

void ImportPrefabLibrary(XmlNodeRef& library, const char* szPath)
{
	CAssetManager* const pAssetManager = GetIEditor()->GetAssetManager();

	std::vector<CAsset*> assets;
	for (int i = 0; i < library->getChildCount(); i++)
	{
		XmlNodeRef itemNode = library->getChild(i);
		if (stricmp(itemNode->getTag(), "Prefab") != 0)
		{
			continue;
		}

		string name;
		CryGUID guid;
		if (!itemNode->getAttr("Name", name) || !itemNode->getAttr("Id", guid))
		{
			continue;
		}

		if (pAssetManager->FindAssetById(guid))
		{
			continue;
		}

		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_COMMENT, "Creating prefab asset %s in %s", name.c_str(), szPath);

		// Create a sub folder for each part of the item name separated by a period.
		name.replace('.', '/');
		const string dataFilePath(PathUtil::Make(szPath, name, "prefab"));

		GetISystem()->GetIPak()->MakeDir(PathUtil::Make(PathUtil::GetGameProjectAssetsPath(), PathUtil::GetPathWithoutFilename(dataFilePath)));

		itemNode->setAttr("Name", PathUtil::GetFileName(name).c_str());
		XmlHelpers::SaveXmlNode(itemNode, dataFilePath.c_str());

		CAsset* pAsset = new CAsset("Prefab", guid, PathUtil::GetFileName(dataFilePath));
		CAssetImportContext ctx;
		CEditableAsset asset = ctx.CreateEditableAsset(*pAsset);
		asset.SetMetadataFile(string().Format("%s.cryasset", dataFilePath.c_str()));
		asset.AddFile(dataFilePath);
		asset.WriteToFile();
		assets.push_back(pAsset);
	}

	if (!assets.empty())
	{
		pAssetManager->MergeAssets(assets);
	}
}

class CObjectsRefOwner
{
public:
	CObjectsRefOwner(const std::vector<CBaseObject*>& objects)
		: m_objects(objects)
	{
		for (auto pObject : m_objects)
		{
			pObject->AddRef();
		}
	}

	void Release()
	{
		if (m_isReleased)
		{
			return;
		}

		m_isReleased = true;
		for (auto pObject : m_objects)
		{
			pObject->Release();
		}
	}

	~CObjectsRefOwner()
	{
		Release();
	}

private:
	const std::vector<CBaseObject*>& m_objects;
	bool                             m_isReleased = { false };
};

}
//////////////////////////////////////////////////////////////////////////
// CUndoGroupObjectOpenClose implementation.
//////////////////////////////////////////////////////////////////////////
class CUndoGroupObjectOpenClose : public IUndoObject
{
public:
	CUndoGroupObjectOpenClose(CPrefabObject* prefabObj)
	{
		m_prefabObject = prefabObj;
		m_bOpenForUndo = m_prefabObject->IsOpen();
	}
protected:
	virtual const char* GetDescription() { return "Prefab's Open/Close"; }

	virtual void        Undo(bool bUndo)
	{
		m_bRedo = bUndo;
		Apply(m_bOpenForUndo);
	}
	virtual void Redo()
	{
		if (m_bRedo)
			Apply(!m_bOpenForUndo);
	}

	void Apply(bool bOpen)
	{
		if (m_prefabObject)
		{
			if (bOpen)
				m_prefabObject->Open();
			else
				m_prefabObject->Close();
		}
	}

private:

	CPrefabObjectPtr m_prefabObject;
	bool             m_bRedo;
	bool             m_bOpenForUndo;
};

//////////////////////////////////////////////////////////////////////////
// CUndoAddObjectsToPrefab implementation.
//////////////////////////////////////////////////////////////////////////

CUndoAddObjectsToPrefab::CUndoAddObjectsToPrefab(CPrefabObject* prefabObj, std::vector<CBaseObject*>& objects)
{
	m_pPrefabObject = prefabObj;

	// Rearrange to parent-first
	for (int i = 0; i < objects.size(); i++)
	{
		if (objects[i]->GetParent())
		{
			// Find later in array.
			for (int j = i + 1; j < objects.size(); j++)
			{
				if (objects[j] == objects[i]->GetParent())
				{
					// Swap the objects.
					std::swap(objects[i], objects[j]);
					i--;
					break;
				}
			}
		}
	}

	m_addedObjects.reserve(objects.size());
	for (size_t i = 0, count = objects.size(); i < count; ++i)
	{
		m_addedObjects.push_back(SObjectsLinks());
		SObjectsLinks& addedObject = m_addedObjects.back();

		addedObject.m_object = objects[i]->GetId();
		// Store parent before the add operation
		addedObject.m_objectParent = objects[i]->GetParent() ? objects[i]->GetParent()->GetId() : CryGUID::Null();
		// Store children before the add operation
		if (const size_t childsCount = objects[i]->GetChildCount())
		{
			addedObject.m_objectsChilds.reserve(childsCount);
			for (size_t j = 0; j < childsCount; ++j)
			{
				addedObject.m_objectsChilds.push_back(objects[i]->GetChild(j)->GetId());
			}
		}
	}
}

void CUndoAddObjectsToPrefab::Undo(bool bUndo)
{
	// Start from the back where the childs are
	for (int i = m_addedObjects.size() - 1; i >= 0; --i)
	{
		IObjectManager* pObjMan = GetIEditorImpl()->GetObjectManager();

		// Remove from prefab
		if (CBaseObject* pMember = pObjMan->FindObject(m_addedObjects[i].m_object))
		{
			m_pPrefabObject->RemoveMember(pMember);
			// Restore parent links
			if (CBaseObject* pParent = pObjMan->FindObject(m_addedObjects[i].m_objectParent))
				pParent->AttachChild(pMember);

			// Restore child links
			if (const int childsCount = m_addedObjects[i].m_objectsChilds.size())
			{
				for (int j = 0; j < childsCount; ++j)
				{
					if (CBaseObject* pChild = pObjMan->FindObject(m_addedObjects[i].m_objectsChilds[j]))
						pMember->AttachChild(pChild);
				}
			}
		}
	}
}

void CUndoAddObjectsToPrefab::Redo()
{
	for (int i = 0, count = m_addedObjects.size(); i < count; ++i)
	{
		if (CBaseObject* pMember = GetIEditorImpl()->GetObjectManager()->FindObject(m_addedObjects[i].m_object))
			m_pPrefabObject->AddMember(pMember);
	}
}

//////////////////////////////////////////////////////////////////////////
// CPrefabManager implementation.
//////////////////////////////////////////////////////////////////////////

CPrefabManager::CPrefabManager()
	: m_pPrefabEvents(nullptr)
{
	m_bUniqNameMap = true;

	m_pPrefabEvents = new CPrefabEvents();

	m_skipPrefabUpdate = false;

	GetIEditorImpl()->GetIUndoManager()->AddListener(this);
}

CPrefabManager::~CPrefabManager()
{
	GetIEditorImpl()->GetIUndoManager()->RemoveListener(this);
	SAFE_DELETE(m_pPrefabEvents);
}

void CPrefabManager::ClearAll()
{
	CBaseLibraryManager::ClearAll();

	m_pPrefabEvents->RemoveAllEventData();
}

IDataBaseItem* CPrefabManager::CreateItem(const string& filename)
{
	const string libraryName = PathUtil::RemoveExtension(filename);
	IDataBaseLibrary* const pLibrary = AddLibrary(libraryName);
	if (!pLibrary)
	{
		return nullptr;
	}

	return CBaseLibraryManager::CreateItem(pLibrary);
}

CBaseLibraryItem* CPrefabManager::MakeNewItem()
{
	return new CPrefabItem;
}

CBaseLibrary* CPrefabManager::MakeNewLibrary()
{
	return new CPrefabLibrary(this);
}

string CPrefabManager::GetRootNodeName()
{
	return "PrefabsLibrary";
}

string CPrefabManager::GetLibsPath()
{
	return {};
}

void CPrefabManager::Serialize(XmlNodeRef& node, bool bLoading)
{
	CRY_PROFILE_FUNCTION(PROFILE_LOADING_ONLY);
	if (bLoading)
	{
		CBaseLibraryManager::Serialize(node, bLoading);
	}
	else
	{
		// TODO: remove when DEV - 5324 is done, since prefabs are assets.
		SaveAllLibs();
	}
}

CPrefabItem* CPrefabManager::MakeFromSelection(const CSelectionGroup* pSelectionGroup)
{
	const CAssetType* const pAssetType = GetIEditor()->GetAssetManager()->FindAssetType("Prefab");
	if (!pAssetType)
	{
		return nullptr;
	}

	const string assetBasePath = CAssetBrowserDialog::SaveSingleAssetForType(pAssetType->GetTypeName());
	if (assetBasePath.empty())
	{
		return nullptr; // Operation cancelled by user.
	}

	const string newAssetPath = string().Format("%s.%s.cryasset", assetBasePath.c_str(), pAssetType->GetFileExtension());

	SPrefabCreateParams createParam(pSelectionGroup);

	pAssetType->Create(newAssetPath, &createParam);
	CAsset* const pAsset = GetIEditor()->GetAssetManager()->FindAssetForMetadata(newAssetPath);
	if (!pAsset)
	{
		return nullptr;
	}
	return static_cast<CPrefabItem*>(GetIEditor()->GetPrefabManager()->LoadItem(pAsset->GetGUID()));
}

void CPrefabManager::AddSelectionToPrefab()
{
	const CSelectionGroup* pSel = GetIEditorImpl()->GetSelection();
	CPrefabObject* pPrefab = 0;
	int selectedPrefabCount = 0;
	for (int i = 0; i < pSel->GetCount(); i++)
	{
		CBaseObject* pObj = pSel->GetObject(i);
		if (pObj->IsKindOf(RUNTIME_CLASS(CPrefabObject)))
		{
			++selectedPrefabCount;
			pPrefab = (CPrefabObject*) pObj;
		}
	}
	if (selectedPrefabCount == 0)
	{
		Warning("Select a prefab and objects");
		return;
	}
	if (selectedPrefabCount > 1)
	{
		Warning("Select only one prefab");
		return;
	}

	AddSelectionToPrefab(pPrefab);
}

void CPrefabManager::OpenSelected()
{
	const CSelectionGroup* pSelection = GetIEditorImpl()->GetSelection();
	if (!pSelection)
		return;

	const int count = pSelection->GetCount();
	if (!count)
		return;

	std::vector<CPrefabObject*> selectedPrefabs(count);

	for (int i = 0; i < count; ++i)
		selectedPrefabs[i] = static_cast<CPrefabObject*>(pSelection->GetObject(i));

	CUndo undo("Open selected prefabs");
	OpenPrefabs(selectedPrefabs);
}

void CPrefabManager::CloseSelected()
{
	const CSelectionGroup* pSelection = GetIEditorImpl()->GetSelection();
	if (!pSelection)
		return;

	const int count = pSelection->GetCount();
	if (!count)
		return;

	std::vector<CPrefabObject*> selectedPrefabs(count);

	for (int i = 0; i < count; ++i)
		selectedPrefabs[i] = static_cast<CPrefabObject*>(pSelection->GetObject(i));

	CUndo undo("Close selected prefabs");
	ClosePrefabs(selectedPrefabs);
}

void CPrefabManager::AddSelectionToPrefab(CPrefabObject* pPrefab)
{
	using namespace Private_PrefabManager;
	const CSelectionGroup* pSel = GetIEditorImpl()->GetSelection();

	std::vector<CBaseObject*> objects;
	objects.reserve(pSel->GetCount());
	for (int i = 0; i < pSel->GetCount(); i++)
	{
		CBaseObject* pObj = pSel->GetObject(i);
		if (pObj != pPrefab)
			objects.push_back(pObj);
	}

	CObjectsRefOwner objectOwner(objects);

	// Check objects if they can be added
	bool invalidAddOperation = false;
	for (int i = 0, count = objects.size(); i < count; ++i)
	{
		if (!pPrefab->CanAddMember(objects[i]))
		{
			Warning("Object %s is already part of a prefab (%s)", objects[i]->GetName(), objects[i]->GetPrefab()->GetName());
			invalidAddOperation = true;
		}
	}

	if (invalidAddOperation)
		return;

	CUndo undo("Add Objects To Prefab");
	if (CUndo::IsRecording())
		CUndo::Record(new CUndoAddObjectsToPrefab(pPrefab, objects));

	pPrefab->AddMembers(objects);

	// If we have nested dependencies between these object send an modify event afterwards to resolve them properly (e.g. shape objects linked to area triggers)
	for (int i = 0; i < objects.size(); i++)
		objects[i]->UpdatePrefab();
}

void CPrefabManager::ExtractObjectsFromPrefabs(std::vector<CBaseObject*>& childObjects)
{
	if (childObjects.empty())
		return;

	CUndo undo("Extract object(s) from prefab");

	CloneObjectsFromPrefabs(childObjects);

	IObjectManager* pObjectManager = GetIEditorImpl()->GetObjectManager();
	for (CBaseObject* pChild : childObjects)
	{
		CPrefabObject* pPrefab = (CPrefabObject*)pChild->GetPrefab();

		for (int i = 0, count = pPrefab->GetChildCount(); i < count; ++i)
		{
			CBaseObject* pChildToReparent = pPrefab->GetChild(i);
			pPrefab->AttachChild(pChildToReparent);
			pChildToReparent->UpdatePrefab();
		}

		pObjectManager->DeleteObject(pChild);
	}
}

void CPrefabManager::ExtractAllFromPrefabs(std::vector<CPrefabObject*>& prefabs)
{
	if (prefabs.empty())
		return;

	CUndo undo("Extract all from prefab(s)");

	CloneAllFromPrefabs(prefabs);

	IObjectManager* pObjectManager = GetIEditorImpl()->GetObjectManager();
	for (CPrefabObject* pPrefab : prefabs)
		pObjectManager->DeleteObject(pPrefab);
}

void CPrefabManager::ExtractAllFromSelection()
{
	const CSelectionGroup* pSelection = GetIEditorImpl()->GetSelection();

	if (!pSelection)
		return;

	std::vector<CPrefabObject*> prefabs;

	for (int i = 0, count = pSelection->GetCount(); i < count; i++)
	{
		if (pSelection->GetObject(i) && pSelection->GetObject(i)->IsKindOf(RUNTIME_CLASS(CPrefabObject)))
		{
			CPrefabObject* pPrefab = static_cast<CPrefabObject*>(pSelection->GetObject(i));
			prefabs.push_back(pPrefab);
		}
	}

	if (!prefabs.empty())
	{
		ExtractAllFromPrefabs(prefabs);
	}
}

bool CPrefabManager::AttachObjectToPrefab(CPrefabObject* pPrefab, CBaseObject* pObject)
{
	if (pPrefab && pObject)
	{
		if (!pPrefab->CanAddMember(pObject))
		{
			Warning("Object %s cannot be added as a Member to prefab %s", pObject->GetName(), pPrefab->GetName());
			return false;
		}

		std::vector<CBaseObject*> objects;

		CUndo undo("Add Object To Prefab");
		if (CUndo::IsRecording())
		{
			// If this is not a group add all the attached children to the prefab,
			// otherwise the group children adding is handled by the AddMember in CPrefabObject
			if (!pObject->IsKindOf(RUNTIME_CLASS(CGroup)))
			{
				objects.reserve(pObject->GetChildCount() + 1);
				objects.push_back(pObject);
				TBaseObjects descendants;
				descendants.reserve(pObject->GetChildCount() + 1);
				pObject->GetAllDescendants(descendants);
				for (const auto& child : descendants)
				{
					objects.push_back(child);
				}
			}
			else
			{
				objects.push_back(pObject);
			}

			CUndo::Record(new CUndoAddObjectsToPrefab(pPrefab, objects));
		}

		for (int i = 0; i < objects.size(); i++)
		{
			pPrefab->AddMember(objects[i]);
		}
		return true;
	}
	return false;
}

void CPrefabManager::CloneObjectsFromPrefabs(std::vector<CBaseObject*>& childObjects)
{
	if (childObjects.empty())
		return;

	CUndo undo("Clone Object(s) from Prefab");

	IObjectManager* pObjectManager = GetIEditorImpl()->GetObjectManager();
	pObjectManager->ClearSelection();

	std::vector<CBaseObject*> clonedObjects;
	std::map<CPrefabObject*, CSelectionGroup> prefabObjectsToBeExtracted;

	for (int i = 0, childCount = childObjects.size(); i < childCount; ++i)
	{
		if (CPrefabObject* pPrefab = (CPrefabObject*)childObjects[i]->GetPrefab())
		{
			CSelectionGroup& selGroup = prefabObjectsToBeExtracted[pPrefab];
			ExpandGroup(childObjects[i], selGroup);
		}
	}

	std::map<CPrefabObject*, CSelectionGroup>::iterator it = prefabObjectsToBeExtracted.begin();
	std::map<CPrefabObject*, CSelectionGroup>::iterator end = prefabObjectsToBeExtracted.end();
	for (; it != end; ++it)
	{
		(it->first)->CloneSelected(&(it->second), clonedObjects);
	}

	pObjectManager->AddObjectsToSelection(clonedObjects);
}

void CPrefabManager::CloneAllFromPrefabs(std::vector<CPrefabObject*>& prefabs)
{
	IObjectManager* pObjectManager = GetIEditor()->GetObjectManager();

	CUndo undo("Clone all from prefab(s)");

	pObjectManager->ClearSelection();

	std::vector<CBaseObject*> objectsToSelect;
	for (CPrefabObject* pPrefab : prefabs)
	{
		bool autoUpdatePrefab = pPrefab->GetAutoUpdatePrefab();
		pPrefab->SetAutoUpdatePrefab(false);
		pPrefab->StoreUndo("Set Auto Update");

		auto childCount = pPrefab->GetChildCount();
		std::vector<CBaseObject*> children;
		std::vector<CBaseObject*> newChildren;
		children.reserve(childCount);
		newChildren.reserve(childCount);

		for (auto i = 0; i < childCount; ++i)
		{
			children.push_back(pPrefab->GetChild(i));
		}

		pObjectManager->CloneObjects(children, newChildren);

		pPrefab->RemoveMembers(newChildren);
		pPrefab->SetAutoUpdatePrefab(autoUpdatePrefab);
		pPrefab->StoreUndo("Set Auto Update");

		objectsToSelect.insert(objectsToSelect.cend(), newChildren.begin(), newChildren.end());
	}

	pObjectManager->AddObjectsToSelection(objectsToSelect);
}

void CPrefabManager::CloneAllFromSelection()
{
	const CSelectionGroup* pSelection = GetIEditorImpl()->GetSelection();

	if (!pSelection)
		return;

	std::vector<CPrefabObject*> prefabs;

	for (int i = 0, count = pSelection->GetCount(); i < count; i++)
	{
		if (pSelection->GetObject(i) && pSelection->GetObject(i)->IsKindOf(RUNTIME_CLASS(CPrefabObject)))
		{
			CPrefabObject* pPrefab = static_cast<CPrefabObject*>(pSelection->GetObject(i));
			prefabs.push_back(pPrefab);
		}
	}

	if (!prefabs.empty())
	{
		GetIEditor()->GetPrefabManager()->CloneAllFromPrefabs(prefabs);
	}
}

bool CPrefabManager::OpenPrefabs(std::vector<CPrefabObject*>& prefabObjects)
{
	if (prefabObjects.empty())
		return false;

	bool bOpenedAtLeastOne = false;

	CUndo undo("Open prefabs");
	for (int i = 0, iCount(prefabObjects.size()); i < iCount; ++i)
	{
		CPrefabObject* pPrefabObj = (CPrefabObject*)prefabObjects[i];
		if (!pPrefabObj->IsOpen())
		{
			if (CUndo::IsRecording())
				CUndo::Record(new CUndoGroupObjectOpenClose(pPrefabObj));

			pPrefabObj->Open();
			bOpenedAtLeastOne = true;
		}
	}

	return bOpenedAtLeastOne;
}

bool CPrefabManager::ClosePrefabs(std::vector<CPrefabObject*>& prefabObjects)
{
	if (prefabObjects.empty())
		return false;

	bool bClosedAtLeastOne = false;

	CUndo undo("Close prefabs");
	for (int i = 0, iCount(prefabObjects.size()); i < iCount; ++i)
	{
		CPrefabObject* pPrefabObj = (CPrefabObject*)prefabObjects[i];
		if (pPrefabObj->IsOpen())
		{
			if (CUndo::IsRecording())
				CUndo::Record(new CUndoGroupObjectOpenClose(pPrefabObj));
			pPrefabObj->Close();
			bClosedAtLeastOne = true;
		}
	}

	return bClosedAtLeastOne;
}

void CPrefabManager::GetPrefabObjects(std::vector<CPrefabObject*>& outPrefabObjects)
{
	std::vector<CBaseObject*> prefabObjects;
	GetIEditorImpl()->GetObjectManager()->FindObjectsOfType(RUNTIME_CLASS(CPrefabObject), prefabObjects);
	if (prefabObjects.empty())
		return;

	for (int i = 0, iCount(prefabObjects.size()); i < iCount; ++i)
	{
		CPrefabObject* pPrefabObj = (CPrefabObject*)prefabObjects[i];
		if (pPrefabObj == NULL)
			continue;

		outPrefabObjects.push_back(pPrefabObj);
	}
}

int CPrefabManager::GetPrefabInstanceCount(IDataBaseItem* pPrefabItem)
{
	int instanceCount = 0;
	std::vector<CPrefabObject*> prefabObjects;
	GetPrefabObjects(prefabObjects);

	if (pPrefabItem)
	{
		for (int i = 0, prefabsFound(prefabObjects.size()); i < prefabsFound; ++i)
		{
			CPrefabObject* pPrefabObject = (CPrefabObject*)prefabObjects[i];
			if (pPrefabObject->GetPrefabGuid() == pPrefabItem->GetGUID())
			{
				++instanceCount;
			}
		}
	}

	return instanceCount;
}

IDataBaseLibrary* CPrefabManager::AddLibrary(const string& library, bool bSetFullFilename /*= false*/)
{
	// Check if library with same name already exist.
	IDataBaseLibrary* const pBaseLib = FindLibrary(library);
	if (pBaseLib)
	{
		return pBaseLib;
	}

	CBaseLibrary* const pLib = MakeNewLibrary();
	pLib->SetName(library);

	const string filename = MakeFilename(library);

	pLib->SetFilename(filename);
	pLib->SetModified(false);

	m_libs.push_back(pLib);
	return pLib;
}

string CPrefabManager::MakeFilename(const string& library)
{
	return string().Format("%s.%s", library.c_str(), GetFileExtension());
}

void CPrefabManager::importAssetsFromLevel(XmlNodeRef& levelRoot)
{
	using namespace Private_PrefabManager;

	XmlNodeRef libs = levelRoot->findChild("PrefabsLibrary");
	if (!libs)
	{
		return;
	}

	ICryPak* const pCryPak = GetIEditor()->GetSystem()->GetIPak();

	for (int i = 0; i < libs->getChildCount(); i++)
	{
		XmlNodeRef libNode = libs->getChild(i);
		string libName;
		if (!libNode->getAttr("Name", libName))
		{
			continue;
		}

		if (strcmp(libNode->getTag(), "LevelLibrary") != 0)
		{
			const char* szRootFolder = "Prefabs";
			const string filename(PathUtil::Make(szRootFolder, libName, "xml"));

			if (pCryPak->IsFileExist(PathUtil::ReplaceExtension(filename.c_str(), "bak"), ICryPak::eFileLocation_OnDisk))
			{
				// already imported
				continue;
			}

			if (!pCryPak->IsFileExist(filename.c_str()))
			{
				CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "Prefab library file not found: %s", filename.c_str());
				continue;
			}

			libNode = XmlHelpers::LoadXmlFromFile(filename);
			const string path = PathUtil::Make(szRootFolder, libName);

			ImportPrefabLibrary(libNode, path);

			FileUtils::BackupFile(PathUtil::Make(PathUtil::GetGameProjectAssetsPath(), path).c_str());
		}
		else
		{
			const char* szRootFolder = "Prefabs/LevelLibrary";
			const string path = PathUtil::Make(szRootFolder, PathUtil::ToGamePath(GetIEditorImpl()->GetLevelFolder()));
			ImportPrefabLibrary(libNode, path);
		}
	}
}

void CPrefabManager::UpdateAllPrefabsToLatestVersion()
{
	CAssetManager* const pAssetManager = GetIEditor()->GetAssetManager();
	const CAssetType* const pPrefabType = pAssetManager->FindAssetType("Prefab");
	pAssetManager->ForeachAssetOfType(pPrefabType, [this](CAsset* pAsset)
	{
		//get the asset guid and use it to load the prefab item (asset guid is the same as prefab guid)
		const CryGUID& guid = pAsset->GetGUID();
		CPrefabItem* pItem = static_cast<CPrefabItem*>(LoadItem(guid));
		//upgrade the object and remember to reload all the objects in the scene to apply the new changes
		pItem->CheckVersionAndUpgrade();
		//update all the instances in the level (if there are any)
		pItem->UpdateObjects();
		//serialize the changes to file
		pItem->GetLibrary()->Save();
	});
}

std::vector<CPrefabItem*> CPrefabManager::GetAllPrefabItems(const std::vector<CBaseObject*>& objects)
{
	std::vector<CPrefabItem*> items;

	for (const CBaseObject* pObject : objects)
	{
		if (pObject->IsKindOf(RUNTIME_CLASS(CPrefabObject)))
		{
			const CPrefabObject* pPrefabObject = static_cast<const CPrefabObject*>(pObject);
			CPrefabItem* pItem = pPrefabObject->GetPrefabItem();

			if (std::find(items.begin(), items.end(), pItem) == items.end())
			{
				items.push_back(pItem);
			}
		}
	}

	return items;
}

std::vector<CBaseObject*> CPrefabManager::FindAllInstancesOfItem(const CPrefabItem* pPrefabItem)
{
	CBaseObjectsArray objects;
	GetIEditor()->GetObjectManager()->FindObjectsOfType(OBJTYPE_PREFAB, objects);

	std::vector<CBaseObject*> instances;
	for (CBaseObject* pObject : objects)
	{
		CPrefabObject* pPrefabObject = static_cast<CPrefabObject*>(pObject);

		if (pPrefabObject->GetPrefabItem()->GetGUID() == pPrefabItem->GetGUID())
		{
			instances.push_back(pPrefabObject);
		}
	}

	return instances;
}

void CPrefabManager::SelectAllInstancesOfItem(const CPrefabItem* pPrefabItem)
{
	std::vector<CBaseObject*> instances = FindAllInstancesOfItem(pPrefabItem);
	GetIEditor()->GetObjectManager()->ClearSelection();
	GetIEditor()->GetObjectManager()->AddObjectsToSelection(instances);
}

void CPrefabManager::DeleteItem(IDataBaseItem* pItem)
{
	assert(pItem);

	CPrefabItem* pPrefabItem = (CPrefabItem*)pItem;

	// Delete all objects from object manager that have this prefab item
	std::vector<CryGUID> guids;
	CBaseObjectsArray objects;
	IObjectManager* pObjMan = GetIEditorImpl()->GetObjectManager();
	pObjMan->GetObjects(objects);
	for (int i = 0; i < objects.size(); ++i)
	{
		CBaseObject* pObj = objects[i];
		if (pObj->IsKindOf(RUNTIME_CLASS(CPrefabObject)))
		{
			CPrefabObject* pPrefab = (CPrefabObject*) pObj;
			if (pPrefab->GetPrefabItem() == pPrefabItem)
			{
				// Collect guids first to delete objects later
				guids.push_back(pPrefab->GetId());
			}
		}
	}
	for (int i = 0; i < guids.size(); ++i)
	{
		CBaseObject* pObj = pObjMan->FindObject(guids[i]);
		if (pObj)
			pObjMan->DeleteObject(pObj);
	}

	IDataBaseLibrary* pLibrary = pItem->GetLibrary();

	CBaseLibraryManager::DeleteItem(pItem);

	//cleanup the library when it's been emptied
	if (pLibrary && !pLibrary->GetItemCount())
	{
		DeleteLibrary(pLibrary->GetName());
	}
}

IDataBaseItem* CPrefabManager::LoadItem(const CryGUID& guid)
{
	IDataBaseItem* pItem = FindItem(guid);
	if (pItem)
	{
		return pItem;
	}

	const CAsset* const pAsset = GetIEditor()->GetAssetManager()->FindAssetById(guid);
	if (pAsset && pAsset->GetFilesCount() && LoadLibrary(pAsset->GetFile(0)))
	{
		return FindItem(guid);
	}

	return nullptr;
}

void CPrefabManager::ExpandGroup(CBaseObject* pObject, CSelectionGroup& selection) const
{
	selection.AddObject(pObject);
	if (pObject->IsKindOf(RUNTIME_CLASS(CGroup)) && !pObject->IsKindOf(RUNTIME_CLASS(CPrefabObject)))
	{
		CGroup* pGroup = static_cast<CGroup*>(pObject);
		for (int i = 0, count = pGroup->GetChildCount(); i < count; ++i)
		{
			ExpandGroup(pGroup->GetChild(i), selection);
		}
	}
}

IDataBaseLibrary* CPrefabManager::LoadLibrary(const string& filename, bool bReload)
{
	IDataBaseLibrary* pLibrary = CBaseLibraryManager::LoadLibrary(filename, bReload);
	if (bReload && pLibrary)
	{
		CPrefabLibrary* pPrefabLibrary = (CPrefabLibrary*)pLibrary;
		pPrefabLibrary->UpdatePrefabObjects();
	}
	return pLibrary;
}