// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ObjectMode.h"
#include "Viewport.h"
#include <Preferences/ViewportPreferences.h>
#include "ViewManager.h"
#include "./Terrain/Heightmap.h"
#include "GameEngine.h"
#include "Objects/EntityObject.h"
#include "Objects/CameraObject.h"
#include "Controls/DynamicPopupMenu.h"
#include "Objects/BrushObject.h"
#include "DeepSelection.h"
#include "SubObjectSelectionReferenceFrameCalculator.h"
#include "Gizmos/IGizmoManager.h"
#include "Objects/ParticleEffectObject.h"
#include "Objects/PrefabObject.h"
#include "Grid.h"
#include "IUndoManager.h"

/////////////////////////////
// CObjectManipulatorOwner
/////////////////////////////
CObjectManipulatorOwner::CObjectManipulatorOwner(CObjectMode* objectModeTool)
	: m_bIsVisible(true)
	, m_visibilityDirty(true)
{
	m_manipulator = GetIEditorImpl()->GetGizmoManager()->AddManipulator(this);
	m_manipulator->signalBeginDrag.Connect(objectModeTool, &CObjectMode::OnManipulatorBeginDrag);
	m_manipulator->signalDragging.Connect(objectModeTool, &CObjectMode::OnManipulatorDrag);
	m_manipulator->signalEndDrag.Connect(objectModeTool, &CObjectMode::OnManipulatorEndDrag);

	GetIEditorImpl()->GetObjectManager()->signalObjectChanged.Connect(this, &CObjectManipulatorOwner::SingleObjectUpdateCallback);
	GetIEditorImpl()->GetObjectManager()->signalSelectionChanged.Connect(this, &CObjectManipulatorOwner::OnSelectionChanged);

	const CSelectionGroup* pSelection = GetIEditorImpl()->GetSelection();
	int totCount = pSelection->GetCount();

	// register to all selected objects on initialization
	for (int i = 0; i < totCount; ++i)
	{
		CBaseObject* pObj = pSelection->GetObject(i);
		pObj->AddEventListener(functor(*this, &CObjectManipulatorOwner::ObjectUpdateCallback));
	}
}

CObjectManipulatorOwner::~CObjectManipulatorOwner()
{
	GetIEditorImpl()->GetObjectManager()->signalObjectChanged.DisconnectObject(this);
	GetIEditorImpl()->GetObjectManager()->signalSelectionChanged.DisconnectObject(this);

	m_manipulator->signalBeginDrag.DisconnectAll();
	m_manipulator->signalDragging.DisconnectAll();
	m_manipulator->signalEndDrag.DisconnectAll();
	GetIEditorImpl()->GetGizmoManager()->RemoveManipulator(m_manipulator);

	m_manipulator = nullptr;

	const CSelectionGroup* pSelection = GetIEditorImpl()->GetSelection();
	int totCount = pSelection->GetCount();

	// unregister from all selected objects
	for (int i = 0; i < totCount; ++i)
	{
		CBaseObject* pObj = pSelection->GetObject(i);
		pObj->RemoveEventListener(functor(*this, &CObjectManipulatorOwner::ObjectUpdateCallback));
	}
}

bool CObjectManipulatorOwner::GetManipulatorMatrix(RefCoordSys coordSys, Matrix34& tm)
{
	const CSelectionGroup* pSelection = GetIEditorImpl()->GetSelection();
	return pSelection->GetManipulatorMatrix(coordSys, tm);
}

