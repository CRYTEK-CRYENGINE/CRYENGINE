// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "PrefabObject.h"

#include "HyperGraph/FlowGraphManager.h"
#include "HyperGraph/Controls/HyperGraphEditorWnd.h"
#include "HyperGraph/Controls/FlowGraphSearchCtrl.h"

#include "Prefabs/PrefabManager.h"
#include "Prefabs/PrefabDialog.h"
#include "Prefabs/PrefabEvents.h"
#include "Prefabs/PrefabLibrary.h"
#include "Prefabs/PrefabItem.h"
#include "BaseLibraryManager.h"

#include "Controls/DynamicPopupMenu.h"

#include "Grid.h"
#include "Viewport.h"
#include <Preferences/ViewportPreferences.h>

#include <CryCore/ToolsHelpers/GuidUtil.h>
#include "Util/BoostPythonHelpers.h"
#include "Objects/EntityObject.h"
#include "Objects/ObjectLoader.h"
#include "Objects/InspectorWidgetCreator.h"
#include "IUndoManager.h"

#include "Serialization/Decorators/EntityLink.h"
#include "Serialization/Decorators/EditorActionButton.h"
#include "Serialization/Decorators/EditToolButton.h"

#include "IDataBaseManager.h"
#include "PickObjectTool.h"
#include "CryEditDoc.h"

#include <CrySystem/ICryLink.h>
#include <CryGame/IGameFramework.h>

REGISTER_CLASS_DESC(CPrefabObjectClassDesc);

//////////////////////////////////////////////////////////////////////////
// CPrefabObject implementation.
//////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNCREATE(CPrefabObject, CGroup)

namespace Private_PrefabObject
{

class CScopedPrefabEventsDelay
{
public:
	CScopedPrefabEventsDelay()
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

	~CScopedPrefabEventsDelay() noexcept(false)
	{
		Resume();
	}

private:
	bool m_resumed{ false };
};

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

//////////////////////////////////////////////////////////////////////////
CUndoChangePivot::CUndoChangePivot(CBaseObject* pObj, const char* undoDescription)
{
	// Stores the current state of this object.
	assert(pObj != nullptr);
	m_undoDescription = undoDescription;
	m_guid = pObj->GetId();

	m_undoPivotPos = pObj->GetWorldPos();
}

//////////////////////////////////////////////////////////////////////////
const char* CUndoChangePivot::GetObjectName()
{
	CBaseObject* object = GetIEditor()->GetObjectManager()->FindObject(m_guid);
	if (!object)
		return "";

	return object->GetName();
}

//////////////////////////////////////////////////////////////////////////
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

//////////////////////////////////////////////////////////////////////////
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

	virtual void SetUserData(const char* key, void* userData) override
	{
		m_picker.m_prefab = static_cast<CPrefabObject*>(userData);
	}

private:
	PrefabLinkPicker m_picker;
};

IMPLEMENT_DYNCREATE(PrefabLinkTool, CPickObjectTool)

//////////////////////////////////////////////////////////////////////////
CPrefabObject::CPrefabObject()
{
	SetColor(RGB(255, 220, 0)); // Yellowish
	ZeroStruct(m_prefabGUID);

	m_bbox.min = m_bbox.max = Vec3(0, 0, 0);
	m_bBBoxValid = false;
	m_autoUpdatePrefabs = true;
	m_bModifyInProgress = false;
	m_bChangePivotPoint = false;
	m_bSettingPrefabObj = false;
	UseMaterialLayersMask(true);
}

//////////////////////////////////////////////////////////////////////////
void CPrefabObject::Done()
{
	LOADING_TIME_PROFILE_SECTION_ARGS(GetName().c_str());
	m_pPrefabItem = 0;

	DeleteAllMembers();
	CBaseObject::Done();
}

//////////////////////////////////////////////////////////////////////////
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

//////////////////////////////////////////////////////////////////////////
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

