// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ExtrusionSnappingHelper.h"
#include "ViewManager.h"
#include "DesignerEditor.h"

namespace Designer
{
ExtrusionSnappingHelper s_SnappingHelper;

void ExtrusionSnappingHelper::SearchForOppositePolygons(PolygonPtr pCapPolygon)
{
	MODEL_SHELF_RECONSTRUCTOR(GetModel());
	GetModel()->SetShelf(eShelf_Base);

	for (int i = 0; i < 2; ++i)
	{
		// i == 0 --> Query push direction polygon
		// i == 1 --> Query pull direction polygon
		m_Opposites[i].Init();

		Opposite opposite;
		Opposite oppositeAdjancent;

		EFindOppositeFlag nFlag = i == 0 ? eFOF_PushDirection : eFOF_PullDirection;
		BrushFloat fScale = kDesignerEpsilon * 10.0f;

		opposite.bTouchAdjacent = false;
		opposite.resultOfFindingOppositePolygon = GetModel()->QueryOppositePolygon(
		  pCapPolygon,
		  nFlag,
		  0,
		  opposite.polygon,
		  opposite.nearestDistanceToPolygon);

		oppositeAdjancent.bTouchAdjacent = true;
		oppositeAdjancent.resultOfFindingOppositePolygon = GetModel()->QueryOppositePolygon(
		  pCapPolygon,
		  nFlag,
		  fScale,
		  oppositeAdjancent.polygon,
		  oppositeAdjancent.nearestDistanceToPolygon);

		if (opposite.resultOfFindingOppositePolygon != eFindOppositePolygon_None && oppositeAdjancent.resultOfFindingOppositePolygon != eFindOppositePolygon_None)
		{
			if (opposite.nearestDistanceToPolygon - oppositeAdjancent.nearestDistanceToPolygon <= kDesignerEpsilon)
				m_Opposites[i] = opposite;
			else
				m_Opposites[i] = oppositeAdjancent;
		}
		else if (opposite.resultOfFindingOppositePolygon != eFindOppositePolygon_None && oppositeAdjancent.resultOfFindingOppositePolygon == eFindOppositePolygon_None)
			m_Opposites[i] = opposite;
		else if (opposite.resultOfFindingOppositePolygon == eFindOppositePolygon_None && oppositeAdjancent.resultOfFindingOppositePolygon != eFindOppositePolygon_None)
			m_Opposites[i] = oppositeAdjancent;
	}
}

bool ExtrusionSnappingHelper::IsOverOppositePolygon(const PolygonPtr& pPolygon, EPushPull pushpull, bool bIgnoreSimilarDirection)
{
	if (m_Opposites[pushpull].polygon)
	{
		if (bIgnoreSimilarDirection)
		{
			if (m_Opposites[pushpull].polygon->GetPlane().IsSameFacing(pPolygon->GetPlane()))
				return false;
		}

		BrushVec3 direction = -pPolygon->GetPlane().Normal();
		BrushFloat fNearestDistance = pPolygon->GetNearestDistance(m_Opposites[pushpull].polygon, direction);

		if (pushpull == ePP_Push)
			return fNearestDistance > -kDesignerEpsilon;
		else
			return fNearestDistance < kDesignerEpsilon;
	}

	return false;
}

void ExtrusionSnappingHelper::ApplyOppositePolygons(PolygonPtr pCapPolygon, EPushPull pushpull, bool bHandleTouch)
{
	MODEL_SHELF_RECONSTRUCTOR(GetModel());

	if (m_Opposites[pushpull].polygon && m_Opposites[pushpull].resultOfFindingOppositePolygon == eFindOppositePolygon_Intersection)
	{
		if (m_Opposites[pushpull].bTouchAdjacent && bHandleTouch)
		{
			GetModel()->SetShelf(eShelf_Base);
			GetModel()->AddUnionPolygon(pCapPolygon);
			GetModel()->SetShelf(eShelf_Construction);
			GetModel()->RemovePolygon(pCapPolygon);
		}
		else if (!m_Opposites[pushpull].bTouchAdjacent)
		{
			pCapPolygon->Flip();
			GetModel()->SetShelf(eShelf_Base);

			std::vector<PolygonPtr> intersetionPolygons;
			GetModel()->QueryIntersectionByPolygon(pCapPolygon, intersetionPolygons);

			PolygonPtr ABInterectPolygon = pCapPolygon->Clone();
			PolygonPtr ABSubtractPolygon = pCapPolygon->Clone();
			PolygonPtr pUnionPolygon = new Polygon;

			for (int i = 0, iIntersectionSize(intersetionPolygons.size()); i < iIntersectionSize; ++i)
				pUnionPolygon->Union(intersetionPolygons[i]);

			ABInterectPolygon->Intersect(pUnionPolygon, eICEII_IncludeCoSame);
			ABSubtractPolygon->Subtract(pUnionPolygon);

			if (ABInterectPolygon->IsValid() && !ABInterectPolygon->IsOpen())
				GetModel()->AddSubtractedPolygon(ABInterectPolygon);

			if (ABSubtractPolygon->IsValid() && !ABSubtractPolygon->IsOpen())
				GetModel()->AddPolygon(ABSubtractPolygon->Flip());

			GetModel()->SetShelf(eShelf_Construction);
			pCapPolygon->Flip();
			std::vector<PolygonPtr> polygonList;
			GetModel()->QueryPolygons(pCapPolygon->GetPlane(), polygonList);
			for (int i = 0, iPolygonCount(polygonList.size()); i < iPolygonCount; ++i)
				GetModel()->RemovePolygon(polygonList[i]);
		}
	}

	m_Opposites[0].polygon = NULL;
	m_Opposites[1].polygon = NULL;
}

BrushFloat ExtrusionSnappingHelper::GetNearestDistanceToOpposite(EPushPull pushpull) const
{
	BrushFloat nearestDistance = m_Opposites[pushpull].nearestDistanceToPolygon;
	if (pushpull == ePP_Push)
		nearestDistance = -nearestDistance;
	return nearestDistance;
}

PolygonPtr ExtrusionSnappingHelper::FindAlignedPolygon(PolygonPtr pCapPolygon, const BrushMatrix34& worldTM, CViewport* view, const CPoint& point) const
{
	if (pCapPolygon == NULL)
		return NULL;

	int nIndex(-1);
	if (!GetModel()->QueryPolygon(GetDesigner()->GetRay(), nIndex))
		return NULL;

	PolygonPtr pPolygon = GetModel()->GetPolygon(nIndex);

	BrushFloat distToCapPolygon = 0;
	bool bHitCapPolygon = pCapPolygon->IsPassed(GetDesigner()->GetRay(), distToCapPolygon);

	BrushFloat distToPolygon = 0;
	pPolygon->GetPlane().HitTest(GetDesigner()->GetRay(), &distToPolygon);

	if ((!bHitCapPolygon || distToPolygon < distToCapPolygon) &&
	    !pPolygon->IsEquivalent(pCapPolygon) &&
	    pPolygon->GetPlane().Normal().IsEquivalent(pCapPolygon->GetPlane().Normal()))
	{
		return pPolygon;
	}

	return NULL;
}
}

