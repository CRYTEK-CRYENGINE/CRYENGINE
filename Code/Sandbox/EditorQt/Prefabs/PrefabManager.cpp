// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "PrefabManager.h"

#include "PrefabItem.h"
#include "PrefabLibrary.h"

#include "GameEngine.h"

#include "DataBaseDialog.h"
#include "PrefabDialog.h"

#include "Objects/PrefabObject.h"

#include "PrefabEvents.h"

#define PREFABS_LIBS_PATH "Prefabs/"

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
	virtual const char* GetDescription() { return "Prefab's Open/Close"; };

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
CUndoAddObjectsToPrefab::CUndoAddObjectsToPrefab(CPrefabObject* prefabObj, TBaseObjects& objects)
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
		// Store childs before the add operation
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
	: m_pPrefabEvents(NULL)
{
	m_bUniqNameMap = true;
	m_pLevelLibrary = (CBaseLibrary*)AddLibrary("Level");
	m_pLevelLibrary->SetLevelLibrary(true);

	m_pPrefabEvents = new CPrefabEvents();

	m_skipPrefabUpdate = false;

	GetIEditorImpl()->GetIUndoManager()->AddListener(this);
}

//////////////////////////////////////////////////////////////////////////
CPrefabManager::~CPrefabManager()
{
	GetIEditorImpl()->GetIUndoManager()->RemoveListener(this);
	SAFE_DELETE(m_pPrefabEvents);
}

//////////////////////////////////////////////////////////////////////////
void CPrefabManager::ClearAll()
{
	CBaseLibraryManager::ClearAll();

	m_pPrefabEvents->RemoveAllEventData();

	m_pLevelLibrary = (CBaseLibrary*)AddLibrary("Level");
	m_pLevelLibrary->SetLevelLibrary(true);
}

//////////////////////////////////////////////////////////////////////////
CBaseLibraryItem* CPrefabManager::MakeNewItem()
{
	return new CPrefabItem;
}
//////////////////////////////////////////////////////////////////////////
CBaseLibrary* CPrefabManager::MakeNewLibrary()
{
	return new CPrefabLibrary(this);
}
//////////////////////////////////////////////////////////////////////////
string CPrefabManager::GetRootNodeName()
{
	return "PrefabsLibrary";
}
//////////////////////////////////////////////////////////////////////////
string CPrefabManager::GetLibsPath()
{
	if (m_libsPath.IsEmpty())
		m_libsPath = PREFABS_LIBS_PATH;
	return m_libsPath;
}

//////////////////////////////////////////////////////////////////////////
void CPrefabManager::Serialize(XmlNodeRef& node, bool bLoading)
{
	LOADING_TIME_PROFILE_SECTION;
	CBaseLibraryManager::Serialize(node, bLoading);
}

//////////////////////////////////////////////////////////////////////////
CPrefabItem* CPrefabManager::MakeFromSelection()
{
	CBaseLibraryDialog* dlg = GetIEditorImpl()->OpenDataBaseLibrary(EDB_TYPE_PREFAB);
	if (dlg && dlg->IsKindOf(RUNTIME_CLASS(CPrefabDialog)))
	{
		CPrefabDialog* pPrefabDialog = (CPrefabDialog*)dlg;
		return pPrefabDialog->GetPrefabFromSelection();
	}
	return 0;
}

//////////////////////////////////////////////////////////////////////////
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
	const CSelectionGroup* pSel = GetIEditorImpl()->GetSelection();

	TBaseObjects objects;
	for (int i = 0; i < pSel->GetCount(); i++)
	{
		CBaseObject* pObj = pSel->GetObject(i);
		if (pObj != pPrefab)
			objects.push_back(pObj);
	}

	// Check objects if they can be added
	bool invalidAddOperation = false;
	for (int i = 0, count = objects.size(); i < count; ++i)
	{
		if (!pPrefab->CanObjectBeAddedAsMember(objects[i]))
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

	for (int i = 0; i < objects.size(); i++)
		pPrefab->AddMember(objects[i]);

	// If we have nested dependencies between these object send an modify event afterwards to resolve them properly (e.g. shape objects linked to area triggers)
	for (int i = 0; i < objects.size(); i++)
		objects[i]->UpdatePrefab();
}

//////////////////////////////////////////////////////////////////////////
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

