// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "PrefabObject.h"
#include "PrefabPicker.h"

#include "HyperGraph/FlowGraphManager.h"
#include "HyperGraph/Controls/HyperGraphEditorWnd.h"
#include "HyperGraph/Controls/FlowGraphSearchCtrl.h"
#include "Objects/EntityObject.h"
#include "Prefabs/PrefabEvents.h"
#include "Prefabs/PrefabItem.h"
#include "Prefabs/PrefabLibrary.h"
#include "Prefabs/PrefabManager.h"
#include "Serialization/Decorators/EntityLink.h"
#include "Util/BoostPythonHelpers.h"
#include "BaseLibraryManager.h"
#include "CryEditDoc.h"
#include "SelectionGroup.h"

#include <AssetSystem/AssetManager.h>
#include <AssetSystem/AssetType.h>
#include <Controls/DynamicPopupMenu.h>
#include <IDataBaseManager.h>
#include <IUndoManager.h>
#include <LevelEditor/Tools/PickObjectTool.h>
#include <Objects/InspectorWidgetCreator.h>
#include <Objects/IObjectLayer.h>
#include <Objects/ObjectLoader.h>
#include <Preferences/SnappingPreferences.h>
#include <Preferences/ViewportPreferences.h>
#include <Serialization/Decorators/EditorActionButton.h>
#include <Serialization/Decorators/EditToolButton.h>
#include <Viewport.h>

#include <Util/MFCUtil.h>

#include <CryCore/ToolsHelpers/GuidUtil.h>
#include <CryGame/IGameFramework.h>
#include <CryMath/Cry_Camera.h>
#include <CryPhysics/physinterface.h>
#include <CrySystem/ICryLink.h>

REGISTER_CLASS_DESC(CPrefabObjectClassDesc);

IMPLEMENT_DYNCREATE(CPrefabObject, CGroup)

namespace Private_PrefabObject
{
class CScopedPrefabEventsDelay
{
public:
	CScopedPrefabEventsDelay()
		: m_resumed{false}
	{
		CPrefabEvents* pPrefabEvents = GetIEditor()->GetPrefabManager()->GetPrefabEvents();
		CRY_ASSERT(pPrefabEvents != nullptr);

		pPrefabEvents->SetCurrentlySettingPrefab(true);
	}

	void Resume()
	{
		if (!m_resumed)
		{
			m_resumed = true;
			auto events = GetIEditor()->GetPrefabManager()->GetPrefabEvents();
			events->SetCurrentlySettingPrefab(false);
		}
	}

	~CScopedPrefabEventsDelay() noexcept (false)
	{
		Resume();
	}

private:
	bool m_resumed;
};

class CUndoChangeGuid : public IUndoObject
{
public:
	CUndoChangeGuid(CBaseObject* pObject, const CryGUID& newGuid)
		: m_newGuid(newGuid)
	{
		m_oldGuid = pObject->GetId();
	}

protected:
	virtual const char* GetDescription() { return "Change GUIDs"; }

	virtual void        Undo(bool bUndo)
	{
		SetGuid(m_newGuid, m_oldGuid);
	}

	virtual void Redo()
	{
		SetGuid(m_oldGuid, m_newGuid);
	}

private:
	void SetGuid(const CryGUID& currentGuid, const CryGUID& newGuid)
	{
		auto pObject = GetIEditor()->GetObjectManager()->FindObject(currentGuid);
		if (pObject)
		{
			GetIEditor()->GetObjectManager()->ChangeObjectId(currentGuid, newGuid);
		}
	}

	CryGUID m_oldGuid;
	CryGUID m_newGuid;
};

// Helper function for removing any references to the 'same' object in different prefab instances
void RemoveDuplicateInstances(std::vector<CBaseObject*>& objects)
{
	if (objects.empty())
		return;

	CBaseObject* pLastSelectedPrefab = objects[objects.size() - 1]->GetPrefab();

	for (auto iteratorObjectA = objects.begin(); iteratorObjectA != objects.end();)
	{
		const CBaseObject* const pObjectA = (*iteratorObjectA);
		const CBaseObject* const pPrefabA = pObjectA->GetPrefab();
		bool increaseIteratorA = true;

		// If this object is not owned by a prefab, then continue searching
		if (!pPrefabA)
		{
			++iteratorObjectA;
			continue;
		}

		for (auto iteratorObjectB = iteratorObjectA + 1; iteratorObjectB != objects.end();)
		{
			const CBaseObject* const pObjectB = (*iteratorObjectB);
			const CBaseObject* const pPrefabB = pObjectB->GetPrefab();

			// Make sure we're not trying to add the same object from different instances of the same prefab type
			if (!pPrefabB || pObjectA->GetIdInPrefab() != pObjectB->GetIdInPrefab())
			{
				++iteratorObjectB;
				continue;
			}

			// If prefabA is the last selected prefab, remove the object found in prefabB from the list
			// this is done because the last selected object's prefab will be used as the parent for the prefab
			// we are about to create, and we want to prioritize objects in the last selected prefab since
			// we most likely would not change it's transform
			if (pPrefabA == pLastSelectedPrefab)
			{
				iteratorObjectB = objects.erase(iteratorObjectB);
			}
			else
			{
				iteratorObjectA = objects.erase(iteratorObjectA);
				// Don't increase iterator for object A since this iterator has already been updated
				increaseIteratorA = false;
				// Since at this point pObjectA and pPrefabA are 'invalid' in our search; break and continue search
				// using new iterators
				break;
			}
		}

		if (increaseIteratorA)
			++iteratorObjectA;
	}
}
}

bool CPrefabChildGuidProvider::IsValidChildGUid(const CryGUID& id, CPrefabObject* pPrefabObject)
{
	const auto& prefabGuid = pPrefabObject->GetId();
	return (prefabGuid.hipart ^ prefabGuid.lopart) == id.hipart;
}

CryGUID CPrefabChildGuidProvider::GetFrom(const CryGUID& loadedGuid) const
{
	const auto& prefabGuid = m_pPrefabObject->GetId();
	return CryGUID(prefabGuid.hipart ^ prefabGuid.lopart, loadedGuid.hipart);
}

CUndoChangePivot::CUndoChangePivot(CBaseObject* pObj, const char* undoDescription)
{
	// Stores the current state of this object.
	assert(pObj != nullptr);
	m_undoDescription = undoDescription;
	m_guid = pObj->GetId();

	m_undoPivotPos = pObj->GetWorldPos();
}

const char* CUndoChangePivot::GetObjectName()
{
	CBaseObject* object = GetIEditor()->GetObjectManager()->FindObject(m_guid);
	if (!object)
		return "";

	return object->GetName();
}

void CUndoChangePivot::Undo(bool bUndo)
{
	CBaseObject* object = GetIEditor()->GetObjectManager()->FindObject(m_guid);
	if (!object)
		return;

	if (bUndo)
	{
		m_redoPivotPos = object->GetWorldPos();
	}

	static_cast<CPrefabObject*>(object)->SetPivot(m_undoPivotPos);
}

void CUndoChangePivot::Redo()
{
	CBaseObject* object = GetIEditor()->GetObjectManager()->FindObject(m_guid);
	if (!object)
		return;

	static_cast<CPrefabObject*>(object)->SetPivot(m_redoPivotPos);
}