//////////////////////////////////////////////////////////////////////////
void CPrefabObject::ConvertToProceduralObject()
{
	GetIEditor()->GetObjectManager()->ClearSelection();

	GetIEditor()->GetIUndoManager()->Suspend();
	GetIEditor()->SetModifiedFlag();
	CBaseObject* pObject = GetIEditor()->GetObjectManager()->NewObject("Entity", 0, "ProceduralObject");
	if (!pObject)
	{
		string sError = "Could not convert prefab to " + this->GetName();
		CryWarning(VALIDATOR_MODULE_ENTITYSYSTEM, VALIDATOR_ERROR, "Conversion Failure.");
		return;
	}

	string sName = this->GetName();
	pObject->SetName(sName + "_prc");
	pObject->SetWorldTM(this->GetWorldTM());

	pObject->SetLayer(GetLayer());
	GetIEditor()->SelectObject(pObject);

	GetIEditor()->SelectObject(pObject);

	CEntityObject* pEntityObject = static_cast<CEntityObject*>(pObject);

	CPrefabItem* pPrefab = static_cast<CPrefabObject*>(this)->GetPrefabItem();
	if (pPrefab)
	{
		if (pPrefab->GetLibrary() && pPrefab->GetLibrary()->GetName())
			pEntityObject->SetEntityPropertyString("filePrefabLibrary", pPrefab->GetLibrary()->GetFilename());

		string sPrefabName = pPrefab->GetFullName();
		pEntityObject->SetEntityPropertyString("sPrefabVariation", sPrefabName);
	}

	GetIEditor()->GetObjectManager()->DeleteObject(this);

	GetIEditor()->GetIUndoManager()->Resume();
}

//////////////////////////////////////////////////////////////////////////
void CPrefabObject::OnContextMenu(CPopupMenuItem* menu)
{
	CGroup::OnContextMenu(menu);
	if (!menu->Empty())
	{
		menu->AddSeparator();
	}

	menu->Add("Open Database View", [ = ] {
		// Ensure that only this one object is selected
		if (!CheckFlags(OBJFLAG_SELECTED) || GetIEditor()->GetISelectionGroup()->GetCount() > 1)
		{
		  CUndo undo("Select Object");
		  // Select just this object, so we need to clear selection
		  GetObjectManager()->ClearSelection();
		  GetObjectManager()->SelectObject(this);
		}

		GetIEditor()->ExecuteCommand("general.open_pane 'DataBase View'");

		CDataBaseDialog* pDataBaseDlg = (CDataBaseDialog*)GetIEditor()->FindView("DataBase View");
		if (!pDataBaseDlg)
			return;
		pDataBaseDlg->SelectDialog(EDB_TYPE_PREFAB);

		CBaseLibraryDialog* prefabDialog = (CBaseLibraryDialog*)pDataBaseDlg->GetCurrent();
		if (!prefabDialog)
			return;

		CPrefabItem* pPrefab = static_cast<CPrefabObject*>(this)->GetPrefabItem();

		if (pPrefab && pPrefab->GetLibrary() && pPrefab->GetLibrary()->GetName())
		{
		  prefabDialog->Reload();
		  prefabDialog->SelectLibrary(pPrefab->GetLibrary()->GetName());
		  prefabDialog->SelectItem(pPrefab);
		}
	});

	menu->Add("Find in FlowGraph", [=](void) { OnShowInFG(); });
	menu->Add("Convert to Procedural Object", [=](void) { ConvertToProceduralObject(); });

}

