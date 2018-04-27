// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "BSPTree3D.h"
#include "Polygon.h"

namespace Designer
{

class BSPTree3DNode
{
public:
	BSPTree3DNode() : m_PosChild(NULL), m_NegChild(NULL)
	{
	}
	~BSPTree3DNode()
	{
		if (m_PosChild)
			delete m_PosChild;
		if (m_NegChild)
			delete m_NegChild;
	}

	void AddCoplnarPolygon(PolygonPtr pPolygon)
	{
		bool bHaveIntersection = false;
		std::set<PolygonPtr> removedPolygons;

		for (int i = 0, iPolygonCount(m_PolygonsOnCoplanar.size()); i < iPolygonCount; ++i)
		{
			if (eIT_None != Polygon::HasIntersection(m_PolygonsOnCoplanar[i], pPolygon))
			{
				bHaveIntersection = true;
				m_PolygonsOnCoplanar[i]->Union(pPolygon);
				removedPolygons.insert(pPolygon);
				pPolygon = m_PolygonsOnCoplanar[i];
			}
		}

		if (!bHaveIntersection)
		{
			m_PolygonsOnCoplanar.push_back(pPolygon->Clone());
		}
		else
		{
			auto ii = m_PolygonsOnCoplanar.begin();
			for (; ii != m_PolygonsOnCoplanar.end(); )
			{
				if (removedPolygons.find(*ii) != removedPolygons.end())
					ii = m_PolygonsOnCoplanar.erase(ii);
				else
					++ii;
			}
		}
	}

	void           SetPositiveChild(BSPTree3DNode* pTree) { m_PosChild = pTree; }
	void           SetNegativeChild(BSPTree3DNode* pTree) { m_NegChild = pTree; }
	BSPTree3DNode* GetPositiveChild() const               { return m_PosChild;  }
	BSPTree3DNode* GetNegativeChild() const               { return m_NegChild;  }

	void           GetPartitions(PolygonPtr& pPolygon, OutputPolygons& outPolygons) const
	{
		DESIGNER_ASSERT(!m_PolygonsOnCoplanar.empty());

		if (m_PolygonsOnCoplanar.empty())
			return;

		const BrushPlane& plane = m_PolygonsOnCoplanar[0]->GetPlane();

		bool bOnCoSamePlane = plane.IsEquivalent(pPolygon->GetPlane());
		bool bOnCoDiffPlane = plane.IsEquivalent(pPolygon->GetPlane().GetInverted());

		if (bOnCoSamePlane || bOnCoDiffPlane)
		{
			PolygonPtr pIntersected = pPolygon->Clone();
			for (int i = 0, iPolygonSize(m_PolygonsOnCoplanar.size()); i < iPolygonSize; ++i)
			{
				if (bOnCoSamePlane)
					pIntersected->Intersect(m_PolygonsOnCoplanar[i]);
				else
					pIntersected->Intersect(m_PolygonsOnCoplanar[i]->Clone()->Flip());
			}

			if (pIntersected->IsValid() && !pIntersected->IsOpen())
			{
				if (bOnCoSamePlane)
					outPolygons.coSameList.push_back(pIntersected);
				else
					outPolygons.coDiffList.push_back(pIntersected);
			}

			PolygonPtr pSubtracted = pPolygon->Clone();
			pSubtracted->Subtract(pIntersected);
			if (pSubtracted->IsValid() && !pSubtracted->IsOpen())
				GetPositivePartitions(pSubtracted, outPolygons);
		}
		else
		{
			std::vector<PolygonPtr> pFrontPolygons;
			std::vector<PolygonPtr> pBackPolygons;
			if (!pPolygon->ClipByPlane(plane, pFrontPolygons, pBackPolygons))
				return;

			if (!pFrontPolygons.empty())
			{
				for (int i = 0, iPolygonCount(pFrontPolygons.size()); i < iPolygonCount; ++i)
					GetPositivePartitions(pFrontPolygons[i], outPolygons);
			}

			if (!pBackPolygons.empty())
			{
				for (int i = 0, iPolygonCount(pBackPolygons.size()); i < iPolygonCount; ++i)
					GetNegativePartitions(pBackPolygons[i], outPolygons);
			}
		}
	}

