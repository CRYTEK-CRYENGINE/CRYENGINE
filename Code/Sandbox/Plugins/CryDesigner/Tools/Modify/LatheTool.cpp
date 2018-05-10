// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "LatheTool.h"
#include "DesignerEditor.h"
#include "Tools/Select/SelectTool.h"
#include "Viewport.h"
#include "Core/Helper.h"
#include "Core/LoopPolygons.h"

namespace Designer
{
void LatheTool::Enter()
{
	__super::Enter();
	m_bChoosePathPolygonPhase = false;
}

bool LatheTool::OnLButtonDown(
  CViewport* view,
  UINT nFlags,
  CPoint point)
{
	if (m_Phase != eMTP_ChooseMoveToTargetPoint)
	{
		if (m_Phase == eMTP_ChooseUpPoint)
		{
			GetIEditor()->GetIUndoManager()->Begin();
			GetModel()->RecordUndo("Designer : Lathe Tool", GetBaseObject());
		}
		return __super::OnLButtonDown(view, nFlags, point);
	}

	PolygonPtr profile = m_pSelectedPolygon;
	PolygonPtr path = m_pTargetPolygon;

	ELatheErrorCode errorCode = CreateShapeAlongPath(profile, path);
	if (errorCode != eLEC_Success)
	{
		switch (errorCode)
		{
		case eLEC_NoPath:
			MessageBox("Warning", "A polygon has to be selected to be used as a path.");
			break;
		case eLEC_InappropriateProfileShape:
			MessageBox("Warning", "The profile polygon is not appropriate.");
			break;
		case eLEC_ProfileShapeTooBig:
			MessageBox("Warning", "The profile polygon is too big or located at a wrong position. You should reduce the width of the profile face or move it.");
			break;
		case eLEC_ShouldBeAtFirstPointInOpenPolygon:
			MessageBox("Warning", "The profile polygon needs to be at the starting position of a segment.");
			break;
		}
		GetIEditor()->GetIUndoManager()->Accept("Designer : Lathe Tool");
		GetIEditor()->GetIUndoManager()->Undo();
	}
	else
	{
		GetModel()->RemovePolygon(profile);
		GetModel()->RemovePolygon(path);
		UpdateSurfaceInfo();
		ApplyPostProcess();
		GetIEditor()->GetIUndoManager()->Accept("Designer : Lathe Tool");
	}

	GetDesigner()->SwitchToPrevTool();

	return true;
}

bool LatheTool::OnMouseMove(
  CViewport* view,
  UINT nFlags,
  CPoint point)
{
	if (!m_bChoosePathPolygonPhase)
		return __super::OnMouseMove(view, nFlags, point);
	return true;
}

void LatheTool::Display(DisplayContext& dc)
{
	if (!m_bChoosePathPolygonPhase)
		MagnetTool::Display(dc);
	else
		SelectTool::Display(dc);
}

std::vector<Vertex> LatheTool::ExtractPath(
  PolygonPtr pPathPolygon,
  bool& bOutClosed)
{
	std::vector<Vertex> vPath;
	bOutClosed = true;

	if (pPathPolygon->IsOpen())
	{
		pPathPolygon->GetLinkedVertices(vPath);
		bOutClosed = false;
	}
	else
	{
		std::vector<PolygonPtr> outSeparatedPolygons = pPathPolygon->GetLoops()->GetOuterPolygons();
		if (outSeparatedPolygons.size() == 1)
			outSeparatedPolygons[0]->GetLinkedVertices(vPath);
	}

	return vPath;
}

std::vector<BrushPlane> LatheTool::CreatePlanesAtEachPointOfPath(
  const std::vector<Vertex>& vPath,
  bool bPathClosed)
{
	std::vector<BrushPlane> planeAtEveryIntersection;
	int iPathCount = vPath.size();
	planeAtEveryIntersection.resize(iPathCount);

	int nPathCount = bPathClosed ? iPathCount : iPathCount - 2;
	for (int i = 0; i < nPathCount; ++i)
	{
		const BrushVec3& vCommon = vPath[(i + 1) % iPathCount].pos;
		BrushVec3 vDir = (vCommon - vPath[i].pos).GetNormalized();
		BrushVec3 vDirNext = (vPath[(i + 2) % iPathCount].pos - vCommon).GetNormalized();
		planeAtEveryIntersection[(i + 1) % iPathCount] = BrushPlane(vCommon, vCommon - vDir.Cross(vDirNext), vCommon - vDir + vDirNext);
	}

	if (!bPathClosed)
	{
		BrushVec3 vDir = (vPath[0].pos - vPath[1].pos).GetNormalized();
		planeAtEveryIntersection[0] = BrushPlane(vDir, -vDir.Dot(vPath[0].pos));
		vDir = (vPath[iPathCount - 1].pos - vPath[iPathCount - 2].pos).GetNormalized();
		planeAtEveryIntersection[iPathCount - 1] = BrushPlane(vDir, -vDir.Dot(vPath[iPathCount - 1].pos));
	}

	return planeAtEveryIntersection;
}

void LatheTool::AddPolygonToDesigner(
  Model* pModel,
  const std::vector<BrushVec3>& vList,
  PolygonPtr pInitPolygon, bool bFlip)
{
	PolygonPtr pPolygon = pInitPolygon->Clone();

	for (int i = 0, iVertexCount(vList.size()); i < iVertexCount; ++i)
		pPolygon->SetPos(i, vList[i]);

	BrushPlane plane;
	int id = DesignerSession::GetInstance()->GetCurrentSubMatID();

	if (pPolygon->GetComputedPlane(plane))
	{
		pPolygon->SetPlane(plane);
		if (bFlip)
			pPolygon->Flip();
		pPolygon->SetSubMatID(id);
		GetModel()->AddPolygon(pPolygon, false);
	}
}

bool LatheTool::QueryPolygonsOnPlane(
  Model* pModel,
  const BrushPlane& plane,
  std::vector<PolygonPtr>& outPolygons)
{
	bool bAdded = false;
	for (int i = 0, iPolygonCount(pModel->GetPolygonCount()); i < iPolygonCount; ++i)
	{
		PolygonPtr polygon = pModel->GetPolygon(i);
		if (polygon->GetPlane().Normal().IsEquivalent(plane.Normal(), 0.005f) &&
		    std::abs(polygon->GetPlane().Distance() - plane.Distance()) < 0.005f)
		{
			bAdded = true;
			outPolygons.push_back(polygon);
		}
	}
	return bAdded;
}

bool LatheTool::GluePolygons(
  PolygonPtr pPathPolygon,
  const std::vector<PolygonPtr>& polygons)
{
	for (int i = 0, iPolygonCount(polygons.size()); i < iPolygonCount; ++i)
	{
		BrushPlane invertedPlane = polygons[i]->GetPlane().GetInverted();

		std::vector<PolygonPtr> candidatedPolygons;
		if (!QueryPolygonsOnPlane(GetModel(), invertedPlane, candidatedPolygons) || candidatedPolygons.empty())
			continue;

		PolygonPtr pFlipedPolygon = polygons[i]->Clone()->Flip();
		bool bSubtracted = false;
		std::vector<std::pair<PolygonPtr, PolygonPtr>> intersectedPolygons;
		for (int k = 0, iCandidateCount(candidatedPolygons.size()); k < iCandidateCount; ++k)
		{
			if (candidatedPolygons[k]->IsOpen())
				continue;
			PolygonPtr pCloneCandidate = candidatedPolygons[k]->Clone();
			pCloneCandidate->SetPlane(pFlipedPolygon->GetPlane());
			if (Polygon::HasIntersection(pCloneCandidate, pFlipedPolygon) == eIT_Intersection)
				intersectedPolygons.push_back(std::pair<PolygonPtr, PolygonPtr>(candidatedPolygons[k], pCloneCandidate));
		}

		if (intersectedPolygons.empty())
			continue;

		for (int k = 0, iIntersectCount(intersectedPolygons.size()); k < iIntersectCount; ++k)
		{
			if (pPathPolygon == intersectedPolygons[k].first)
				continue;
			bSubtracted = true;
			intersectedPolygons[k].second->Subtract(pFlipedPolygon);
			intersectedPolygons[k].second->ResetUVs();
			*intersectedPolygons[k].first = *intersectedPolygons[k].second;
		}

		if (bSubtracted)
		{
			GetModel()->RemovePolygon(polygons[i]);
			return true;
		}
	}
	return false;
}

ELatheErrorCode LatheTool::CreateShapeAlongPath(
  PolygonPtr pInitProfilePolygon,
  PolygonPtr pPathPolygon)
{
	MODEL_SHELF_RECONSTRUCTOR(GetModel());

	if (!pInitProfilePolygon || !pPathPolygon)
		return eLEC_NoPath;

	bool bClosed = true;
	std::vector<Vertex> vPath = ExtractPath(pPathPolygon, bClosed);
	if (vPath.empty())
		return eLEC_NoPath;

	std::vector<BrushPlane> planeAtEveryIntersection = CreatePlanesAtEachPointOfPath(vPath, bClosed);

	int iPathCount = vPath.size();
	int nStartIndex = 0;
	BrushFloat fNearestDist = (BrushFloat)3e10;
	BrushVec3 vProfilePos = pInitProfilePolygon->GetRepresentativePosition();
	for (int i = 0; i < iPathCount; ++i)
	{
		BrushFloat fDist = vProfilePos.GetDistance(vPath[i].pos);
		if (fDist < fNearestDist)
		{
			fNearestDist = fDist;
			nStartIndex = i;
		}
	}

	if (!bClosed && nStartIndex != 0)
		return eLEC_ShouldBeAtFirstPointInOpenPolygon;

	int nVertexCount = pInitProfilePolygon->GetVertexCount();
	int iEdgeCount = pInitProfilePolygon->GetEdgeCount();

	std::vector<BrushVec3> prevVertices(nVertexCount);
	std::vector<BrushPlane> prevPlanes(iEdgeCount);

	BrushVec3 vEdgeDir = (vPath[(nStartIndex + 1) % iPathCount].pos - vPath[nStartIndex].pos).GetNormalized();
	for (int k = 0; k < nVertexCount; ++k)
	{
		const BrushVec3& v = pInitProfilePolygon->GetPos(k);

		bool bHitTest0 = planeAtEveryIntersection[nStartIndex].HitTest(BrushRay(v, vEdgeDir), NULL, &prevVertices[k]);
		DESIGNER_ASSERT(bHitTest0);
		if (!bHitTest0)
			return eLEC_InappropriateProfileShape;
	}

	for (int k = 0; k < iEdgeCount; ++k)
		prevPlanes[k] = BrushPlane(BrushVec3(0, 0, 0), 0);

	GetModel()->SetShelf(eShelf_Construction);

	if (!bClosed)
		AddPolygonToDesigner(GetModel(), prevVertices, pInitProfilePolygon, false);

	int nPathCount = bClosed ? iPathCount : iPathCount - 1;
	for (int i = 0; i < nPathCount; ++i)
	{
		int nPathIndex = (nStartIndex + i) % iPathCount;
		int nNextPathIndex = (nPathIndex + 1) % iPathCount;

		const BrushPlane& planeNext = planeAtEveryIntersection[nNextPathIndex];
		vEdgeDir = (vPath[nNextPathIndex].pos - vPath[nPathIndex].pos).GetNormalized();

		std::vector<BrushVec3> vertices(nVertexCount);
		for (int k = 0; k < nVertexCount; ++k)
		{
			if (planeNext.Distance(prevVertices[k]) > kDesignerEpsilon)
			{
				GetModel()->Clear();
				ApplyPostProcess(ePostProcess_Mesh);
				return eLEC_ProfileShapeTooBig;
			}

			bool bHitTest0 = planeNext.HitTest(BrushRay(prevVertices[k], vEdgeDir), NULL, &vertices[k]);
			DESIGNER_ASSERT(bHitTest0);
			if (!bHitTest0)
				continue;
		}

		for (int k = 0; k < iEdgeCount; ++k)
		{
			const IndexPair& e = pInitProfilePolygon->GetEdgeIndexPair(k);

			std::vector<BrushVec3> vList(4);

			vList[0] = prevVertices[e.m_i[1]];
			vList[1] = prevVertices[e.m_i[0]];
			vList[2] = vertices[e.m_i[0]];
			vList[3] = vertices[e.m_i[1]];

			PolygonPtr pSidePolygon = new Polygon(vList);

			if (prevPlanes[k].Normal().IsZero())
			{
				prevPlanes[k] = pSidePolygon->GetPlane();
			}
			else
			{
				BrushFloat d0 = std::abs(prevPlanes[k].Distance(vPath[nPathIndex].pos));
				BrushFloat d1 = std::abs(prevPlanes[k].Distance(vPath[nNextPathIndex].pos));
				if (std::abs(d0 - d1) < kDesignerEpsilon && pSidePolygon->GetPlane().IsSameFacing(prevPlanes[k]))
					pSidePolygon->SetPlane(prevPlanes[k]);
			}

			AddPolygonWithSubMatID(pSidePolygon);
		}

		prevVertices = vertices;
	}

	if (!bClosed)
		AddPolygonToDesigner(GetModel(), prevVertices, pInitProfilePolygon, true);

	std::vector<PolygonPtr> newPolygons;
	for (int i = 0, iPolygonCount(GetModel()->GetPolygonCount()); i < iPolygonCount; ++i)
		newPolygons.push_back(GetModel()->GetPolygon(i));

	GetModel()->SetShelf(eShelf_Base);
	GetModel()->RemovePolygon(pInitProfilePolygon);
	GetModel()->MovePolygonsBetweenShelves(eShelf_Construction, eShelf_Base);

	bool bSubtractedFloor = GluePolygons(pPathPolygon, newPolygons);

	ElementSet* pSelected = DesignerSession::GetInstance()->GetSelectedElements();
	pSelected->Clear();
	for (int i = 0, iCount(newPolygons.size()); i < iCount; ++i)
		pSelected->Add(Element(GetBaseObject(), newPolygons[i]));

	return eLEC_Success;
}
}

REGISTER_DESIGNER_TOOL_AND_COMMAND(eDesigner_Lathe, eToolGroup_Modify, "Lathe", LatheTool,
                                   lathe, "runs lathe tool", "designer.lathe")