//////////////////////////////////////////////////////////////////////////
void CPrefabObject::Display(CObjectRenderHelper& objRenderHelper)
{
	DisplayContext& dc = objRenderHelper.GetDisplayContextRef();


	if (!gViewportDebugPreferences.showPrefabObjectHelper)
		return;

	DrawDefault(dc, GetColor());

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
		if (gViewportPreferences.alwaysShowPrefabBox)
		{
			if (IsFrozen())
				dc.SetFreezeColor();
			else
				dc.SetColor(GetColor(), 0.2f);

			dc.DepthWriteOff();
			;
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

	if (gViewportDebugPreferences.showPrefabSubObjectHelper)
	{
		if (HaveChilds())
		{
			int numObjects = GetChildCount();
			for (int i = 0; i < numObjects; i++)
			{
				RecursivelyDisplayObject(GetChild(i), objRenderHelper);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CPrefabObject::RecursivelyDisplayObject(CBaseObject* obj, CObjectRenderHelper& objRenderHelper)
{
	DisplayContext& dc = objRenderHelper.GetDisplayContextRef();

	if (!obj->CheckFlags(OBJFLAG_PREFAB) || obj->IsHidden())
		return;

	AABB bbox;
	obj->GetBoundBox(bbox);
	if (dc.flags & DISPLAY_2D)
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

	//////////////////////////////////////////////////////////////////////////
	int numObjects = obj->GetChildCount();
	for (int i = 0; i < numObjects; i++)
		RecursivelyDisplayObject(obj->GetChild(i), objRenderHelper);

	int numLinkedObjects = obj->GetLinkedObjectCount();
	for (int i = 0; i < numLinkedObjects; ++i)
		RecursivelyDisplayObject(obj->GetLinkedObject(i), objRenderHelper);
}

//////////////////////////////////////////////////////////////////////////
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

//////////////////////////////////////////////////////////////////////////
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

//////////////////////////////////////////////////////////////////////////
XmlNodeRef CPrefabObject::Export(const string& levelPath, XmlNodeRef& xmlNode)
{
	// Do not export.
	return 0;
}

//////////////////////////////////////////////////////////////////////////
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

//////////////////////////////////////////////////////////////////////////
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
	;
	// Send event to all prefab childs.
	if (event != EVENT_ALIGN_TOGRID)
	{
		RecursivelySendEventToPrefabChilds(this, event);
	}
	CBaseObject::OnEvent(event);
}

//////////////////////////////////////////////////////////////////////////
void CPrefabObject::DeleteAllPrefabObjects()
{
	LOADING_TIME_PROFILE_SECTION
	std::vector<CBaseObject*> children;
	GetAllPrefabFlagedChildren(children);
	DetachAll(false, true);
	GetObjectManager()->DeleteObjects(children);
}

//////////////////////////////////////////////////////////////////////////
void CPrefabObject::SetPrefab(CryGUID guid, bool bForceReload)
{
	if (m_prefabGUID == guid && bForceReload == false)
		return;

	m_prefabGUID = guid;

	//m_fullPrototypeName = prototypeName;
	CPrefabManager* pManager = GetIEditor()->GetPrefabManager();
	CPrefabItem* pPrefab = (CPrefabItem*)pManager->FindItem(guid);
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
		           CryLinkService::CCryLinkUriFactory::GetUriV("Editor", "general.select_and_go_to_object %s", GetName()));

		SetMinSpec(GetMinSpec(), true);   // to make sure that all children get the right spec
	}
}

//////////////////////////////////////////////////////////////////////////
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

	m_pPrefabItem = pPrefab;
	m_prefabGUID = pPrefab->GetGUID();
	m_prefabName = pPrefab->GetFullName();

	StoreUndo("Set Prefab");

	CScopedSuspendUndo suspendUndo;

	// Make objects from this prefab.
	XmlNodeRef objectsXml = pPrefab->GetObjectsNode();
	if (!objectsXml)
	{
		CryWarning(VALIDATOR_MODULE_EDITOR, VALIDATOR_WARNING, "Prefab %s does not contain objects %s", (const char*)m_prefabName,
		           CryLinkService::CCryLinkUriFactory::GetUriV("Editor", "general.select_and_go_to_object %s", GetName()));
		return;
	}

	IObjectLayer* pThisLayer = GetLayer();

	//////////////////////////////////////////////////////////////////////////
	// Spawn objects.
	//////////////////////////////////////////////////////////////////////////
	pPrefabManager->SetSkipPrefabUpdate(true);

	CObjectArchive ar(GetObjectManager(), objectsXml, true);
	ar.EnableProgressBar(false); // No progress bar is shown when loading objects.
	CPrefabChildGuidProvider guidProvider = { this };
	ar.SetGuidProvider(&guidProvider);
	ar.EnableReconstructPrefabObject(true);
	// new prefabs are instantiated in current layer to avoid mishaps with missing layers. Then, we just set their layer to our own below
	ar.LoadInCurrentLayer(true);
	ar.LoadObjects(objectsXml);
	//force using this ID, incremental. Keep high part of CryGUID (stays compatible with old GUID.Data1)
	GetObjectManager()->ForceID(GetId().hipart >> 32);
	ar.ResolveObjects();

	AttachLoadedChildrenToPrefab(ar, pThisLayer);

	// Forcefully validate TM and then trigger InvalidateTM() on prefab (and all its children).
	GetWorldTM();
	InvalidateTM(0);

	GetObjectManager()->ForceID(0);//disable
	InvalidateBBox();

	SyncParentObject();

	eventsDelay.Resume();
	//Previous calls can potentially set up the skip prefab update to false so reset it again since we are not changing the prefab skip the lib update
	pPrefabManager->SetSkipPrefabUpdate(true);

	SetMinSpec(GetMinSpec(), true);   // to make sure that all children get the right spec

	pPrefabManager->SetSkipPrefabUpdate(false);
}

void CPrefabObject::AttachLoadedChildrenToPrefab(CObjectArchive &ar, IObjectLayer* pLayer)
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

	const bool keepPos = false;
	const bool invalidateTM = false; // Don't invalidate each child independently - we'll do it later.
	AttachChildren(objects, keepPos, invalidateTM);
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

//////////////////////////////////////////////////////////////////////////
void CPrefabObject::SetObjectPrefabFlagAndLayer(CBaseObject* object)
{
	object->SetFlags(OBJFLAG_PREFAB);
	object->SetLayer(GetLayer());
}

//////////////////////////////////////////////////////////////////////////
void CPrefabObject::InitObjectPrefabId(CBaseObject* object)
{
	if (object->GetIdInPrefab() == CryGUID::Null())
		object->SetIdInPrefab(object->GetId());
}

//////////////////////////////////////////////////////////////////////////
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

//////////////////////////////////////////////////////////////////////////
bool CPrefabObject::HitTest(HitContext& hc)
{
	if (IsOpen())
	{
		return CGroup::HitTest(hc);
	}

	if (CGroup::HitTest(hc))
	{
		hc.object = this;
		return true;
	}

	return false;
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
			ar(Serialization::ActionButton([ = ] {
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

//////////////////////////////////////////////////////////////////////////
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

//////////////////////////////////////////////////////////////////////////
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

	CSelectionGroup allPrefabChilds;
	GetAllPrefabFlagedChildren(allPrefabChilds);

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

//////////////////////////////////////////////////////////////////////////
void CPrefabObject::AddMember(CBaseObject* pObj, bool bKeepPos /*=true */)
{
	if (!m_pPrefabItem)
	{
		SetPrefab(m_prefabGUID, true);
		if (!m_pPrefabItem)
			return;
	}
	if (pObj->IsKindOf(RUNTIME_CLASS(CPrefabObject)))
	{
		if (static_cast<CPrefabObject*>(pObj)->m_pPrefabItem == m_pPrefabItem)
		{
			Warning("Object has the same prefab item");
			return;
		}
	}

	GetIEditor()->GetIUndoManager()->Suspend();

	if (!pObj->IsChildOf(this) && !pObj->GetParent())
	{
		AttachChild(pObj, bKeepPos);
	}

	for (auto i = 0; i < pObj->GetLinkedObjectCount(); ++i)
	{
		SetObjectPrefabFlagAndLayer(pObj->GetLinkedObject(i));
	}

	SetObjectPrefabFlagAndLayer(pObj);
	InitObjectPrefabId(pObj);

	CryGUID newGuid = CPrefabChildGuidProvider(this).GetFor(pObj);
	GetObjectManager()->ChangeObjectId(pObj->GetId(), newGuid);

	SObjectChangedContext context;
	context.m_operation = eOCOT_Add;
	context.m_modifiedObjectGlobalId = pObj->GetId();
	context.m_modifiedObjectGuidInPrefab = pObj->GetIdInPrefab();

	SyncPrefab(context);

	GetIEditor()->GetIUndoManager()->Resume();
	IObjectManager* pObjectManager = GetIEditor()->GetObjectManager();

	pObjectManager->NotifyPrefabObjectChanged(this);

	// if the currently modified prefab is selected make sure to refresh the inspector
	if (pObjectManager->GetSelection()->IsContainObject(this))
		pObjectManager->EmitPopulateInspectorEvent();
}

//////////////////////////////////////////////////////////////////////////
void CPrefabObject::RemoveMembers(std::vector<CBaseObject*>& members, bool keepPos /*= true*/, bool placeOnRoot /*= false*/) 
{
	LOADING_TIME_PROFILE_SECTION;
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

//////////////////////////////////////////////////////////////////////////
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

//////////////////////////////////////////////////////////////////////////
void CPrefabObject::SyncPrefab(const SObjectChangedContext& context)
{
	LOADING_TIME_PROFILE_SECTION;
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

	//Group delete
	if (context.m_operation == eOCOT_Delete)
	{
		CBaseObject* pObj = GetIEditor()->GetObjectManager()->FindObject(context.m_modifiedObjectGlobalId);
		if (pObj && pObj->IsKindOf(RUNTIME_CLASS(CGroup)) && !pObj->IsKindOf(RUNTIME_CLASS(CPrefabObject)))
		{
			TBaseObjects children;
			for (int i = 0, count = pObj->GetChildCount(); i < count; ++i)
			{
				children.push_back(pObj->GetChild(i));
			}
			for (int i = 0, count = children.size(); i < count; ++i)
			{
				RemoveMember(children[i], true);
			}
		}
	}

	if (m_pPrefabItem)
	{
		m_pPrefabItem->UpdateFromPrefabObject(this, context);
	}

	//Group add
	if (context.m_operation == eOCOT_Add)
	{
		CBaseObject* pObj = GetIEditor()->GetObjectManager()->FindObject(context.m_modifiedObjectGlobalId);
		if (pObj && pObj->IsKindOf(RUNTIME_CLASS(CGroup)) && !pObj->IsKindOf(RUNTIME_CLASS(CPrefabObject)))
		{
			for (int i = 0, count = pObj->GetChildCount(); i < count; ++i)
			{
				AddMember(pObj->GetChild(i), true);
			}
		}
	}

	InvalidateBBox();
}

//////////////////////////////////////////////////////////////////////////
void CPrefabObject::SyncParentObject()
{
	if (GetParent() && GetParent()->GetType() == OBJTYPE_GROUP)
	{
		static_cast<CGroup*>(GetParent())->InvalidateBBox();
	}
}

//////////////////////////////////////////////////////////////////////////
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

/////////////////////////////////////////////////////////////////////////
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

//////////////////////////////////////////////////////////////////////////
void CPrefabObject::RemoveChild(CBaseObject* child)
{
	CBaseObject::RemoveChild(child);
}

//////////////////////////////////////////////////////////////////////////
void CPrefabObject::SetMaterial(IEditorMaterial* pMaterial)
{
	if (pMaterial)
	{
		for (int i = 0; i < GetChildCount(); i++)
			GetChild(i)->SetMaterial(pMaterial);
	}
	CBaseObject::SetMaterial(pMaterial);
}

//////////////////////////////////////////////////////////////////////////
void CPrefabObject::SetWorldTM(const Matrix34& tm, int flags /* = 0 */)
{
	if (m_bChangePivotPoint)
		SetPivot(tm.GetTranslation());
	else
		CBaseObject::SetWorldTM(tm, flags);
}

//////////////////////////////////////////////////////////////////////////
void CPrefabObject::SetWorldPos(const Vec3& pos, int flags /* = 0 */)
{
	if (m_bChangePivotPoint)
		SetPivot(pos);
	else
		CBaseObject::SetWorldPos(pos, flags);
}

//////////////////////////////////////////////////////////////////////////
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

	__super::SetName(name);

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

//////////////////////////////////////////////////////////////////////////
bool CPrefabObject::HitTestMembers(HitContext& hcOrg)
{
	float mindist = FLT_MAX;

	HitContext hc = hcOrg;

	CBaseObject* selected = 0;
	std::vector<CBaseObject*> allChildrenObj;
	GetAllPrefabFlagedChildren(allChildrenObj);
	int numberOfChildren = allChildrenObj.size();
	for (int i = 0; i < numberOfChildren; ++i)
	{
		CBaseObject* pObj = allChildrenObj[i];

		if (pObj == this || pObj->IsFrozen() || pObj->IsHidden())
			continue;

		if (!GetObjectManager()->HitTestObject(pObj, hc))
			continue;

		if (hc.dist >= mindist)
			continue;

		mindist = hc.dist;

		if (hc.object)
			selected = hc.object;
		else
			selected = pObj;

		hc.object = 0;
	}

	if (selected)
	{
		hcOrg.object = selected;
		hcOrg.dist = mindist;
		return true;
	}
	return false;
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

bool CPrefabObject::CanObjectBeAddedAsMember(CBaseObject* pObject)
{
	if (pObject->CheckFlags(OBJFLAG_PREFAB))
	{
		CBaseObject* pParentObject = pObject->GetParent();
		while (pParentObject)
		{
			if (pParentObject->IsKindOf(RUNTIME_CLASS(CPrefabObject)))
				return GetPrefabGuid() == static_cast<CPrefabObject*>(pParentObject)->GetPrefabGuid();
			pParentObject = pParentObject->GetParent();
		}
	}
	return true;
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

//////////////////////////////////////////////////////////////////////////

void CPrefabObjectClassDesc::EnumerateObjects(IObjectEnumerator* pEnumerator)
{
	RegisterAsDataBaseListener(EDB_TYPE_PREFAB);
	GetIEditor()->GetDBItemManager(EDB_TYPE_PREFAB)->EnumerateObjects(pEnumerator);
}

//////////////////////////////////////////////////////////////////////////

bool CPrefabObjectClassDesc::IsCreatable() const
{
	// Only allow use in legacy projects
	return gEnv->pGameFramework->GetIGame() != nullptr;
}

//////////////////////////////////////////////////////////////////////////
namespace
{
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

static void PyNewPrefabFromSelection(const char* libraryName, const char* groupName, const char* itemName)
{
	IDataBaseLibrary* pLibrary = GetIEditor()->GetPrefabManager()->FindLibrary(libraryName);
	if (pLibrary == NULL)
		IDataBaseLibrary* pLibrary = GetIEditor()->GetPrefabManager()->LoadLibrary(libraryName);

	const CSelectionGroup* pSel = GetIEditorImpl()->GetSelection();
	if (pSel->IsEmpty())
		return;

	string strFullName = "";
	strFullName.Format("%s.%s", groupName, itemName);

	CUndo undo("Add prefab library item");
	CPrefabItem* pPrefab = static_cast<CPrefabItem*>(GetIEditor()->GetPrefabManager()->CreateItem(pLibrary));

	pPrefab->SetName(strFullName);
	pPrefab->MakeFromSelection(*pSel);
	pPrefab->Update();

	GetIEditor()->GetPrefabManager()->SetSelectedItem(pPrefab);
}

static void PyDeletePrefabItem(const char* itemName, const char* libraryName)
{
	IDataBaseLibrary* pLibrary = GetIEditor()->GetPrefabManager()->FindLibrary(libraryName);
	if (pLibrary == NULL)
		IDataBaseLibrary* pLibrary = GetIEditor()->GetPrefabManager()->LoadLibrary(libraryName);

	IDataBaseItem* pItem = GetIEditor()->GetPrefabManager()->FindItemByName(itemName);
	if (pItem != NULL)
	{
		CUndo undo("Delete Prefab Item");
		GetIEditor()->GetPrefabManager()->DeleteItem(pItem);
		pLibrary->SetModified();
		GetIEditor()->Notify(eNotify_OnDataBaseUpdate);
	}
}

static std::vector<std::string> PyGetPrefabLibrarys()
{
	CPrefabManager* pPrefabManager = GetIEditor()->GetPrefabManager();
	if (!pPrefabManager)
	{
		throw std::runtime_error("Invalid Prefab Manager.");
	}

	std::vector<std::string> results;
	int intLibCount = pPrefabManager->GetLibraryCount();
	for (int i = 0; i < pPrefabManager->GetLibraryCount(); i++)
	{
		IDataBaseLibrary* pPrefabLibrary = pPrefabManager->GetLibrary(i);
		results.push_back(static_cast<std::string>(pPrefabLibrary->GetName()));
	}
	return results;
}

static std::vector<std::string> PyGetPrefabGroups(const char* pLibraryName)
{
	std::vector<std::string> results;
	if (pLibraryName == NULL)
		return results;

	CPrefabManager* pPrefabManager = GetIEditor()->GetPrefabManager();
	if (!pPrefabManager)
	{
		throw std::runtime_error("Invalid Prefab Manager.");
	}

	IDataBaseLibrary* pPrefabLibrary = pPrefabManager->FindLibrary(pLibraryName);
	if (!pPrefabLibrary)
	{
		throw std::runtime_error("Invalid Prefab Library.");
	}

	for (int i = 0; i < pPrefabLibrary->GetItemCount(); i++)
	{
		CPrefabItem* pPrefabItem = static_cast<CPrefabItem*>(pPrefabLibrary->GetItem(i));
		stl::push_back_unique(results, static_cast<std::string>(pPrefabItem->GetGroupName()));
	}
	return results;
}

static std::vector<std::string> PyGetPrefabItems(const char* pLibraryName, const char* pGroupName)
{
	std::vector<std::string> results;
	if (pLibraryName == NULL || pGroupName == NULL)
		return results;

	CPrefabManager* pPrefabManager = GetIEditor()->GetPrefabManager();
	if (!pPrefabManager)
	{
		throw std::runtime_error("Invalid Prefab Manager.");
	}

	IDataBaseLibrary* pPrefabLibrary = pPrefabManager->FindLibrary(pLibraryName);
	if (!pPrefabLibrary)
	{
		throw std::runtime_error("Invalid Prefab Library.");
	}

	for (int i = 0; i < pPrefabLibrary->GetItemCount(); i++)
	{
		// Get short name will return item only.
		CPrefabItem* pPrefabItem = static_cast<CPrefabItem*>(pPrefabLibrary->GetItem(i));
		if (pPrefabItem->GetGroupName() == pGroupName)
		{
			stl::push_back_unique(results, static_cast<std::string>(pPrefabItem->GetShortName()));
		}
	}
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

static bool PyHasPrefabLibrary(const char* pLibraryName)
{
	CPrefabManager* pPrefabManager = GetIEditor()->GetPrefabManager();
	if (!pPrefabManager)
	{
		throw std::runtime_error("Invalid Prefab Manager.");
	}

	IDataBaseLibrary* pPrefabLibrary = pPrefabManager->FindLibrary(pLibraryName);
	if (!pPrefabLibrary)
	{
		return false;
	}
	return true;
}

static bool PyHasPrefabGroup(const char* pLibraryName, const char* pGroupName)
{
	CPrefabManager* pPrefabManager = GetIEditor()->GetPrefabManager();
	if (!pPrefabManager)
	{
		throw std::runtime_error("Invalid Prefab Manager.");
	}

	IDataBaseLibrary* pPrefabLibrary = pPrefabManager->FindLibrary(pLibraryName);
	if (!pPrefabLibrary)
	{
		return false;
	}

	for (int i = 0; i < pPrefabLibrary->GetItemCount(); i++)
	{
		CPrefabItem* pPrefabItem = static_cast<CPrefabItem*>(pPrefabLibrary->GetItem(i));
		if (pPrefabItem->GetGroupName() == pGroupName)
		{
			return true;
		}
	}
	return false;
}

static bool PyHasPrefabItem(const char* pLibraryName, const char* pItemName)
{
	CPrefabManager* pPrefabManager = GetIEditor()->GetPrefabManager();
	if (!pPrefabManager)
	{
		throw std::runtime_error("Invalid Prefab Manager.");
	}

	IDataBaseLibrary* pPrefabLibrary = pPrefabManager->FindLibrary(pLibraryName);
	if (!pPrefabLibrary)
	{
		return false;
	}

	CPrefabItem* pPrefabItem = static_cast<CPrefabItem*>(pPrefabLibrary->FindItem(pItemName));
	if (!pPrefabItem)
	{
		return false;
	}
	return true;
}
}

DECLARE_PYTHON_MODULE(prefab);

REGISTER_PYTHON_COMMAND(PyNewPrefabFromSelection, prefab, new_prefab_from_selection, "Set the pivot position of a specified prefab.");
REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE(PyGetPrefabOfChild, prefab, get_parent,
                                          "Get the parent prefab object of a given child object.",
                                          "prefab.get_parent(str childName)");

REGISTER_PYTHON_COMMAND(PyDeletePrefabItem, prefab, delete_prefab_item, "Delete a prefab item from a specified prefab library.");
REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE(PyGetPrefabLibrarys, prefab, get_librarys,
                                          "Get all avalible prefab libraries.",
                                          "prefab.get_librarys()");
REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE(PyGetPrefabGroups, prefab, get_groups,
                                          "Get all avalible prefab groups of a specified library.",
                                          "prefab.get_groups()");
REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE(PyGetPrefabItems, prefab, get_items,
                                          "Get the avalible prefab item of a specified library and group.",
                                          "prefab.get_items()");
REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE(PyGetPrefabChildWorldPos, prefab, get_world_pos,
                                          "Get the absolute world position of the specified prefab object.",
                                          "prefab.get_world_pos()");
REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE(PyHasPrefabLibrary, prefab, has_library,
                                          "Return true if the specified prefab library exists.",
                                          "prefab.has_library()");
REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE(PyHasPrefabGroup, prefab, has_group,
                                          "Return true if in the specified prefab library, the specified group exists.",
                                          "prefab.has_group()");
REGISTER_ONLY_PYTHON_COMMAND_WITH_EXAMPLE(PyHasPrefabItem, prefab, has_item,
                                          "Return true if in the specified prefab library, and in the specified group, the specified item exists.",
                                          "prefab.has_item()");