class PrefabLinkTool : public CPickObjectTool
{
	DECLARE_DYNCREATE(PrefabLinkTool)

	struct PrefabLinkPicker : IPickObjectCallback
	{
		CPrefabObject* m_prefab;

		PrefabLinkPicker()
			: m_prefab(nullptr)
		{
		}

		void OnPick(CBaseObject* pObj) override
		{
			if (m_prefab)
			{
				GetIEditor()->GetPrefabManager()->AttachObjectToPrefab(m_prefab, pObj);
			}
		}

		bool OnPickFilter(CBaseObject* pObj)
		{
			if (m_prefab)
			{
				if (pObj->CheckFlags(OBJFLAG_PREFAB) || pObj == m_prefab)
					return false;
			}

			return true;
		}

		void OnCancelPick() override
		{
		}
	};

public:
	PrefabLinkTool()
		: CPickObjectTool(&m_picker)
	{
	}

	~PrefabLinkTool()
	{
		m_picker.OnCancelPick();
	}

	virtual void SetUserData(const char* key, void* userData) override
	{
		m_picker.m_prefab = static_cast<CPrefabObject*>(userData);
	}

private:
	PrefabLinkPicker m_picker;
};

IMPLEMENT_DYNCREATE(PrefabLinkTool, CPickObjectTool)

CPrefabObject::CPrefabObject()
{
	SetColor(ColorB(255, 220, 0)); // Yellowish
	ZeroStruct(m_prefabGUID);

	m_bbox.min = m_bbox.max = Vec3(0, 0, 0);
	m_bBBoxValid = false;
	m_autoUpdatePrefabs = true;
	m_bModifyInProgress = false;
	m_bChangePivotPoint = false;
	m_bSettingPrefabObj = false;
	UseMaterialLayersMask(true);
}

void CPrefabObject::Done()
{
	CRY_PROFILE_FUNCTION_ARG(PROFILE_LOADING_ONLY, GetName().c_str());

	SetPrefab(nullptr);
	DeleteAllMembers();
	CBaseObject::Done();
}

bool CPrefabObject::CreateFrom(std::vector<CBaseObject*>& objects)
{
	// Clear selection
	GetIEditorImpl()->GetObjectManager()->ClearSelection();
	CBaseObject* pLastSelectedObject = nullptr;
	CBaseObject* pParent = nullptr;
	// Put the newly created group on the last selected object's layer
	if (objects.size())
	{
		pLastSelectedObject = objects[objects.size() - 1];
		GetIEditorImpl()->GetIUndoManager()->Suspend();
		SetLayer(pLastSelectedObject->GetLayer());
		GetIEditorImpl()->GetIUndoManager()->Resume();
		pParent = pLastSelectedObject->GetParent();
	}

	//Check if the children come from more than one prefab, as that's not allowed
	CPrefabObject* pPrefabToCompareAgainst = nullptr;
	CPrefabObject* pObjectPrefab = nullptr;

	Private_PrefabObject::RemoveDuplicateInstances(objects);

	for (auto pObject : objects)
	{
		pObjectPrefab = (CPrefabObject*)pObject->GetPrefab();

		// Sanity check if user is trying to group objects from different prefabs
		if (pPrefabToCompareAgainst && pObjectPrefab && pPrefabToCompareAgainst->GetPrefabGuid() != pObjectPrefab->GetPrefabGuid())
		{
			CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "Cannot Create a new prefab from these objects, they are already owned by different prefabs");
			return false;
		}

		if (!pPrefabToCompareAgainst)
			pPrefabToCompareAgainst = pObjectPrefab;
	}
	//If we are creating a prefab inside another prefab we first remove all the objects from the previous owner prefab and then we add them to the new one
	for (CBaseObject* pObject : objects)
	{
		if (pObject->IsPartOfPrefab())
		{
			pObject->GetPrefab()->RemoveMember(pObject, true, true);
		}
	}
	//Add them to the new one, serialize into the prefab item and update the library
	for (CBaseObject* pObject : objects)
	{
		AddMember(pObject);
	}

	//add the prefab itself to the last selected object parent
	if (pParent)
		pParent->AddMember(this);

	GetIEditorImpl()->GetObjectManager()->SelectObject(this);
	GetIEditorImpl()->SetModifiedFlag();

	CRY_ASSERT_MESSAGE(m_pPrefabItem, "Trying to create a prefab that has no Prefab Item");
	m_pPrefabItem->SetModified();

	return true;
}

CPrefabObject* CPrefabObject::CreateFrom(std::vector<CBaseObject*>& objects, Vec3 center, CPrefabItem* pItem)
{
	CUndo undo("Create Prefab");

	CPrefabObject* pPrefab = static_cast<CPrefabObject*>(GetIEditorImpl()->NewObject(PREFAB_OBJECT_CLASS_NAME, pItem->GetGUID().ToString().c_str()));
	pPrefab->SetPrefab(pItem, false);

	if (!pPrefab)
	{
		undo.Cancel();
		return nullptr;
	}

	// Snap center to grid.
	pPrefab->SetPos(gSnappingPreferences.Snap3D(center));

	if (!pPrefab->CreateFrom(objects))
	{
		undo.Cancel();
		return nullptr;
	}

	return pPrefab;
}

bool CPrefabObject::Init(CBaseObject* prev, const string& file)
{
	bool res = CBaseObject::Init(prev, file);
	if (!file.IsEmpty())
	{
		SetPrefabGuid(CryGUID::FromString(file));
	}
	return res;
}

void CPrefabObject::PostInit(const string& file)
{
	if (!file.IsEmpty())
	{
		SetPrefab(m_prefabGUID, true);
	}
}

void CPrefabObject::OnShowInFG()
{
	CWnd* pWnd = GetIEditor()->OpenView("Flow Graph");
	if (pWnd && pWnd->IsKindOf(RUNTIME_CLASS(CHyperGraphDialog)))
	{
		CHyperGraphDialog* pHGDlg = static_cast<CHyperGraphDialog*>(pWnd);
		CFlowGraphSearchCtrl* pSC = pHGDlg->GetSearchControl();
		if (pSC)
		{
			CFlowGraphSearchOptions* pOpts = CFlowGraphSearchOptions::GetSearchOptions();
			pOpts->m_bIncludeValues = true;
			pOpts->m_findSpecial = CFlowGraphSearchOptions::eFLS_None;
			pOpts->m_LookinIndex = CFlowGraphSearchOptions::eFL_All;
			pSC->Find(GetName().c_str(), false, true, true);
		}
	}
}