//////////////////////////////////////////////////////////////////////////
void CPrefabManager::ExtractAllFromPrefabs(std::vector<CPrefabObject*>& prefabs)
{
	if (prefabs.empty())
		return;

	CUndo undo("Extract all from prefab(s)");
	CloneAllFromPrefabs(prefabs);

	IObjectManager* pObjectManager = GetIEditor()->GetObjectManager();
	for (CPrefabObject* pPrefab : prefabs)
	{
		pObjectManager->DeleteObject(pPrefab);
	}
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

bool CPrefabManager::AttachObjectToPrefab(CPrefabObject* prefab, CBaseObject* obj)
{
	if (prefab && obj)
	{
		if (!prefab->CanObjectBeAddedAsMember(obj))
		{
			Warning("Object %s is already part of a prefab (%s)", obj->GetName(), obj->GetPrefab()->GetName());
			return false;
		}

		TBaseObjects objects;

		CUndo undo("Add Object To Prefab");
		if (CUndo::IsRecording())
		{
			// If this is not a group add all the attached children to the prefab,
			// otherwise the group children adding is handled by the AddMember in CPrefabObject
			if (!obj->IsKindOf(RUNTIME_CLASS(CGroup)))
			{
				objects.reserve(obj->GetChildCount() + 1);
				objects.push_back(obj);
				obj->GetAllChildren(objects);
			}
			else
			{
				objects.push_back(obj);
			}

			CUndo::Record(new CUndoAddObjectsToPrefab(prefab, objects));
		}

		for (int i = 0; i < objects.size(); i++)
		{
			prefab->AddMember(objects[i]);
		}
		return true;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
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

	pObjectManager->SelectObjects(clonedObjects);
}

//////////////////////////////////////////////////////////////////////////
void CPrefabManager::CloneAllFromPrefabs(std::vector<CPrefabObject*>& prefabs)
{
	IObjectManager* pObjectManager = GetIEditor()->GetObjectManager();

	// We must clone the objects within the prefab before detaching them because if the user
	// undoes this operation, the first thing that will happen is that a prefab of this type will be created.
	// Doing this, will cause the prefab to be fully constructed, rather than just the empty prefab.
	// This will cause GUID collision with the already existing objects (the ones we will extract now)
	// when the action was first executed
	CObjectCloneContext cloneContext;

	CUndo undo("Clone all from prefab(s)");

	pObjectManager->ClearSelection();

	std::vector<CBaseObject*> objectsToSelect;
	for (auto pPrefab : prefabs)
	{
		pPrefab->SetAutoUpdatePrefab(false);

		auto childCount = pPrefab->GetChildCount();
		std::vector<CBaseObject*> children;
		children.reserve(childCount);
		objectsToSelect.reserve(childCount);

		for (auto i = 0; i < childCount; ++i)
		{
			// Clone every object.
			CBaseObject* pFromObject = pPrefab->GetChild(i);
			CBaseObject* pNewObj = pObjectManager->CloneObject(pFromObject);
			if (!pNewObj) // can be null, e.g. sequence can't be cloned
			{
				continue;
			}
			cloneContext.AddClone(pFromObject, pNewObj);
			children.push_back(pNewObj);
		}

		// Only after everything was cloned, call PostClone on all cloned objects.
		// Copy objects map as it can be invalidated during PostClone
		auto objectsMap = cloneContext.m_objectsMap;
		for (auto it : objectsMap)
		{
			CBaseObject* pFromObject = it.first;
			CBaseObject* pClonedObject = it.second;
			if (pClonedObject)
			{
				pClonedObject->PostClone(pFromObject, cloneContext);
				objectsToSelect.push_back(pClonedObject);
			}
		}

		pPrefab->RemoveMembers(children);
	}

	pObjectManager->SelectObjects(objectsToSelect);
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

//////////////////////////////////////////////////////////////////////////
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

//////////////////////////////////////////////////////////////////////////
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

//////////////////////////////////////////////////////////////////////////
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

//////////////////////////////////////////////////////////////////////////
int CPrefabManager::GetPrefabInstanceCount(CPrefabItem* pPrefabItem)
{
	int instanceCount = 0;
	std::vector<CPrefabObject*> prefabObjects;
	GetPrefabObjects(prefabObjects);

	if (pPrefabItem)
	{
		CPrefabManager* pManager = GetIEditor()->GetPrefabManager();
		for (int i = 0, prefabsFound(prefabObjects.size()); i < prefabsFound; ++i)
		{
			CPrefabObject* pPrefabObject = (CPrefabObject*)prefabObjects[i];
			CPrefabItem* pRefItem = (CPrefabItem*)pManager->FindItem(pPrefabObject->GetPrefabGuid());

			if (pRefItem == pPrefabItem)
			{
				++instanceCount;
			}
		}
	}

	return instanceCount;
}

//////////////////////////////////////////////////////////////////////////
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

	__super::DeleteItem(pItem);
}

//////////////////////////////////////////////////////////////////////////
void CPrefabManager::ExpandGroup(CBaseObject* pObject, CSelectionGroup& selection) const
{
	selection.AddObject(pObject);
	if (pObject->IsKindOf(RUNTIME_CLASS(CGroup)) && !pObject->IsKindOf(RUNTIME_CLASS(CPrefabObject)))
	{
		CGroup* pGroup = static_cast<CGroup*>(pObject);
		const TBaseObjects& groupMembers = pGroup->GetMembers();
		for (int i = 0, count = groupMembers.size(); i < count; ++i)
			ExpandGroup(groupMembers[i], selection);
	}
}

//////////////////////////////////////////////////////////////////////////
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

namespace Private_PrefabCommands
{
static void PyCreateFromSelection()
{
	GetIEditorImpl()->SetEditTool(nullptr);
	GetIEditorImpl()->GetPrefabManager()->MakeFromSelection();
}

static void PyAddSelection()
{
	GetIEditorImpl()->GetPrefabManager()->AddSelectionToPrefab();
}

static void PyExtractAllFromSelection()
{
	GetIEditorImpl()->GetPrefabManager()->ExtractAllFromSelection();
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
}

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_PrefabCommands::PyCreateFromSelection, prefab, create_from_selection,
                                   CCommandDescription("Create prefab"));
REGISTER_EDITOR_COMMAND_ICON(prefab, create_from_selection, "icons:Tools/Create_Prefab.ico");

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_PrefabCommands::PyAddSelection, prefab, add_to_prefab,
                                   CCommandDescription("Add selection to prefab"));

REGISTER_EDITOR_AND_SCRIPT_COMMAND(Private_PrefabCommands::PyExtractAllFromSelection, prefab, extract_all,
                                   CCommandDescription("Extract all objects"));

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
