// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "AreaSolid.h"
#include "BSPTree3D.h"
#include <CryRenderer/IRenderAuxGeom.h>

const float AreaUtil::EPSILON = 0.005f;
static const float fEnoughBigNumber(3e10f);

class CSegment
{
public:
	CSegment(const Vec3* verticesOfConvexHull, int numberOfVertices, CAreaSolid::ESegmentType segmentType, const AreaUtil::CPlane& plane)
	{
		m_numberOfPoints = numberOfVertices;
		m_Type = segmentType;

		m_Points.resize(m_numberOfPoints);
		for (int i = 0; i < m_numberOfPoints; ++i)
			m_Points[i] = plane.WorldToPlane(verticesOfConvexHull[i]);

		m_Lines.resize(m_numberOfPoints);
		for (int i = 0; i < m_numberOfPoints; ++i)
		{
			int nexti = (i + 1) % m_numberOfPoints;
			PREFAST_ASSUME(nexti >= 0 && nexti < m_numberOfPoints);
			m_Lines[i] = AreaUtil::CLine(m_Points[i], m_Points[nexti]);
		}
	}
	bool IsValid() const
	{
		if (m_numberOfPoints < 3)
			return false;
		return true;
	}
	CAreaSolid::ESegmentType GetType() const
	{
		return m_Type;
	}
	bool IsInside(const Vec2& vPosInPlane) const
	{
		for (int i = 0; i < m_numberOfPoints; ++i)
		{
			if (m_Lines[i].Distance(vPosInPlane) > AreaUtil::EPSILON)
				return false;
		}
		return true;
	}
	bool QueryNearest(const Vec2& vPosInPlaneCoord, Vec2& outNearestPos) const
	{
		if (IsInside(vPosInPlaneCoord))
		{
			outNearestPos = vPosInPlaneCoord;
			return true;
		}

		float fNearestSquaredDistance(fEnoughBigNumber);
		float fSquaredDistance(0);

		for (int i = 0; i < m_numberOfPoints; ++i)
		{
			const Vec2& point = m_Points[i];
			float dot(m_Lines[i].DotWithNormal(vPosInPlaneCoord - point));
			if (dot <= 0)
				continue;
			const Vec2& nextPoint = m_Points[(i + 1) % m_numberOfPoints];
			EResultDistance result(GetSquaredDistance(point, nextPoint, vPosInPlaneCoord, fSquaredDistance));

			// A segment need to be a convex hull.
			// So if the current distance is greater than the nearest distance,
			// the nearest distance which we have calculated until this time is what we've wanted.
			if (fSquaredDistance >= fNearestSquaredDistance)
				break;

			if (fSquaredDistance < fNearestSquaredDistance)
			{
				switch (result)
				{
				case eResultDistance_EdgeP0:
					outNearestPos = point;
					break;
				case eResultDistance_EdgeP1:
					outNearestPos = nextPoint;
					break;
				case eResultDistance_Middle:
					{
						Vec2 edge(nextPoint - point);
						outNearestPos = point + (edge.Dot(vPosInPlaneCoord - point) / edge.Dot(edge)) * edge;
					}
					break;
				}
				fNearestSquaredDistance = fSquaredDistance;
			}
		}

		return fNearestSquaredDistance < fEnoughBigNumber;
	}

	void Draw(const AreaUtil::CPlane& plane, const Matrix34& worldTM, const ColorB& color0, const ColorB& color1) const
	{
		IRenderAuxGeom* pRC = gEnv->pAuxGeomRenderer;

		Vec3 worldV0 = worldTM.TransformPoint(plane.PlaneToWorld(m_Points[0]));
		Vec3 worldV1 = worldTM.TransformPoint(plane.PlaneToWorld(m_Points[1]));

		for (int i = 1; i < m_numberOfPoints; ++i)
		{
			pRC->DrawLine(worldV0, color0, worldV1, color0);
			worldV0 = worldV1;
			worldV1 = worldTM.TransformPoint(plane.PlaneToWorld(m_Points[(i + 1) % m_numberOfPoints]));
		}
	}

	void GetMemoryUsage(ICrySizer* pSizer) const
	{
		SIZER_COMPONENT_NAME(pSizer, "CSegment");
		pSizer->AddContainer(m_Points);
		pSizer->AddContainer(m_Lines);
		pSizer->AddObject(this, sizeof(*this));
	}