void CPrefabObject::OnContextMenu(CPopupMenuItem* menu)
{
	CGroup::OnContextMenu(menu);
	if (!menu->Empty())
	{
		menu->AddSeparator();
	}

	menu->Add("Find in FlowGraph", [=]() { OnShowInFG(); });
	menu->Add("Swap Prefab...", [this]()
	{
		CPrefabPicker picker;
		const CSelectionGroup* selection = GetIEditor()->GetObjectManager()->GetSelection();
		std::vector<CPrefabObject*> prefabsInSelection;
	
		for (int i = 0; i < selection->GetCount(); i++)
		{
			if (selection->GetObject(i)->IsKindOf(RUNTIME_CLASS(CPrefabObject)))
			{
				prefabsInSelection.push_back(static_cast<CPrefabObject*>(selection->GetObject(i)));
			}
		}

		picker.SwapPrefab(prefabsInSelection);
	});

	CBaseObjectsArray objects;
	GetIEditorImpl()->GetSelection()->GetObjects(objects);
	CPrefabManager* pPrefabManager = GetIEditor()->GetPrefabManager();
	std::vector<CPrefabItem*> items = pPrefabManager->GetAllPrefabItems(objects);
	//Show the menu only if we have one type of prefab in selection
	if (items.size() == 1)
	{
		CPrefabItem* pItem = items[0];
		menu->AddCommand("Select all Prefabs of Type \"" + pItem->GetName() + "\"", "prefab.select_all_instances_of_type");
	}
}

int CPrefabObject::MouseCreateCallback(IDisplayViewport* view, EMouseEvent event, CPoint& point, int flags)
{
	int creationState = CBaseObject::MouseCreateCallback(view, event, point, flags);

	if (creationState == MOUSECREATE_CONTINUE)
	{
		CSelectionGroup descendants;
		GetAllDescendants(descendants);
		for (int i = 0; i < descendants.GetCount(); i++)
		{
			if (descendants.GetObject(i)->GetCollisionEntity())
			{
				IPhysicalEntity* pCollisionEntity = descendants.GetObject(i)->GetCollisionEntity();
				pe_params_part collision;
				pCollisionEntity->GetParams(&collision);
				collision.flagsAND &= ~(geom_colltype_ray);
				pCollisionEntity->SetParams(&collision);
			}
		}
	}

	if (creationState == MOUSECREATE_OK)
	{
		CSelectionGroup descendants;
		GetAllDescendants(descendants);
		for (int i = 0; i < descendants.GetCount(); i++)
		{
			if (descendants.GetObject(i)->GetCollisionEntity())
			{
				IPhysicalEntity* collisionEntity = descendants.GetObject(i)->GetCollisionEntity();
				pe_params_part collision;
				collisionEntity->GetParams(&collision);
				collision.flagsOR |= (geom_colltype_ray);
				collisionEntity->SetParams(&collision);
			}
		}
	}

	return creationState;
}

void CPrefabObject::Display(CObjectRenderHelper& objRenderHelper)
{
	SDisplayContext& dc = objRenderHelper.GetDisplayContextRef();
	if (!dc.showPrefabHelper)
	{
		return;
	}

	DrawDefault(dc, CMFCUtils::ColorBToColorRef(GetColor()));

	dc.PushMatrix(GetWorldTM());

	bool bSelected = IsSelected();
	if (bSelected)
	{
		dc.SetSelectedColor();
		dc.DrawWireBox(m_bbox.min, m_bbox.max);

		dc.DepthWriteOff();
		dc.SetSelectedColor(0.2f);
		dc.DrawSolidBox(m_bbox.min, m_bbox.max);
		dc.DepthWriteOn();
	}
	else
	{
		if (dc.showPrefabBounds)
		{
			if (IsFrozen())
			{
				dc.SetFreezeColor();
			}
			else
			{
				ColorB color = GetColor();
				color.a = 51;
				dc.SetColor(color);
			}

			dc.DepthWriteOff();
			dc.DrawSolidBox(m_bbox.min, m_bbox.max);
			dc.DepthWriteOn();

			if (IsFrozen())
				dc.SetFreezeColor();
			else
				dc.SetColor(GetColor());
			dc.DrawWireBox(m_bbox.min, m_bbox.max);
		}
	}
	dc.PopMatrix();

	if (dc.showPrefabChildrenHelpers)
	{
		if (HasChildren())
		{
			int numObjects = GetChildCount();
			for (int i = 0; i < numObjects; i++)
			{
				RecursivelyDisplayObject(GetChild(i), objRenderHelper);
			}
		}
	}
}

void CPrefabObject::RecursivelyDisplayObject(CBaseObject* obj, CObjectRenderHelper& objRenderHelper)
{
	SDisplayContext& dc = objRenderHelper.GetDisplayContextRef();

	if (!obj->CheckFlags(OBJFLAG_PREFAB) || obj->IsHidden())
		return;

	AABB bbox;
	obj->GetBoundBox(bbox);
	if (dc.display2D)
	{
		if (dc.box.IsIntersectBox(bbox))
		{
			obj->Display(objRenderHelper);
		}
	}
	else
	{
		if (dc.camera && dc.camera->IsAABBVisible_F(AABB(bbox.min, bbox.max)))
		{
			obj->Display(objRenderHelper);
		}
	}

	int numObjects = obj->GetChildCount();
	for (int i = 0; i < numObjects; i++)
		RecursivelyDisplayObject(obj->GetChild(i), objRenderHelper);

	int numLinkedObjects = obj->GetLinkedObjectCount();
	for (int i = 0; i < numLinkedObjects; ++i)
		RecursivelyDisplayObject(obj->GetLinkedObject(i), objRenderHelper);
}

void CPrefabObject::Serialize(CObjectArchive& ar)
{
	bool bSuspended(SuspendUpdate(false));
	CBaseObject::Serialize(ar);
	if (bSuspended)
		ResumeUpdate();

	if (ar.bLoading)
	{
		ar.node->getAttr("PrefabGUID", m_prefabGUID);
	}
	else
	{
		ar.node->setAttr("PrefabGUID", m_prefabGUID);
		ar.node->setAttr("PrefabName", m_prefabName);
	}
}

void CPrefabObject::PostLoad(CObjectArchive& ar)
{
	__super::PostLoad(ar);

	SetPrefab(m_prefabGUID, true);
	uint32 nLayersMask = GetMaterialLayersMask();
	if (nLayersMask)
		SetMaterialLayersMask(nLayersMask);

	// If all children are Designer Objects, this prefab object should have an open status.
	int iChildCount(GetChildCount());

	if (iChildCount > 0)
	{
		bool bAllDesignerObject = true;

		for (int i = 0; i < iChildCount; ++i)
		{
			if (GetChild(i)->GetType() != OBJTYPE_SOLID)
			{
				bAllDesignerObject = false;
			}
		}

		if (bAllDesignerObject)
		{
			Open();
		}
	}
}

string CPrefabObject::GetAssetPath() const
{
	if (!m_pPrefabItem || !m_pPrefabItem->GetLibrary())
	{
		return "";
	}

	return m_pPrefabItem->GetLibrary()->GetFilename();
}

XmlNodeRef CPrefabObject::Export(const string& levelPath, XmlNodeRef& xmlNode)
{
	// Do not export.
	return nullptr;
}

inline void RecursivelySendEventToPrefabChilds(CBaseObject* obj, ObjectEvent event)
{
	for (int i = 0; i < obj->GetChildCount(); i++)
	{
		CBaseObject* c = obj->GetChild(i);
		if (c->CheckFlags(OBJFLAG_PREFAB))
		{
			c->OnEvent(event);
			if (c->GetChildCount() > 0)
			{
				RecursivelySendEventToPrefabChilds(c, event);
			}
		}
	}
}

