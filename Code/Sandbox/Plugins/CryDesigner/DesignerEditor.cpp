// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "DesignerEditor.h"
#include "ViewManager.h"
#include "Objects/DesignerObject.h"
#include "Objects/DisplayContext.h"
#include "Core/ModelCompiler.h"
#include "ToolFactory.h"
#include "Core/Model.h"
#include "Tools/BaseTool.h"
#include "Tools/Select/SelectTool.h"
#include "Util/Undo.h"
#include "Core/Helper.h"
#include "Objects/ObjectLayer.h"
#include "Objects/ObjectLayerManager.h"
#include "Objects/BrushObject.h"
#include "Util/ElementSet.h"
#include "Material/MaterialManager.h"
#include "SurfaceInfoPicker.h"
#include "ToolFactory.h"
#include "Gizmos/ITransformManipulator.h"
#include "Gizmos/IGizmoManager.h"
#include "Util/ExcludedEdgeManager.h"
#include "Util/Display.h"
#include <Preferences/ViewportPreferences.h>
#include "CryEditDoc.h"
#include "QtUtil.h"
#include "Util/Converter.h"
#include "RecursionLoopGuard.h"

namespace Designer
{

class DesignerEditor_ClassDesc : public IClassDesc
{
	virtual ESystemClassID SystemClassID()   { return ESYSTEM_CLASS_EDITTOOL; }
	virtual const char*    ClassName()       { return "EditTool.DesignerEditor"; };
	virtual const char*    Category()        { return "Brush"; };
	virtual CRuntimeClass* GetRuntimeClass() { return RUNTIME_CLASS(DesignerEditor); }
};

REGISTER_CLASS_DESC(DesignerEditor_ClassDesc);

IMPLEMENT_DYNCREATE(DesignerEditor, CEditTool);

DesignerEditor::DesignerEditor()
	: m_Tool(eDesigner_Invalid)
	, m_PreviousTool(eDesigner_Invalid)
	, m_PreviousSelectTool(eDesigner_Invalid)
	, m_pView(nullptr)
	, m_pEditingObject(nullptr)
	, m_ScreenPos(0, 0)
	, m_iter(-1)
	, m_pManipulator(nullptr)
{
	m_ViewportRect.left = m_ViewportRect.right = m_ViewportRect.top = m_ViewportRect.bottom = 0;
	BeginEdit();
}

void DesignerEditor::StartCreation(const char* szObjectType)
{
	DesignerSession::GetInstance()->EndSession();

	BaseTool* pTool = GetTool(m_Tool);
	if (pTool->GetModel())
	{
		pTool->Leave();
	}

	Create(szObjectType);

	pTool->Enter();
}

void DesignerEditor::Create(const char* szObjectType)
{
	// We must begin and end an undo step here because cancelling shape creation
	// will cancel the current undo stack and kill our CBaseObject along with the current shape.
	// This will lead to crashes.
	GetIEditor()->GetIUndoManager()->Begin();
	m_pEditingObject = GetIEditor()->NewObject(szObjectType, nullptr, true);
	CRY_ASSERT(m_pEditingObject);
	DesignerSession::GetInstance()->SetBaseObject(m_pEditingObject);
	GetIEditor()->GetIUndoManager()->Accept("Create Designer");
}

DesignerEditor::~DesignerEditor()
{
	EndEdit();
	EndCurrentDesignerTool();
}

void DesignerEditor::OnEditorNotifyEvent(EEditorNotifyEvent event)
{
	if (event == eNotify_OnEndUndoRedo)
	{
		if (!IsSelectElementMode(m_Tool) &&
		    m_Tool != eDesigner_ExtrudeMultiple &&
		    m_Tool != eDesigner_ExtrudeEdge)
		{
			DesignerSession::GetInstance()->GetSelectedElements()->Clear();
			DesignerSession::GetInstance()->ClearPolygonSelections();
		}
	}

	if (GetCurrentTool())
		GetCurrentTool()->OnEditorNotifyEvent(event);
}

void DesignerEditor::BeginEdit()
{
	m_Tool = eDesigner_Invalid;
	m_PreviousSelectTool = eDesigner_Polygon;

	// Handle manipulator code, port over from old object mode tool initialization.
	DesignerSession* pSession = DesignerSession::GetInstance();

	// at this point we are guaranteed that our selection is only composed of designer type objects
	if (m_pEditingObject)
	{
		// TODO: This shouldn't even be needed
		m_pEditingObject->UpdateHighlightPassState(false, false);
	}
	else
	{
		// Disable, any tool that needs this will reenable
		const ISelectionGroup* pSel = GetIEditor()->GetISelectionGroup();

		// at this point we are guaranteed that our selection is only composed of designer type objects
		for (int i = 0, totSelected = pSel->GetCount(); i < totSelected; ++i)
		{
			CBaseObject* pObj = pSel->GetObject(i);
			pObj->UpdateHighlightPassState(false, false);
		}
	}

	pSession->signalDesignerEvent(eDesignerNotify_EnterDesignerEditor, nullptr);
}

void DesignerEditor::EndEdit()
{
	LeaveCurrentTool();

	ReleaseObjectGizmo();

	DesignerSession* pSession = DesignerSession::GetInstance();

	// Notify everyone that we have no tool now
	ToolchangeEvent evt;

	evt.current = eDesigner_Invalid;
	evt.previous = m_Tool;

	pSession->signalDesignerEvent(eDesignerNotify_SubToolChanged, &evt);

	m_Tool = eDesigner_Invalid;

	// delete the object if its shelf is empty
	// TODO check what happens with multi object editing
	Model* pModel = nullptr;
	Designer::GetModel(m_pEditingObject, pModel);

	// revert the selected state of the objects back for highlighting
	if (m_pEditingObject)
	{
		// TODO: This shouldn't even be needed
		m_pEditingObject->UpdateHighlightPassState(m_pEditingObject->IsSelected(), false);
	}
	else
	{
		MainContext ctx = pSession->GetMainContext();
		CBaseObject* pObject = ctx.pObject;
		pModel = ctx.pModel;

		// revert the selected state of the objects back for highlighting
		const ISelectionGroup* pSel = GetIEditor()->GetISelectionGroup();

		for (int i = 0, totSelected = pSel->GetCount(); i < totSelected; ++i)
		{
			CBaseObject* pObj = pSel->GetObject(i);
			pObj->UpdateHighlightPassState(pObj->IsSelected(), false);
		}
	}

	if (!pModel || pModel->IsEmpty(eShelf_Base))
	{
		GetIEditor()->GetIUndoManager()->Suspend();
		GetIEditor()->GetObjectManager()->DeleteObject(m_pEditingObject);
		GetIEditor()->GetIUndoManager()->Resume();
	}
}

void DesignerEditor::Display(DisplayContext& dc)
{
	DesignerSession* pSession = DesignerSession::GetInstance();
	MainContext ctx = pSession->GetMainContext();
	CBaseObject* pObject = ctx.pObject;
	Model* model = ctx.pModel;

	if (!model)
		return;

	m_Camera = dc.GetCamera();
	int vp_x = 0, vp_y = 0, vp_w = dc.GetWidth(), vp_h = dc.GetHeight();
	m_ViewportRect.left = vp_x;
	m_ViewportRect.top = vp_y;
	m_ViewportRect.right = vp_x + vp_w + 1;
	m_ViewportRect.bottom = vp_y + vp_h + 1;

	dc.SetDrawInFrontMode(true);
	dc.PushMatrix(pObject->GetWorldTM());

	if (!gDesignerSettings.bHighlightElements || !IsEdgeSelectMode(m_Tool))
	{
		Display::DisplayModel(dc, model, pSession->GetExcludedEdgeManager());
	}

	if (m_Tool != eDesigner_Invalid && GetTool(m_Tool))
		GetTool(m_Tool)->Display(dc);

	if (gDesignerSettings.bDisplayTriangulation)
		Display::DisplayTriangulation(dc, ctx);

	if (gDesignerSettings.bDisplayVertexNormals)
		Display::DisplayVertexNormals(dc, ctx);

	if (gDesignerSettings.bDisplayPolygonNormals)
		Display::DisplayPolygonNormals(dc, ctx);

	ctx.pSelected->Display(pObject, dc);

	dc.PopMatrix();
	dc.SetDrawInFrontMode(false);
}

bool DesignerEditor::MouseCallback(
  CViewport* view,
  EMouseEvent event,
  CPoint& point,
  int flags)
{
	MainContext ctx = DesignerSession::GetInstance()->GetMainContext();
	CBaseObject* pObject = ctx.pObject;
	Model* pModel = ctx.pModel;

	if (m_Tool == eDesigner_Invalid)
	{
		//TODO: hack to support non modal operators - remove
		GetIEditor()->SetEditTool(nullptr);
		return false;
	}

	_smart_ptr<BaseTool> pTool = GetTool(m_Tool);
	if (pTool == NULL)
		return false;

	if (!pModel)
		return false;

	m_pView = view;
	m_ScreenPos = point;

	bool bDoSomething = false;

	if (event == eMouseLDown)
	{
		if (GetCurrentTool() &&
		    IsCreationTool(m_Tool) &&
		    pModel->IsEmpty() &&
		    GetCurrentTool()->IsPhaseFirstStepOnPrimitiveCreation())
		{
			CSurfaceInfoPicker surfacePicker;
			surfacePicker.SetPickOptionFlag(CSurfaceInfoPicker::ePickOption_IncludeFrozenObject);
			SRayHitInfo hitInfo;
			if (surfacePicker.Pick(point, hitInfo, NULL, CSurfaceInfoPicker::ePOG_GeneralObjects | CSurfaceInfoPicker::ePOG_Terrain))
			{
				GetIEditor()->GetIUndoManager()->Suspend();
				Matrix34 worldTM = Matrix34::CreateIdentity();
				worldTM.SetTranslation(view->SnapToGrid(hitInfo.vHitPos));
				pObject->SetWorldTM(worldTM);
				GetIEditor()->GetIUndoManager()->Resume();
			}
		}

		bDoSomething = pTool->OnLButtonDown(view, flags, point);
	}
	else if (event == eMouseLUp)
	{
		bDoSomething = pTool->OnLButtonUp(view, flags, point);
	}
	else if (event == eMouseMove)
	{
		ElementSet* pSelected = ctx.pSelected;
		std::vector<DesignerObject*> selectedDesignerObjects = Designer::GetSelectedDesignerObjects();
		if (flags == 0 && pTool->EnabledSeamlessSelection() && (gDesignerSettings.bSeamlessEdit || selectedDesignerObjects.size() > 1))
		{
			if (pSelected->IsEmpty())
				SelectDesignerObject(point);
		}
		bDoSomething = pTool->OnMouseMove(view, flags, point);
	}
	else if (event == eMouseLDblClick)
	{
		bDoSomething = pTool->OnLButtonDblClk(view, flags, point);
	}
	else if (event == eMouseRDown)
	{
		bDoSomething = pTool->OnRButtonDown(view, flags, point);
	}
	else if (event == eMouseRUp)
	{
		bDoSomething = pTool->OnRButtonUp(view, flags, point);
	}
	else if (event == eMouseMDown)
	{
		bDoSomething = pTool->OnMButtonDown(view, flags, point);
	}
	else if (event == eMouseWheel)
	{
		if (pTool->OnMouseWheel(view, flags, point))
			return true;
	}
	else if (event == eMouseFocusLeave)
	{
		bDoSomething = pTool->OnFocusLeave(view, flags, point);
	}
	else if (event == eMouseLeave)
	{
		bDoSomething = pTool->OnFocusLeave(view, flags, point);
	}
	else if (event == eMouseEnter)
	{
		bDoSomething = pTool->OnFocusEnter(view, flags, point);
	}

	return bDoSomething;
}

bool DesignerEditor::OnKeyDown(CViewport* pView, uint32 nChar, uint32 nRepCnt, uint32 nFlags)
{
	if (m_Tool == eDesigner_Invalid)
		return false;

	if (nChar == Qt::Key_Delete)
	{
		SwitchTool(eDesigner_Remove);
		return true;
	}

	BaseTool* pTool = GetTool(m_Tool);
	if (pTool == NULL)
		return false;

	if (pTool->OnKeyDown(pView, nChar, nRepCnt, nFlags))
	{
		if (nChar == Qt::Key_Escape)
		{
			// Release the current tool by ending the current session
			DesignerSession::GetInstance()->EndSession();
		}
		return true;
	}

	return false;
}

bool DesignerEditor::SetSelectionDesignerMode(
  EDesignerTool tool,
  bool bAllowMultipleMode)
{
	// TODO: Selection should not be a tool, but state of the session, that tools use to operate
	if (!IsSelectElementMode(tool) || m_DisabledTools.find(tool) != m_DisabledTools.end())
		return false;

	int nCombinationDesignerMode = m_Tool;

	bool bPressedCtrl = QtUtil::IsModifierKeyDown(Qt::ControlModifier) && bAllowMultipleMode;
	if (bPressedCtrl)
	{
		SelectTool* pTool = (SelectTool*)GetCurrentTool();
		if (pTool && pTool->CheckPickFlag(ePF_Vertex) && tool == eDesigner_Vertex)
		{
			nCombinationDesignerMode &= ~(ePF_Vertex);
		}
		else if (pTool && pTool->CheckPickFlag(ePF_Edge) && tool == eDesigner_Edge)
		{
			nCombinationDesignerMode &= ~(ePF_Edge);
		}
		else if (pTool && pTool->CheckPickFlag(ePF_Polygon) && tool == eDesigner_Polygon)
		{
			nCombinationDesignerMode &= ~(ePF_Polygon);
		}
		else if (IsSelectElementMode(m_Tool))
		{
			nCombinationDesignerMode |= tool;
		}
		else
		{
			nCombinationDesignerMode = tool;
		}
	}
	else
	{
		nCombinationDesignerMode = tool;
	}

	if (!IsSelectElementMode((EDesignerTool)nCombinationDesignerMode))
		return false;

	BaseTool* pTool = GetTool((EDesignerTool)nCombinationDesignerMode);
	if (pTool == NULL)
		return false;

	SelectTool* pNextTool = (SelectTool*)pTool;

	LeaveCurrentTool();
	m_PreviousTool = IsSelectElementMode(m_Tool) ? m_PreviousTool : m_Tool;
	m_Tool = (EDesignerTool)nCombinationDesignerMode;
	pNextTool->Enter();

	ToolchangeEvent evt;

	evt.current = m_Tool;
	evt.previous = m_PreviousTool;

	DesignerSession::GetInstance()->signalDesignerEvent(eDesignerNotify_SelectModeChanged, &evt);

	GetIEditor()->GetActiveView()->Invalidate();

	m_PreviousSelectTool = m_Tool;

	return true;
}

bool DesignerEditor::SwitchTool(
  EDesignerTool tool,
  bool bForceChange,
  bool bAllowMultipleMode)
{
	DesignerSession* pSession = DesignerSession::GetInstance();

	if (!pSession->GetBaseObject() ||
	    m_DisabledTools.find(tool) != m_DisabledTools.end())
	{
		return false;
	}

	if (SetSelectionDesignerMode(tool, bAllowMultipleMode))
		return true;

	if (m_Tool == tool && !bForceChange)
	{
		return false;
	}

	LeaveCurrentTool();

	m_PreviousTool = m_Tool;
	m_Tool = tool;

	ToolchangeEvent evt;

	evt.current = m_Tool;
	evt.previous = m_PreviousTool;

	pSession->signalDesignerEvent(eDesignerNotify_SubToolChanged, &evt);

	_smart_ptr<BaseTool> pTool = GetCurrentTool();
	if (pTool)
		pTool->Enter();

	auto activeView = GetIEditor()->GetActiveView();
	if (activeView)
	{
		activeView->Invalidate();
	}
	return true;
}

bool DesignerEditor::EndCurrentDesignerTool()
{
	ReleaseObjectGizmo();
	return true;
}

void DesignerEditor::SwitchToPrevTool()
{
	if (m_PreviousTool == eDesigner_UVMapping ||
	    m_PreviousTool == eDesigner_SmoothingGroup)
	{
		SwitchTool(m_PreviousTool);
	}
	else
	{
		SwitchToSelectTool();
	}
}

void DesignerEditor::DisableTool(
  IDesignerPanel* pEditPanel,
  EDesignerTool tool)
{
	if (pEditPanel)
		pEditPanel->DisableButton(tool);
	m_DisabledTools.insert(tool);
}

void DesignerEditor::OnManipulatorDrag(
  IDisplayViewport* view,
  ITransformManipulator* pManipulator,
  const Vec2i& p0,
  const Vec3& value,
  int nFlags)
{
	if (pManipulator != m_pManipulator)
	{
		return;
	}

	BaseTool* pTool = GetCurrentTool();
	if (pTool == NULL)
	{
		return;
	}

	CPoint p(p0.x, p0.y);
	pTool->OnManipulatorDrag(
	  view,
	  pManipulator,
	  p,
	  value,
	  nFlags);
}

void DesignerEditor::OnManipulatorBegin(IDisplayViewport* view, ITransformManipulator* pManipulator, const Vec2i& point, int flags)
{
	if (pManipulator != m_pManipulator)
		return;

	BaseTool* pTool = GetCurrentTool();
	if (pTool == NULL)
		return;

	CPoint p(point.x, point.y);
	pTool->OnManipulatorBegin(view, pManipulator, p, flags);
}

void DesignerEditor::OnManipulatorEnd(IDisplayViewport* view, ITransformManipulator* pManipulator)
{
	if (pManipulator != m_pManipulator)
		return;

	BaseTool* pTool = GetCurrentTool();
	if (pTool == NULL)
		return;

	pTool->OnManipulatorEnd(view, pManipulator);
}

BaseTool* DesignerEditor::GetCurrentTool() const
{
	if (m_Tool == eDesigner_Invalid)
		return NULL;

	BaseTool* pTool = GetTool(m_Tool);
	if (pTool == NULL)
		return NULL;

	return pTool;
}

void DesignerEditor::LeaveCurrentTool()
{
	if (GetCurrentTool())
		GetCurrentTool()->Leave();
}

void DesignerEditor::EnterCurrentTool()
{
	if (GetCurrentTool())
		GetCurrentTool()->Enter();
}

void DesignerEditor::SelectAllElements()
{
	if (IsSelectElementMode(m_Tool))
		((SelectTool*)GetTool(m_Tool))->SelectAllElements();
}

bool DesignerEditor::IsDisplayGrid()
{
	// Designer tools don't like grids.
	return false;
}

void DesignerEditor::StoreSelectionUndo(bool bRestoreAfterUndoing)
{
	DesignerSession* pSession = DesignerSession::GetInstance();
	CBaseObject* pObject = pSession->GetBaseObject();
	ElementSet* pSelected = pSession->GetSelectedElements();

	if (CUndo::IsRecording())
		CUndo::Record(new UndoSelection(
		                *pSelected,
		                pObject,
		                bRestoreAfterUndoing));
}

BaseTool* DesignerEditor::GetTool(EDesignerTool tool) const
{
	if (m_ToolMap.find(tool) == m_ToolMap.end())
		m_ToolMap[tool] = ToolFactory::the().Create(tool);
	return m_ToolMap[tool];
}

void DesignerEditor::ReleaseObjectGizmo()
{
	if (m_pManipulator)
	{
		m_pManipulator->signalBeginDrag.DisconnectAll();
		m_pManipulator->signalDragging.DisconnectAll();
		m_pManipulator->signalEndDrag.DisconnectAll();
		GetIEditor()->GetGizmoManager()->RemoveManipulator(m_pManipulator);
		m_pManipulator = nullptr;
	}
}

bool DesignerEditor::SelectDesignerObject(CPoint point)
{
	DesignerSession* pSession = DesignerSession::GetInstance();
	MainContext ctx = pSession->GetMainContext();
	CBaseObject* pObject = ctx.pObject;
	Model* pModel = ctx.pModel;

	if (!pModel)
	{
		return false;
	}

	if (pObject && pObject->GetType() != OBJTYPE_SOLID)
		return false;

	std::vector<DesignerObject*> selectedObjects;
	std::set<DesignerObject*> selectedObjectSet;
	selectedObjects = Designer::GetSelectedDesignerObjects();
	selectedObjectSet.insert(selectedObjects.begin(), selectedObjects.end());

	CSurfaceInfoPicker picker;
	SRayHitInfo hitInfo;
	if (picker.Pick(point, hitInfo, NULL, CSurfaceInfoPicker::ePOG_DesignerObject))
	{
		CBaseObject* pPickedObj = picker.GetPickedObject();

		if (!pPickedObj || pObject == pPickedObj)
			return false;

		if (!pPickedObj->IsKindOf(RUNTIME_CLASS(DesignerObject)))
			return false;

		if (!gDesignerSettings.bSeamlessEdit && selectedObjectSet.size() > 2 &&
		    selectedObjectSet.find((DesignerObject*)pPickedObj) == selectedObjectSet.end())
		{
			return false;
		}

		DesignerObject* pNewDesignerObj = (DesignerObject*)pPickedObj;
		if (!pNewDesignerObj->GetCompiler() ||
		    !pNewDesignerObj->GetModel())
		{
			return false;
		}

		pSession->SetBaseObject(pNewDesignerObj);
		ElementSet* pSelected = pSession->GetSelectedElements();
		pSelected->Clear();
		return true;
	}

	return false;
}

void DesignerEditor::UpdateTMManipulatorBasedOnElements(ElementSet* elements)
{
	Model* pModel = DesignerSession::GetInstance()->GetModel();

	if (elements->IsEmpty())
		return;
	BrushVec3 averagePos(0, 0, 0);
	int iResultSize(0);
	for (int i = 0, iListSize(elements->GetCount()); i < iListSize; ++i)
	{
		if ((*elements)[i].IsPolygon() && (*elements)[i].m_pPolygon)
		{
			averagePos += (*elements)[i].m_pPolygon->GetRepresentativePosition();
			++iResultSize;
		}
		else
		{
			for (int k = 0, iElementSize((*elements)[i].m_Vertices.size()); k < iElementSize; ++k)
			{
				averagePos += (*elements)[i].m_Vertices[k];
				++iResultSize;
			}
		}
	}
	averagePos /= iResultSize;
	UpdateTMManipulator(averagePos, elements->GetOrts(pModel));
}

void DesignerEditor::UpdateTMManipulator(
  const BrushVec3& localPos,
  const BrushVec3& localNormal)
{
	const Matrix33 localOrthogonalTM = Matrix33::CreateOrthogonalBase(localNormal);
	return UpdateTMManipulator(localPos, localOrthogonalTM);
}

void DesignerEditor::UpdateTMManipulator(
  const BrushVec3& localPos,
  const Matrix33& localOrts)
{
	const CBaseObject* const pObject = DesignerSession::GetInstance()->GetBaseObject();

	if (!m_pManipulator)
	{
		m_pManipulator = GetIEditor()->GetGizmoManager()->AddManipulator(this);
		m_pManipulator->signalBeginDrag.Connect(this, &DesignerEditor::OnManipulatorBegin);
		m_pManipulator->signalDragging.Connect(this, &DesignerEditor::OnManipulatorDrag);
		m_pManipulator->signalEndDrag.Connect(this, &DesignerEditor::OnManipulatorEnd);
	}

	const Matrix34 worldTM = pObject->GetWorldTM();
	m_manipulatorMatrix = worldTM * localOrts;
	m_manipulatorPosition = worldTM * Vec3(localPos);
	m_pManipulator->Invalidate();
}

bool DesignerEditor::GetManipulatorMatrix(RefCoordSys coordSys, Matrix34& tm)
{
	DesignerSession* pSession = DesignerSession::GetInstance();
	CBaseObject* pObject = pSession->GetBaseObject();

	if (coordSys == COORDS_LOCAL)
	{
		tm = m_manipulatorMatrix;
		return true;
	}
	else if (coordSys == COORDS_PARENT)
	{
		if (pObject)
		{
			tm = pObject->GetWorldTM();
		}
		else
		{
			tm = m_manipulatorMatrix;
		}
		return true;
	}
	return false;
}

void DesignerEditor::GetManipulatorPosition(Vec3& position)
{
	position = m_manipulatorPosition;
}

bool DesignerEditor::IsManipulatorVisible()
{
	BaseTool* pTool = GetCurrentTool();
	return pTool && pTool->IsManipulatorVisible();
};

BrushRay DesignerEditor::GetRay() const
{
	BrushRay ray;
	CBaseObject* pObject = DesignerSession::GetInstance()->GetBaseObject();

	if (m_pView)
		GetLocalViewRay(pObject->GetWorldTM(), m_pView, m_ScreenPos, ray);
	return ray;
}

IMPLEMENT_DYNCREATE(CreateDesignerObjectTool, DesignerEditor)

class CreateDesignerObjectTool_ClassDesc : public IClassDesc
{
	ESystemClassID SystemClassID()   { return ESYSTEM_CLASS_EDITTOOL; }
	const char*    ClassName()       { return "EditTool.CreateDesignerObjectTool"; };
	const char*    Category()        { return "Object"; };
	CRuntimeClass* GetRuntimeClass() { return RUNTIME_CLASS(CreateDesignerObjectTool); }
};

REGISTER_CLASS_DESC(CreateDesignerObjectTool_ClassDesc);

CreateDesignerObjectTool::CreateDesignerObjectTool()
{
	StartCreation("Designer");
}

}

