// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "DiscTool.h"
#include "Core/Model.h"
#include "DesignerEditor.h"
#include "ToolFactory.h"
#include "Core/Helper.h"
#include "Util/Display.h"
#include "DesignerSession.h"

namespace Designer
{
void DiscTool::Enter()
{
	__super::Enter();
	m_Phase = eDiscPhase_SetCenter;
	m_bSnapped = false;
}

void DiscTool::Leave()
{
	if (m_Phase == eDiscPhase_Done)
	{
		Register();
		GetIEditor()->GetIUndoManager()->Accept("Designer : Create a Disc");
	}
	else
	{
		MODEL_SHELF_RECONSTRUCTOR(GetModel());
		GetModel()->SetShelf(eShelf_Construction);
		GetModel()->Clear();
		CompileShelf(eShelf_Construction);
		GetIEditor()->GetIUndoManager()->Cancel();
	}
	UpdateSurfaceInfo();
	__super::Leave();
}

void DiscTool::Register()
{
	Register2DShape(m_pBasePolygon->Flip());
}

bool DiscTool::OnMouseMove(CViewport* view, UINT nFlags, CPoint point)
{
	bool bKeepInitPlane = m_Phase == eDiscPhase_Radius ? true : false;
	bool kSearchAllShelves = m_Phase == eDiscPhase_Done ? true : false;
	if (m_Phase == eDiscPhase_SetCenter ||
	    m_Phase == eDiscPhase_Radius ||
	    m_Phase == eDiscPhase_Done)
	{
		if (!ShapeTool::UpdateCurrentSpotPosition(
		      view,
		      nFlags,
		      point,
		      bKeepInitPlane,
		      kSearchAllShelves))
		{
			return true;
		}
	}

	if (m_Phase == eDiscPhase_Radius)
	{
		BrushVec2 vSpotPos2D = GetPlane().W2P(GetCurrentSpotPos());

		m_DiscParameter.m_Radius = (vSpotPos2D - m_vCenterOnPlane).GetLength();
		const BrushFloat kSmallestRadius = 0.05f;
		if (m_DiscParameter.m_Radius < kSmallestRadius)
			m_DiscParameter.m_Radius = kSmallestRadius;

		m_fAngle = ComputeAnglePointedByPos(m_vCenterOnPlane, vSpotPos2D);

		if (m_DiscParameter.m_b90DegreeSnap)
			m_bSnapped = SnapAngleBy90Degrees(m_fAngle, m_fAngle);
		else
			m_bSnapped = false;

		UpdateShape();
		DesignerSession::GetInstance()->signalDesignerEvent(eDesignerNotify_SubtoolOptionChanged, nullptr);
	}

	return true;
}

bool DiscTool::SnapAngleBy90Degrees(float angle, float& outSnappedAngle)
{
	const float range = 0.3f;

	if (std::abs(angle) < range)
		outSnappedAngle = 0;
	else if (std::abs(angle - 0.25f * PI2) < range)
		outSnappedAngle = 0.25f * PI2;
	else if (std::abs(angle + 0.25f * PI2) < range)
		outSnappedAngle = -0.25f * PI2;
	else if (std::abs(angle + 0.5f * PI2) < range)
		outSnappedAngle = -0.5f * PI2;
	else
	{
		outSnappedAngle = angle;
		return false;
	}

	return true;
}

bool DiscTool::OnLButtonDown(CViewport* view, UINT nFlags, CPoint point)
{
	if (m_Phase == eDiscPhase_Done)
	{
		m_Phase = eDiscPhase_SetCenter;
		if (m_pBasePolygon && m_pBasePolygon->IsValid())
		{
			Register();
			GetIEditor()->GetIUndoManager()->Accept("Designer : Create a Disc");
		}
	}

	if (m_Phase == eDiscPhase_SetCenter)
	{
		if (ShapeTool::UpdateCurrentSpotPosition(view, nFlags, point, false, true))
		{
			SetStartSpot(GetCurrentSpot());
			SetPlane(GetCurrentSpot().m_Plane);
			GetIEditor()->GetIUndoManager()->Begin();
			GetModel()->RecordUndo("Designer : Create a Disc", GetBaseObject());
			StoreSeparateStatus();

			m_Phase = eDiscPhase_Radius;
			m_vCenterOnPlane = GetPlane().W2P(GetCurrentSpotPos());
			m_pBasePolygon = NULL;
		}
	}
	else if (m_Phase == eDiscPhase_Radius)
	{
		m_Phase = eDiscPhase_Done;
	}

	return true;
}

bool DiscTool::OnKeyDown(CViewport* view, uint32 nChar, uint32 nRepCnt, uint32 nFlags)
{
	if (nChar == Qt::Key_Escape)
	{
		if (m_Phase == eDiscPhase_Radius || m_Phase == eDiscPhase_RaiseHeight)
		{
			CancelCreation();
			m_Phase = eDiscPhase_SetCenter;
			return true;
		}
		else if (m_Phase == eDiscPhase_Done)
		{
			Register();
			GetIEditor()->GetIUndoManager()->Accept("Designer : Create a Disc");
		}
		return GetDesigner()->EndCurrentDesignerTool();
	}
	return true;
}

void DiscTool::Display(DisplayContext& dc)
{
	DisplayCurrentSpot(dc);

	if (m_Phase == eDiscPhase_Radius && m_pBasePolygon)
		DisplayDimensionHelper(dc, m_pBasePolygon->GetBoundBox());
	else
		DisplayDimensionHelper(dc, eShelf_Construction);

	Display::DisplayBottomPolygon(dc, m_pBasePolygon, m_bSnapped ? PolygonParallelToAxis : PolygonLineColor);

	if (GetStartSpot().m_pPolygon)
		DisplayDimensionHelper(dc);
}

void DiscTool::UpdateShape()
{
	if (m_Phase == eDiscPhase_SetCenter ||
	    m_DiscParameter.m_NumOfSubdivision < 3)
	{
		return;
	}

	std::vector<BrushVec2> vertices2D;
	MakeSectorOfCircle(
	  m_DiscParameter.m_Radius,
	  m_vCenterOnPlane,
	  m_fAngle,
	  PI2,
	  m_DiscParameter.m_NumOfSubdivision + 1,
	  vertices2D);

	vertices2D.erase(vertices2D.begin());
	TexInfo texInfo = GetStartSpot().m_pPolygon ? GetStartSpot().m_pPolygon->GetTexInfo() : GetTexInfo();
	int nMaterialID = GetStartSpot().m_pPolygon ? GetStartSpot().m_pPolygon->GetSubMatID() : 0;

	m_pBasePolygon = new Polygon(vertices2D, GetPlane(), GetMatID(), &texInfo, true);
	m_pBasePolygon->Flip();
	m_pBasePolygon->SetSubMatID(nMaterialID);

	MODEL_SHELF_RECONSTRUCTOR(GetModel());
	GetModel()->SetShelf(eShelf_Construction);
	GetModel()->Clear();
	GetModel()->AddPolygon(m_pBasePolygon->Clone()->Flip());

	ApplyPostProcess(ePostProcess_Mirror);
	CompileShelf(eShelf_Construction);
}

void DiscTool::OnEditorNotifyEvent(EEditorNotifyEvent event)
{
	switch (event)
	{
	case eNotify_OnBeginUndoRedo:
	case eNotify_OnBeginSceneSave:
		if (m_Phase == eDiscPhase_Done)
		{
			GetIEditor()->GetIUndoManager()->Accept("Designer : Create a Disc");
			Register();
		}
		else
		{
			GetIEditor()->GetIUndoManager()->Cancel();
			CancelCreation();
		}
		m_Phase = eDiscPhase_SetCenter;
	}
}

void DiscTool::Serialize(Serialization::IArchive& ar)
{
	__super::Serialize(ar);
	m_DiscParameter.Serialize(ar);
}

bool DiscTool::IsPhaseFirstStepOnPrimitiveCreation() const
{
	return m_Phase == eDiscPhase_SetCenter;
}

void DiscTool::OnChangeParameter(bool continuous)
{
	UpdateShape();
}
}

REGISTER_DESIGNER_TOOL_WITH_PROPERTYTREE_PANEL_AND_COMMAND(eDesigner_Disc, eToolGroup_Shape, "Disc", DiscTool,
                                                           disc, "runs disc tool", "designer.disc")

