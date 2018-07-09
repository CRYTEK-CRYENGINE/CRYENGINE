// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "BoxTool.h"
#include "DesignerEditor.h"
#include "Tools/Select/SelectTool.h"
#include "Viewport.h"
#include "Util/HeightManipulator.h"
#include "Util/ExtrusionSnappingHelper.h"
#include "ToolFactory.h"
#include "Core/Helper.h"
#include "DesignerSession.h"

namespace Designer
{
void BoxTool::Serialize(Serialization::IArchive& ar)
{
	__super::Serialize(ar);
	m_BoxParameter.Serialize(ar);
}

void BoxTool::OnLButtonDownAboutPlaceStartingPointPhase(CViewport* view, UINT nFlags, CPoint point)
{
	__super::OnLButtonDownAboutPlaceStartingPointPhase(view, nFlags, point);

	StoreSeparateStatus();
	m_BoxParameter.m_Height = 0;
	s_SnappingHelper.Init(GetModel());
}

bool BoxTool::OnLButtonDown(CViewport* view, UINT nFlags, CPoint point)
{
	__super::OnLButtonDown(view, nFlags, point);

	if (m_Phase == eBoxPhase_RaiseHeight)
		m_Phase = eBoxPhase_Done;

	return true;
}

bool BoxTool::OnLButtonUp(CViewport* view, UINT nFlags, CPoint point)
{
	if (m_Phase == eBoxPhase_DrawRectangle)
	{
		if (std::abs(GetCurrentSpotPos().x - GetStartSpotPos().x) > (BrushFloat)0.01 ||
		    std::abs(GetCurrentSpotPos().y - GetStartSpotPos().y) > (BrushFloat)0.01 ||
		    std::abs(GetCurrentSpotPos().z - GetStartSpotPos().z) > (BrushFloat)0.01)
		{
			m_Phase = eBoxPhase_RaiseHeight;
			s_SnappingHelper.SearchForOppositePolygons(m_pCapPolygon);
			m_pCapPolygon = NULL;
			m_bIsOverOpposite = false;
			s_HeightManipulator.Init(GetPlane(), GetCurrentSpotPos());
		}
	}

	return true;
}

bool BoxTool::OnMouseMove(CViewport* view, UINT nFlags, CPoint point)
{
	__super::OnMouseMove(view, nFlags, point);

	if (m_Phase == eBoxPhase_RaiseHeight)
	{
		m_BoxParameter.m_Height = (float)s_HeightManipulator.Update(GetWorldTM(), view, GetDesigner()->GetRay());

		if (m_BoxParameter.m_Height < (float)kInitialPrimitiveHeight)
			m_BoxParameter.m_Height = 0;

		if (m_BoxParameter.m_bAlignment)
		{
			PolygonPtr pAlignedPolygon = s_SnappingHelper.FindAlignedPolygon(m_pCapPolygon, GetWorldTM(), view, point);
			if (pAlignedPolygon)
				m_BoxParameter.m_Height = (float)(GetPlane().Distance() - pAlignedPolygon->GetPlane().Distance());
		}

		UpdateShapeWithBoundaryCheck();
	}

	return true;
}

void BoxTool::UpdateShapeWithBoundaryCheck(bool bUpdateUIs)
{
	UpdateShape(bUpdateUIs);
	m_bIsOverOpposite = m_pCapPolygon && s_SnappingHelper.IsOverOppositePolygon(m_pCapPolygon, ePP_Pull);
	if (m_bIsOverOpposite)
	{
		m_BoxParameter.m_Height = (float)s_SnappingHelper.GetNearestDistanceToOpposite(ePP_Pull);
		UpdateShape(bUpdateUIs);
	}
}

void BoxTool::UpdateShape(bool bUpdateUIs)
{
	std::vector<PolygonPtr> polygonList;

	m_v[0] = GetStartSpotPos();
	m_v[1] = GetCurrentSpotPos();

	BrushVec2 p0 = GetPlane().W2P(m_v[0]);
	BrushVec2 p1 = GetPlane().W2P(m_v[1]);

	std::vector<BrushVec3> vList(4);
	vList[0] = m_v[0];
	vList[1] = GetPlane().P2W(BrushVec2(p1.x, p0.y));
	vList[2] = m_v[1];
	vList[3] = GetPlane().P2W(BrushVec2(p0.x, p1.y));

	polygonList.push_back(new Polygon(vList));
	if (polygonList[0]->IsOpen())
		return;
	if (polygonList[0]->GetPlane().IsSameFacing(GetPlane()))
		polygonList[0]->Flip();

	for (int i = 0; i < 4; ++i)
		vList[i] += GetPlane().Normal() * (BrushFloat)m_BoxParameter.m_Height;
	polygonList.push_back(new Polygon(vList));
	if (!polygonList[1]->GetPlane().IsSameFacing(GetPlane()))
		polygonList[1]->Flip();

	for (int i = 0; i < 4; ++i)
	{
		BrushEdge3D e = polygonList[0]->GetEdge(i);

		vList[0] = e.m_v[1];
		vList[1] = e.m_v[0];
		vList[2] = e.m_v[0] + GetPlane().Normal() * (BrushFloat)m_BoxParameter.m_Height;
		vList[3] = e.m_v[1] + GetPlane().Normal() * (BrushFloat)m_BoxParameter.m_Height;

		polygonList.push_back(new Polygon(vList));
	}

	MODEL_SHELF_RECONSTRUCTOR(GetModel());

	GetModel()->SetShelf(eShelf_Construction);
	GetModel()->Clear();

	for (int i = 0, iCount(polygonList.size()); i < iCount; ++i)
	{
		if (polygonList[i]->IsOpen())
			continue;

		if (GetStartSpot().m_pPolygon)
		{
			polygonList[i]->SetTexInfo(GetStartSpot().m_pPolygon->GetTexInfo());
			polygonList[i]->SetSubMatID(GetStartSpot().m_pPolygon->GetSubMatID());
		}

		AddPolygonWithSubMatID(polygonList[i]);

		if (Comparison::IsEquivalent(polygonList[i]->GetPlane().Normal(), GetPlane().Normal()))
			m_pCapPolygon = polygonList[i];
	}

	if (bUpdateUIs)
	{
		m_RectangleParameter.m_Width = std::abs(p0.x - p1.x);
		m_RectangleParameter.m_Depth = std::abs(p0.y - p1.y);
		DesignerSession::GetInstance()->signalDesignerEvent(eDesignerNotify_SubtoolOptionChanged, nullptr);
	}

	ApplyPostProcess(ePostProcess_Mesh | ePostProcess_Mirror | ePostProcess_SmoothingGroup);
}

void BoxTool::Display(DisplayContext& dc)
{
	DisplayCurrentSpot(dc);
	DisplayDimensionHelper(dc, eShelf_Construction);
	if (GetStartSpot().m_pPolygon)
		DisplayDimensionHelper(dc);
	s_HeightManipulator.Display(dc);
}

void BoxTool::Register()
{
	RegisterShape(GetStartSpot().m_pPolygon);
}

void BoxTool::RegisterShape(PolygonPtr pFloorPolygon)
{
	if (!GetModel() || GetModel()->IsEmpty(eShelf_Construction))
		return;

	MODEL_SHELF_RECONSTRUCTOR(GetModel());

	if (m_pCapPolygon)
	{
		GetModel()->SetShelf(eShelf_Construction);
		if (m_bIsOverOpposite)
		{
			s_SnappingHelper.ApplyOppositePolygons(m_pCapPolygon, ePP_Pull, true);
			GetModel()->RemovePolygon(GetModel()->QueryEquivalentPolygon(m_pCapPolygon));
		}

		if (!IsSeparateStatus())
		{
			std::vector<PolygonPtr> sidePolygons;

			BrushPlane invertedCapPolygonPlane = m_pCapPolygon->GetPlane().GetInverted();
			for (int i = 0, iPolygonCount(GetModel()->GetPolygonCount()); i < iPolygonCount; ++i)
			{
				PolygonPtr pPolygon = GetModel()->GetPolygon(i);
				if (pPolygon != m_pCapPolygon && !Comparison::IsEquivalent(invertedCapPolygonPlane.Normal(), pPolygon->GetPlane().Normal()))
					sidePolygons.push_back(pPolygon);
			}

			std::vector<PolygonPtr> intersectedSidePolygons;
			for (int i = 0, iSidePolygonCount(sidePolygons.size()); i < iSidePolygonCount; ++i)
			{
				for (int k = 0; k < 2; ++k)
				{
					PolygonPtr pSidePolygon = k == 0 ? sidePolygons[i]->Clone()->Flip() : sidePolygons[i];

					GetModel()->SetShelf(eShelf_Base);
					bool bHasIntersected = GetModel()->HasIntersection(pSidePolygon, true);
					bool bTouched = GetModel()->HasTouched(pSidePolygon);
					if ((!bHasIntersected && !bTouched) || (bTouched && k == 0))
						continue;

					GetModel()->SetShelf(eShelf_Construction);
					GetModel()->RemovePolygon(sidePolygons[i]);

					GetModel()->SetShelf(eShelf_Base);
					if (bHasIntersected)
					{
						GetModel()->AddXORPolygon(pSidePolygon);
						intersectedSidePolygons.push_back(pSidePolygon);
					}
					else if (bTouched)
					{
						GetModel()->AddUnionPolygon(pSidePolygon);
					}
					break;
				}
			}

			for (int i = 0, iCount(intersectedSidePolygons.size()); i < iCount; ++i)
			{
				GetModel()->SetShelf(eShelf_Base);
			}
		}
	}

	__super::RegisterShape(pFloorPolygon);
}

}

REGISTER_DESIGNER_TOOL_WITH_PROPERTYTREE_PANEL_AND_COMMAND(eDesigner_Box, eToolGroup_Shape, "Box", BoxTool,
                                                           box, "runs box tool", "designer.box");