	int         GetNumberOfPoints() const { return m_numberOfPoints;  }
	const Vec2* GetPoints() const         { return m_Points.data();  }

private:

	enum EResultDistance
	{
		eResultDistance_EdgeP0,
		eResultDistance_EdgeP1,
		eResultDistance_Middle
	};
	static EResultDistance GetSquaredDistance(const Vec2& edgeP0, const Vec2& edgeP1, const Vec2& vPoint, float& outDistance)
	{
		Vec2 vP0toP1(edgeP1 - edgeP0);
		Vec2 vP0toPoint(vPoint - edgeP0);
		float t = vP0toP1.Dot(vP0toPoint);

		if (t <= 0)
		{
			outDistance = vP0toPoint.Dot(vP0toPoint);
			return eResultDistance_EdgeP0;
		}

		float SquaredP0toP1 = vP0toP1.Dot(vP0toP1);
		if (t >= SquaredP0toP1)
		{
			Vec2 vP1toPoint(vPoint - edgeP1);
			outDistance = vP1toPoint.Dot(vPoint);
			return eResultDistance_EdgeP1;
		}

		outDistance = vP0toPoint.Dot(vP0toPoint) - t * t / SquaredP0toP1;
		return eResultDistance_Middle;
	}

private:

	int                          m_numberOfPoints;
	std::vector<Vec2>            m_Points;

	std::vector<AreaUtil::CLine> m_Lines;
	CAreaSolid::ESegmentType     m_Type;
};

// CSegmentSet is to manage coplanar segments. It makes it possible to go up the search speed.
class CSegmentSet
{
public:

	CSegmentSet() = default;

	~CSegmentSet()
	{
		for (auto const pSegment : m_Segments)
		{
			delete pSegment;
		}

		m_Segments.clear();
	}
	void AddSegment(CSegment* pSegment)
	{
		m_Segments.push_back(pSegment);
	}
	const CSegment* GetSegment(int nIndex) const
	{
		return m_Segments[nIndex];
	}
	int GetSegmentSize() const
	{
		return m_Segments.size();
	}
	const AreaUtil::CPlane& GetPlane() const
	{
		return m_Plane;
	}
	void SetPlane(const AreaUtil::CPlane& plane)
	{
		m_Plane = plane;
	}
	void GetMemoryUsage(ICrySizer* pSizer) const
	{
		SIZER_COMPONENT_NAME(pSizer, "CSegmentSet");
		pSizer->AddObject(this, sizeof(*this));

		for (auto const pSegment : m_Segments)
		{
			pSegment->GetMemoryUsage(pSizer);
		}
	}

private:
	AreaUtil::CPlane       m_Plane;
	std::vector<CSegment*> m_Segments;
};

CAreaSolid::CAreaSolid() : m_nRefCount(0)
{
	m_BSPTree = nullptr;
	m_BoundBox.Reset();
}

void CAreaSolid::Clear()
{
	for (auto const pSegmentSet : m_SegmentSets)
	{
		delete pSegmentSet;
	}

	m_SegmentSets.clear();
	m_BoundBox.Reset();
	if (m_BSPTree)
	{
		delete m_BSPTree;
		m_BSPTree = nullptr;
	}
}

void CAreaSolid::AddSegment(const Vec3* verticesOfConvexhull, bool bObstruction, int numberOfVertices)
{
	AreaUtil::CPlane plane = AreaUtil::CPlane(verticesOfConvexhull[0], verticesOfConvexhull[1], verticesOfConvexhull[2]);
	CSegment* pSegment = new CSegment(verticesOfConvexhull, numberOfVertices, bObstruction ? eSegmentType_Close : eSegmentType_Open, plane);

	if (pSegment->IsValid())
	{
		bool bExistSet(false);

		for (auto const pSegmentSet : m_SegmentSets)
		{
			if (pSegmentSet->GetPlane().IsEquivalent(plane))
			{
				pSegmentSet->AddSegment(pSegment);
				bExistSet = true;
				break;
			}
		}

		if (!bExistSet)
		{
			CSegmentSet* pSegmentSet = new CSegmentSet;
			pSegmentSet->AddSegment(pSegment);
			pSegmentSet->SetPlane(plane);
			m_SegmentSets.push_back(pSegmentSet);
		}
		for (int k = 0; k < numberOfVertices; ++k)
			m_BoundBox.Add(verticesOfConvexhull[k]);
	}
	else
	{
		delete pSegment;
	}
}