void CPrefabObject::OnEvent(ObjectEvent event)
{
	switch (event)
	{
	case EVENT_PREFAB_REMAKE:
		{
			CPrefabManager::SkipPrefabUpdate skipUpdates;
			SetPrefab(GetPrefabItem(), true);
			break;
		}
	}
	// Send event to all prefab childs.
	if (event != EVENT_ALIGN_TOGRID)
	{
		RecursivelySendEventToPrefabChilds(this, event);
	}
	CBaseObject::OnEvent(event);
}

void CPrefabObject::DeleteAllPrefabObjects()
{
	CRY_PROFILE_FUNCTION(PROFILE_LOADING_ONLY)
	std::vector<CBaseObject*> descendants;
	GetAllPrefabFlagedDescendants(descendants);
	DetachAll(false, true);
	GetObjectManager()->DeleteObjects(descendants);
}

void CPrefabObject::SetPrefab(CryGUID guid, bool bForceReload)
{
	if (m_prefabGUID == guid && bForceReload == false)
		return;

	m_prefabGUID = guid;

	//m_fullPrototypeName = prototypeName;
	CPrefabManager* pManager = GetIEditor()->GetPrefabManager();
	CPrefabItem* pPrefab = static_cast<CPrefabItem*>(pManager->LoadItem(guid));

	if (pPrefab)
	{
		SetPrefab(pPrefab, bForceReload);
	}
	else
	{
		if (m_prefabName.IsEmpty())
			m_prefabName = "Unknown Prefab";

		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "Cannot find Prefab %s with GUID: %s for Object %s %s",
		           (const char*)m_prefabName, guid.ToString(), (const char*)GetName(),
		           CryLinkService::CCryLinkUriFactory::GetUriV("Editor", "selection.select_and_go_to %s", GetName()));

		SetMinSpec(GetMinSpec(), true);   // to make sure that all children get the right spec
	}
}

void CPrefabObject::SetPrefab(CPrefabItem* pPrefab, bool bForceReload)
{
	using namespace Private_PrefabObject;
	assert(pPrefab);

	if (pPrefab == nullptr || (pPrefab == m_pPrefabItem && !bForceReload))
		return;

	CPrefabManager* pPrefabManager = GetIEditor()->GetPrefabManager();
	CRY_ASSERT(pPrefabManager != nullptr);

	// Prefab events needs to be notified to delay determining event data till after prefab is set (Only then is name + instance name determined)
	CScopedPrefabEventsDelay eventsDelay;

	DeleteChildrenWithoutUpdating();

	SetPrefab(pPrefab);

	m_prefabGUID = pPrefab->GetGUID();
	m_prefabName = pPrefab->GetFullName();

	StoreUndo("Set Prefab");

	CScopedSuspendUndo suspendUndo;

	// Make objects from this prefab.
	XmlNodeRef objectsXml = pPrefab->GetObjectsNode();
	if (!objectsXml)
	{
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "Prefab %s does not contain objects %s", (const char*)m_prefabName,
		           CryLinkService::CCryLinkUriFactory::GetUriV("Editor", "selection.select_and_go_to %s", GetName()));
		return;
	}

	IObjectLayer* pThisLayer = GetLayer();

	// Spawn objects.

	pPrefabManager->SetSkipPrefabUpdate(true);

	CObjectArchive ar(GetObjectManager(), objectsXml, true);
	ar.EnableProgressBar(false); // No progress bar is shown when loading objects.
	/*
	   Here we set the guid provider to null so that it will not attempt to regenerate GUIDS in LoadObjects (specifically in the call to NewObject).
	   We need the serialized Id to stay the same as the prefab xml Id until AddMember (called in AttachLoadedChildrenToPrefab) is called
	   AddMember will "slide" the current Id to IdInPrefab (which we need for prefab instance and library update) and generate a new unique Id for the prefab instance
	 */
	ar.SetGuidProvider(nullptr);
	ar.EnableReconstructPrefabObject(true);
	// new prefabs are instantiated in current layer to avoid mishaps with missing layers. Then, we just set their layer to our own below
	ar.LoadInCurrentLayer(true);
	ar.LoadObjects(objectsXml);
	//force using this ID, incremental. Keep high part of CryGUID (stays compatible with old GUID.Data1)
	GetObjectManager()->ForceID(GetId().hipart >> 32);
	ar.ResolveObjects();

	//Now actually set a GUID provider
	CPrefabChildGuidProvider guidProvider = { this };
	ar.SetGuidProvider(&guidProvider);
	AttachLoadedChildrenToPrefab(ar, pThisLayer);

	// Forcefully validate TM and then trigger InvalidateTM() on prefab (and all its children).
	GetWorldTM();
	InvalidateTM(0);

	GetObjectManager()->ForceID(0);//disable
	InvalidateBBox();

	SyncParentObject();

	eventsDelay.Resume();

	pPrefabManager->SetSkipPrefabUpdate(false);
}

void CPrefabObject::SetPrefab(CPrefabItem* pPrefab)
{
	if (m_pPrefabItem)
	{
		m_pPrefabItem->signalNameChanged.DisconnectObject(this);
	}

	if (pPrefab)
	{
		pPrefab->signalNameChanged.Connect(this, &CBaseObject::UpdateUIVars);
	}

	m_pPrefabItem = pPrefab;
}

void CPrefabObject::AttachLoadedChildrenToPrefab(CObjectArchive& ar, IObjectLayer* pLayer)
{
	int numObjects = ar.GetLoadedObjectsCount();
	std::vector<CBaseObject*> objects;
	objects.reserve(numObjects);
	for (int i = 0; i < numObjects; i++)
	{
		CBaseObject* obj = ar.GetLoadedObject(i);

		obj->SetLayer(pLayer);

		// Only attach objects without a parent object to this prefab.
		if (!obj->GetParent() && !obj->GetLinkedTo())
		{
			objects.push_back(obj);
		}
		SetObjectPrefabFlagAndLayer(obj);
	}
	//This is necessary to avoid prefab item modify and add to be called when loading nested prefab (SetSkipPrefabUpdate will be set to false when exiting from nested SetPrefab)
	//As we are loading from an archive we don't need to do any kind of update operation
	SetModifyInProgress(true);
	AddMembers(objects, false);
	SetModifyInProgress(false);
}

void CPrefabObject::DeleteChildrenWithoutUpdating()
{
	CScopedSuspendUndo suspendUndo;

	bool bSuspended(SuspendUpdate(false));
	DeleteAllPrefabObjects();
	if (bSuspended)
	{
		ResumeUpdate();
	}
}

void CPrefabObject::SetPrefabFlagForLinkedObjects(CBaseObject* pObject)
{
	for (auto i = 0; i < pObject->GetLinkedObjectCount(); ++i)
	{
		CBaseObject* pLinkedObject = pObject->GetLinkedObject(i);
		pLinkedObject->SetFlags(OBJFLAG_PREFAB);
		SetPrefabFlagForLinkedObjects(pLinkedObject);
	}
}

void CPrefabObject::SetObjectPrefabFlagAndLayer(CBaseObject* object)
{
	object->SetFlags(OBJFLAG_PREFAB);
	object->SetLayer(GetLayer());
}