void CObjectManipulatorOwner::GetManipulatorPosition(Vec3& position)
{
	const CSelectionGroup* pSelection = GetIEditorImpl()->GetSelection();

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

void CObjectManipulatorOwner::ObjectUpdateCallback(CBaseObject* pObj, int evt)
{
	if (evt == OBJECT_ON_TRANSFORM)
	{
		// tag for update next cycle
		m_manipulator->Invalidate();
	}
	else if (evt == OBJECT_ON_VISIBILITY)
	{
		m_manipulator->Invalidate();
		m_visibilityDirty = true;
	}
}

void CObjectManipulatorOwner::SingleObjectUpdateCallback(CObjectEvent& event)
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
		pObject->RemoveEventListener(functor(*this, &CObjectManipulatorOwner::ObjectUpdateCallback));
	}

	for (auto& pObject : selected)
	{
		pObject->AddEventListener(functor(*this, &CObjectManipulatorOwner::ObjectUpdateCallback));
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
	const CSelectionGroup* pSelection = GetIEditorImpl()->GetSelection();
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
CObjectMode::CObjectMode()
	: m_objectManipulatorOwner(this)
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

//////////////////////////////////////////////////////////////////////////
CObjectMode::~CObjectMode()
{
	CBaseObject* pMouseOverObject = nullptr;
	if (m_MouseOverObject != CryGUID::Null())
	{
		pMouseOverObject = GetIEditorImpl()->GetObjectManager()->FindObject(m_MouseOverObject);
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
		GetIEditorImpl()->GetGizmoManager()->RemoveManipulator(m_normalMoveGizmo);
		m_normalMoveGizmo = nullptr;
	}
}

void CObjectMode::DrawSelectionPreview(struct DisplayContext& dc, CBaseObject* drawObject)
{
	int childColVal = 0;

	AABB bbox;
	drawObject->GetBoundBox(bbox);

	// If CGroup/CPrefabObject
	if (drawObject->GetChildCount() > 0)
	{
		// Draw object name label on top of object
		Vec3 vTopEdgeCenterPos = bbox.GetCenter();

		dc.SetColor(gViewportSelectionPreferences.colorGroupBBox);
		vTopEdgeCenterPos(vTopEdgeCenterPos.x, vTopEdgeCenterPos.y, bbox.max.z);
		dc.DrawTextLabel(vTopEdgeCenterPos, 1.3f, drawObject->GetName());
		// Draw bounding box wireframe
		dc.DrawWireBox(bbox.min, bbox.max);
	}
	else
	{
		dc.SetColor(Vec3(1, 1, 1));
		dc.DrawTextLabel(ConvertToTextPos(bbox.GetCenter(), Matrix34::CreateIdentity(), dc.view, dc.flags & DISPLAY_2D), 1, drawObject->GetName());
	}

	// Object Geometry Highlight

	// Default, CBrush object
	ColorB selColor = gViewportSelectionPreferences.geometryHighlightColor;

	// CDesignerBrushObject
	if (drawObject->GetType() == OBJTYPE_SOLID)
		selColor = gViewportSelectionPreferences.solidBrushGeometryColor;

	// In case it is a child object, use a different alpha value
	if (drawObject->GetParent())
		selColor.a = (uint8)(gViewportSelectionPreferences.childObjectGeomAlpha * 255);

	// Draw geometry in custom color
	SGeometryDebugDrawInfo dd;
	dd.tm = drawObject->GetWorldTM();
	dd.color = selColor;
	dd.lineColor = selColor;
	dd.bDrawInFront = true;

	if (drawObject->IsKindOf(RUNTIME_CLASS(CGroup)) || drawObject->IsKindOf(RUNTIME_CLASS(CPrefabObject)))
	{
		CGroup* paintObj = (CGroup*)drawObject;

		dc.DepthTestOff();

		if (drawObject->GetClassDesc()->GetRuntimeClass() == RUNTIME_CLASS(CPrefabObject))
			dc.SetColor(gViewportSelectionPreferences.colorPrefabBBox);
		else
			dc.SetColor(gViewportSelectionPreferences.colorGroupBBox);

		dc.DrawSolidBox(bbox.min, bbox.max);
		dc.DepthTestOn();
	}
	else if (drawObject->IsKindOf(RUNTIME_CLASS(CBrushObject)))
	{
		if (!(dc.flags & DISPLAY_2D))
		{
			CBrushObject* paintObj = (CBrushObject*)drawObject;
			IStatObj* pStatObj = paintObj->GetIStatObj();
			if (pStatObj)
				pStatObj->DebugDraw(dd);
		}
	}
	else if (drawObject->GetType() == OBJTYPE_SOLID)
	{
		if (!(dc.flags & DISPLAY_2D))
		{
			IStatObj* pStatObj = drawObject->GetIStatObj();
			if (pStatObj)
				pStatObj->DebugDraw(dd);
		}
	}
	else if (drawObject->IsKindOf(RUNTIME_CLASS(CEntityObject)))
	{
		dc.DepthTestOff();
		dc.SetColor(gViewportSelectionPreferences.colorEntityBBox);
		dc.DrawSolidBox(bbox.min, bbox.max);
		dc.DepthTestOn();
	}

	// Highlight also children objects if this object is opened
	if (drawObject->GetChildCount() > 0)
	{
		CGroup* group = (CGroup*)drawObject;
		if (!group->IsOpen())
			return;

		for (int gNo = 0; gNo < drawObject->GetChildCount(); ++gNo)
		{
			if (std::find(m_PreviewGUIDs.begin(), m_PreviewGUIDs.end(), drawObject->GetChild(gNo)->GetId()) == m_PreviewGUIDs.end())
				DrawSelectionPreview(dc, drawObject->GetChild(gNo));
		}
	}
}

void CObjectMode::DisplaySelectionPreview(struct DisplayContext& dc)
{
	CViewport* view = GetIEditorImpl()->GetViewManager()->GetActiveViewport();
	if (!view)
		return;

	IObjectManager* objMan = GetIEditorImpl()->GetObjectManager();

	CRect rc = view->GetSelectionRectangle();

	if (GetCommandMode() == SelectMode)
	{
		if (rc.Width() > 1 && rc.Height() > 1)
		{
			GetIEditorImpl()->GetObjectManager()->FindObjectsInRect(view, rc, m_PreviewGUIDs);

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
				CBaseObject* pObject = GetIEditorImpl()->GetObjectManager()->FindObject(m_PreviewGUIDs[i]);

				if (!pObject)
					continue;

				if (pObject->GetType() & ~gViewportSelectionPreferences.objectSelectMask)
					continue;

				DrawSelectionPreview(dc, pObject);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CObjectMode::Display(struct DisplayContext& dc)
{
	// Selection Candidates Preview
	DisplaySelectionPreview(dc);
}

//////////////////////////////////////////////////////////////////////////
bool CObjectMode::MouseCallback(CViewport* view, EMouseEvent event, CPoint& point, int flags)
{
	switch (event)
	{
	case eMouseLDown:
		return OnLButtonDown(view, flags, point);
		break;
	case eMouseLUp:
		return OnLButtonUp(view, flags, point);
		break;
	case eMouseLDblClick:
		return OnLButtonDblClk(view, flags, point);
		break;
	case eMouseRDown:
		return OnRButtonDown(view, flags, point);
		break;
	case eMouseRUp:
		return OnRButtonUp(view, flags, point);
		break;
	case eMouseMove:
	case eMouseEnter:
		return OnMouseMove(view, flags, point);
		break;
	case eMouseMDown:
		return OnMButtonDown(view, flags, point);
		break;
	case eMouseFocusLeave:
	case eMouseLeave:
		SetObjectCursor(view, 0, IObjectManager::ESelectOp::eNone);
		return true;
		break;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CObjectMode::OnKeyDown(CViewport* view, uint32 nChar, uint32 nRepCnt, uint32 nFlags)
{
	if (nChar == Qt::Key_Escape)
	{
		GetIEditorImpl()->ClearSelection();
		if (GetIEditorImpl()->GetEditMode() == eEditModeSelectArea)
			GetIEditorImpl()->SetEditMode(eEditModeSelect);
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CObjectMode::OnKeyUp(CViewport* view, uint32 nChar, uint32 nRepCnt, uint32 nFlags)
{
	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CObjectMode::OnLButtonDown(CViewport* view, int nFlags, CPoint point)
{
	if (m_bMoveByFaceNormManipShown)
	{
		HideMoveByFaceNormGizmo();
	}

	// CPointF ptMarker;
	CPoint ptCoord;
	int iCurSel = -1;

	if (GetIEditorImpl()->IsInGameMode())
	{
		// Ignore clicks while in game.
		return false;
	}

	// Save the mouse down position
	m_cMouseDownPos = point;

	m_bDragThresholdExceeded = false;

	view->ResetSelectionRegion();

	Vec3 pos = view->SnapToGrid(view->ViewToWorld(point));
	int editMode = GetIEditorImpl()->GetEditMode();

	// Show marker position in the status bar
	//cry_sprintf(szNewStatusText, "X:%g Y:%g Z:%g",pos.x,pos.y,pos.z );

	// Swap X/Y
	float unitSize = 1;
	CHeightmap* pHeightmap = GetIEditorImpl()->GetHeightmap();
	if (pHeightmap)
		unitSize = pHeightmap->GetUnitSize();
	float hx = pos.y / unitSize;
	float hy = pos.x / unitSize;
	float hz = GetIEditorImpl()->GetTerrainElevation(pos.x, pos.y);

	// Get control key status.
	const bool bAltClick = (nFlags & MK_ALT);
	const bool bCtrlClick = (nFlags & MK_CONTROL);
	const bool bShiftClick = (nFlags & MK_SHIFT);

	const bool bAddSelect = bShiftClick;
	const bool bToggle = bCtrlClick;
	// Alt click might be beginning of marque deselection - so we must not remove selection
	const bool bNoRemoveSelection = bAddSelect || bToggle || bAltClick;

	// Check deep selection mode activated
	// The Deep selection has two mode.
	// The normal mode pops the context menu, another is the cyclic selection on clinking.
	bool bTabPressed = CheckVirtualKey(VK_TAB);
	bool bZKeyPressed = CheckVirtualKey('Z');

	CDeepSelection::EDeepSelectionMode dsMode =
	  (bTabPressed ? (bZKeyPressed ? CDeepSelection::DSM_POP : CDeepSelection::DSM_CYCLE) : CDeepSelection::DSM_NONE);

	bool bLockSelection = GetIEditorImpl()->IsSelectionLocked();

	int numUnselected = 0;
	int numSelected = 0;

	//	m_activeAxis = 0;

	HitContext hitInfo;
	hitInfo.view = view;

	if (dsMode == CDeepSelection::DSM_POP)
	{
		m_pDeepSelection->Reset(true);
		m_pDeepSelection->SetMode(dsMode);
		hitInfo.pDeepSelection = m_pDeepSelection;
	}
	else if (dsMode == CDeepSelection::DSM_CYCLE)
	{
		if (!m_pDeepSelection->OnCycling(point))
		{
			// Start of the deep selection cycling mode.
			m_pDeepSelection->Reset(false);
			m_pDeepSelection->SetMode(dsMode);
			hitInfo.pDeepSelection = m_pDeepSelection;
		}
	}
	else
	{
		if (m_pDeepSelection->GetPreviousMode() == CDeepSelection::DSM_NONE)
			m_pDeepSelection->Reset(true);

		m_pDeepSelection->SetMode(CDeepSelection::DSM_NONE);
		hitInfo.pDeepSelection = 0;

	}

	if (view->HitTest(point, hitInfo))
	{
		if (hitInfo.axis != 0)
		{
			GetIEditorImpl()->SetAxisConstrains((AxisConstrains)hitInfo.axis);
			// if edit mode is set to selection, then we treat gizmo as a selection component and we should not lock the selection
			if (editMode != eEditModeSelect)
			{
				bLockSelection = true;
			}
			view->SetAxisConstrain(hitInfo.axis);
		}

		//////////////////////////////////////////////////////////////////////////
		// Deep Selection
		CheckDeepSelection(hitInfo, view);
	}

	CBaseObject* hitObj = hitInfo.object;

	RefCoordSys coordSys = GetIEditorImpl()->GetReferenceCoordSys();
	Vec3 gridPosition = (hitObj) ? hitObj->GetWorldPos() : pos;

	if (coordSys == COORDS_USERDEFINED)
	{
		Matrix34 userTM = gSnappingPreferences.GetMatrix();
		userTM.SetTranslation(gridPosition);
		view->SetConstructionMatrix(userTM);
	}
	else if (coordSys == COORDS_WORLD || !hitObj)
	{
		Matrix34 tm = Matrix34::CreateIdentity();
		tm.SetTranslation(gridPosition);
		view->SetConstructionMatrix(tm);
	}
	else if (coordSys == COORDS_PARENT)
	{
		if (hitInfo.object->GetParent())
		{
			Matrix34 parentTM = hitInfo.object->GetParent()->GetWorldTM();
			parentTM.OrthonormalizeFast();
			parentTM.SetTranslation(gridPosition);
			view->SetConstructionMatrix(parentTM);
		}
		else
		{
			view->SetConstructionMatrix(hitInfo.object->GetWorldTM());
		}
	}
	else if (coordSys == COORDS_LOCAL)
	{
		view->SetConstructionMatrix(hitInfo.object->GetWorldTM());
	}

	if (GetIEditorImpl()->IsSnapToTerrainEnabled() && view->GetAxisConstrain() != AXIS_Z)
	{
		m_mouseDownWorldPos = view->ViewToWorld(point);
	}
	else
	{
		m_mouseDownWorldPos = view->MapViewToCP(point);
	}

	if (editMode != eEditModeTool)
	{
		// Check for Move to position.
		if (bCtrlClick && bShiftClick)
		{
			// Ctrl-Click on terrain will move selected objects to specified location.
			MoveSelectionToPos(view, pos, bAltClick, point);
			bLockSelection = true;
		}
	}

	if (editMode == eEditModeMove)
	{
		if (!bNoRemoveSelection)
			SetCommandMode(MoveMode);

		if (hitObj && hitObj->IsSelected() && !bNoRemoveSelection)
			bLockSelection = true;
	}
	else if (editMode == eEditModeRotate)
	{
		if (!bNoRemoveSelection)
			SetCommandMode(RotateMode);
		if (hitObj && hitObj->IsSelected() && !bNoRemoveSelection)
			bLockSelection = true;
	}
	else if (editMode == eEditModeScale)
	{
		if (!bNoRemoveSelection)
		{
			SetCommandMode(ScaleMode);
		}

		if (hitObj && hitObj->IsSelected() && !bNoRemoveSelection)
			bLockSelection = true;
	}
	else if (hitObj != 0 && GetIEditorImpl()->GetSelectedObject() == hitObj && !bAddSelect && !bToggle)
	{
		bLockSelection = true;
	}

	if (!bLockSelection)
	{
		// If not selection locked.
		view->BeginUndo();

		IObjectManager* pObjectManager = GetIEditorImpl()->GetObjectManager();

		if (!bNoRemoveSelection)
		{
			// Current selection should be cleared
			numSelected = pObjectManager->GetSelection()->GetCount();
			pObjectManager->ClearSelection();
		}

		if (hitObj)
		{
			numSelected = 1;

			if (!bToggle)
			{
				pObjectManager->SelectObject(hitObj);
			}
			else
			{
				if (hitObj->IsSelected())
				{
					pObjectManager->UnselectObject(hitObj);
				}
				else
				{
					pObjectManager->SelectObject(hitObj);
				}
			}
		}
		if (view->IsUndoRecording())
		{
			view->AcceptUndo("Select Object(s)");
		}

		if (numSelected == 0 || editMode == eEditModeSelect || editMode == eEditModeSelectArea)
		{
			// If object is not selected.
			// Capture mouse input for this window.
			SetCommandMode(SelectMode);
		}
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

//////////////////////////////////////////////////////////////////////////
bool CObjectMode::OnLButtonUp(CViewport* view, int nFlags, CPoint point)
{
	if (GetIEditorImpl()->IsInGameMode())
	{
		// Ignore clicks while in game.
		return true;
	}

	if (m_bTransformChanged)
	{
		const CSelectionGroup* pSelection = GetIEditorImpl()->GetSelection();
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

	if (GetCommandMode() == SelectMode && (!GetIEditorImpl()->IsSelectionLocked()))
	{
		const bool bUnselect = (nFlags & MK_ALT);
		const bool bToggle = (nFlags & MK_CONTROL);

		const IObjectManager::ESelectOp selectOp = bUnselect ? IObjectManager::ESelectOp::eUnselect :
		                                           (bToggle ? IObjectManager::ESelectOp::eToggle : IObjectManager::ESelectOp::eSelect);

		CRect selectRect = view->GetSelectionRectangle();
		if (!selectRect.IsRectEmpty())
		{
			// Ignore too small rectangles.
			if (selectRect.Width() > 5 && selectRect.Height() > 5)
			{
				GetIEditorImpl()->GetObjectManager()->SelectObjectsInRect(view, selectRect, selectOp);
			}
		}

		if (GetIEditorImpl()->GetEditMode() == eEditModeSelectArea)
		{
			AABB box;
			GetIEditorImpl()->GetSelectedRegion(box);

			//////////////////////////////////////////////////////////////////////////
			GetIEditorImpl()->ClearSelection();

			/*
			   SEntityProximityQuery query;
			   query.box = box;
			   gEnv->pEntitySystem->QueryProximity( query );
			   for (int i = 0; i < query.nCount; i++)
			   {
			   IEntity *pIEntity = query.pEntities[i];
			   CEntityObject *pEntity = CEntityObject::FindFromEntityId( pIEntity->GetId() );
			   if (pEntity)
			   {
			    GetIEditorImpl()->GetObjectManager()->SelectObject( pEntity );
			   }
			   }
			 */
			//////////////////////////////////////////////////////////////////////////
			/*

			   if (fabs(box.min.x-box.max.x) > 0.5f && fabs(box.min.y-box.max.y) > 0.5f)
			   {
			   //@FIXME: restore it later.
			   //Timur[1/14/2003]
			   //SelectRectangle( box,!bUnselect );
			   //SelectObjectsInRect( m_selectedRect,!bUnselect );
			   GetIEditorImpl()->GetObjectManager()->SelectObjects( box,bUnselect );
			   GetIEditorImpl()->UpdateViews(eUpdateObjects);
			   }
			 */
		}
	}

	// If command mode is still "NothingMode" this object was just created and placed and needs this update too.
	if (GetCommandMode() == ScaleMode || GetCommandMode() == MoveMode || GetCommandMode() == RotateMode || GetCommandMode() == NothingMode)
	{
		m_suspendHighlightChange = false;
		GetIEditorImpl()->GetObjectManager()->GetSelection()->ObjectModified();
	}

	if (GetIEditorImpl()->GetEditMode() != eEditModeSelectArea)
	{
		view->ResetSelectionRegion();
	}
	// Reset selected rectangle.
	view->SetSelectionRectangle(CPoint(0, 0), CPoint(0, 0));

	// Restore default editor axis constrain.
	if (GetIEditorImpl()->GetAxisConstrains() != view->GetAxisConstrain())
	{
		view->SetAxisConstrain(GetIEditorImpl()->GetAxisConstrains());
	}

	SetCommandMode(NothingMode);

	return true;
}

//////////////////////////////////////////////////////////////////////////
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
			float height = p.z - GetIEditorImpl()->GetTerrainElevation(p.x, p.y);
			if (height < 1) height = 1;
			p.x = v.x;
			p.y = v.y;
			p.z = GetIEditorImpl()->GetTerrainElevation(p.x, p.y) + height;
			tm.SetTranslation(p);
			view->SetViewTM(tm);
		}
	}
	else
	{
		// Check if double clicked on object.
		HitContext hitInfo;
		view->HitTest(point, hitInfo);

		CBaseObject* hitObj = hitInfo.object;
		if (hitObj)
		{
			// Fire double click event on hit object.
			hitObj->OnEvent(EVENT_DBLCLICK);
		}
	}
	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CObjectMode::OnRButtonDown(CViewport* view, int nFlags, CPoint point)
{
	if (gViewportPreferences.enableContextMenu)
	{
		// Check if right clicked on object.
		HitContext hitInfo;
		if (view->HitTest(point, hitInfo) && hitInfo.object)
		{
			m_openContext = true;
			m_rMouseDownPos = point;
			return true;
		}
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CObjectMode::OnRButtonUp(CViewport* view, int nFlags, CPoint point)
{
	if (m_openContext)
	{
		m_openContext = false;

		// check if we are close to the original mouse position
		int halfLength = gViewportPreferences.dragSquareSize / 2;
		CRect rcDrag(m_rMouseDownPos.x, m_rMouseDownPos.y, m_rMouseDownPos.x, m_rMouseDownPos.y);
		InflateRect(rcDrag, halfLength, halfLength);

		if (!PtInRect(rcDrag, point))
			return false;

		// Check if right clicked on object.
		HitContext hitInfo;
		if (!view->HitTest(point, hitInfo))
			return false;
		if (!hitInfo.object)
			return false;

		CDynamicPopupMenu menu;
		hitInfo.object->OnContextMenu(&menu.GetRoot());

		if (!menu.GetRoot().Empty())
		{
			// suspend highlight change for the duration of the popup...
			m_suspendHighlightChange = true;

			// and resume on close
			menu.SetOnHideFunctor([ = ]
			{
				if (GetIEditorImpl()->GetEditTool() == this)
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

//////////////////////////////////////////////////////////////////////////
bool CObjectMode::OnMButtonDown(CViewport* view, int nFlags, CPoint point)
{
	if (GetIEditorImpl()->GetGameEngine()->GetSimulationMode())
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
		else
		{
			// Update AI move simulation when not holding Ctrl down.
			return m_AIMoveSimulation.UpdateAIMoveSimulation(view, point);
		}
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
void CObjectMode::AwakeObjectAtPoint(CViewport* view, CPoint point)
{
	// In simulation mode awake objects under the cursor.
	// Check if double clicked on object.
	HitContext hitInfo;
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

//////////////////////////////////////////////////////////////////////////
bool CObjectMode::CheckVirtualKey(int virtualKey)
{
	GetAsyncKeyState(virtualKey);
	if (GetAsyncKeyState(virtualKey))
		return true;
	return false;
}

//////////////////////////////////////////////////////////////////////////
void CObjectMode::MoveSelectionToPos(CViewport* view, Vec3& pos, bool align, const CPoint& point)
{
	view->BeginUndo();
	// Find center of selection.
	Vec3 center = GetIEditorImpl()->GetSelection()->GetCenter();
	GetIEditorImpl()->GetSelection()->Move(pos - center, CSelectionGroup::eMS_None, true, point);

	if (align)
		GetIEditorImpl()->GetSelection()->Align();

	view->AcceptUndo("Move Selection");
}

//////////////////////////////////////////////////////////////////////////
bool CObjectMode::OnMouseMove(CViewport* view, int nFlags, CPoint point)
{
	if (GetIEditorImpl()->IsInGameMode())
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

	Vec3 pos = view->SnapToGrid(view->ViewToWorld(point));

	// get world/local coordinate system setting.
	int coordSys = GetIEditorImpl()->GetReferenceCoordSys();

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
				GetIEditorImpl()->GetSelection()->FilterParents();
				GetIEditorImpl()->GetSelection()->SaveFilteredTransform();
			}
			else
			{
				return true;
			}
		}

		Vec3 v;
		Vec3 p1 = m_mouseDownWorldPos;
		if (GetIEditorImpl()->IsSnapToTerrainEnabled() && view->GetAxisConstrain() != AXIS_Z)
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

		int selectionFlags = CSelectionGroup::eMS_None;
		if (GetIEditorImpl()->IsSnapToTerrainEnabled() && view->GetAxisConstrain() != AXIS_Z)
			selectionFlags = CSelectionGroup::eMS_FollowTerrain;

		if (GetIEditorImpl()->IsSnapToGeometryEnabled())
			selectionFlags |= CSelectionGroup::eMS_FollowGeometry;

		if (GetIEditorImpl()->IsSnapToNormalEnabled())
			selectionFlags |= CSelectionGroup::eMS_SnapToNormal;

		if ((nFlags & MK_CONTROL) && !(nFlags & MK_SHIFT))
			selectionFlags = CSelectionGroup::eMS_FollowGeometry | CSelectionGroup::eMS_SnapToNormal;

		if (!v.IsEquivalent(Vec3(0, 0, 0)))
			m_bTransformChanged = true;

#pragma message("TODO")
		//CTrackViewSequenceNoNotificationContext context(pSequence);

		if (m_bDragThresholdExceeded)
		{
			// All moved objects should have pushed an undo after moving, so suspend here
			// if we don't we'll end up with a huge undo step with duplicate entries for the objects.
			// TODO: improve undo system to only push when really necessary.
			GetIEditorImpl()->GetIUndoManager()->Suspend();
		}

		GetIEditorImpl()->GetSelection()->Move(v, selectionFlags, coordSys, point, true);

		if (m_bDragThresholdExceeded)
		{
			GetIEditorImpl()->GetIUndoManager()->Resume();
		}

		m_bDragThresholdExceeded = true;
		bSomethingDone = true;
	}
	else if (GetCommandMode() == RotateMode)
	{
		GetIEditorImpl()->GetIUndoManager()->Restore();

		Ang3 ang(0, 0, 0);
		float ax = point.x - m_cMouseDownPos.x;
		float ay = point.y - m_cMouseDownPos.y;
		switch (view->GetAxisConstrain())
		{
		case AXIS_X:
			ang.x = ay;
			break;
		case AXIS_Y:
			ang.y = ay;
			break;
		case AXIS_Z:
			ang.z = ay;
			break;
		case AXIS_XY:
			ang(ax, ay, 0);
			break;
		case AXIS_XZ:
			ang(ax, 0, ay);
			break;
		case AXIS_YZ:
			ang(0, ay, ax);
			break;
		}
		;

		ang = gSnappingPreferences.SnapAngle(ang);

		if (!ang.IsEquivalent(Ang3(0, 0, 0)))
			m_bTransformChanged = true;

		//m_cMouseDownPos = point;
		GetIEditorImpl()->GetSelection()->Rotate(ang, coordSys);
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
				GetIEditorImpl()->GetSelection()->FilterParents();
				GetIEditorImpl()->GetSelection()->SaveFilteredTransform();
			}
			else
			{
				return true;
			}
		}

		if (m_bTransformChanged)
		{
			GetIEditorImpl()->GetIUndoManager()->Suspend();
		}
		GetIEditorImpl()->GetSelection()->Scale(scale, coordSys);
		if (m_bTransformChanged)
		{
			GetIEditorImpl()->GetIUndoManager()->Resume();
		}
		m_bTransformChanged = true;
		bSomethingDone = true;
	}
	else if (GetCommandMode() == SelectMode)
	{
		// Ignore select when selection locked.
		if (GetIEditorImpl()->IsSelectionLocked())
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
		if (GetIEditorImpl()->GetEditMode() == eEditModeSelectArea)
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
		CGizmo* highlightedGizmo = GetIEditorImpl()->GetGizmoManager()->GetHighlightedGizmo();

		HitContext hitInfo;
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

	if ((nFlags & MK_MBUTTON) && GetIEditorImpl()->GetGameEngine()->GetSimulationMode())
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

//////////////////////////////////////////////////////////////////////////
void CObjectMode::SetObjectCursor(CViewport* view, CBaseObject* hitObj, IObjectManager::ESelectOp selectMode)
{
	EStdCursor cursor = STD_CURSOR_DEFAULT;
	string m_cursorStr;

	CBaseObject* pMouseOverObject = NULL;
	if (m_MouseOverObject != CryGUID::Null() && !m_suspendHighlightChange)
	{
		pMouseOverObject = GetIEditorImpl()->GetObjectManager()->FindObject(m_MouseOverObject);
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
		if (GetCommandMode() != SelectMode && !GetIEditorImpl()->IsSelectionLocked())
		{
			if (pMouseOverObject->CanBeHightlighted() && GetIEditor()->IsHelpersDisplayed())
				pMouseOverObject->SetHighlight(true);

			m_cursorStr = pMouseOverObject->GetName();

			string comment(pMouseOverObject->GetComment());
			if (!comment.IsEmpty())
			{
				m_cursorStr += "\n";
				m_cursorStr += comment;
			}

			if (gViewportDebugPreferences.showMeshStatsOnMouseOver)
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

	bool bLockSelection = GetIEditorImpl()->IsSelectionLocked();

	if (GetCommandMode() == SelectMode || GetCommandMode() == NothingMode)
	{
		if (bAddSelect)
			cursor = STD_CURSOR_SEL_PLUS;
		if (bUnselect)
			cursor = STD_CURSOR_SEL_MINUS;

		if ((bHitSelectedObject && !bNoRemoveSelection) || bLockSelection)
		{
			int editMode = GetIEditorImpl()->GetEditMode();
			if (editMode == eEditModeMove)
			{
				cursor = STD_CURSOR_MOVE;
			}
			else if (editMode == eEditModeRotate)
			{
				cursor = STD_CURSOR_ROTATE;
			}
			else if (editMode == eEditModeScale)
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

//////////////////////////////////////////////////////////////////////////
// Class description.
//////////////////////////////////////////////////////////////////////////
class CObjectMode_ClassDesc : public IClassDesc
{
	//! This method returns an Editor defined GUID describing the class this plugin class is associated with.
	virtual ESystemClassID SystemClassID() { return ESYSTEM_CLASS_EDITTOOL; }

	//! This method returns the human readable name of the class.
	virtual const char* ClassName() { return "EditTool.ObjectMode"; };

	//! This method returns Category of this class, Category is specifing where this plugin class fits best in
	//! create panel.
	virtual const char*    Category()        { return "Select"; };
	virtual CRuntimeClass* GetRuntimeClass() { return RUNTIME_CLASS(CObjectMode); }
	//////////////////////////////////////////////////////////////////////////
};

REGISTER_CLASS_DESC(CObjectMode_ClassDesc);

//////////////////////////////////////////////////////////////////////////
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

	int axisConstrain = view->GetAxisConstrain();

	if (axisConstrain < AXIS_XYZ && GetIEditorImpl()->IsAxisVectorLocked())
	{
		axisConstrain = AXIS_XYZ;
	}

	switch (axisConstrain)
	{
	case AXIS_X:
		scl(ay, 1, 1);
		break;
	case AXIS_Y:
		scl(1, ay, 1);
		break;
	case AXIS_Z:
		scl(1, 1, ay);
		break;
	case AXIS_XY:
		scl(ay, ay, ay);
		break;
	case AXIS_XZ:
		scl(ay, ay, ay);
		break;
	case AXIS_YZ:
		scl(ay, ay, ay);
		break;
	case AXIS_XYZ:
		scl(ay, ay, ay);
		break;
	}
	;

	if (GetIEditorImpl()->IsSnapToTerrainEnabled())
		scl(ay, ay, ay);

	OutScale = scl;

	return OutScale;
}

void CObjectMode::OnManipulatorBeginDrag(IDisplayViewport* view, ITransformManipulator* pManipulator, const Vec2i& point, int flags)
{
	m_cMouseDownPos = CPoint(point.x, point.y);
	m_bGizmoDrag = true;
	int editMode = GetIEditorImpl()->GetEditMode();

	if (editMode == eEditModeScale || editMode == eEditModeMove)
	{
		const CSelectionGroup* pSelGrp = GetIEditorImpl()->GetSelection();
		pSelGrp->FilterParents();
		pSelGrp->SaveFilteredTransform();

		// Hack: Only for move mode, set the command mode to move, so that grid shows up in the viewport
		if (editMode == eEditModeMove)
		{
			m_bDragThresholdExceeded = false;
		}
	}

	if (editMode == eEditModeMove)
	{
		m_commandMode = MoveMode;
	}
	else if (editMode == eEditModeScale)
	{
		m_commandMode = ScaleMode;
	}
	else if (editMode == eEditModeRotate)
	{
		m_commandMode = RotateMode;
	}

	((CViewport*)view)->DegradateQuality(true);
}

void CObjectMode::OnManipulatorEndDrag(IDisplayViewport* view, ITransformManipulator* pManipulator)
{
	if (m_commandMode == ScaleMode || m_commandMode == MoveMode)
	{
		if (m_bDragThresholdExceeded)
		{
			const CSelectionGroup* pSelGrp = GetIEditorImpl()->GetSelection();
			pSelGrp->FinishChanges();
			m_bDragThresholdExceeded = false;
		}
	}
	m_commandMode = NothingMode;
	m_bGizmoDrag = false;

	((CViewport*)view)->DegradateQuality(false);
	GetIEditorImpl()->UpdateViews(eUpdateObjects);
}

//////////////////////////////////////////////////////////////////////////
// This callback is currently called only to handle the case of the 'move by the face normal'.
// Other movements of the object are handled in the 'CObjectMode::OnMouseMove()' method.
void CObjectMode::OnManipulatorDrag(IDisplayViewport* view, ITransformManipulator* pManipulator, const Vec2i& point, const Vec3& value, int flags)
{
	CPoint p(point.x, point.y);

	RefCoordSys coordSys = GetIEditorImpl()->GetReferenceCoordSys();

	if (m_commandMode == MoveMode)
	{
		int halfLength = gViewportPreferences.dragSquareSize / 2;
		CRect rcDrag(m_cMouseDownPos.x, m_cMouseDownPos.y, m_cMouseDownPos.x, m_cMouseDownPos.y);
		InflateRect(rcDrag, halfLength, halfLength);

		if (PtInRect(rcDrag, p))
		{
			return;
		}

		const CSelectionGroup* pSelGrp = GetIEditorImpl()->GetSelection();

		if (m_bDragThresholdExceeded)
		{
			// All moved objects should have pushed an undo after moving, so suspend here
			// if we don't we'll end up with a huge undo step with duplicate entries for the objects.
			// TODO: improve undo system to only push when really necessary.
			GetIEditorImpl()->GetIUndoManager()->Suspend();
		}

		int selectionFlags = CSelectionGroup::eMS_None;
		if (GetIEditorImpl()->IsSnapToTerrainEnabled())
			selectionFlags = CSelectionGroup::eMS_FollowTerrain;

		if (GetIEditorImpl()->IsSnapToNormalEnabled())
			selectionFlags |= CSelectionGroup::eMS_SnapToNormal;

		pSelGrp->Move(value, selectionFlags, coordSys, p, true);

		if (m_pHitObject)
			UpdateMoveByFaceNormGizmo(m_pHitObject);

		if (m_bDragThresholdExceeded)
		{
			GetIEditorImpl()->GetIUndoManager()->Resume();
		}
		m_bDragThresholdExceeded = true;
	}
	else if (m_commandMode == RotateMode)
	{
		GetIEditorImpl()->GetIUndoManager()->Restore();
		const CSelectionGroup* pSelGrp = GetIEditorImpl()->GetSelection();

		// unfortunately more conversion to fit selectiongroup format
		Ang3 angles = RAD2DEG(-value);
		pSelGrp->Rotate(angles, coordSys);
	}
	else if (m_commandMode == ScaleMode)
	{
		const CSelectionGroup* pSelGrp = GetIEditorImpl()->GetSelection();
		pSelGrp->Scale(value, coordSys);
		m_bDragThresholdExceeded = true;
	}
}

void CObjectMode::HandleMoveByFaceNormal(HitContext& hitInfo)
{
	CBaseObject* pHitObject = hitInfo.object;
	bool bFaceNormalMovePossible = pHitObject && GetIEditorImpl()->GetEditMode() == eEditModeMove
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
			m_normalMoveGizmo = GetIEditorImpl()->GetGizmoManager()->AddManipulator(&m_normalGizmoOwner);
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
		GetIEditorImpl()->GetGizmoManager()->RemoveManipulator(m_normalMoveGizmo);
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

