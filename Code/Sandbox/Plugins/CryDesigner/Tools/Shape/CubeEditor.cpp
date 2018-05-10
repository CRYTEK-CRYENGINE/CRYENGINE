// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Core/BSPTree3D.h"
#include "ViewManager.h"
#include "Util/PrimitiveShape.h"
#include "Material/MaterialManager.h"
#include "Util/HeightManipulator.h"
#include "CubeEditor.h"
#include "DesignerEditor.h"
#include "ToolFactory.h"
#include "Core/Helper.h"
#include "Core/LoopPolygons.h"
#include "DesignerSession.h"
#include "Objects/DisplayContext.h"

namespace Designer
{
void CubeEditor::Enter()
{
	__super::Enter();
	m_BrushAABB = AABB(Vec3(0, 0, 0), Vec3(0, 0, 0));
	m_CurMousePos = CPoint(-1, -1);
}

void CubeEditor::Leave()
{
	__super::Leave();
	UpdateSurfaceInfo();
}

BrushVec2 ConvertTwoPositionsToDirInViewport(CViewport* view, const Vec3& v0, const Vec3& v1)
{
	POINT p0 = view->WorldToView(v0);
	POINT p1 = view->WorldToView(v1);
	BrushVec2 vDir = BrushVec2(BrushFloat(p1.x - p0.x), BrushFloat(p1.y - p0.y));
	if (vDir.x != 0 || vDir.y != 0)
		vDir.Normalize();
	return vDir;
}

bool CubeEditor::OnLButtonDown(CViewport* view, UINT nFlags, CPoint point)
{
	if (nFlags & MK_SHIFT)
	{
		m_DS.m_bPressingShift = true;
		BrushVec3 vPickedPos, vNormal;
		GetBrushPos(view, point, m_DS.m_StartingPos, vPickedPos, &m_DS.m_StartingNormal);
		m_BrushAABB = GetBrushBox(view, point);
	}
	else
	{
		m_DS.m_bPressingShift = false;
	}

	return true;
}

bool CubeEditor::OnLButtonUp(CViewport* view, UINT nFlags, CPoint point)
{
	if (!m_DS.m_bPressingShift)
	{
		m_BrushAABB = GetBrushBox(view, point);
		AddBrush(m_BrushAABB);
	}

	m_DS.m_bPressingShift = false;
	m_CurMousePos = point;

	if (m_BrushAABBs.empty() || GetBaseObject() == NULL)
		return true;

	CUndo undo("Designer : CubeEditor");
	GetModel()->RecordUndo("Designer : CubeEditor", GetBaseObject());

	bool bEmptyDesigner = GetModel()->IsEmpty();

	for (int i = 0, iBrushAABBCount(m_BrushAABBs.size()); i < iBrushAABBCount; ++i)
	{
		if (GetEditMode() == eCubeEditorMode_Add)
			AddCube(m_BrushAABBs[i]);
		else if (GetEditMode() == eCubeEditorMode_Remove)
			RemoveCube(m_BrushAABBs[i]);
		else if (GetEditMode() == eCubeEditorMode_Paint)
			PaintCube(m_BrushAABBs[i]);
	}

	if (bEmptyDesigner)
	{
		AABB aabb = GetModel()->GetBoundBox();
		PivotToPos(GetBaseObject(), GetModel(), aabb.min);
	}

	ApplyPostProcess();

	m_BrushAABBs.clear();

	return true;
}

BrushFloat CubeEditor::GetCubeSize() const
{
	return m_cubeSize;
}

void CubeEditor::SetCubeSize(float csize)
{
	if (csize != m_cubeSize)
	{
		m_cubeSize = csize;
	}

	DesignerSession::GetInstance()->signalDesignerEvent(eDesignerNotify_SubtoolOptionChanged, nullptr);
}

bool CubeEditor::IsAddMode() const
{
	return m_mode == eCubeEditorMode_Add;
}

bool CubeEditor::IsRemoveMode() const
{
	return m_mode == eCubeEditorMode_Remove;
}

bool CubeEditor::IsPaintMode() const
{
	return m_mode == eCubeEditorMode_Paint;
}

void CubeEditor::SelectPrevBrush()
{
	switch (m_mode)
	{
	case eCubeEditorMode_Add:
		m_mode = eCubeEditorMode_Paint;
		break;
	case eCubeEditorMode_Remove:
		m_mode = eCubeEditorMode_Add;
		break;
	case eCubeEditorMode_Paint:
		m_mode = eCubeEditorMode_Remove;
		break;
	case eCubeEditorMode_Invalid:
		m_mode = eCubeEditorMode_Add;
		break;
	}

	DesignerSession::GetInstance()->signalDesignerEvent(eDesignerNotify_SubtoolOptionChanged, nullptr);
}

void CubeEditor::SelectNextBrush()
{
	switch (m_mode)
	{
	case eCubeEditorMode_Add:
		m_mode = eCubeEditorMode_Remove;
		break;
	case eCubeEditorMode_Remove:
		m_mode = eCubeEditorMode_Paint;
		break;
	case eCubeEditorMode_Paint:
		m_mode = eCubeEditorMode_Add;
		break;
	case eCubeEditorMode_Invalid:
		m_mode = eCubeEditorMode_Add;
		break;
	}
	DesignerSession::GetInstance()->signalDesignerEvent(eDesignerNotify_SubtoolOptionChanged, nullptr);
}

bool CubeEditor::IsSideMerged() const
{
	return m_bSideMerged;
}

void CubeEditor::SetSideMerged(bool bsideMerged)
{
	if (m_bSideMerged != bsideMerged)
	{
		m_bSideMerged = bsideMerged;
		DesignerSession::GetInstance()->signalDesignerEvent(eDesignerNotify_SubtoolOptionChanged, nullptr);
	}
}

BrushVec3 CubeEditor::Snap(const BrushVec3& vPos) const
{
	BrushVec3 vCorrectedPos(CorrectVec3(vPos));
	BrushFloat fCubeSize = GetCubeSize();

	std::vector<BrushFloat> fCutUnits;
	if (std::abs(fCubeSize - (BrushFloat)0.125) < kDesignerEpsilon)
	{
		fCutUnits.push_back((BrushFloat)0.125);
		fCutUnits.push_back((BrushFloat)0.25);
		fCutUnits.push_back((BrushFloat)0.375);
		fCutUnits.push_back((BrushFloat)0.5);
		fCutUnits.push_back((BrushFloat)0.625);
		fCutUnits.push_back((BrushFloat)0.75);
		fCutUnits.push_back((BrushFloat)0.875);
	}
	else if (std::abs(fCubeSize - (BrushFloat)0.25) < kDesignerEpsilon)
	{
		fCutUnits.push_back((BrushFloat)0.25);
		fCutUnits.push_back((BrushFloat)0.5);
		fCutUnits.push_back((BrushFloat)0.75);
	}
	else if (std::abs(fCubeSize - (BrushFloat)0.5) < kDesignerEpsilon)
	{
		fCutUnits.push_back((BrushFloat)0.5);
	}

	for (int i = 0; i < 3; ++i)
	{
		for (int k = 0, iCutUnitCount(fCutUnits.size()); k < iCutUnitCount; ++k)
		{
			BrushFloat fGreatedLessThan = std::floor(vCorrectedPos[i]) + fCutUnits[k];
			if (std::abs(vCorrectedPos[i] - fGreatedLessThan) < kDesignerEpsilon)
			{
				vCorrectedPos[i] = fGreatedLessThan;
				break;
			}
		}
	}

	BrushVec3 snapped;
	snapped.x = std::floor(vCorrectedPos.x / fCubeSize) * fCubeSize;
	snapped.y = std::floor(vCorrectedPos.y / fCubeSize) * fCubeSize;
	snapped.z = std::floor(vCorrectedPos.z / fCubeSize) * fCubeSize;

	return snapped;
}

bool CubeEditor::GetBrushPos(CViewport* view, CPoint point, BrushVec3& outPos, BrushVec3& outPickedPos, BrushVec3* pOutNormal)
{
	BrushPlane plane;
	if (GetModel()->QueryPosition(GetDesigner()->GetRay(), outPickedPos, &plane))
	{
		if (pOutNormal)
			*pOutNormal = plane.Normal();
		outPos = Snap(outPickedPos);
		return true;
	}

	if (!IsAddMode())
		return false;

	Vec3 vPickedPosInWorld;
	if (PickPosFromWorld(view, point, vPickedPosInWorld))
	{
		BrushVec3 vLocalPickedPos = GetWorldTM().GetInverted().TransformPoint(ToBrushVec3(vPickedPosInWorld));
		if (pOutNormal)
			*pOutNormal = BrushVec3(0, 0, 1);
		outPickedPos = ToVec3(vPickedPosInWorld);
		outPos = Snap(vLocalPickedPos);
		return true;
	}

	return false;
}

bool CubeEditor::OnMouseMove(CViewport* view, UINT nFlags, CPoint point)
{
	if (!m_DS.m_bPressingShift)
		m_BrushAABB = GetBrushBox(view, point);

	if (nFlags & MK_LBUTTON)
	{
		if (m_DS.m_bPressingShift)
		{
			BrushVec3 vWorldPivot = GetWorldTM().TransformPoint(m_DS.m_StartingPos);
			BrushVec2 vRightDirInViewport = ConvertTwoPositionsToDirInViewport(view, vWorldPivot, vWorldPivot + GetBaseObject()->GetWorldTM().GetColumn0());
			BrushVec2 vForwardDirInViewport = ConvertTwoPositionsToDirInViewport(view, vWorldPivot, vWorldPivot + GetBaseObject()->GetWorldTM().GetColumn1());
			BrushVec2 vUpDirInViewport = ConvertTwoPositionsToDirInViewport(view, vWorldPivot, vWorldPivot + GetBaseObject()->GetWorldTM().GetColumn2());

			CPoint startPosInViewport = view->WorldToView(vWorldPivot);

			BrushVec2 dir = BrushVec2(point.x - startPosInViewport.x, point.y - startPosInViewport.y).GetNormalized();

			BrushFloat angleToUp = dir.Dot(vUpDirInViewport);
			BrushFloat angleToRight = dir.Dot(vRightDirInViewport);
			BrushFloat angleToForward = dir.Dot(vForwardDirInViewport);

			BrushFloat fInvertUp = 1, fInvertRight = 1, fInvertForward = 1;
			if (angleToUp < 0)
			{
				angleToUp = dir.Dot(-vUpDirInViewport);
				fInvertUp = -1;
			}
			if (angleToRight < 0)
			{
				angleToRight = dir.Dot(-vRightDirInViewport);
				fInvertRight = -1;
			}
			if (angleToForward < 0)
			{
				angleToForward = dir.Dot(-vForwardDirInViewport);
				fInvertForward = -1;
			}

			if (angleToUp > angleToRight && angleToUp > angleToForward)
				m_DS.m_StraightDir = GetWorldTM().GetColumn2() * fInvertUp;
			else if (angleToRight > angleToUp && angleToRight > angleToForward)
				m_DS.m_StraightDir = GetWorldTM().GetColumn0() * fInvertRight;
			else
				m_DS.m_StraightDir = GetWorldTM().GetColumn1() * fInvertForward;

			HeightManipulator adjustHeightHelper;
			adjustHeightHelper.Init(BrushPlane(m_DS.m_StraightDir, -m_DS.m_StraightDir.Dot(m_DS.m_StartingPos)), m_DS.m_StartingPos);
			BrushFloat fLength = adjustHeightHelper.Update(GetWorldTM(), view, GetDesigner()->GetRay());
			BrushFloat fCubeSize = GetCubeSize();

			int nNumber = fLength / fCubeSize;
			m_BrushAABBs.clear();
			for (int i = 0; i <= nNumber; ++i)
			{
				BrushVec3 vPickedPos = m_DS.m_StartingPos + m_DS.m_StraightDir * fCubeSize * i;
				AddBrush(GetBrushBox(Snap(vPickedPos), vPickedPos, m_DS.m_StartingNormal));
			}

			m_DS.m_StraightDir *= fLength;
		}
		AddBrush(m_BrushAABB);
	}
	m_CurMousePos = point;
	return true;
}

bool CubeEditor::OnMouseWheel(CViewport* view, UINT nFlags, CPoint point)
{
	if (!(nFlags & MK_CONTROL))
		return false;

	short zDelta = (short)point.x;

	if (zDelta > 0)
		SelectPrevBrush();
	else if (zDelta < 0)
		SelectNextBrush();

	if (m_CurMousePos.x == -1)
		m_CurMousePos = point;

	m_BrushAABB = GetBrushBox(view, m_CurMousePos);
	return true;
}

void CubeEditor::Display(DisplayContext& dc)
{
	__super::Display(dc);
	DisplayBrush(dc);
}

void CubeEditor::DisplayBrush(DisplayContext& dc)
{
	for (int i = 0, iBrushCount(m_BrushAABBs.size()); i < iBrushCount; ++i)
	{
		if (m_BrushAABBs[i].IsReset())
			continue;
		dc.SetColor(0.43f, 0.43f, 0, 0.43f);
		dc.DrawSolidBox(ToVec3(m_BrushAABBs[i].min), ToVec3(m_BrushAABBs[i].max));
	}

	if (!m_BrushAABB.IsReset())
	{
		dc.SetColor(0.8392f, 0.58f, 0, 0.43f);
		dc.DrawSolidBox(ToVec3(m_BrushAABB.min), ToVec3(m_BrushAABB.max));
	}
}

AABB CubeEditor::GetBrushBox(CViewport* view, CPoint point)
{
	BrushVec3 vBrushPos, vPickedPos, vNormal;
	if (!GetBrushPos(view, point, vBrushPos, vPickedPos, &vNormal))
	{
		AABB brushAABB;
		brushAABB.Reset();
		return brushAABB;
	}
	return GetBrushBox(vBrushPos, vPickedPos, vNormal);
}

AABB CubeEditor::GetBrushBox(const BrushVec3& vSnappedPos, const BrushVec3& vPickedPos, const BrushVec3& vNormal)
{
	AABB brushAABB;
	brushAABB.Reset();

	int nElement = -1;
	if (std::abs(vNormal.x) > (BrushFloat)0.999)
		nElement = 0;
	else if (std::abs(vNormal.y) > (BrushFloat)0.999)
		nElement = 1;
	else if (std::abs(vNormal.z) > (BrushFloat)0.999)
		nElement = 2;

	const float fCubeSize = ToFloat(GetCubeSize());
	Vec3 vSize(fCubeSize, fCubeSize, fCubeSize);

	AABB aabb;
	aabb.Reset();
	aabb.Add(ToVec3(vSnappedPos));
	aabb.Add(ToVec3(vSnappedPos + vSize));
	aabb.Expand(Vec3(-kDesignerEpsilon, -kDesignerEpsilon, -kDesignerEpsilon));
	if (aabb.IsContainPoint(ToVec3(vPickedPos)))
		nElement = -1;

	if (nElement != -1)
	{
		int nSign = vNormal[nElement] > 0 ? 1 : -1;
		if (GetEditMode() != eCubeEditorMode_Add)
			nSign = -nSign;
		vSize[nElement] = vSize[nElement] * nSign;
	}

	brushAABB.Add(ToVec3(vSnappedPos));
	brushAABB.Add(ToVec3(vSnappedPos + vSize));

	return brushAABB;
}

std::vector<PolygonPtr> CubeEditor::GetBrushPolygons(const AABB& aabb) const
{
	std::vector<PolygonPtr> brushPolygons;
	if (!aabb.IsReset())
	{
		PrimitiveShape bp;
		bp.CreateBox(aabb.min, aabb.max, &brushPolygons);

		int nMatID = DesignerSession::GetInstance()->GetCurrentSubMatID();
		for (int i = 0, iPolygonCount(brushPolygons.size()); i < iPolygonCount; ++i)
			brushPolygons[i]->SetSubMatID(nMatID);
	}
	return brushPolygons;
}

void CubeEditor::AddCube(const AABB& brushAABB)
{
	std::vector<PolygonPtr> brushPolygons = GetBrushPolygons(brushAABB);
	if (brushPolygons.empty())
		return;

	AABB expandedBrushAABB = brushAABB;
	const float kOffset = 0.001f;
	expandedBrushAABB.Expand(Vec3(kOffset, kOffset, kOffset));
	std::vector<PolygonPtr> intersectedPolygons;

	for (int i = 0, iPolygonCount(brushPolygons.size()); i < iPolygonCount; ++i)
	{
		if (brushPolygons[i] == NULL)
			continue;

		const BrushPlane& plane = brushPolygons[i]->GetPlane();
		std::vector<PolygonPtr> candidatePolygons;
		GetModel()->QueryPolygons(plane, candidatePolygons);
		if (!candidatePolygons.empty())
		{
			PolygonPtr pPolygon = brushPolygons[i];
			std::vector<PolygonPtr> touchedPolygons;
			GetModel()->QueryIntersectionByPolygon(pPolygon, touchedPolygons);
			if (!touchedPolygons.empty())
			{
				if (IsSideMerged())
					GetModel()->AddUnionPolygon(pPolygon);
				else
					GetModel()->AddSplitPolygon(pPolygon);
				continue;
			}
		}

		BrushPlane invPlane = plane.GetInverted();
		candidatePolygons.clear();
		GetModel()->QueryPolygons(invPlane, candidatePolygons);
		if (!candidatePolygons.empty())
		{
			PolygonPtr pInvertedPolygon = brushPolygons[i]->Clone()->Flip();
			std::vector<PolygonPtr> touchedPolygons;
			GetModel()->QueryIntersectionByPolygon(pInvertedPolygon, touchedPolygons);
			if (!touchedPolygons.empty())
			{
				for (int k = 0, iTouchedPolygonCount(touchedPolygons.size()); k < iTouchedPolygonCount; ++k)
				{
					if (expandedBrushAABB.ContainsBox(touchedPolygons[k]->GetExpandedBoundBox()))
					{
						GetModel()->RemovePolygon(touchedPolygons[k]);
						std::vector<PolygonPtr> outsidePolygons = touchedPolygons[k]->GetLoops()->GetOuterClones();
						for (int a = 0, iOutsidePolygonCount(outsidePolygons.size()); a < iOutsidePolygonCount; ++a)
							pInvertedPolygon->Subtract(outsidePolygons[a]);
					}
					else if (Polygon::HasIntersection(touchedPolygons[k], pInvertedPolygon) == eIT_Intersection)
					{
						std::vector<PolygonPtr> outsidePolygons = touchedPolygons[k]->GetLoops()->GetOuterClones();
						touchedPolygons[k]->Subtract(pInvertedPolygon);

						for (int a = 0, iOutsidePolygonCount(outsidePolygons.size()); a < iOutsidePolygonCount; ++a)
							pInvertedPolygon->Subtract(outsidePolygons[a]);

						if (touchedPolygons[k]->IsValid())
						{
							touchedPolygons[k]->ResetUVs();
							if (touchedPolygons[k]->GetLoops()->GetIslands().size() > 1)
								GetModel()->AddPolygon(touchedPolygons[k]);
						}
						else
						{
							GetModel()->RemovePolygon(touchedPolygons[k]);
						}
					}
				}
				if (pInvertedPolygon->IsValid() && !pInvertedPolygon->IsOpen())
				{
					pInvertedPolygon->Flip();
					pInvertedPolygon->ResetUVs();
					GetModel()->AddPolygon(pInvertedPolygon);
				}
				continue;
			}
		}

		if (IsSideMerged())
			GetModel()->AddUnionPolygon(brushPolygons[i]);
		else
			GetModel()->AddPolygon(brushPolygons[i]);
	}
}

void CubeEditor::RemoveCube(const AABB& brushAABB)
{
	std::vector<PolygonPtr> brushPolygons = GetBrushPolygons(brushAABB);
	if (brushPolygons.empty())
		return;

	std::vector<PolygonPtr> intersectedPolygons;
	if (!GetModel()->QueryIntersectedPolygonsByAABB(brushAABB, intersectedPolygons))
		return;

	if (intersectedPolygons.empty())
		return;

	std::vector<PolygonPtr> brushPolygonsForBSPTree;
	CopyPolygons(brushPolygons, brushPolygonsForBSPTree);
	_smart_ptr<BSPTree3D> pBrushBSP3D = new BSPTree3D(brushPolygonsForBSPTree);

	std::vector<PolygonPtr> boundaryPolygons;
	CopyPolygons(intersectedPolygons, boundaryPolygons);

	auto ii = boundaryPolygons.begin();
	for (; ii != boundaryPolygons.end(); )
	{
		bool bBoundaryPolygon = true;
		for (int k = 0, iBrushPolygonCount(brushPolygons.size()); k < iBrushPolygonCount; ++k)
		{
			if (brushPolygons[k]->GetPlane().GetInverted().IsEquivalent((*ii)->GetPlane()))
			{
				bBoundaryPolygon = false;
				break;
			}
		}
		if (!bBoundaryPolygon)
			ii = boundaryPolygons.erase(ii);
		else
			++ii;
	}

	if (boundaryPolygons.empty())
		return;

	_smart_ptr<BSPTree3D> pBoundaryBSPTree = new BSPTree3D(boundaryPolygons);

	Model model;
	auto iBrush = brushPolygons.begin();
	for (; iBrush != brushPolygons.end(); )
	{
		bool bSubtracted = false;
		auto ii = intersectedPolygons.begin();
		for (; ii != intersectedPolygons.end(); )
		{
			if (!(*iBrush)->IsPlaneEquivalent(*ii) || Polygon::HasIntersection(*iBrush, *ii) != eIT_Intersection)
			{
				++ii;
				continue;
			}
			bSubtracted = true;
			PolygonPtr pClone = (*ii)->Clone();
			(*ii)->Subtract(*iBrush);
			(*iBrush)->Subtract(pClone);
			GetModel()->RemovePolygon(*ii);
			if (!(*ii)->IsValid())
			{
				ii = intersectedPolygons.erase(ii);
			}
			else
			{
				model.AddUnionPolygon(*ii);
				++ii;
			}
		}

		if (bSubtracted && !(*iBrush)->IsValid())
			iBrush = brushPolygons.erase(iBrush);
		else
			++iBrush;
	}

	for (auto ii = intersectedPolygons.begin(); ii != intersectedPolygons.end(); )
	{
		if (brushAABB.ContainsBox((*ii)->GetExpandedBoundBox()))
		{
			bool bTouched = false;
			for (int i = 0, iBrushPolygonCount(brushPolygons.size()); i < iBrushPolygonCount; ++i)
			{
				BrushPlane invertedBrushPlane = brushPolygons[i]->GetPlane().GetInverted();
				if (invertedBrushPlane.IsEquivalent((*ii)->GetPlane()))
				{
					bTouched = true;
					break;
				}
			}
			if (!bTouched)
			{
				GetModel()->RemovePolygon(*ii);
				ii = intersectedPolygons.erase(ii);
			}
			else
			{
				++ii;
			}
		}
		else if (brushAABB.IsIntersectBox((*ii)->GetExpandedBoundBox()))
		{
			OutputPolygons output;
			pBrushBSP3D->GetPartitions(*ii, output);
			if (!output.negList.empty())
			{
				GetModel()->RemovePolygon(*ii);
				for (int i = 0, iPosCount(output.posList.size()); i < iPosCount; ++i)
					model.AddUnionPolygon(output.posList[i]);
				ii = intersectedPolygons.erase(ii);
			}
			else
			{
				++ii;
			}
		}
		else
		{
			++ii;
		}
	}

	for (int i = 0, iBrushPolygonCount(brushPolygons.size()); i < iBrushPolygonCount; ++i)
	{
		PolygonPtr pInvertedPolygon = brushPolygons[i]->Clone()->Flip();

		OutputPolygons output;
		pBoundaryBSPTree->GetPartitions(pInvertedPolygon, output);
		if (output.negList.empty())
			continue;

		if (!output.posList.empty() || !output.coSameList.empty() || !output.coDiffList.empty())
		{
			for (int k = 0, negListCount(output.negList.size()); k < negListCount; ++k)
				model.AddUnionPolygon(output.negList[k]);
		}
		else
		{
			model.AddUnionPolygon(pInvertedPolygon);
		}
	}

	for (int i = 0, iPolygonCount(model.GetPolygonCount()); i < iPolygonCount; ++i)
	{
		if (IsSideMerged() || !pBoundaryBSPTree->IsValidTree())
		{
			GetModel()->AddUnionPolygon(model.GetPolygon(i));
		}
		else
		{
			model.GetPolygon(i)->ResetUVs();
			GetModel()->AddPolygon(model.GetPolygon(i));
		}
	}
}

void CubeEditor::PaintCube(const AABB& brushAABB)
{
	std::vector<PolygonPtr> brushPolygons = GetBrushPolygons(brushAABB);
	if (brushPolygons.empty())
		return;

	std::vector<PolygonPtr> candidatePolygons;
	GetModel()->QueryIntersectedPolygonsByAABB(brushAABB, candidatePolygons);
	if (candidatePolygons.empty())
		return;

	int submatID = DesignerSession::GetInstance()->GetCurrentSubMatID();
	auto ii = candidatePolygons.begin();
	for (; ii != candidatePolygons.end(); )
	{
		if (brushAABB.ContainsBox((*ii)->GetExpandedBoundBox()))
		{
			(*ii)->SetSubMatID(submatID);
			ii = candidatePolygons.erase(ii);
		}
		else
		{
			++ii;
		}
	}

	if (candidatePolygons.empty())
		return;

	_smart_ptr<BSPTree3D> pBrushTree = new BSPTree3D(brushPolygons);

	Model designer[2];
	for (int i = 0, iCandidatePolygonCount(candidatePolygons.size()); i < iCandidatePolygonCount; ++i)
	{
		OutputPolygons output;
		pBrushTree->GetPartitions(candidatePolygons[i], output);

		std::vector<PolygonPtr> polygonsInside;
		polygonsInside.insert(polygonsInside.end(), output.negList.begin(), output.negList.end());
		polygonsInside.insert(polygonsInside.end(), output.coSameList.begin(), output.coSameList.end());
		polygonsInside.insert(polygonsInside.end(), output.coDiffList.begin(), output.coDiffList.end());

		if (polygonsInside.empty())
			continue;

		GetModel()->RemovePolygon(candidatePolygons[i]);
		for (int k = 0, iPosPolygonCount(output.posList.size()); k < iPosPolygonCount; ++k)
			designer[0].AddUnionPolygon(output.posList[k]);

		int submatID = DesignerSession::GetInstance()->GetCurrentSubMatID();
		for (int k = 0, iPolygonCountInside(polygonsInside.size()); k < iPolygonCountInside; ++k)
		{
			polygonsInside[k]->SetSubMatID(submatID);
			designer[1].AddUnionPolygon(polygonsInside[k]);
		}
	}

	for (int i = 0; i < 2; ++i)
	{
		for (int k = 0, iPolygonCount(designer[i].GetPolygonCount()); k < iPolygonCount; ++k)
			GetModel()->AddPolygon(designer[i].GetPolygon(k));
	}
}

ECubeEditorMode CubeEditor::GetEditMode() const
{
	return m_mode;
}

void CubeEditor::SetEditMode(ECubeEditorMode mode)
{
	if (mode != m_mode)
	{
		m_mode = mode;
		DesignerSession::GetInstance()->signalDesignerEvent(eDesignerNotify_SubtoolOptionChanged, nullptr);
	}
}

void CubeEditor::AddBrush(const AABB& aabb)
{
	if (aabb.IsReset())
		return;
	bool bHaveSame = false;
	for (int i = 0, iCount(m_BrushAABBs.size()); i < iCount; ++i)
	{
		if (Comparison::IsEquivalent(m_BrushAABBs[i].min, aabb.min) && Comparison::IsEquivalent(m_BrushAABBs[i].max, aabb.max))
		{
			bHaveSame = true;
			break;
		}
	}
	if (!bHaveSame)
		m_BrushAABBs.push_back(aabb);
}
}

#include "UIs/CubeEditorPanel.h"
REGISTER_DESIGNER_TOOL_WITH_PANEL_AND_COMMAND(eDesigner_CubeEditor, eToolGroup_Shape, "Cube Editor", CubeEditor, CubeEditorPanel,
                                              cube_editor, "runs cube editor", "designer.cube_editor")