void CPrefabObject::InitObjectPrefabId(CBaseObject* object)
{
	if (object->GetIdInPrefab() == CryGUID::Null())
		object->SetIdInPrefab(object->GetId());
}

void CPrefabObject::PostClone(CBaseObject* pFromObject, CObjectCloneContext& ctx)
{
	// We must do SetPrefab here so newly cloned children get cloned after prefab has been added to the scene properly,
	// else object browser will crash because we are trying to parent to a missing object.
	// Moving children init to PostClone was copied by Group Objects which do something similar
	if (pFromObject)
	{
		// Cloning.
		CPrefabObject* prevObj = (CPrefabObject*)pFromObject;

		SetPrefab(prevObj->m_pPrefabItem, false);

		if (prevObj->IsOpen())
		{
			Open();
		}
	}

	CBaseObject* pFromParent = pFromObject->GetParent();
	if (pFromParent)
	{
		CBaseObject* pChildParent = ctx.FindClone(pFromParent);
		if (pChildParent)
			pChildParent->AddMember(this, false);
		else
			pFromParent->AddMember(this, false);
	}
}

const ColorB& CPrefabObject::GetSelectionPreviewHighlightColor()
{
	return gViewportSelectionPreferences.colorPrefabBBox;
}

void CPrefabObject::SerializeMembers(Serialization::IArchive& ar)
{
	if (ar.isEdit())
	{
		if (ar.openBlock("prefabtools", "Prefab Tools"))
		{
			Serialization::SEditToolButton pickButton("");
			pickButton.SetToolClass(RUNTIME_CLASS(PrefabLinkTool), nullptr, this);

			ar(pickButton, "picker", "^Pick");
			ar(Serialization::ActionButton([=]
			{
				CUndo undo("Clear targets");

				bool hasDeleted = false;
				while (GetChildCount() > 0)
				{
				  hasDeleted = true;
				  GetIEditor()->GetObjectManager()->DeleteObject(GetChild(0));
				}

				if (hasDeleted)
				{
				  GetIEditor()->GetObjectManager()->InvalidateVisibleList();
				}
			}), "picker", "^Clear");

			ar.closeBlock();
		}
	}

	std::vector<Serialization::PrefabLink> links;

	for (int i = 0; i < GetChildCount(); i++)
	{
		CBaseObject* obj = GetChild(i);
		links.emplace_back(obj->GetId(), (LPCTSTR)obj->GetName(), GetId());
	}

	ar(links, "prefab_obj", "!Prefab Objects");

	// The hard part. If this is an input, we need to determine which objects have been added or removed and deal with it.
	if (ar.isInput())
	{
		// iterate quickly on both input and existing arrays and check if our objects have changed
		bool changed = false;
		if (links.size() == GetChildCount())
		{
			for (size_t i = 0; i < links.size(); ++i)
			{
				CBaseObject* pObj = GetChild(i);

				if (pObj->GetId() != links[i].guid)
				{
					changed = true;
					break;
				}
			}
		}
		else
		{
			changed = true;
		}

		if (changed)
		{

			auto childCount = GetChildCount();
			std::unordered_set<CryGUID> childGuids;
			childGuids.reserve(childCount);
			for (auto i = 0; i < childCount; ++i)
			{
				CBaseObject* pChild = GetChild(i);
				if (!pChild)
					continue;
				childGuids.insert(pChild->GetId());
			}

			CUndo undo("Modify Prefab");
			for (Serialization::PrefabLink& link : links)
			{
				// If the guid is not in the prefab's list of children, then we must attach the object to the prefab
				if (childGuids.find(link.guid) == childGuids.end())
				{
					CBaseObject* pObject = GetIEditor()->GetObjectManager()->FindObject(link.guid);
					if (pObject->GetParent() != this)
					{
						GetIEditor()->GetPrefabManager()->AttachObjectToPrefab(this, pObject);
					}
				}
				else // if the guid is already there, then remove it from the list (because remaining guids will be removed from the prefab)
					childGuids.erase(link.guid);
			}

			// Any remaining guids will be removed from the prefab
			for (auto& idToBeRemoved : childGuids)
			{
				CBaseObject* pObject = GetIEditor()->GetObjectManager()->FindObject(idToBeRemoved);
				GetIEditor()->GetObjectManager()->DeleteObject(pObject);
			}
		}
	}
}

void CPrefabObject::CreateInspectorWidgets(CInspectorWidgetCreator& creator)
{
	CGroup::CreateInspectorWidgets(creator);

	creator.AddPropertyTree<CPrefabObject>("Prefab", [](CPrefabObject* pObject, Serialization::IArchive& ar, bool bMultiEdit)
	{
		bool bAutoUpdate = pObject->GetAutoUpdatePrefab();
		bool bOldAutoupdate = bAutoUpdate;

		ar(bAutoUpdate, "autoupdate", "Auto Update All Instances");

		if (bAutoUpdate != bOldAutoupdate)
		{
		  pObject->SetAutoUpdatePrefab(bAutoUpdate);
		}

		ar(pObject->m_bChangePivotPoint, "pivotmode", "Transform Pivot Mode");

		if (ar.openBlock("operators", "Operators"))
		{
		  CPrefabManager* pPrefabManager = GetIEditor()->GetPrefabManager();
		  if (ar.openBlock("objects", "Objects"))
		  {
		    ar(Serialization::ActionButton(std::bind(&CPrefabManager::ExtractAllFromSelection, pPrefabManager)), "extract_all", "^Extract All");
		    ar(Serialization::ActionButton(std::bind(&CPrefabManager::CloneAllFromSelection, pPrefabManager)), "clone_all", "^Clone All");
		    ar.closeBlock();
			}

		  if (ar.openBlock("edit", "Edit"))
		  {
		    if (bMultiEdit)
		    {
		      ar(Serialization::ActionButton(std::bind(&CPrefabManager::CloseSelected, pPrefabManager)), "close", "^Close");
		      ar(Serialization::ActionButton(std::bind(&CPrefabManager::OpenSelected, pPrefabManager)), "open", "^Open");
				}
		    else
		    {
		      if (pObject->m_opened)
		      {
		        ar(Serialization::ActionButton(std::bind(&CPrefabManager::CloseSelected, pPrefabManager)), "close", "^Close");
					}
		      else
		      {
		        ar(Serialization::ActionButton(std::bind(&CPrefabManager::OpenSelected, pPrefabManager)), "open", "^Open");
					}
				}
		    ar.closeBlock();
			}
		  ar.closeBlock();
		}

		if (!bMultiEdit)
		{
		  pObject->SerializeMembers(ar);
		}
	});
}

