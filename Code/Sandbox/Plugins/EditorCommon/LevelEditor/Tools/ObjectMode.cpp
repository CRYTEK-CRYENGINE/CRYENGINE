// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ObjectMode.h"

#include "Controls/DynamicPopupMenu.h"
#include "Gizmos/IGizmoManager.h"
#include "LevelEditor/Tools/DeepSelection.h"
#include "LevelEditor/Tools/SubObjectSelectionReferenceFrameCalculator.h"
#include "Objects/ISelectionGroup.h"
#include "Preferences/SnappingPreferences.h"
#include "Preferences/ViewportPreferences.h"
#include "IUndoManager.h"
#include "Viewport.h"

#include "Objects/EntityObject.h"
#include "Objects/CameraObject.h"
#include "Objects/BrushObject.h"
#include "Objects/ParticleEffectObject.h"
#include "Objects/PrefabObject.h"
#include "GameEngine.h"
#include "Terrain/Heightmap.h"
#include "ViewManager.h"
#include <IEditor.h>

/////////////////////////////
// CObjectManipulatorOwner
/////////////////////////////
CObjectManipulatorOwner::CObjectManipulatorOwner(CObjectMode* objectModeTool)
	: m_bIsVisible(true)
	, m_visibilityDirty(true)
{
	m_manipulator = GetIEditor()->GetGizmoManager()->AddManipulator(this);
	m_manipulator->signalBeginDrag.Connect(objectModeTool, &CObjectMode::OnManipulatorBeginDrag);
	m_manipulator->signalDragging.Connect(objectModeTool, &CObjectMode::OnManipulatorDrag);
	m_manipulator->signalEndDrag.Connect(objectModeTool, &CObjectMode::OnManipulatorEndDrag);

	GetIEditor()->GetObjectManager()->signalObjectsChanged.Connect(this, &CObjectManipulatorOwner::OnObjectsChanged);
	GetIEditor()->GetObjectManager()->signalSelectionChanged.Connect(this, &CObjectManipulatorOwner::OnSelectionChanged);

	const ISelectionGroup* pSelection = GetIEditor()->GetISelectionGroup();
	int totCount = pSelection->GetCount();

	// register to all selected objects on initialization
	for (int i = 0; i < totCount; ++i)
	{
		CBaseObject* pObj = pSelection->GetObject(i);
		pObj->signalChanged.Connect(this, &CObjectManipulatorOwner::OnObjectChanged);
	}
}

CObjectManipulatorOwner::~CObjectManipulatorOwner()
{
	GetIEditor()->GetObjectManager()->signalObjectsChanged.DisconnectObject(this);
	GetIEditor()->GetObjectManager()->signalSelectionChanged.DisconnectObject(this);

	m_manipulator->signalBeginDrag.DisconnectAll();
	m_manipulator->signalDragging.DisconnectAll();
	m_manipulator->signalEndDrag.DisconnectAll();
	GetIEditor()->GetGizmoManager()->RemoveManipulator(m_manipulator);

	m_manipulator = nullptr;

	const ISelectionGroup* pSelection = GetIEditor()->GetISelectionGroup();
	int totCount = pSelection->GetCount();

	// unregister from all selected objects
	for (int i = 0; i < totCount; ++i)
	{
		CBaseObject* pObj = pSelection->GetObject(i);
		pObj->signalChanged.DisconnectObject(this);
	}
}

bool CObjectManipulatorOwner::GetManipulatorMatrix(Matrix34& tm)
{
	const ISelectionGroup* pSelection = GetIEditor()->GetISelectionGroup();
	return pSelection->GetManipulatorMatrix(tm);
}

void CObjectManipulatorOwner::GetManipulatorPosition(Vec3& position)
{
	const ISelectionGroup* pSelection = GetIEditor()->GetISelectionGroup();

	position.Set(0.0f, 0.0f, 0.0f);

	int totCount = pSelection->GetCount();
	for (int i = 0; i < totCount; ++i)
	{
		CBaseObject* pObj = pSelection->GetObject(i);
		if (pObj->IsVisible() && !pObj->IsFrozen())
		{
			position += pObj->GetWorldPos();
		}
	}

	position *= (1.0f / totCount);
}

void CObjectManipulatorOwner::OnObjectChanged(const CBaseObject* pObject, const CObjectEvent& event)
{
	if (event.m_type == OBJECT_ON_TRANSFORM)
	{
		// tag for update next cycle
		m_manipulator->Invalidate();
	}
	else if (event.m_type == OBJECT_ON_VISIBILITY)
	{
		m_manipulator->Invalidate();
		m_visibilityDirty = true;
	}
}

void CObjectManipulatorOwner::OnObjectsChanged(const std::vector<CBaseObject*>& objects, const CObjectEvent& event)
{
	if (event.m_type == OBJECT_ON_TRANSFORM)
	{
		// tag for update next cycle
		m_manipulator->Invalidate();
	}
}

void CObjectManipulatorOwner::OnSelectionChanged(const std::vector<CBaseObject*>& selected, const std::vector<CBaseObject*>& deselected)
{
	CRY_PROFILE_FUNCTION(PROFILE_EDITOR);
	// unregister from all manipulated objects
	for (auto& pObject : deselected)
	{
		pObject->signalChanged.DisconnectObject(this);
	}

	for (auto& pObject : selected)
	{
		pObject->signalChanged.Connect(this, &CObjectManipulatorOwner::OnObjectChanged);
	}

	m_manipulator->Invalidate();
	m_visibilityDirty = true;
}

bool CObjectManipulatorOwner::IsManipulatorVisible()
{
	if (m_visibilityDirty)
	{
		UpdateVisibilityState();
		m_manipulator->Invalidate();
		m_visibilityDirty = false;
	}
	return m_bIsVisible;
}