bool CAreaSolid::QueryNearest(const Vec3& vPos, int queryFlag, Vec3& outNearestPos, float& outNearestDistance) const
{
	bool bHasReverseFlag = (queryFlag & eSegmentQueryFlag_UsingReverseSegment) ? true : false;
	Vec2 vNearestPos2DFromSegment;

	outNearestDistance = fEnoughBigNumber;

	for (auto const pSegmentSet : m_SegmentSets)
	{
		const AreaUtil::CPlane& plane(pSegmentSet->GetPlane());
		float fDistanceToPlane(plane.Distance(vPos));
		Vec2 vPlanePos = plane.WorldToPlane(vPos);
		for (int k = 0, iSegmentSize(pSegmentSet->GetSegmentSize()); k < iSegmentSize; ++k)
		{
			const CSegment* pSegment = pSegmentSet->GetSegment(k);
			ESegmentType segmentType(pSegment->GetType());

			if (IsQueryTypeIdenticalToSegmentType(queryFlag, segmentType) == false)
				continue;

			if (segmentType == eSegmentType_Close && (!bHasReverseFlag && fDistanceToPlane <= -AreaUtil::EPSILON || bHasReverseFlag && fDistanceToPlane >= AreaUtil::EPSILON))
				continue;

			if (pSegment->QueryNearest(vPlanePos, vNearestPos2DFromSegment))
			{
				Vec3 vNearestPosFromSegment = plane.PlaneToWorld(vNearestPos2DFromSegment);
				float fDistance = (vPos - vNearestPosFromSegment).GetLengthSquared();
				if (fDistance < outNearestDistance)
				{
					outNearestDistance = fDistance;
					outNearestPos = vNearestPosFromSegment;
				}
			}
		}
	}

	return outNearestDistance < fEnoughBigNumber;
}

bool CAreaSolid::IsInside(const Vec3& vPos) const
{
	if (m_BSPTree == nullptr)
		return false;

	if (!m_BoundBox.IsContainPoint(vPos))
		return false;

	if (!m_BSPTree->IsInside(vPos))
		return false;

	return true;
}

void CAreaSolid::Draw(const Matrix34& worldTM, const ColorB& color0, const ColorB& color1) const
{
	for (auto const pSegmentSet : m_SegmentSets)
	{
		for (int k = 0, iSegmentSize(pSegmentSet->GetSegmentSize()); k < iSegmentSize; ++k)
		{
			const CSegment* pSegment = pSegmentSet->GetSegment(k);
			pSegment->Draw(pSegmentSet->GetPlane(), worldTM, color0, color1);
		}
	}
}

void CAreaSolid::BuildBSP()
{
	if (m_BSPTree)
	{
		delete m_BSPTree;
		m_BSPTree = nullptr;
	}

	IBSPTree3D::FaceList faceList;

	for (auto const pSegmentSet : m_SegmentSets)
	{
		const AreaUtil::CPlane& plane = pSegmentSet->GetPlane();

		for (int k = 0, iSegmentSize(pSegmentSet->GetSegmentSize()); k < iSegmentSize; ++k)
		{
			const CSegment* pSegment = pSegmentSet->GetSegment(k);
			int iPointSize(pSegment->GetNumberOfPoints());
			const Vec2* pPoints = pSegment->GetPoints();

			IBSPTree3D::CFace face;
			face.reserve(iPointSize);
			for (int a = 0; a < iPointSize; ++a)
				face.push_back(plane.PlaneToWorld(pPoints[a]));
			faceList.push_back(face);
		}
	}

	m_BSPTree = new CBSPTree3D(faceList);
}

void CAreaSolid::GetMemoryUsage(ICrySizer* pSizer) const
{
	SIZER_COMPONENT_NAME(pSizer, "CAreaSolid");

	if (m_BSPTree)
	{
		m_BSPTree->GetMemoryUsage(pSizer);
	}

	for (auto const pSegmentSet : m_SegmentSets)
	{
		pSegmentSet->GetMemoryUsage(pSizer);
	}

	pSizer->AddObject(this, sizeof(*this));
}