void CPrefabObject::CloneAll(std::vector<CBaseObject*>& extractedObjects)
{
	if (!m_pPrefabItem || !m_pPrefabItem->GetObjectsNode())
		return;

	// Take the prefab lib representation and clone it
	XmlNodeRef objectsNode = m_pPrefabItem->GetObjectsNode();

	const Matrix34 prefabPivotTM = GetWorldTM();

	CObjectArchive clonedObjectArchive(GetObjectManager(), objectsNode, true);
	clonedObjectArchive.EnableProgressBar(false); // No progress bar is shown when loading objects.
	CPrefabChildGuidProvider guidProvider = { this };
	clonedObjectArchive.SetGuidProvider(&guidProvider);
	clonedObjectArchive.LoadInCurrentLayer(true);
	clonedObjectArchive.EnableReconstructPrefabObject(true);
	clonedObjectArchive.LoadObjects(objectsNode);
	clonedObjectArchive.ResolveObjects();

	extractedObjects.reserve(extractedObjects.size() + clonedObjectArchive.GetLoadedObjectsCount());
	IObjectLayer* pThisLayer = GetLayer();

	CScopedSuspendUndo suspendUndo;
	for (int i = 0, numObjects = clonedObjectArchive.GetLoadedObjectsCount(); i < numObjects; ++i)
	{
		CBaseObject* pClonedObject = clonedObjectArchive.GetLoadedObject(i);

		// Add to selection
		pClonedObject->SetIdInPrefab(CryGUID::Null());
		// If we don't have a parent transform with the world matrix
		if (!pClonedObject->GetParent())
			pClonedObject->SetWorldTM(prefabPivotTM * pClonedObject->GetWorldTM());

		pClonedObject->SetLayer(pThisLayer);
		extractedObjects.push_back(pClonedObject);
	}
}

void CPrefabObject::CloneSelected(CSelectionGroup* pSelectedGroup, std::vector<CBaseObject*>& clonedObjects)
{
	if (pSelectedGroup == NULL || !pSelectedGroup->GetCount())
		return;

	XmlNodeRef objectsNode = XmlHelpers::CreateXmlNode("Objects");
	std::map<CryGUID, XmlNodeRef> objects;
	for (int i = 0, count = pSelectedGroup->GetCount(); i < count; ++i)
	{
		CBaseObject* pSelectedObj = pSelectedGroup->GetObject(i);
		XmlNodeRef serializedObject = m_pPrefabItem->FindObjectByGuid(pSelectedObj->GetIdInPrefab(), true);
		if (!serializedObject)
			return;

		XmlNodeRef cloneObject = serializedObject->clone();

		CryGUID cloneObjectID = CryGUID::Null();
		if (cloneObject->getAttr("Id", cloneObjectID))
			objects[cloneObjectID] = cloneObject;

		objectsNode->addChild(cloneObject);
	}

	std::vector<Matrix34> clonedObjectsPivotLocalTM;

	const Matrix34 prefabPivotTM = GetWorldTM();
	const Matrix34 prefabPivotInvTM = prefabPivotTM.GetInverted();

	// Delete outside referenced objects which were not part of the selected Group
	for (int i = 0, count = objectsNode->getChildCount(); i < count; ++i)
	{
		XmlNodeRef object = objectsNode->getChild(i);
		CryGUID objectID = CryGUID::Null();
		object->getAttr("Id", objectID);
		// If parent is not part of the selection remove it
		if (object->getAttr("Parent", objectID) && objects.find(objectID) == objects.end())
			object->delAttr("Parent");

		const CBaseObject* pChild = pSelectedGroup->GetObjectByGuidInPrefab(objectID);
		const Matrix34 childTM = pChild->GetWorldTM();
		const Matrix34 childRelativeToPivotTM = prefabPivotInvTM * childTM;

		clonedObjectsPivotLocalTM.push_back(childRelativeToPivotTM);
	}

	CObjectArchive clonedObjectArchive(GetObjectManager(), objectsNode, true);
	clonedObjectArchive.EnableProgressBar(false); // No progress bar is shown when loading objects.
	CPrefabChildGuidProvider guidProvider = { this };
	clonedObjectArchive.SetGuidProvider(&guidProvider);
	clonedObjectArchive.EnableReconstructPrefabObject(true);
	clonedObjectArchive.LoadObjects(objectsNode);
	clonedObjectArchive.ResolveObjects();

	CScopedSuspendUndo suspendUndo;
	clonedObjects.reserve(clonedObjects.size() + clonedObjectArchive.GetLoadedObjectsCount());
	for (int i = 0, numObjects = clonedObjectArchive.GetLoadedObjectsCount(); i < numObjects; ++i)
	{
		CBaseObject* pClonedObject = clonedObjectArchive.GetLoadedObject(i);

		// Add to selection
		pClonedObject->SetIdInPrefab(CryGUID::Null());
		// If we don't have a parent transform with the world matrix
		if (!pClonedObject->GetParent())
			pClonedObject->SetWorldTM(prefabPivotTM * clonedObjectsPivotLocalTM[i]);

		clonedObjects.push_back(pClonedObject);
	}
}

void CPrefabObject::AddMember(CBaseObject* pObj, bool bKeepPos /*=true */)
{
	std::vector<CBaseObject*> objects = { pObj };
	AddMembers(objects, bKeepPos);
}

void CPrefabObject::AddMembers(std::vector<CBaseObject*>& objects, bool shouldKeepPos /* = true*/)
{
	using namespace Private_PrefabObject;
	if (!m_pPrefabItem)
	{
		SetPrefab(m_prefabGUID, true);
		if (!m_pPrefabItem)
			return;
	}

	AttachChildren(objects, shouldKeepPos);

	//As we are moving things in the prefab new guids need to be generated for every object we are adding
	//The guids generated here are serialized in IdInPrefab, also the prefab flag and the correct layer is set
	for (CBaseObject* pObject : objects)
	{
		GenerateGUIDsForObjectAndChildren(pObject);

		//Add the top level object to the prefab so that it can be serialized and serialize all the children
		SObjectChangedContext context;
		context.m_operation = eOCOT_Add;
		context.m_modifiedObjectGlobalId = pObject->GetId();
		context.m_modifiedObjectGuidInPrefab = pObject->GetIdInPrefab();

		//Call a sync with eOCOT_Modify
		SyncPrefab(context);

		//In the case that we have moved something inside the prefab from the same layer (e.g group from layer to Prefab), the layer needs to be marked as modified.
		pObject->GetLayer()->SetModified(true);
	}

	IObjectManager* pObjectManager = GetIEditor()->GetObjectManager();
	pObjectManager->NotifyPrefabObjectChanged(this);

	// if the currently modified prefab is selected make sure to refresh the inspector
	if (GetPrefab() && GetIEditor()->GetObjectManager()->GetSelection()->IsContainObject(this) || pObjectManager->GetSelection()->IsContainObject(GetPrefab()))
		pObjectManager->EmitPopulateInspectorEvent();
}

