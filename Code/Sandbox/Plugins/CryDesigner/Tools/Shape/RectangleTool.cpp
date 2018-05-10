// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "RectangleTool.h"
#include "DesignerEditor.h"
#include "ToolFactory.h"
#include "Core/Helper.h"
#include "Util/Display.h"
#include "DesignerSession.h"

namespace Designer
{
void RectangleTool::Enter()
{
	__super::Enter();
	//! Sets the first phase.
	m_Phase = eBoxPhase_PlaceStartingPoint;
	m_pRectPolygon = NULL;
}

void RectangleTool::Leave()
{
	//! If this tool exits when the phase is eBoxPhase_Done, the drawn rectangle is registered to the Model instance.
	//! Otherwise all is cancelled.
	if (m_Phase == eBoxPhase_Done)
	{
		Register();
		GetIEditor()->GetIUndoManager()->Accept("Designer : Create a Rectangle");
	}
	else
	{
		//! MODEL_SHELF_RECONSTRUCTOR is for restoring the old shelf id of the model at the end of the current scope.
		MODEL_SHELF_RECONSTRUCTOR(GetModel());

		GetModel()->SetShelf(eShelf_Construction);
		GetModel()->Clear();
		CompileShelf(eShelf_Construction);
		GetIEditor()->GetIUndoManager()->Cancel();
	}
	__super::Leave();
}

void RectangleTool::OnLButtonDownAboutPlaceStartingPointPhase(CViewport* view, UINT nFlags, CPoint point)
{
	//! A spot in CryDesigner means a point with some extra information such as the polygon below the point, the spot status and position etc.
	//! ShapeTool::UpdateCurrentSpotPosition() function finds the spot from the intersection between the ray from the camera and the world or the mesh
	//! , and set it to the current spot.
	if (ShapeTool::UpdateCurrentSpotPosition(view, nFlags, point, false, true))
	{
		//! The plane picked by the current cursor is set.
		SetPlane(GetCurrentSpot().m_Plane);

		//! The current spot is set as the starting spot.
		SetStartSpot(GetCurrentSpot());

		//! Begins the undo.
		GetIEditor()->GetIUndoManager()->Begin();
		GetModel()->RecordUndo(
		  "Designer : Create a Rectangle",
		  GetBaseObject());

		//! Stores whether the rectangle which will be drawn is separated from the current designer object or not.
		StoreSeparateStatus();

		//! Now this tool gets ready for drawing a rectangle.
		m_Phase = eBoxPhase_DrawRectangle;
	}
}

bool RectangleTool::OnLButtonDown(CViewport* view, UINT nFlags, CPoint point)
{
	//! The drawn rectangle polygon is registered to the model after eBoxPhase_Done
	//! and then this tool gets ready for the next rectangle.
	if (m_Phase == eBoxPhase_Done)
	{
		Register();
		m_Phase = eBoxPhase_PlaceStartingPoint;
		m_pRectPolygon = NULL;
		GetIEditor()->GetIUndoManager()->Accept("Designer : Create a Rectangle");
	}

	if (m_Phase == eBoxPhase_PlaceStartingPoint)
		OnLButtonDownAboutPlaceStartingPointPhase(view, nFlags, point);

	return true;
}

bool RectangleTool::OnKeyDown(CViewport* view, uint32 nChar, uint32 nRepCnt, uint32 nFlags)
{
	if (nChar == Qt::Key_Escape)
	{
		//! If ESC is pressed,
		if (m_Phase == eBoxPhase_Done)
		{
			Register();
			GetIEditor()->GetIUndoManager()->Accept("Designer : Create a Rectangle");
		}
		else if (m_Phase != eBoxPhase_PlaceStartingPoint)
		{
			CancelCreation();
			m_Phase = eBoxPhase_PlaceStartingPoint;
			return true;
		}
		return GetDesigner()->EndCurrentDesignerTool();
	}
	return true;
}

bool RectangleTool::OnLButtonUp(CViewport* view, UINT nFlags, CPoint point)
{
	if (m_Phase == eBoxPhase_DrawRectangle)
	{
		if (std::abs(GetCurrentSpotPos().x - GetStartSpotPos().x) > (BrushFloat)0.01 ||
		    std::abs(GetCurrentSpotPos().y - GetStartSpotPos().y) > (BrushFloat)0.01 ||
		    std::abs(GetCurrentSpotPos().z - GetStartSpotPos().z) > (BrushFloat)0.01)
		{
			m_Phase = eBoxPhase_Done;
			UpdateShape(true);
		}
	}

	return true;
}

bool RectangleTool::OnMouseMove(CViewport* view, UINT nFlags, CPoint point)
{
	if (m_Phase == eBoxPhase_PlaceStartingPoint || m_Phase == eBoxPhase_Done)
	{
		UpdateCurrentSpotPosition(view, nFlags, point, false, true);
		if (m_Phase == eBoxPhase_PlaceStartingPoint)
			SetPlane(GetCurrentSpot().m_Plane);
	}
	else if (m_Phase == eBoxPhase_DrawRectangle)
	{
		if ((nFlags & MK_LBUTTON) && UpdateCurrentSpotPosition(view, nFlags, point, true, true))
			UpdateShape(true);
	}

	return true;
}

void RectangleTool::Register()
{
	Register2DShape(m_pRectPolygon);
}

void RectangleTool::UpdateShapeWithBoundaryCheck(bool bUpdateUIs)
{
	UpdateShape(bUpdateUIs);
}

void RectangleTool::UpdateShape(bool bUpdateUIs)
{
	if (!m_pRectPolygon)
		m_pRectPolygon = new Polygon();

	//! Gets the starting and end position for the rectangle.
	BrushVec3 v0 = GetStartSpotPos();
	BrushVec3 v1 = GetCurrentSpotPos();

	std::vector<BrushVec3> vList(4);

	//! The two 3D positions are converted to the 2D positions to calculate the other two positions of the rectangle by projecting the 3D position to the plane.
	BrushVec2 p0 = GetPlane().W2P(v0);
	BrushVec2 p1 = GetPlane().W2P(v1);

	m_v[0] = v0;
	m_v[1] = v1;

	//! Calculates the 4 points for the rectangle.
	vList[0] = v0;
	vList[1] = GetPlane().P2W(BrushVec2(p1.x, p0.y));
	vList[2] = v1;
	vList[3] = GetPlane().P2W(BrushVec2(p0.x, p1.y));

	if (bUpdateUIs)
	{
		m_RectangleParameter.m_Width = std::abs(p0.x - p1.x);
		m_RectangleParameter.m_Depth = std::abs(p0.y - p1.y);
		DesignerSession::GetInstance()->signalDesignerEvent(eDesignerNotify_SubtoolOptionChanged, nullptr);
	}

	//! Creates a rectangle polygon from the 4 3D points calculated above.
	*m_pRectPolygon = Polygon(vList);

	//! If the rectangle polygon is not oriented toward the plane, the rectangle polygon should be flipped.
	if (!m_pRectPolygon->GetPlane().IsSameFacing(GetPlane()))
		m_pRectPolygon->Flip();

	//! The new rectangle polygon is added to the shelf 1 of the model to be rendered.
	MODEL_SHELF_RECONSTRUCTOR(GetModel());
	GetModel()->SetShelf(eShelf_Construction);
	GetModel()->Clear();

	if (GetStartSpot().m_pPolygon)
		m_pRectPolygon->SetTexInfo(GetStartSpot().m_pPolygon->GetTexInfo());

	//! Adds the rectangle polygon to the current shelf of the model with the sub material ID picked by the starting spot.
	if (!m_pRectPolygon->IsOpen())
		AddPolygonWithSubMatID(m_pRectPolygon);

	//! Applies the mirror.
	ApplyPostProcess(ePostProcess_Mirror);

	//! Compiles only the shelf 1.
	CompileShelf(eShelf_Construction);
}

void RectangleTool::Display(DisplayContext& dc)
{
	DisplayCurrentSpot(dc);

	if (m_Phase == eBoxPhase_DrawRectangle && m_pRectPolygon)
		DisplayDimensionHelper(dc, m_pRectPolygon->GetBoundBox());
	else
		DisplayDimensionHelper(dc, eShelf_Construction);

	if (GetStartSpot().m_pPolygon)
		DisplayDimensionHelper(dc);

	//! Draws the outline of the rectangle.
	if (m_Phase == eBoxPhase_DrawRectangle || m_Phase == eBoxPhase_Done)
		Display::DisplayBottomPolygon(dc, m_pRectPolygon);
}

void RectangleTool::OnEditorNotifyEvent(EEditorNotifyEvent event)
{
	switch (event)
	{
	case eNotify_OnBeginUndoRedo:
	case eNotify_OnBeginSceneSave:
		if (m_Phase == eBoxPhase_Done)
		{
			GetIEditor()->GetIUndoManager()->Accept("Designer : Create a Rectangle");
			Register();
		}
		else
		{
			GetIEditor()->GetIUndoManager()->Cancel();
			CancelCreation();
		}
		m_Phase = eBoxPhase_PlaceStartingPoint;
	}
}

void RectangleTool::OnChangeParameter(bool continuous)
{
	BrushVec2 p0 = GetPlane().W2P(m_v[0]);
	BrushVec2 p1 = GetPlane().W2P(m_v[1]);
	BrushVec2 center = (p0 + p1) * 0.5f;
	p0.x = center.x - m_RectangleParameter.m_Width * 0.5f;
	p1.x = center.x + m_RectangleParameter.m_Width * 0.5f;
	p0.y = center.y - m_RectangleParameter.m_Depth * 0.5f;
	p1.y = center.y + m_RectangleParameter.m_Depth * 0.5f;
	SetStartSpotPos(GetPlane().P2W(p0));
	SetCurrentSpotPos(GetPlane().P2W(p1));
	UpdateShapeWithBoundaryCheck(false);
}

void RectangleTool::Serialize(Serialization::IArchive& ar)
{
	__super::Serialize(ar);
	m_RectangleParameter.Serialize(ar);
}

bool RectangleTool::IsPhaseFirstStepOnPrimitiveCreation() const
{
	return m_Phase == eBoxPhase_PlaceStartingPoint;
}

bool RectangleTool::EnabledSeamlessSelection() const
{
	return m_Phase == eBoxPhase_PlaceStartingPoint && !IsModelEmpty() ? true : false;
}

}

//! Registers the rectangle tool to CryDesigner
REGISTER_DESIGNER_TOOL_WITH_PROPERTYTREE_PANEL_AND_COMMAND(eDesigner_Rectangle, eToolGroup_Shape, "Rectangle", RectangleTool,
                                                           rectangle, "runs rectangle tool", "designer.rectangle")

