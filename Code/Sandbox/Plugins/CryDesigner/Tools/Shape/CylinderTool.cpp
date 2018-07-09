// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "CylinderTool.h"
#include "Core/Model.h"
#include "Util/PrimitiveShape.h"
#include "DesignerEditor.h"
#include "Util/HeightManipulator.h"
#include "Util/ExtrusionSnappingHelper.h"
#include "ViewManager.h"
#include "ToolFactory.h"
#include "Core/Helper.h"
#include "Util/Display.h"
#include "DesignerSession.h"

namespace Designer
{
bool CylinderTool::OnMouseMove(CViewport* view, UINT nFlags, CPoint point)
{
	__super::OnMouseMove(view, nFlags, point);

	if (m_Phase == eDiscPhase_RaiseHeight)
	{
		m_CylinderParameter.m_Height = s_HeightManipulator.Update(GetWorldTM(), view, GetDesigner()->GetRay());
		if (m_CylinderParameter.m_Height < kInitialPrimitiveHeight)
			m_CylinderParameter.m_Height = 0;
		if (nFlags & MK_SHIFT)
		{
			PolygonPtr pAlignedPolygon = s_SnappingHelper.FindAlignedPolygon(m_pCapPolygon, GetWorldTM(), view, point);
			if (pAlignedPolygon)
				m_CylinderParameter.m_Height = GetPlane().Distance() - pAlignedPolygon->GetPlane().Distance();
		}
		UpdateHeightWithBoundaryCheck(m_CylinderParameter.m_Height);
		DesignerSession::GetInstance()->signalDesignerEvent(eDesignerNotify_SubtoolOptionChanged, nullptr);
	}

	return true;
}

bool CylinderTool::OnLButtonDown(CViewport* view, UINT nFlags, CPoint point)
{
	EDiscToolPhase origianlPhase = m_Phase;

	__super::OnLButtonDown(view, nFlags, point);

	if (origianlPhase == eDiscPhase_Radius)
	{
		if (ShapeTool::UpdateCurrentSpotPosition(view, nFlags, point, false, true))
			s_HeightManipulator.Init(GetPlane(), GetCurrentSpotPos());

		s_SnappingHelper.Init(GetModel());

		UpdateShape(0);

		if (m_pCapPolygon)
			s_SnappingHelper.SearchForOppositePolygons(m_pCapPolygon);

		m_bIsOverOpposite = false;
		m_Phase = eDiscPhase_RaiseHeight;
	}
	else if (origianlPhase == eDiscPhase_RaiseHeight)
	{
		m_Phase = eDiscPhase_Done;
	}

	return true;
}

void CylinderTool::Display(DisplayContext& dc)
{
	if (!GetModel() || !GetBaseObject())
		return;

	DisplayCurrentSpot(dc);

	if (m_Phase == eDiscPhase_Radius ||
	    m_Phase == eDiscPhase_RaiseHeight ||
	    m_Phase == eDiscPhase_Done)
	{
		Display::DisplayBottomPolygon(dc, m_pBasePolygon, m_bSnapped ? PolygonParallelToAxis : PolygonLineColor);
	}

	if (m_Phase == eDiscPhase_Radius && m_pBasePolygon)
		DisplayDimensionHelper(dc, m_pBasePolygon->GetBoundBox());
	else
		DisplayDimensionHelper(dc, eShelf_Construction);

	if (GetStartSpot().m_pPolygon)
		DisplayDimensionHelper(dc);

	s_HeightManipulator.Display(dc);
}

void CylinderTool::UpdateHeightWithBoundaryCheck(BrushFloat fHeight)
{
	UpdateShape(fHeight);
	m_bIsOverOpposite = m_pCapPolygon && s_SnappingHelper.IsOverOppositePolygon(m_pCapPolygon, ePP_Pull);
	if (m_bIsOverOpposite)
	{
		fHeight = s_SnappingHelper.GetNearestDistanceToOpposite(ePP_Pull);
		UpdateShape(fHeight);
	}
}

void CylinderTool::UpdateShape(float fHeight)
{
	if (m_Phase == eDiscPhase_SetCenter)
		return;

	DESIGNER_ASSERT(m_pBasePolygon);
	if (!m_pBasePolygon)
		return;

	std::vector<PolygonPtr> polygonList;
	PrimitiveShape bp;
	bp.CreateCylinder(m_pBasePolygon, fHeight, &polygonList);

	MODEL_SHELF_RECONSTRUCTOR(GetModel());
	GetModel()->SetShelf(eShelf_Construction);
	GetModel()->Clear();
	for (int i = 0, iPolygonCount(polygonList.size()); i < iPolygonCount; ++i)
	{
		AddPolygonWithSubMatID(polygonList[i]);
		if (Comparison::IsEquivalent(polygonList[i]->GetPlane().Normal(), GetPlane().Normal()))
			m_pCapPolygon = polygonList[i];
	}

	ApplyPostProcess(ePostProcess_Mirror);
	CompileShelf(eShelf_Construction);
}

void CylinderTool::Register()
{
	if (m_bIsOverOpposite)
		s_SnappingHelper.ApplyOppositePolygons(m_pCapPolygon, ePP_Pull);
	ShapeTool::RegisterShape(GetStartSpot().m_pPolygon);
}

bool CylinderTool::EnabledSeamlessSelection() const
{
	return m_Phase == eDiscPhase_SetCenter && !IsModelEmpty() ? true : false;
}

void CylinderTool::Serialize(Serialization::IArchive& ar)
{
	__super::Serialize(ar);
	m_CylinderParameter.Serialize(ar);
}

void CylinderTool::OnChangeParameter(bool continuous)
{
	DiscTool::UpdateShape();
	if (m_Phase == eDiscPhase_Done)
		UpdateHeightWithBoundaryCheck(m_CylinderParameter.m_Height);
}

}

REGISTER_DESIGNER_TOOL_WITH_PROPERTYTREE_PANEL_AND_COMMAND(eDesigner_Cylinder, eToolGroup_Shape, "Cylinder", CylinderTool,
                                                           cylinder, "runs cylinder tool", "designer.cylinder")