void CPrefabObject::RemoveMembers(std::vector<CBaseObject*>& members, bool keepPos /*= true*/, bool placeOnRoot /*= false*/)
{
	CRY_PROFILE_FUNCTION(PROFILE_LOADING_ONLY);
	if (!m_pPrefabItem)
	{
		SetPrefab(m_prefabGUID, true);
		if (!m_pPrefabItem)
			return;
	}

	for (auto pObject : members)
	{
		SObjectChangedContext context;
		context.m_operation = eOCOT_Delete;
		context.m_modifiedObjectGuidInPrefab = pObject->GetIdInPrefab();
		context.m_modifiedObjectGlobalId = pObject->GetId();

		SyncPrefab(context);

		pObject->ClearFlags(OBJFLAG_PREFAB);

		//In the case that we have moved something outside the prefab from the same layer (e.g group from Prefab to Layer), the layer needs to be marked as modified.
		pObject->GetLayer()->SetModified(true);
	}

	CGroup::ForEachParentOf(members, [placeOnRoot, this](CGroup* pParent, std::vector<CBaseObject*>& children)
	{
		if (pParent == this)
		{
		  pParent->DetachChildren(children, true, placeOnRoot);
		}
	});

	IObjectManager* pObjectManager = GetIEditor()->GetObjectManager();
	pObjectManager->NotifyPrefabObjectChanged(this);

	// if the currently modified prefab is selected make sure to refresh the inspector
	if (pObjectManager->GetSelection()->IsContainObject(this))
	{
		pObjectManager->EmitPopulateInspectorEvent();
	}
}

void CPrefabObject::DeleteAllMembers()
{
	GetIEditor()->GetIUndoManager()->Suspend();
	std::vector<CBaseObject*> children;
	children.reserve(GetChildCount());
	for (int i = 0; i < GetChildCount(); ++i)
	{
		children.push_back(GetChild(i));
	}
	DetachAll(false, true);
	GetObjectManager()->DeleteObjects(children);
	GetIEditor()->GetIUndoManager()->Resume();
}

void CPrefabObject::SyncPrefab(const SObjectChangedContext& context)
{
	CRY_PROFILE_FUNCTION(PROFILE_LOADING_ONLY);
	if (!m_autoUpdatePrefabs)
	{
		for (SObjectChangedContext& change : m_pendingChanges)
		{
			if (change.m_modifiedObjectGlobalId == context.m_modifiedObjectGlobalId && change.m_operation == context.m_operation)
			{
				change = context;
				return;
			}
		}

		m_pendingChanges.push_back(context);
		return;
	}

	if (m_pPrefabItem)
	{
		m_pPrefabItem->UpdateFromPrefabObject(this, context);
	}

	InvalidateBBox();
}

void CPrefabObject::SyncParentObject()
{
	if (GetParent() && GetParent()->GetType() == OBJTYPE_GROUP)
	{
		static_cast<CGroup*>(GetParent())->InvalidateBBox();
	}
}

static void Prefab_RecursivelyGetBoundBox(CBaseObject* object, AABB& box, const Matrix34& parentTM)
{
	if (!object->CheckFlags(OBJFLAG_PREFAB))
		return;

	Matrix34 worldTM = parentTM * object->GetLocalTM();
	AABB b;
	object->GetLocalBounds(b);
	b.SetTransformedAABB(worldTM, b);
	box.Add(b.min);
	box.Add(b.max);

	int numChilds = object->GetChildCount();
	for (int i = 0; i < numChilds; i++)
		Prefab_RecursivelyGetBoundBox(object->GetChild(i), box, worldTM);

	int numLinkedObjects = object->GetLinkedObjectCount();
	for (int i = 0; i < numLinkedObjects; ++i)
		Prefab_RecursivelyGetBoundBox(object->GetLinkedObject(i), box, worldTM);
}

void CPrefabObject::CalcBoundBox()
{
	Matrix34 identityTM;
	identityTM.SetIdentity();

	// Calc local bounds box..
	AABB box;
	box.Reset();

	int numChilds = GetChildCount();
	for (int i = 0; i < numChilds; i++)
	{
		if (GetChild(i)->CheckFlags(OBJFLAG_PREFAB))
		{
			Prefab_RecursivelyGetBoundBox(GetChild(i), box, identityTM);
		}
	}

	if (numChilds == 0)
	{
		box.min = Vec3(-1, -1, -1);
		box.max = Vec3(1, 1, 1);
	}

	m_bbox = box;
	m_bBBoxValid = true;
}

void CPrefabObject::RemoveChild(CBaseObject* child)
{
	CGroup::RemoveChild(child);
}

void CPrefabObject::GenerateGUIDsForObjectAndChildren(CBaseObject* pObject)
{
	using namespace Private_PrefabObject;

	TBaseObjects objectsToAssign;

	objectsToAssign.push_back(pObject);

	bool isObjectPrefab = pObject->IsKindOf(RUNTIME_CLASS(CPrefabObject));

	if (isObjectPrefab)
	{
		CRY_ASSERT_MESSAGE(static_cast<CPrefabObject*>(pObject)->m_pPrefabItem != m_pPrefabItem, "Object has the same prefab item");
	}

	SetObjectPrefabFlagAndLayer(pObject);
	//This is serialized in the IdInPrefab field and also assigned as the new prefab GUID
	InitObjectPrefabId(pObject);
	//We need this for search, serialization and other things
	SetPrefabFlagForLinkedObjects(pObject);

	CryGUID newGuid = CPrefabChildGuidProvider(this).GetFor(pObject);

	if (CUndo::IsRecording())
	{
		CUndo::Record(new CUndoChangeGuid(pObject, newGuid));
	}

	//Assign the new GUID
	GetObjectManager()->ChangeObjectId(pObject->GetId(), newGuid);

	if (isObjectPrefab)
	{
		CPrefabObject* pPrefabObject = static_cast<CPrefabObject*>(pObject);
		for (size_t i = 0, childCount = pObject->GetChildCount(); i < childCount; ++i)
		{
			pPrefabObject->GenerateGUIDsForObjectAndChildren(pPrefabObject->GetChild(i));
		}
	}
	else
	{
		for (size_t i = 0, childCount = pObject->GetChildCount(); i < childCount; ++i)
		{
			GenerateGUIDsForObjectAndChildren(pObject->GetChild(i));
		}
	}
}

void CPrefabObject::SetMaterial(IEditorMaterial* pMaterial)
{
	if (pMaterial)
	{
		for (int i = 0; i < GetChildCount(); i++)
			GetChild(i)->SetMaterial(pMaterial);
	}
	CBaseObject::SetMaterial(pMaterial);
}

void CPrefabObject::SetWorldTM(const Matrix34& tm, int flags /* = 0 */)
{
	if (m_bChangePivotPoint)
		SetPivot(tm.GetTranslation());
	else
		CBaseObject::SetWorldTM(tm, flags);
}

void CPrefabObject::SetWorldPos(const Vec3& pos, int flags /* = 0 */)
{
	if (m_bChangePivotPoint)
		SetPivot(pos);
	else
		CBaseObject::SetWorldPos(pos, flags);
}

void CPrefabObject::SetMaterialLayersMask(uint32 nLayersMask)
{
	for (int i = 0; i < GetChildCount(); i++)
	{
		if (GetChild(i)->CheckFlags(OBJFLAG_PREFAB))
			GetChild(i)->SetMaterialLayersMask(nLayersMask);
	}

	CBaseObject::SetMaterialLayersMask(nLayersMask);
}

void CPrefabObject::SetName(const string& name)
{
	const string oldName = GetName();

	CBaseObject::SetName(name);

	// Prefab events are linked to prefab + instance name, need to notify events
	if (oldName != name)
	{
		CPrefabManager* pPrefabManager = GetIEditor()->GetPrefabManager();
		CRY_ASSERT(pPrefabManager != NULL);
		CPrefabEvents* pPrefabEvents = pPrefabManager->GetPrefabEvents();
		CRY_ASSERT(pPrefabEvents != NULL);

		pPrefabEvents->OnPrefabObjectNameChange(this, oldName, name);
	}
}

