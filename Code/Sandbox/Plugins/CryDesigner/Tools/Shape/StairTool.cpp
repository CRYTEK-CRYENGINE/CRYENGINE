// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "StairTool.h"
#include "Core/Model.h"
#include "Viewport.h"
#include "Objects/DesignerObject.h"
#include "DesignerEditor.h"
#include "ToolFactory.h"
#include "Core/Helper.h"
#include "Util/ExtrusionSnappingHelper.h"
#include "DesignerSession.h"

namespace Designer
{
void StairTool::Serialize(Serialization::IArchive& ar)
{
	m_StairParameter.Serialize(ar);
	__super::Serialize(ar);
}

void StairTool::AcceptUndo()
{
	GetIEditor()->GetIUndoManager()->Begin();
	GetModel()->RecordUndo("Designer : Create a Stair", GetBaseObject());
	GetIEditor()->GetIUndoManager()->Accept("Designer : Create a Stair");
}

PolygonPtr StairTool::CreatePolygon(const std::vector<BrushVec3>& vList, bool bFlip, PolygonPtr pBasePolygon)
{
	if (vList.size() < 3)
		return NULL;
	PolygonPtr pPolygon = new Polygon(vList);
	if (pPolygon->IsOpen())
		return NULL;
	if (bFlip)
		pPolygon->Flip();
	if (pBasePolygon)
	{
		pPolygon->SetTexInfo(pBasePolygon->GetTexInfo());
		pPolygon->SetSubMatID(pBasePolygon->GetSubMatID());
	}
	return pPolygon;
}

void StairTool::CreateStair(const BrushVec3& vStartPos,
                            const BrushVec3& vEndPos,
                            BrushFloat fBoxWidth,
                            BrushFloat fBoxDepth,
                            BrushFloat fBoxHeight,
                            const BrushPlane& floorPlane,
                            float fStepRise,
                            bool bWidthIsLonger,
                            bool bMirrored,
                            bool bRotationBy90Degree,
                            PolygonPtr pBasePolygon,
                            OutputStairParameter& out)
{
	if (bRotationBy90Degree)
		bWidthIsLonger = !bWidthIsLonger;

	int nCompleteStepNum = (int)(fBoxHeight / fStepRise);
	BrushFloat fRestStepRise = fBoxHeight - fStepRise * nCompleteStepNum;
	BrushFloat fRatioRestStepLengthToFullLength = fRestStepRise / fBoxHeight;

	BrushVec2 startSpotPos = floorPlane.W2P(vStartPos);
	BrushVec2 endSpotPos = floorPlane.W2P(vEndPos);

	endSpotPos.x = startSpotPos.x + fBoxWidth;
	endSpotPos.y = startSpotPos.y + fBoxDepth;

	if (bMirrored)
	{
		std::swap(startSpotPos, endSpotPos);
		fBoxDepth = -fBoxDepth;
		fBoxWidth = -fBoxWidth;
	}

	BrushFloat fStairSize = bWidthIsLonger ? fBoxWidth : fBoxDepth;

	BrushFloat fStepTread = nCompleteStepNum == 0 ? 0 : ((1 - fRatioRestStepLengthToFullLength) * std::abs(fStairSize)) / nCompleteStepNum;
	BrushFloat fRestStepTread = std::abs(fStairSize) - fStepTread * nCompleteStepNum;

	int nWidthSign = fBoxWidth > 0 ? 1 : -1;
	int nDepthSign = fBoxDepth > 0 ? 1 : -1;

	std::vector<BrushVec3> vSideList[2];
	std::vector<BrushVec3> vBackList;
	std::vector<BrushVec3> vBottomList;

	bool bFlip = bWidthIsLonger ? nWidthSign * nDepthSign == -1 : nWidthSign * nDepthSign == 1;

	for (int i = 0; i <= nCompleteStepNum; ++i)
	{
		BrushFloat x = startSpotPos.x + i * fStepTread * nWidthSign;
		BrushFloat next_x = i < nCompleteStepNum ? startSpotPos.x + (i + 1) * fStepTread * nWidthSign : x + fRestStepTread * nWidthSign;

		BrushFloat z = startSpotPos.y + i * fStepTread * nDepthSign;
		BrushFloat next_z = i < nCompleteStepNum ? startSpotPos.y + (i + 1) * fStepTread * nDepthSign : z + fRestStepTread * nDepthSign;

		BrushFloat y = i * fStepRise;
		BrushFloat next_y = i < nCompleteStepNum ? (i + 1) * fStepRise : y + fRestStepRise;

		std::vector<BrushVec3> vStepRiseList(4);
		if (bWidthIsLonger)
		{
			vStepRiseList[0] = floorPlane.P2W(BrushVec2(x, startSpotPos.y));
			vStepRiseList[1] = floorPlane.P2W(BrushVec2(x, endSpotPos.y));
			vStepRiseList[0] += floorPlane.Normal() * y;
			vStepRiseList[1] += floorPlane.Normal() * y;

			vStepRiseList[2] = floorPlane.P2W(BrushVec2(x, endSpotPos.y));
			vStepRiseList[3] = floorPlane.P2W(BrushVec2(x, startSpotPos.y));
			vStepRiseList[2] += floorPlane.Normal() * next_y;
			vStepRiseList[3] += floorPlane.Normal() * next_y;
		}
		else
		{
			vStepRiseList[0] = floorPlane.P2W(BrushVec2(startSpotPos.x, z));
			vStepRiseList[1] = floorPlane.P2W(BrushVec2(endSpotPos.x, z));
			vStepRiseList[0] += floorPlane.Normal() * y;
			vStepRiseList[1] += floorPlane.Normal() * y;

			vStepRiseList[2] = floorPlane.P2W(BrushVec2(endSpotPos.x, z));
			vStepRiseList[3] = floorPlane.P2W(BrushVec2(startSpotPos.x, z));
			vStepRiseList[2] += floorPlane.Normal() * next_y;
			vStepRiseList[3] += floorPlane.Normal() * next_y;
		}
		PolygonPtr pStepRisePolygon = CreatePolygon(vStepRiseList, bFlip, pBasePolygon);
		if (pStepRisePolygon)
			out.polygons.push_back(pStepRisePolygon);

		if (i == 0)
		{
			vBottomList.push_back(vStepRiseList[1]);
			vBottomList.push_back(vStepRiseList[0]);

			if (pStepRisePolygon)
				out.polygonsNeedPostProcess.push_back(pStepRisePolygon);
		}

		vSideList[0].push_back(vStepRiseList[0]);
		vSideList[0].push_back(vStepRiseList[3]);

		vSideList[1].push_back(vStepRiseList[1]);
		vSideList[1].push_back(vStepRiseList[2]);

		std::vector<BrushVec3> vStepTreadList(4);
		if (bWidthIsLonger)
		{
			vStepTreadList[0] = floorPlane.P2W(BrushVec2(x, startSpotPos.y));
			vStepTreadList[1] = floorPlane.P2W(BrushVec2(x, endSpotPos.y));
			vStepTreadList[2] = floorPlane.P2W(BrushVec2(next_x, endSpotPos.y));
			vStepTreadList[3] = floorPlane.P2W(BrushVec2(next_x, startSpotPos.y));

			vStepTreadList[0] += floorPlane.Normal() * next_y;
			vStepTreadList[1] += floorPlane.Normal() * next_y;
			vStepTreadList[2] += floorPlane.Normal() * next_y;
			vStepTreadList[3] += floorPlane.Normal() * next_y;
		}
		else
		{
			vStepTreadList[0] = floorPlane.P2W(BrushVec2(startSpotPos.x, z));
			vStepTreadList[1] = floorPlane.P2W(BrushVec2(endSpotPos.x, z));
			vStepTreadList[2] = floorPlane.P2W(BrushVec2(endSpotPos.x, next_z));
			vStepTreadList[3] = floorPlane.P2W(BrushVec2(startSpotPos.x, next_z));

			vStepTreadList[0] += floorPlane.Normal() * next_y;
			vStepTreadList[1] += floorPlane.Normal() * next_y;
			vStepTreadList[2] += floorPlane.Normal() * next_y;
			vStepTreadList[3] += floorPlane.Normal() * next_y;
		}
		PolygonPtr pStepTreadPolygon = CreatePolygon(vStepTreadList, bFlip, pBasePolygon);
		if (pStepTreadPolygon)
			out.polygons.push_back(pStepTreadPolygon);

		if (i >= nCompleteStepNum - 1)
		{
			if (pStepTreadPolygon)
				out.pCapPolygon = pStepTreadPolygon;
		}

		if (i == nCompleteStepNum)
		{
			vSideList[0].push_back(vStepTreadList[3]);
			if (bWidthIsLonger)
				vSideList[0].push_back(floorPlane.P2W(BrushVec2(next_x, startSpotPos.y)));
			else
				vSideList[0].push_back(floorPlane.P2W(BrushVec2(startSpotPos.x, next_z)));
			PolygonPtr pSidePolygon0 = CreatePolygon(vSideList[0], bFlip, pBasePolygon);
			if (pSidePolygon0)
			{
				out.polygons.push_back(pSidePolygon0);
				out.polygonsNeedPostProcess.push_back(pSidePolygon0);
			}

			vSideList[1].push_back(vStepTreadList[2]);
			if (bWidthIsLonger)
				vSideList[1].push_back(floorPlane.P2W(BrushVec2(next_x, endSpotPos.y)));
			else
				vSideList[1].push_back(floorPlane.P2W(BrushVec2(endSpotPos.x, next_z)));
			PolygonPtr pSidePolygon1 = CreatePolygon(vSideList[1], !bFlip, pBasePolygon);
			if (pSidePolygon1)
			{
				out.polygons.push_back(pSidePolygon1);
				out.polygonsNeedPostProcess.push_back(pSidePolygon1);
			}

			vBackList.push_back(vSideList[0][vSideList[0].size() - 1]);
			vBackList.push_back(vSideList[0][vSideList[0].size() - 2]);
			vBackList.push_back(vSideList[1][vSideList[1].size() - 2]);
			vBackList.push_back(vSideList[1][vSideList[1].size() - 1]);
			PolygonPtr pBackPolygon = CreatePolygon(vBackList, bFlip, pBasePolygon);
			if (pBackPolygon)
			{
				out.polygons.push_back(pBackPolygon);
				out.polygonsNeedPostProcess.push_back(pBackPolygon);
			}

			vBottomList.push_back(vBackList[0]);
			vBottomList.push_back(vBackList[3]);
			out.polygons.push_back(CreatePolygon(vBottomList, bFlip, pBasePolygon));
		}
	}
}

void StairTool::UpdateShapeWithBoundaryCheck(bool bUpdateUIs)
{
	UpdateShape(bUpdateUIs);
}

void StairTool::UpdateShape(bool bUpdateUIs)
{
	if (m_Phase == eBoxPhase_PlaceStartingPoint)
		return;

	m_v[0] = GetStartSpotPos();
	m_v[1] = GetCurrentSpotPos();

	BrushVec2 p0 = GetStartSpot().m_Plane.W2P(m_v[0]);
	BrushVec2 p1 = GetStartSpot().m_Plane.W2P(m_v[1]);
	float fWidth = p1.x - p0.x;
	float fDepth = p1.y - p0.y;

	m_bWidthIsLonger = m_Phase == eBoxPhase_DrawRectangle ? std::abs(fWidth) >= std::abs(fDepth) : m_bWidthIsLonger;

	if ((BrushFloat)m_BoxParameter.m_Height <= kDesignerEpsilon)
	{
		__super::UpdateShape(false);
		return;
	}

	OutputStairParameter output;
	CreateStair(
	  m_v[0],
	  m_v[1],
	  fWidth,
	  fDepth,
	  (BrushFloat)m_BoxParameter.m_Height,
	  GetStartSpot().m_Plane,
	  m_StairParameter.m_StepRise,
	  m_bWidthIsLonger,
	  m_StairParameter.m_bMirror,
	  m_StairParameter.m_bRotation90Degree,
	  GetStartSpot().m_pPolygon,
	  output);

	m_pCapPolygon = output.pCapPolygon;

	MODEL_SHELF_RECONSTRUCTOR(GetModel());
	GetModel()->SetShelf(eShelf_Construction);
	GetModel()->Clear();

	m_PolygonsNeedPostProcess = output.polygonsNeedPostProcess;
	for (int i = 0, iPolygonCount(output.polygons.size()); i < iPolygonCount; ++i)
	{
		DESIGNER_ASSERT(output.polygons[i]);
		if (output.polygons[i])
			AddPolygonWithSubMatID(output.polygons[i]);
	}

	ApplyPostProcess(ePostProcess_Mirror);
	CompileShelf(eShelf_Construction);

	if (bUpdateUIs)
	{
		m_RectangleParameter.m_Width = std::abs(p0.x - p1.x);
		m_RectangleParameter.m_Depth = std::abs(p0.y - p1.y);
		DesignerSession::GetInstance()->signalDesignerEvent(eDesignerNotify_SubtoolOptionChanged, nullptr);
	}
}

void StairTool::RegisterShape(PolygonPtr pFloorPolygon)
{
	if (!GetModel() || GetModel()->IsEmpty(eShelf_Construction))
		return;

	MODEL_SHELF_RECONSTRUCTOR(GetModel());

	if (m_bIsOverOpposite)
	{
		UpdateShape();
		s_SnappingHelper.ApplyOppositePolygons(m_pCapPolygon, ePP_Pull, true);
	}

	for (int k = 0, iCount(m_PolygonsNeedPostProcess.size()); k < iCount; ++k)
	{
		PolygonPtr pPostPolygon = m_PolygonsNeedPostProcess[k];
		PolygonPtr pFlippedPostPolygon = pPostPolygon->Clone()->Flip();

		GetModel()->SetShelf(eShelf_Base);
		std::vector<PolygonPtr> intersectedPolygons;

		for (int i = 0, iPolygonCount(GetModel()->GetPolygonCount()); i < iPolygonCount; ++i)
		{
			PolygonPtr pPolygon = GetModel()->GetPolygon(i);
			if (Polygon::HasIntersection(pPolygon, pFlippedPostPolygon))
				intersectedPolygons.push_back(pPolygon);
		}

		if (!intersectedPolygons.empty())
		{
			GetModel()->SetShelf(eShelf_Construction);
			GetModel()->RemovePolygon(GetModel()->QueryEquivalentPolygon(pPostPolygon));

			for (int i = 0, iPolygonCount(intersectedPolygons.size()); i < iPolygonCount; ++i)
			{
				PolygonPtr pCopiedFlippedPostPolygon = pFlippedPostPolygon->Clone();
				pFlippedPostPolygon->Subtract(intersectedPolygons[i]);
				intersectedPolygons[i]->Subtract(pCopiedFlippedPostPolygon);
				intersectedPolygons[i]->ResetUVs();
			}

			if (pFlippedPostPolygon->IsValid() && !pFlippedPostPolygon->IsOpen())
				GetModel()->AddPolygon(pFlippedPostPolygon->Flip());
		}
	}

	ShapeTool::RegisterShape(GetStartSpot().m_pPolygon);
}
}

REGISTER_DESIGNER_TOOL_WITH_PROPERTYTREE_PANEL_AND_COMMAND(eDesigner_Stair, eToolGroup_Shape, "Stair", StairTool,
                                                           stair, "runs stair tool", "designer.stair")