	bool IsInside(const BrushVec3& vPos) const
	{
		DESIGNER_ASSERT(!m_PolygonsOnCoplanar.empty());
		if (m_PolygonsOnCoplanar.empty())
			return false;

		BrushFloat distance(m_PolygonsOnCoplanar[0]->GetPlane().Distance(vPos));

		if (distance > kDesignerEpsilon)
		{
			if (m_PosChild)
				return m_PosChild->IsInside(vPos);
			else
				return false;
		}
		else if (distance < -kDesignerEpsilon)
		{
			if (m_NegChild)
				return m_NegChild->IsInside(vPos);
			else
				return true;
		}
		else if (m_PosChild || m_NegChild)
		{
			for (int i = 0, iCoPolygonCount(m_PolygonsOnCoplanar.size()); i < iCoPolygonCount; ++i)
			{
				if (m_PolygonsOnCoplanar[i]->Include(vPos))
					return true;
			}
		}
		return false;
	}

private:

	void GetPositivePartitions(PolygonPtr& pPolygon, OutputPolygons& outPolygons) const
	{
		if (m_PosChild)
			m_PosChild->GetPartitions(pPolygon, outPolygons);
		else
			outPolygons.posList.push_back(pPolygon);
	}

	void GetNegativePartitions(PolygonPtr& pPolygon, OutputPolygons& outPolygons) const
	{
		if (m_NegChild)
			m_NegChild->GetPartitions(pPolygon, outPolygons);
		else
			outPolygons.negList.push_back(pPolygon);
	}

private:
	std::vector<PolygonPtr> m_PolygonsOnCoplanar;
	BSPTree3DNode*          m_PosChild;
	BSPTree3DNode*          m_NegChild;
};

BSPTree3D::BSPTree3D(std::vector<PolygonPtr>& polygonList)
{
	m_bValidTree = true;
	m_pRootNode = BuildBSP(polygonList);
}

BSPTree3D::~BSPTree3D()
{
	if (m_pRootNode)
		delete m_pRootNode;
}

void BSPTree3D::GetPartitions(PolygonPtr& pPolygon, OutputPolygons& outPolygons) const
{
	m_pRootNode->GetPartitions(pPolygon, outPolygons);
}

bool BSPTree3D::IsInside(const BrushVec3& vPos) const
{
	return m_pRootNode->IsInside(vPos);
}

BSPTree3DNode* BSPTree3D::BuildBSP(std::vector<PolygonPtr>& polygonList)
{
	BSPTree3DNode* pNode = new BSPTree3DNode;

	PolygonPtr pNodePolygon = polygonList[0];
	pNode->AddCoplnarPolygon(pNodePolygon);

	std::vector<PolygonPtr> posList, negList;

	for (int i = 1, iPolygonSize(polygonList.size()); i < iPolygonSize; ++i)
	{
		if (pNodePolygon->GetPlane().IsEquivalent(polygonList[i]->GetPlane()))
		{
			pNode->AddCoplnarPolygon(polygonList[i]);
			continue;
		}

		std::vector<PolygonPtr> pFrontPolygons;
		std::vector<PolygonPtr> pBackPolygons;
		if (polygonList[i]->ClipByPlane(pNodePolygon->GetPlane(), pFrontPolygons, pBackPolygons, NULL))
		{
			for (int k = 0; k < pFrontPolygons.size(); ++k)
				posList.push_back(pFrontPolygons[k]);
			for (int k = 0; k < pBackPolygons.size(); ++k)
				negList.push_back(pBackPolygons[k]);
		}
		else
		{
			m_bValidTree = false;
		}

		DESIGNER_ASSERT(!pFrontPolygons.empty() || !pBackPolygons.empty());
		if (pFrontPolygons.empty() && pBackPolygons.empty())
			m_bValidTree = false;
	}

	if (!posList.empty())
		pNode->SetPositiveChild(BuildBSP(posList));

	if (!negList.empty())
		pNode->SetNegativeChild(BuildBSP(negList));

	return pNode;
}

}