void CObjectManipulatorOwner::UpdateVisibilityState()
{
	const ISelectionGroup* pSelection = GetIEditor()->GetISelectionGroup();
	m_bIsVisible = false;
	int totCount = pSelection->GetCount();
	for (int i = 0; i < totCount; ++i)
	{
		CBaseObject* pObj = pSelection->GetObject(i);

		if (pObj->IsVisible() && !pObj->IsFrozen())
		{
			m_bIsVisible = true;
			break;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
IMPLEMENT_DYNCREATE(CObjectMode, CEditTool)

SDeepSelectionPreferences gDeepSelectionPreferences;
REGISTER_PREFERENCES_PAGE_PTR(SDeepSelectionPreferences, &gDeepSelectionPreferences)

//////////////////////////////////////////////////////////////////////////
// Class description.
//////////////////////////////////////////////////////////////////////////
class CObjectMode_ClassDesc : public IClassDesc
{
	//! This method returns an Editor defined GUID describing the class this plugin class is associated with.
	virtual ESystemClassID SystemClassID() { return ESYSTEM_CLASS_EDITTOOL; }

	//! This method returns the human readable name of the class.
	virtual const char* ClassName() { return "EditTool.ObjectMode"; }

	//! This method returns Category of this class, Category is specifing where this plugin class fits best in
	//! create panel.
	virtual const char*    Category()        { return "Select"; }
	virtual CRuntimeClass* GetRuntimeClass() { return RUNTIME_CLASS(CObjectMode); }
};

REGISTER_CLASS_DESC(CObjectMode_ClassDesc);

//////////////////////////////////////////////////////////////////////////
CObjectMode::CObjectMode()
	: m_objectManipulatorOwner(this)
	, m_bDragThresholdExceeded(false)
{
	m_openContext = false;
	m_commandMode = NothingMode;
	m_MouseOverObject = CryGUID::Null();

	m_pDeepSelection = new CDeepSelection();
	m_bMoveByFaceNormManipShown = false;
	m_pHitObject = NULL;

	m_bTransformChanged = false;

	m_suspendHighlightChange = false;
	m_normalMoveGizmo = nullptr;

	m_bGizmoDrag = false;
}

CObjectMode::~CObjectMode()
{
	GetIEditor()->UnRegisterAllObjectModeSubTools();
	CAutoRegisterObjectModeSubToolHelper::UnregisterAll();

	CBaseObject* pMouseOverObject = nullptr;
	if (m_MouseOverObject != CryGUID::Null())
	{
		pMouseOverObject = GetIEditor()->GetObjectManager()->FindObject(m_MouseOverObject);
	}

	if (pMouseOverObject)
	{
		pMouseOverObject->SetHighlight(false);
	}

	if (m_normalMoveGizmo)
	{
		m_normalMoveGizmo->signalBeginDrag.DisconnectAll();
		m_normalMoveGizmo->signalDragging.DisconnectAll();
		m_normalMoveGizmo->signalEndDrag.DisconnectAll();
		GetIEditor()->GetGizmoManager()->RemoveManipulator(m_normalMoveGizmo);
		m_normalMoveGizmo = nullptr;
	}
}

void CObjectMode::Activate()
{
	CAutoRegisterObjectModeSubToolHelper::RegisterAll();
	GetIEditor()->RegisterAllObjectModeSubTools();
}

void CObjectMode::RegisterSubTool(ISubTool* pSubTool)
{
	m_subTools.insert(pSubTool);
}

void CObjectMode::UnRegisterSubTool(ISubTool* pSubTool)
{
	m_subTools.erase(pSubTool);
}

void CObjectMode::DisplaySelectionPreview(SDisplayContext& dc)
{
	IDisplayViewport* pDisplayViewport = dc.view;
	if (!pDisplayViewport)
		return;

	CViewport* pViewport = static_cast<CViewport*>(pDisplayViewport);
	if (!pViewport)
		return;

	IObjectManager* objMan = GetIEditor()->GetObjectManager();

	CRect rc = pViewport->GetSelectionRectangle();

	if (GetCommandMode() == SelectMode)
	{
		if (rc.Width() > m_areaRectMinSizeWidth && rc.Height() > m_areaRectMinSizeHeight)
		{
			GetIEditor()->GetObjectManager()->FindObjectsInRect(pViewport, rc, m_PreviewGUIDs);

			// Do not include child objects in the count of object candidates
			int childNo = 0;
			for (int objNo = 0; objNo < m_PreviewGUIDs.size(); ++objNo)
			{
				auto foundObj = objMan->FindObject(m_PreviewGUIDs[objNo]);
				if (foundObj && foundObj->GetParent())
					++childNo;
			}

			// Draw Preview for objects
			for (size_t i = 0; i < m_PreviewGUIDs.size(); ++i)
			{
				CBaseObject* pObject = GetIEditor()->GetObjectManager()->FindObject(m_PreviewGUIDs[i]);

				if (!pObject)
					continue;

				if (pObject->GetType() & ~gViewportSelectionPreferences.objectSelectMask)
					continue;

				pObject->DrawSelectionPreviewHighlight(dc);
			}
		}
	}
}

void CObjectMode::Display(SDisplayContext& dc)
{
	// Selection Candidates Preview
	DisplaySelectionPreview(dc);
}

bool CObjectMode::MouseCallback(CViewport* view, EMouseEvent event, CPoint& point, int flags)
{
	// Sub tools get to handle the event first, if it goes unhandled, object mode will then try to handle it
	for (ISubTool* pSubTool : m_subTools)
	{
		if (pSubTool->HandleMouseEvent(view, event, point, flags))
		{
			return true;
		}
	}

	switch (event)
	{
	case eMouseLDown:
		return OnLButtonDown(view, flags, point);

	case eMouseLUp:
		return OnLButtonUp(view, flags, point);

	case eMouseLDblClick:
		return OnLButtonDblClk(view, flags, point);

	case eMouseRDown:
		return OnRButtonDown(view, flags, point);

	case eMouseRUp:
		return OnRButtonUp(view, flags, point);

	case eMouseMove:
	case eMouseEnter:
		return OnMouseMove(view, flags, point);

	case eMouseMDown:
		return OnMButtonDown(view, flags, point);

	case eMouseFocusLeave:
	case eMouseLeave:
		SetObjectCursor(view, 0, IObjectManager::ESelectOp::eNone);
		return true;

	}
	return false;
}

bool CObjectMode::OnKeyDown(CViewport* view, uint32 nChar, uint32 nRepCnt, uint32 nFlags)
{
	if (nChar == Qt::Key_Escape)
	{
		GetIEditor()->GetObjectManager()->ClearSelection();
		CLevelEditorSharedState* pLevelEditor = GetIEditor()->GetLevelEditorSharedState();

		if (pLevelEditor->GetEditMode() == CLevelEditorSharedState::EditMode::SelectArea)
			pLevelEditor->SetEditMode(CLevelEditorSharedState::EditMode::Select);
	}
	return false;
}

bool CObjectMode::OnKeyUp(CViewport* view, uint32 nChar, uint32 nRepCnt, uint32 nFlags)
{
	return false;
}

bool CObjectMode::OnLButtonDown(CViewport* view, int nFlags, CPoint point)
{
	if (m_bMoveByFaceNormManipShown)
	{
		HideMoveByFaceNormGizmo();
	}

	if (GetIEditor()->IsInGameMode())
	{
		// Ignore clicks while in game.
		return false;
	}

	// Save the mouse down position
	m_cMouseDownPos = point;

	m_bDragThresholdExceeded = false;

	view->ResetSelectionRegion();

	Vec3 pos = view->SnapToGrid(view->ViewToWorld(point));
	CLevelEditorSharedState::EditMode editMode = GetIEditor()->GetLevelEditorSharedState()->GetEditMode();

	// Get control key status.
	const bool bAltClick = (nFlags & MK_ALT);
	const bool bCtrlClick = (nFlags & MK_CONTROL);
	const bool bShiftClick = (nFlags & MK_SHIFT);

	const bool bAddSelect = bShiftClick;
	const bool bToggle = bCtrlClick;
	// Alt click might be beginning of marque deselection - so we must not remove selection
	const bool bNoRemoveSelection = bAddSelect || bToggle || bAltClick;

	bool bLockSelection = GetIEditor()->IsSelectionLocked();

	HitContext hitInfo(view);
	// Take in consideration all objects (including frozen), while snapping via (Ctrl+Shift+LMB)
	hitInfo.ignoreFrozenObjects = !(bCtrlClick && bShiftClick);
	HitTest(hitInfo, view, point);

	if (hitInfo.axis != CLevelEditorSharedState::Axis::None)
	{
		GetIEditor()->GetLevelEditorSharedState()->SetAxisConstraint(hitInfo.axis);
		// if edit mode is set to selection, then we treat gizmo as a selection component and we should not lock the selection
		if (editMode != CLevelEditorSharedState::EditMode::Select)
		{
			bLockSelection = true;
		}
	}

	CBaseObject* hitObj = hitInfo.object;

	CLevelEditorSharedState::CoordSystem coordSys = GetIEditor()->GetLevelEditorSharedState()->GetCoordSystem();
	Vec3 gridPosition = (hitObj) ? hitObj->GetWorldPos() : pos;

	Matrix34 constructionMatrix = Matrix34::CreateIdentity();

	if (coordSys == CLevelEditorSharedState::CoordSystem::UserDefined)
	{
		GetIEditor()->GetISelectionGroup()->GetManipulatorMatrix(constructionMatrix);
		constructionMatrix.SetTranslation(gridPosition);
	}
	else if (coordSys == CLevelEditorSharedState::CoordSystem::World || !hitObj)
	{
		constructionMatrix.SetTranslation(gridPosition);
	}
	else if (coordSys == CLevelEditorSharedState::CoordSystem::Parent)
	{
		if (hitInfo.object->GetParent())
		{
			constructionMatrix = hitInfo.object->GetParent()->GetWorldTM();
			constructionMatrix.OrthonormalizeFast();
			constructionMatrix.SetTranslation(gridPosition);
		}
		else
		{
			constructionMatrix = hitInfo.object->GetWorldTM();
		}
	}
	else if (coordSys == CLevelEditorSharedState::CoordSystem::Local)
	{
		constructionMatrix = hitInfo.object->GetWorldTM();
	}

	view->SetConstructionMatrix(constructionMatrix);

	if (gSnappingPreferences.IsSnapToTerrainEnabled() && GetIEditor()->GetLevelEditorSharedState()->GetAxisConstraint() != CLevelEditorSharedState::Axis::Z)
	{
		m_mouseDownWorldPos = view->ViewToWorld(point);
	}
	else
	{
		m_mouseDownWorldPos = view->MapViewToCP(point);
	}

	if (editMode != CLevelEditorSharedState::EditMode::Tool)
	{
		// Check for Move to position.
		if (bCtrlClick && bShiftClick)
		{
			// If we didn't hit against an object nor terrain, then we shouldn't move to selection
			bool hasCollidedWithTerrain;
			view->ViewToWorld(point, &hasCollidedWithTerrain);
			if (hitInfo.object || hasCollidedWithTerrain)
			{
				HandleMoveSelectionToPosition(pos, point, bAltClick);
				bLockSelection = true;
			}
		}
	}

	if (editMode == CLevelEditorSharedState::EditMode::Move)
	{
		if (!bNoRemoveSelection)
			SetCommandMode(MoveMode);

		if (hitObj && hitObj->IsSelected() && !bNoRemoveSelection)
			bLockSelection = true;
	}
	else if (editMode == CLevelEditorSharedState::EditMode::Rotate)
	{
		if (!bNoRemoveSelection)
			SetCommandMode(RotateMode);
		if (hitObj && hitObj->IsSelected() && !bNoRemoveSelection)
			bLockSelection = true;
	}
	else if (editMode == CLevelEditorSharedState::EditMode::Scale)
	{
		if (!bNoRemoveSelection)
		{
			SetCommandMode(ScaleMode);
		}

		if (hitObj && hitObj->IsSelected() && !bNoRemoveSelection)
			bLockSelection = true;
	}
	else if (hitObj != 0 && GetIEditor()->GetSelectedObject() == hitObj && !bAddSelect && !bToggle)
	{
		bLockSelection = true;
	}

	int numSelected = 0;

	if (!bLockSelection)
	{
		numSelected = ApplyMouseSelection(view, hitObj, bNoRemoveSelection, bToggle, bShiftClick);
	}

	if (!bLockSelection && numSelected == 0 || editMode == CLevelEditorSharedState::EditMode::Select || editMode == CLevelEditorSharedState::EditMode::SelectArea)
	{
		// If object is not selected.
		// Capture mouse input for this window.
		SetCommandMode(SelectMode);
	}

	if (GetCommandMode() == MoveMode ||
	    GetCommandMode() == RotateMode ||
	    GetCommandMode() == ScaleMode)
	{
		m_suspendHighlightChange = true;
		view->BeginUndo();
	}

	//////////////////////////////////////////////////////////////////////////
	// Change cursor, must be before Capture mouse.
	//////////////////////////////////////////////////////////////////////////
	SetObjectCursor(view, hitObj, bAddSelect ? IObjectManager::ESelectOp::eSelect : IObjectManager::ESelectOp::eNone);

	m_bTransformChanged = false;

	if (m_pDeepSelection->GetMode() == CDeepSelection::DSM_POP)
		return OnLButtonUp(view, nFlags, point);

	return true;
}

int CObjectMode::ApplyMouseSelection(CViewport* pView, CBaseObject* pHitObject, bool bNoRemoveSelection, bool bToggle, bool bAdd)
{
	int numSelected = 0;

	// If not selection locked.
	pView->BeginUndo();

	IObjectManager* pObjectManager = GetIEditor()->GetObjectManager();

	if (pHitObject)
	{
		numSelected = 1;

		if (!bToggle)
		{
			if (bAdd)
			{
				pObjectManager->AddObjectToSelection(pHitObject);
			}
			else
			{
				pObjectManager->SelectObject(pHitObject);
			}
		}
		else
		{
			if (pHitObject->IsSelected())
			{
				pObjectManager->UnselectObject(pHitObject);
			}
			else
			{
				pObjectManager->AddObjectToSelection(pHitObject);
			}
		}
	}
	else
	{
		if (!bNoRemoveSelection)
		{
			// Current selection should be cleared
			numSelected = GetIEditor()->GetISelectionGroup()->GetCount();
			pObjectManager->ClearSelection();
		}
	}

	if (pView->IsUndoRecording())
	{
		pView->AcceptUndo("Select Object(s)");
	}

	return numSelected;
}

bool CObjectMode::OnLButtonUp(CViewport* view, int nFlags, CPoint point)
{
	if (GetIEditor()->IsInGameMode())
	{
		// Ignore clicks while in game.
		return true;
	}

	if (m_bTransformChanged)
	{
		const ISelectionGroup* pSelection = GetIEditor()->GetISelectionGroup();
		if (pSelection)
			pSelection->FinishChanges();
		m_bTransformChanged = false;
	}

	// Undo may have been accepted already for MoveMode (if mouse has moved from original position), so view->IsUndoRecording()
	// will be false here, but we still need to cleanup. AcceptUndo is safe in any case because it checks if we are using undo.
	if (view->IsUndoRecording())
	{
		if (GetCommandMode() == MoveMode)
		{
			view->AcceptUndo("Move Selection");
		}
		else if (GetCommandMode() == RotateMode)
		{
			view->AcceptUndo("Rotate Selection");
		}
		else if (GetCommandMode() == ScaleMode)
		{
			view->AcceptUndo("Scale Selection");
		}
		else
		{
			view->CancelUndo();
		}
	}

	if (GetCommandMode() == MoveMode)
	{
		m_bDragThresholdExceeded = false;
	}

	//////////////////////////////////////////////////////////////////////////

	if (GetCommandMode() == SelectMode && (!GetIEditor()->IsSelectionLocked()))
	{
		const bool bUnselect = (nFlags & MK_ALT);
		const bool bToggle = (nFlags & MK_CONTROL);

		const IObjectManager::ESelectOp selectOp = bUnselect ? IObjectManager::ESelectOp::eUnselect :
		                                           (bToggle ? IObjectManager::ESelectOp::eToggle : IObjectManager::ESelectOp::eSelect);

		//If a rectangle is too small we don't want to create a selection from it
		CRect selectRect = view->GetSelectionRectangle();
		if (!selectRect.IsRectEmpty())
		{
			if (selectRect.Width() > m_areaRectMinSizeWidth && selectRect.Height() > m_areaRectMinSizeHeight)
			{
				GetIEditor()->GetObjectManager()->SelectObjectsInRect(view, selectRect, selectOp);
			}
		}

		if (GetIEditor()->GetLevelEditorSharedState()->GetEditMode() == CLevelEditorSharedState::EditMode::SelectArea)
		{
			GetIEditor()->GetObjectManager()->ClearSelection();
		}
	}

	// If command mode is still "NothingMode" this object was just created and placed and needs this update too.
	if (GetCommandMode() == ScaleMode || GetCommandMode() == MoveMode || GetCommandMode() == RotateMode || GetCommandMode() == NothingMode)
	{
		m_suspendHighlightChange = false;
		GetIEditor()->GetISelectionGroup()->ObjectModified();
	}

	if (GetIEditor()->GetLevelEditorSharedState()->GetEditMode() != CLevelEditorSharedState::EditMode::SelectArea)
	{
		view->ResetSelectionRegion();
	}
	// Reset selected rectangle.
	view->SetSelectionRectangle(CPoint(0, 0), CPoint(0, 0));

	SetCommandMode(NothingMode);

	return true;
}

bool CObjectMode::OnLButtonDblClk(CViewport* view, int nFlags, CPoint point)
{
	// If shift clicked, Move the camera to this place.
	if (nFlags & MK_SHIFT)
	{
		// Get the heightmap coordinates for the click position
		Vec3 v = view->ViewToWorld(point);
		if (!(v.x == 0 && v.y == 0 && v.z == 0))
		{
			Matrix34 tm = view->GetViewTM();
			Vec3 p = tm.GetTranslation();
			float height = p.z - GetIEditor()->GetTerrainElevation(p.x, p.y);
			if (height < 1) height = 1;
			p.x = v.x;
			p.y = v.y;
			p.z = GetIEditor()->GetTerrainElevation(p.x, p.y) + height;
			tm.SetTranslation(p);
			view->SetViewTM(tm);
		}
	}
	else
	{
		HitContext hitInfo(view);
		hitInfo.ignoreHierarchyLocks = true;
		HitTest(hitInfo, view, point);
		CBaseObject* pObject = hitInfo.object;

		if (pObject)
		{
			GetIEditor()->GetObjectManager()->SelectObject(pObject);
			pObject->OnEvent(EVENT_DBLCLICK);
		}
	}
	return true;
}

bool CObjectMode::OnRButtonDown(CViewport* view, int nFlags, CPoint point)
{
	if (gViewportPreferences.enableContextMenu)
	{
		// Check if right clicked on object.
		HitContext hitInfo(view);
		if (view->HitTest(point, hitInfo) && hitInfo.object)
		{
			m_openContext = true;
			m_rMouseDownPos = point;
			return true;
		}
	}
	return false;
}

bool CObjectMode::OnRButtonUp(CViewport* view, int nFlags, CPoint point)
{
	if (m_openContext)
	{
		m_openContext = false;

		// Get control key status.
		const bool bAddSelect = (nFlags & MK_SHIFT) || (nFlags & MK_CONTROL);
		const bool bNoRemoveSelection = bAddSelect;

		// check if we are close to the original mouse position
		int halfLength = gViewportPreferences.dragSquareSize / 2;
		CRect rcDrag(m_rMouseDownPos.x, m_rMouseDownPos.y, m_rMouseDownPos.x, m_rMouseDownPos.y);
		InflateRect(rcDrag, halfLength, halfLength);

		if (!PtInRect(rcDrag, point))
			return false;

		// Check if right clicked on object.
		HitContext hitInfo(view);
		if (!view->HitTest(point, hitInfo))
			return false;
		if (!hitInfo.object)
			return false;

		//If we have an hit object and it's already selected we don't need to run selection query code as nothing changes
		bool bSkipSelectionQuery = false;
		if (hitInfo.object && hitInfo.object->IsSelected())
		{
			bSkipSelectionQuery = true;
		}

		if (!bSkipSelectionQuery)
		{
			//on right mouse button we ignore every modifier that can remove from selection (because we need to spawn the context button)
			ApplyMouseSelection(view, hitInfo.object, bNoRemoveSelection, false, bAddSelect);
		}

		CDynamicPopupMenu menu;
		hitInfo.object->OnContextMenu(&menu.GetRoot());

		if (!menu.GetRoot().Empty())
		{
			// suspend highlight change for the duration of the popup...
			m_suspendHighlightChange = true;

			// and resume on close
			menu.SetOnHideFunctor([=]
			{
				if (GetIEditor()->GetLevelEditorSharedState()->GetEditTool() == this)
				{
				  AllowHighlightChange();
				}
			});
			menu.SpawnAtCursor();
		}
		return true;
	}
	return false;
}

bool CObjectMode::OnMButtonDown(CViewport* view, int nFlags, CPoint point)
{
	if (GetIEditor()->GetGameEngine()->GetSimulationMode())
	{
		// Get control key status.
		const bool bAltClick = (nFlags & MK_ALT);
		const bool bCtrlClick = (nFlags & MK_CONTROL);
		const bool bShiftClick = (nFlags & MK_SHIFT);

		if (bCtrlClick)
		{
			// In simulation mode awake objects under the cursor when Ctrl+MButton pressed.
			AwakeObjectAtPoint(view, point);
			return true;
		}
	}
	return false;
}

void CObjectMode::AwakeObjectAtPoint(CViewport* view, CPoint point)
{
	// In simulation mode awake objects under the cursor.
	// Check if double clicked on object.
	HitContext hitInfo(view);
	view->HitTest(point, hitInfo);
	CBaseObject* hitObj = hitInfo.object;
	if (hitObj)
	{
		IPhysicalEntity* pent = hitObj->GetCollisionEntity();
		if (pent)
		{
			pe_action_awake pa;
			pa.bAwake = true;
			pent->Action(&pa);
		}
	}
}

bool CObjectMode::CheckVirtualKey(int virtualKey)
{
	GetAsyncKeyState(virtualKey);
	if (GetAsyncKeyState(virtualKey))
		return true;
	return false;
}

void CObjectMode::HandleMoveSelectionToPosition(Vec3& pos, const CPoint& point, bool overrideSnapToNormal)
{
	GetIEditor()->GetIUndoManager()->Begin();
	// Find center of selection.

	int selectionFlags = ISelectionGroup::eMS_FollowGeometry;
	if (overrideSnapToNormal)
		selectionFlags |= ISelectionGroup::eMS_SnapToNormal;

	// Save current constraints so we can remove and re-add after we've moved objects to position
	// If not then having a constraint on Z, for example, will result in moving to position
	// completely failing
	auto pLevelEditorState = GetIEditor()->GetLevelEditorSharedState();
	auto currentConstraints = pLevelEditorState->GetAxisConstraint();
	pLevelEditorState->SetAxisConstraint(CLevelEditorSharedState::Axis::XY);

	// Save the current positions, move tool relies on them - change from relying on undo system.
	auto pSelection = GetIEditor()->GetISelectionGroup();
	pSelection->FilterParents();
	pSelection->SaveFilteredTransform();

	pSelection->Move(pos - pSelection->GetCenter(), selectionFlags, point, true);

	pSelection->FinishChanges();

	pLevelEditorState->SetAxisConstraint(currentConstraints);

	GetIEditor()->GetIUndoManager()->Accept("Move Selection");
}

bool CObjectMode::OnMouseMove(CViewport* view, int nFlags, CPoint point)
{
	if (GetIEditor()->IsInGameMode())
	{
		// Ignore while in game.
		return true;
	}
	const bool bAltClick = (nFlags & MK_ALT);
	const bool bCtrlClick = (nFlags & MK_CONTROL);
	const bool bShiftClick = (nFlags & MK_SHIFT);

	bool bSomethingDone = false;

	bool bAdding = false;
	bool bRemoving = false;

	m_openContext = false;

	// get current axis constrains.
	if (GetCommandMode() == MoveMode)
	{
		if (!m_bDragThresholdExceeded)
		{
			int halfLength = gViewportPreferences.dragSquareSize / 2;
			CRect rcDrag(m_cMouseDownPos.x, m_cMouseDownPos.y, m_cMouseDownPos.x, m_cMouseDownPos.y);
			InflateRect(rcDrag, halfLength, halfLength);

			if (!PtInRect(rcDrag, point))
			{
				// Save the current positions, move tool relies on them - change from relying on undo system.
				GetIEditor()->GetISelectionGroup()->FilterParents();
				GetIEditor()->GetISelectionGroup()->SaveFilteredTransform();
			}
			else
			{
				return true;
			}
		}

		Vec3 v;
		Vec3 p1 = m_mouseDownWorldPos;
		if (gSnappingPreferences.IsSnapToTerrainEnabled() && GetIEditor()->GetLevelEditorSharedState()->GetAxisConstraint() != CLevelEditorSharedState::Axis::Z)
		{
			Vec3 p2 = view->SnapToGrid(view->ViewToWorld(point));
			v = view->GetCPVector(p1, p2);
			v.z = 0;
		}
		else
		{
			Vec3 p2 = view->MapViewToCP(point);
			if (p1.IsZero() || p2.IsZero())
				return true;

			v = view->GetCPVector(p1, p2);
		}

		int selectionFlags = ISelectionGroup::eMS_None;
		if (gSnappingPreferences.IsSnapToTerrainEnabled() && GetIEditor()->GetLevelEditorSharedState()->GetAxisConstraint() != CLevelEditorSharedState::Axis::Z)
			selectionFlags = ISelectionGroup::eMS_FollowTerrain;

		if (gSnappingPreferences.IsSnapToGeometryEnabled())
			selectionFlags |= ISelectionGroup::eMS_FollowGeometry;

		if (gSnappingPreferences.IsSnapToNormalEnabled())
			selectionFlags |= ISelectionGroup::eMS_SnapToNormal;

		if ((nFlags & MK_CONTROL) && !(nFlags & MK_SHIFT))
			selectionFlags = ISelectionGroup::eMS_FollowGeometry | ISelectionGroup::eMS_SnapToNormal;

		if (!v.IsEquivalent(Vec3(0, 0, 0)))
			m_bTransformChanged = true;

#pragma message("TODO")
		//CTrackViewSequenceNoNotificationContext context(pSequence);

		if (m_bDragThresholdExceeded)
		{
			// All moved objects should have pushed an undo after moving, so suspend here
			// if we don't we'll end up with a huge undo step with duplicate entries for the objects.
			// TODO: improve undo system to only push when really necessary.
			GetIEditor()->GetIUndoManager()->Suspend();
		}

		GetIEditor()->GetISelectionGroup()->Move(v, selectionFlags, point, true);

		if (m_bDragThresholdExceeded)
		{
			GetIEditor()->GetIUndoManager()->Resume();
		}

		m_bDragThresholdExceeded = true;
		bSomethingDone = true;
	}
	else if (GetCommandMode() == RotateMode)
	{
		GetIEditor()->GetIUndoManager()->Restore();

		Ang3 ang(0, 0, 0);
		float ax = point.x - m_cMouseDownPos.x;
		float ay = point.y - m_cMouseDownPos.y;
		switch (GetIEditor()->GetLevelEditorSharedState()->GetAxisConstraint())
		{
		case CLevelEditorSharedState::Axis::X:
			ang.x = ay;
			break;
		case CLevelEditorSharedState::Axis::Y:
			ang.y = ay;
			break;
		case CLevelEditorSharedState::Axis::Z:
			ang.z = ay;
			break;
		case CLevelEditorSharedState::Axis::XY:
			ang(ax, ay, 0);
			break;
		case CLevelEditorSharedState::Axis::XZ:
			ang(ax, 0, ay);
			break;
		case CLevelEditorSharedState::Axis::YZ:
			ang(0, ay, ax);
			break;
		}

		ang = gSnappingPreferences.SnapAngle(ang);

		if (!ang.IsEquivalent(Ang3(0, 0, 0)))
			m_bTransformChanged = true;

		GetIEditor()->GetISelectionGroup()->Rotate(ang);
		bSomethingDone = true;
	}
	else if (GetCommandMode() == ScaleMode)
	{
		Vec3 scale;

		GetScale(view, point, scale);

		if (!m_bTransformChanged)
		{
			if (!scale.IsEquivalent(Vec3(0, 0, 0)))
			{
				// Save the current positions, scale tool relies on them - change from relying on undo system.
				GetIEditor()->GetISelectionGroup()->FilterParents();
				GetIEditor()->GetISelectionGroup()->SaveFilteredTransform();
			}
			else
			{
				return true;
			}
		}

		if (m_bTransformChanged)
		{
			GetIEditor()->GetIUndoManager()->Suspend();
		}
		GetIEditor()->GetISelectionGroup()->Scale(scale);
		if (m_bTransformChanged)
		{
			GetIEditor()->GetIUndoManager()->Resume();
		}
		m_bTransformChanged = true;
		bSomethingDone = true;
	}
	else if (GetCommandMode() == SelectMode)
	{
		// Ignore select when selection locked.
		if (GetIEditor()->IsSelectionLocked())
			return true;

		if (bShiftClick)
		{
			bAdding = true;
		}
		else if (bAltClick)
		{
			bRemoving = true;
		}

		CRect rc(m_cMouseDownPos, point);
		if (GetIEditor()->GetLevelEditorSharedState()->GetEditMode() == CLevelEditorSharedState::EditMode::SelectArea)
		{
			view->OnDragSelectRectangle(CPoint(rc.left, rc.top), CPoint(rc.right, rc.bottom), false);
		}
		else
		{
			view->SetSelectionRectangle(rc.TopLeft(), rc.BottomRight());
		}
		//else
		//OnDragSelectRectangle( CPoint(rc.left,rc.top),CPoint(rc.right,rc.bottom),true );

		bSomethingDone = true;
	}

	if (!(nFlags & MK_RBUTTON || nFlags & MK_MBUTTON))
	{
		const IObjectManager::ESelectOp selectOp = bAdding ? IObjectManager::ESelectOp::eSelect :
		                                           (bRemoving ? IObjectManager::ESelectOp::eUnselect : IObjectManager::ESelectOp::eNone);

		// Track mouse movements.
		CGizmo* highlightedGizmo = GetIEditor()->GetGizmoManager()->GetHighlightedGizmo();

		HitContext hitInfo(view);
		if (!highlightedGizmo && view->HitTest(point, hitInfo))
		{
			SetObjectCursor(view, hitInfo.object, selectOp);
		}
		else
		{
			SetObjectCursor(view, nullptr, selectOp);
		}

		HandleMoveByFaceNormal(hitInfo);
	}
	else
	{
		// pan/rotate case
		SetObjectCursor(view, nullptr, IObjectManager::ESelectOp::eNone);
	}

	if ((nFlags & MK_MBUTTON) && GetIEditor()->GetGameEngine()->GetSimulationMode())
	{
		// Get control key status.
		if (bCtrlClick)
		{
			// In simulation mode awake objects under the cursor when Ctrl+MButton pressed.
			AwakeObjectAtPoint(view, point);
		}
	}

	return bSomethingDone;
}

void CObjectMode::SetObjectCursor(CViewport* view, CBaseObject* hitObj, IObjectManager::ESelectOp selectMode)
{
	EStdCursor cursor = STD_CURSOR_DEFAULT;
	string m_cursorStr;

	CBaseObject* pMouseOverObject = NULL;
	if (m_MouseOverObject != CryGUID::Null() && !m_suspendHighlightChange)
	{
		pMouseOverObject = GetIEditor()->GetObjectManager()->FindObject(m_MouseOverObject);
	}

	//HCURSOR hPrevCursor = m_hCurrCursor;
	if (pMouseOverObject)
	{
		pMouseOverObject->SetHighlight(false);
	}

	if (!m_suspendHighlightChange)
	{
		if (hitObj)
			m_MouseOverObject = hitObj->GetId();
		else
			m_MouseOverObject = CryGUID::Null();
		pMouseOverObject = hitObj;
	}

	bool bHitSelectedObject = false;
	if (pMouseOverObject)
	{
		if (GetCommandMode() != SelectMode && !GetIEditor()->IsSelectionLocked())
		{
			if (pMouseOverObject->CanBeHightlighted() && view->GetHelperSettings().enabled)
				pMouseOverObject->SetHighlight(true);

			m_cursorStr = pMouseOverObject->GetName();

			string comment(pMouseOverObject->GetComment());
			if (!comment.IsEmpty())
			{
				m_cursorStr += "\n";
				m_cursorStr += comment;
			}

			if (view->GetHelperSettings().enabled && view->GetHelperSettings().showMeshStatsOnMouseOver)
			{
				const string triangleCountText = pMouseOverObject->GetMouseOverStatisticsText();

				if (!triangleCountText.IsEmpty())
				{
					m_cursorStr += triangleCountText;
				}
			}

			string warnings(pMouseOverObject->GetWarningsText());
			if (!warnings.IsEmpty())
			{
				m_cursorStr += warnings;
			}

			cursor = STD_CURSOR_HIT;
			if (pMouseOverObject->IsSelected())
				bHitSelectedObject = true;
		}
	}
	else
	{
		m_cursorStr = "";
		cursor = STD_CURSOR_DEFAULT;
	}

	const bool bAddSelect = (selectMode == IObjectManager::ESelectOp::eSelect);
	const bool bUnselect = (selectMode == IObjectManager::ESelectOp::eUnselect);
	const bool bNoRemoveSelection = bAddSelect || bUnselect;

	bool bLockSelection = GetIEditor()->IsSelectionLocked();

	if (GetCommandMode() == SelectMode || GetCommandMode() == NothingMode)
	{
		if (bAddSelect)
			cursor = STD_CURSOR_SEL_PLUS;
		if (bUnselect)
			cursor = STD_CURSOR_SEL_MINUS;

		if ((bHitSelectedObject && !bNoRemoveSelection) || bLockSelection)
		{
			CLevelEditorSharedState::EditMode editMode = GetIEditor()->GetLevelEditorSharedState()->GetEditMode();
			if (editMode == CLevelEditorSharedState::EditMode::Move)
			{
				cursor = STD_CURSOR_MOVE;
			}
			else if (editMode == CLevelEditorSharedState::EditMode::Rotate)
			{
				cursor = STD_CURSOR_ROTATE;
			}
			else if (editMode == CLevelEditorSharedState::EditMode::Scale)
			{
				cursor = STD_CURSOR_SCALE;
			}
		}
	}
	else if (GetCommandMode() == MoveMode)
	{
		cursor = STD_CURSOR_MOVE;
	}
	else if (GetCommandMode() == RotateMode)
	{
		cursor = STD_CURSOR_ROTATE;
	}
	else if (GetCommandMode() == ScaleMode)
	{
		cursor = STD_CURSOR_SCALE;
	}

	view->SetCurrentCursor(cursor, m_cursorStr);
}

void CObjectMode::HitTest(HitContext& hitContext, CViewport* pView, const CPoint& point)
{
	// Check deep selection mode activated
	// The Deep selection has two mode.
	// The normal mode pops the context menu, another is the cyclic selection on clinking.
	bool bTabPressed = CheckVirtualKey(VK_TAB);
	bool bZKeyPressed = CheckVirtualKey('Z');

	CDeepSelection::EDeepSelectionMode dsMode =
		(bTabPressed ? (bZKeyPressed ? CDeepSelection::DSM_POP : CDeepSelection::DSM_CYCLE) : CDeepSelection::DSM_NONE);

	if (dsMode == CDeepSelection::DSM_POP)
	{
		m_pDeepSelection->Reset(true);
		m_pDeepSelection->SetMode(dsMode);
		hitContext.pDeepSelection = m_pDeepSelection;
	}
	else if (dsMode == CDeepSelection::DSM_CYCLE)
	{
		if (!m_pDeepSelection->OnCycling(point))
		{
			// Start of the deep selection cycling mode.
			m_pDeepSelection->Reset(false);
			m_pDeepSelection->SetMode(dsMode);
			hitContext.pDeepSelection = m_pDeepSelection;
		}
	}
	else
	{
		if (m_pDeepSelection->GetPreviousMode() == CDeepSelection::DSM_NONE)
			m_pDeepSelection->Reset(true);

		m_pDeepSelection->SetMode(CDeepSelection::DSM_NONE);
		hitContext.pDeepSelection = 0;
	}

	if (pView->HitTest(point, hitContext))
	{
		CheckDeepSelection(hitContext, pView);
	}
}

void CObjectMode::CheckDeepSelection(HitContext& hitContext, CViewport* pWnd)
{
	if (hitContext.pDeepSelection)
	{
		m_pDeepSelection->CollectCandidate(hitContext.dist, gDeepSelectionPreferences.deepSelectionRange);
	}

	if (m_pDeepSelection->GetCandidateObjectCount() > 1)
	{
		// Deep Selection Pop Mode
		if (m_pDeepSelection->GetMode() == CDeepSelection::DSM_POP)
		{
			CMenu popUpDeepSelect;
			popUpDeepSelect.CreatePopupMenu();

			for (int i = 0; i < m_pDeepSelection->GetCandidateObjectCount(); ++i)
			{
				popUpDeepSelect.AppendMenu(MF_STRING, i + 1, m_pDeepSelection->GetCandidateObject(i)->GetName());
			}

			CPoint p;
			::GetCursorPos(&p);
			int nSelect = popUpDeepSelect.TrackPopupMenu(TPM_NONOTIFY | TPM_RETURNCMD | TPM_CENTERALIGN, p.x, p.y, CWnd::FromHandle((HWND)pWnd->GetSafeHwnd()), NULL);

			if (nSelect > 0)
			{
				// Update HitContext hitInfo.
				hitContext.object = m_pDeepSelection->GetCandidateObject(nSelect - 1);
				m_pDeepSelection->ExcludeHitTest(nSelect - 1);
			}
		}
		else if (m_pDeepSelection->GetMode() == CDeepSelection::DSM_CYCLE)
		{
			int selPos = m_pDeepSelection->GetCurrentSelectPos();
			hitContext.object = m_pDeepSelection->GetCandidateObject(selPos + 1);
			m_pDeepSelection->ExcludeHitTest(selPos + 1);
		}
	}
}

Vec3& CObjectMode::GetScale(const CViewport* view, const CPoint& point, Vec3& OutScale)
{
	float ay = 1.0f - 0.01f * (point.y - m_cMouseDownPos.y);

	if (ay < 0.01f) ay = 0.01f;

	Vec3 scl(ay, ay, ay);

	CLevelEditorSharedState::Axis axisConstraint = GetIEditor()->GetLevelEditorSharedState()->GetAxisConstraint();

	switch (axisConstraint)
	{
	case CLevelEditorSharedState::Axis::X:
		scl(ay, 1, 1);
		break;
	case CLevelEditorSharedState::Axis::Y:
		scl(1, ay, 1);
		break;
	case CLevelEditorSharedState::Axis::Z:
		scl(1, 1, ay);
		break;
	case CLevelEditorSharedState::Axis::XY:
		scl(ay, ay, ay);
		break;
	case CLevelEditorSharedState::Axis::XZ:
		scl(ay, ay, ay);
		break;
	case CLevelEditorSharedState::Axis::YZ:
		scl(ay, ay, ay);
		break;
	case CLevelEditorSharedState::Axis::XYZ:
		scl(ay, ay, ay);
		break;
	}

	if (gSnappingPreferences.IsSnapToTerrainEnabled())
		scl(ay, ay, ay);

	OutScale = scl;

	return OutScale;
}

void CObjectMode::OnManipulatorBeginDrag(IDisplayViewport* view, ITransformManipulator* pManipulator, const Vec2i& point, int flags)
{
	m_cMouseDownPos = CPoint(point.x, point.y);
	m_bGizmoDrag = true;
	CLevelEditorSharedState::EditMode editMode = GetIEditor()->GetLevelEditorSharedState()->GetEditMode();

	if (editMode == CLevelEditorSharedState::EditMode::Scale || editMode == CLevelEditorSharedState::EditMode::Move)
	{
		const ISelectionGroup* pSelection = GetIEditor()->GetISelectionGroup();
		pSelection->FilterParents();
		pSelection->SaveFilteredTransform();

		// Hack: Only for move mode, set the command mode to move, so that grid shows up in the viewport
		if (editMode == CLevelEditorSharedState::EditMode::Move)
		{
			m_bDragThresholdExceeded = false;
		}
	}

	if (editMode == CLevelEditorSharedState::EditMode::Move)
	{
		m_commandMode = MoveMode;
	}
	else if (editMode == CLevelEditorSharedState::EditMode::Scale)
	{
		m_commandMode = ScaleMode;
	}
	else if (editMode == CLevelEditorSharedState::EditMode::Rotate)
	{
		m_commandMode = RotateMode;
	}
}

void CObjectMode::OnManipulatorEndDrag(IDisplayViewport* view, ITransformManipulator* pManipulator)
{
	if (m_commandMode == ScaleMode || m_commandMode == MoveMode)
	{
		if (m_bDragThresholdExceeded)
		{
			const ISelectionGroup* pSelection = GetIEditor()->GetISelectionGroup();
			pSelection->FinishChanges();
			m_bDragThresholdExceeded = false;
		}
	}
	m_commandMode = NothingMode;
	m_bGizmoDrag = false;

	GetIEditor()->UpdateViews(eUpdateObjects);
}

//////////////////////////////////////////////////////////////////////////
// This callback is currently called only to handle the case of the 'move by the face normal'.
// Other movements of the object are handled in the 'CObjectMode::OnMouseMove()' method.
void CObjectMode::OnManipulatorDrag(IDisplayViewport* view, ITransformManipulator* pManipulator, const Vec2i& point, const Vec3& value, int flags)
{
	if (m_commandMode == MoveMode)
	{
		int halfLength = gViewportPreferences.dragSquareSize / 2;
		CRect rcDrag(m_cMouseDownPos.x, m_cMouseDownPos.y, m_cMouseDownPos.x, m_cMouseDownPos.y);
		InflateRect(rcDrag, halfLength, halfLength);

		CPoint p(point.x, point.y);
		if (PtInRect(rcDrag, p))
		{
			return;
		}

		const ISelectionGroup* pSelection = GetIEditor()->GetISelectionGroup();

		if (m_bDragThresholdExceeded)
		{
			// All moved objects should have pushed an undo after moving, so suspend here
			// if we don't we'll end up with a huge undo step with duplicate entries for the objects.
			// TODO: improve undo system to only push when really necessary.
			GetIEditor()->GetIUndoManager()->Suspend();
		}

		int selectionFlags = ISelectionGroup::eMS_None;
		if (gSnappingPreferences.IsSnapToTerrainEnabled())
			selectionFlags = ISelectionGroup::eMS_FollowTerrain;

		if (gSnappingPreferences.IsSnapToNormalEnabled())
			selectionFlags |= ISelectionGroup::eMS_SnapToNormal;

		pSelection->Move(value, selectionFlags, p, true);

		if (m_pHitObject)
			UpdateMoveByFaceNormGizmo(m_pHitObject);

		if (m_bDragThresholdExceeded)
		{
			GetIEditor()->GetIUndoManager()->Resume();
		}
		m_bDragThresholdExceeded = true;
	}
	else if (m_commandMode == RotateMode)
	{
		// Unfortunately more conversion to fit selectiongroup format
		Ang3 angles = RAD2DEG(-value);

		const ISelectionGroup* pSelection = GetIEditor()->GetISelectionGroup();
		pSelection->Rotate(angles);
	}
	else if (m_commandMode == ScaleMode)
	{
		const ISelectionGroup* pSelection = GetIEditor()->GetISelectionGroup();
		pSelection->Scale(value);
		m_bDragThresholdExceeded = true;
	}
}

void CObjectMode::HandleMoveByFaceNormal(HitContext& hitInfo)
{
	CBaseObject* pHitObject = hitInfo.object;
	bool bFaceNormalMovePossible = pHitObject && GetIEditor()->GetLevelEditorSharedState()->GetEditMode() == CLevelEditorSharedState::EditMode::Move
	                               && (pHitObject->GetType() == OBJTYPE_SOLID || pHitObject->GetType() == OBJTYPE_BRUSH)
	                               && pHitObject->IsSelected();
	bool bNKeyPressed = CheckVirtualKey('N');
	if (bFaceNormalMovePossible && bNKeyPressed)
	{
		// Test a hit for its faces.
		hitInfo.nSubObjFlags = SO_HIT_POINT | SO_HIT_SELECT | SO_HIT_NO_EDIT | SO_HIT_ELEM_FACE;
		pHitObject->SetFlags(OBJFLAG_SUBOBJ_EDITING);
		pHitObject->HitTest(hitInfo);
		pHitObject->ClearFlags(OBJFLAG_SUBOBJ_EDITING);

		UpdateMoveByFaceNormGizmo(pHitObject);
	}
	else if (m_bMoveByFaceNormManipShown && !bNKeyPressed)
	{
		HideMoveByFaceNormGizmo();
	}
}

void CObjectMode::UpdateMoveByFaceNormGizmo(CBaseObject* pHitObject)
{
	Matrix34 refFrame;
	refFrame.SetIdentity();
	SubObjectSelectionReferenceFrameCalculator calculator(SO_ELEM_FACE);
	pHitObject->CalculateSubObjectSelectionReferenceFrame(&calculator);
	if (calculator.GetFrame(refFrame) == false)
	{
		HideMoveByFaceNormGizmo();
	}
	else
	{
		if (!m_normalMoveGizmo)
		{
			m_normalMoveGizmo = GetIEditor()->GetGizmoManager()->AddManipulator(&m_normalGizmoOwner);
		}
		m_bMoveByFaceNormManipShown = true;
		m_pHitObject = pHitObject;
		m_normalMoveGizmo->SetCustomTransform(true, refFrame);
	}
}

void CObjectMode::HideMoveByFaceNormGizmo()
{
	if (m_normalMoveGizmo)
	{
		m_normalMoveGizmo->signalBeginDrag.DisconnectAll();
		m_normalMoveGizmo->signalDragging.DisconnectAll();
		m_normalMoveGizmo->signalEndDrag.DisconnectAll();
		GetIEditor()->GetGizmoManager()->RemoveManipulator(m_normalMoveGizmo);
		m_normalMoveGizmo = nullptr;
	}
	m_bMoveByFaceNormManipShown = false;
	m_pHitObject = NULL;
}

// only display the grid when we actually transform something
bool CObjectMode::IsDisplayGrid()
{
	return
	  (!m_bGizmoDrag && m_commandMode != NothingMode)
	  || (m_commandMode == MoveMode);
}