bool CPrefabObject::CanAddMembers(std::vector<CBaseObject*>& objects)
{
	if (!CGroup::CanAddMembers(objects))
	{
		return false;
	}

	/*
	   We need to gather all prefab objects from the hierarchy of this prefab and the hierarchies of each object we want to add.
	   Then we compare them, if they have the same prefab items, but from different objects, we cannot add the member because it means that we'll have recursive references (aka prefab in prefab).
	   If the items are from the same objects it's ok as it means we are in the same prefab instance
	 */
	for (CBaseObject* pToAdd : objects)
	{

		//Go to the top of pObject hierarchy
		CBaseObject* pToAddRoot = pToAdd;
		while (pToAddRoot->GetParent())
		{
			pToAddRoot = pToAddRoot->GetParent();
		}

		//Get all the prefab objects
		std::vector<CPrefabObject*> toAddPrefabDescendants;
		CPrefabPicker::GetAllPrefabObjectDescendants(pToAddRoot, toAddPrefabDescendants);
		//we also need to check against pToAddTop as it could be a prefab
		if (pToAddRoot->IsKindOf(RUNTIME_CLASS(CPrefabObject)))
		{
			toAddPrefabDescendants.push_back(static_cast<CPrefabObject*>(pToAddRoot));
		}

		//Go trough all the prefabs and find if some have the same items
		for (auto pToAddPrefabDescendant : toAddPrefabDescendants)
		{
			//If we are on the same instance (pCurrentPrefabDescendant == pToAddPrefabDescendant) it's fine, objects can be moved
			if (this != pToAddPrefabDescendant && this->GetPrefabItem() == pToAddPrefabDescendant->GetPrefabItem()) // same item but another hierarchy
			{
				//If we are on a different instance then we need to check if we are on the same hierarchy (i.e same nested prefabs, we cannot add) and stop it if necessary
				if (this != this)
				{
					//Same prefab parents means same hierarchy
					CBaseObject* pCurrentPrefabDescendantParent = this->GetPrefab();
					CBaseObject* pToAddPrefabDescendantParent = pToAddPrefabDescendant->GetPrefab();
					//no matching parents, definitely different hierarchy
					if (!pCurrentPrefabDescendantParent || !pToAddPrefabDescendantParent)
					{
						continue;
					} //matching parents, but different item, not the same hierarchy
					else if (pCurrentPrefabDescendantParent && pToAddPrefabDescendantParent && (static_cast<CPrefabObject*>(pCurrentPrefabDescendantParent))->GetPrefabItem() != (static_cast<CPrefabObject*>(pToAddPrefabDescendantParent))->GetPrefabItem())
					{
						continue;
					}
				}

				//same parents and same item, we definitely cannot add
				return false;
			}
		}
	}

	return true;
}

bool CPrefabObject::SuspendUpdate(bool bForceSuspend)
{
	if (m_bSettingPrefabObj)
		return false;

	if (!m_pPrefabItem)
	{
		if (!bForceSuspend)
			return false;
		if (m_prefabGUID == CryGUID::Null())
			return false;
		m_bSettingPrefabObj = true;
		SetPrefab(m_prefabGUID, true);
		m_bSettingPrefabObj = false;
		if (!m_pPrefabItem)
			return false;
	}

	return true;
}

void CPrefabObject::ResumeUpdate()
{
	if (!m_pPrefabItem || m_bSettingPrefabObj)
		return;
}

void CPrefabObject::UpdatePivot(const Vec3& newWorldPivotPos)
{
	// Update this prefab pivot
	SetModifyInProgress(true);
	const Matrix34 worldTM = GetWorldTM();
	const Matrix34 invWorldTM = worldTM.GetInverted();
	const Vec3 prefabPivotLocalSpace = invWorldTM.TransformPoint(newWorldPivotPos);

	CGroup::UpdatePivot(newWorldPivotPos);
	SetModifyInProgress(false);

	TBaseObjects childs;
	childs.reserve(GetChildCount());
	// Cache childs ptr because in the update prefab we are modifying the m_childs array since we are attaching/detaching before we save in the prefab lib xml
	for (int i = 0, iChildCount = GetChildCount(); i < iChildCount; ++i)
	{
		childs.push_back(GetChild(i));
	}

	// Update the prefab lib and reposition all prefab childs according to the new pivot
	for (int i = 0, iChildCount = childs.size(); i < iChildCount; ++i)
	{
		childs[i]->UpdatePrefab(eOCOT_ModifyTransformInLibOnly);
	}

	// Update all the rest prefab instance of the same type
	CBaseObjectsArray objects;
	GetObjectManager()->FindObjectsOfType(RUNTIME_CLASS(CPrefabObject), objects);

	for (int i = 0, iCount(objects.size()); i < iCount; ++i)
	{
		CPrefabObject* const pPrefabInstanceObj = static_cast<CPrefabObject*>(objects[i]);
		if (pPrefabInstanceObj->GetPrefabGuid() != GetPrefabGuid() || pPrefabInstanceObj == this)
			continue;

		pPrefabInstanceObj->SetModifyInProgress(true);
		const Matrix34 prefabInstanceWorldTM = pPrefabInstanceObj->GetWorldTM();
		const Vec3 prefabInstancePivotPoint = prefabInstanceWorldTM.TransformPoint(prefabPivotLocalSpace);
		pPrefabInstanceObj->CGroup::UpdatePivot(prefabInstancePivotPoint);
		pPrefabInstanceObj->SetModifyInProgress(false);
	}
}

void CPrefabObject::SetPivot(const Vec3& newWorldPivotPos)
{
	if (CUndo::IsRecording())
		CUndo::Record(new CUndoChangePivot(this, "Change pivot of Prefab"));
	UpdatePivot(newWorldPivotPos);
}

void CPrefabObject::SetAutoUpdatePrefab(bool autoUpdate)
{
	m_autoUpdatePrefabs = autoUpdate;
	if (m_autoUpdatePrefabs)
	{
		for (const SObjectChangedContext& change : m_pendingChanges)
		{
			SyncPrefab(change);
		}
		m_pendingChanges.clear();
	}
}

const char* CPrefabObjectClassDesc::GenerateObjectName(const char* szCreationParams)
{
	//szCreationParams is the GUID of the prefab item.
	//This item might not have been loaded yet, so we need to make sure it is
	CPrefabItem* item = static_cast<CPrefabItem*>(GetIEditor()->GetPrefabManager()->LoadItem(CryGUID::FromString(szCreationParams)));

	if (item)
	{
		return item->GetName();
	}

	return ClassName();
}

void CPrefabObjectClassDesc::EnumerateObjects(IObjectEnumerator* pEnumerator)
{
	GetIEditor()->GetPrefabManager()->EnumerateObjects(pEnumerator);
}

bool CPrefabObjectClassDesc::IsCreatable() const
{
	// Prefabs can only be placed from Asset Browser
	return false;
}

