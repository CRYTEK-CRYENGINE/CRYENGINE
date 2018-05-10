// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "Area.h"
#include "AreaSolid.h"
#include "Entity.h"
#include <CryRenderer/IRenderAuxGeom.h>
#include <CryMath/GeomQuery.h>

namespace
{
static CArea::TAreaBoxes s_areaBoxes;
}

//////////////////////////////////////////////////////////////////////////
CArea::CArea(CAreaManager* pManager)
	: m_origin(0.0f)
	, m_height(0.0f)
	, m_pAreaManager(pManager)
	, m_proximity(5.0f)
	, m_fadeDistance(-1.0f)
	, m_environmentFadeDistance(-1.0f)
	, m_greatestFadeDistance(-1.0f)
	, m_innerFadeDistance(0.0f)
	, m_areaGroupId(INVALID_AREA_GROUP_ID)
	, m_priority(0)
	, m_areaId(-1)
	, m_entityId(INVALID_ENTITYID)
	, m_boxMin(ZERO)
	, m_boxMax(ZERO)
	, m_sphereCenter(0)
	, m_sphereRadius(0)
	, m_sphereRadius2(0)
	, m_bAllObstructed(0)
	, m_numObstructed(0)
	, m_pAreaSolid(nullptr)
	, m_nullVec(ZERO)
{
	m_areaType = ENTITY_AREA_TYPE_SHAPE;
	m_invMatrix.SetIdentity();
	m_worldTM.SetIdentity();
	m_mapEntityCachedAreaData.reserve(256);

	// All sides not obstructed by default
	memset(&m_boxSideObstruction, 0, 6);

	m_bbox_holder = s_areaBoxes.size();
	s_areaBoxes.emplace_back();
	s_areaBoxes[m_bbox_holder].area = this;
}

//////////////////////////////////////////////////////////////////////////
CArea::~CArea()
{
	RemoveEntities();
	m_pAreaManager->Unregister(this);
	ReleaseAreaData();

	size_t last = s_areaBoxes.size() - 1;
	s_areaBoxes[m_bbox_holder].box = s_areaBoxes[last].box;
	(s_areaBoxes[m_bbox_holder].area = s_areaBoxes[last].area)->m_bbox_holder = m_bbox_holder;
	s_areaBoxes.erase(s_areaBoxes.begin() + last, s_areaBoxes.end());
}

//////////////////////////////////////////////////////////////////////////
void CArea::Release()
{
	delete this;
}

//////////////////////////////////////////////////////////////////////////
void CArea::SetSoundObstructionOnAreaFace(size_t const index, bool const bObstructs)
{
	CRY_PROFILE_FUNCTION(PROFILE_ENTITY);
	switch (m_areaType)
	{
	case ENTITY_AREA_TYPE_BOX:
		{
			m_boxSideObstruction[index].bObstructed = bObstructs ? 1 : 0;
			m_numObstructed = 0;

			for (auto const& i : m_boxSideObstruction)
			{
				if (i.bObstructed)
				{
					++m_numObstructed;
				}
			}

			m_bAllObstructed = 0;

			if (m_numObstructed == 6)
			{
				m_bAllObstructed = 1;
			}
		}
		break;
	case ENTITY_AREA_TYPE_SHAPE:
		{
			size_t const numSegments = m_areaSegments.size();

			if (numSegments > 0)
			{
				if (index < numSegments)
				{
					m_areaSegments[index]->bObstructSound = bObstructs;
				}
				else
				{
					// We exceed segment count which could mean
					// that the user wants to set roof and floor sound obstruction
					if (index == numSegments)
					{
						// The user wants to set roof sound obstruction
						if (bObstructs)
						{
							m_state |= Cry::AreaManager::EAreaState::ObstructRoof;
						}
						else
						{
							m_state &= ~Cry::AreaManager::EAreaState::ObstructRoof;
						}
					}
					else if (index == numSegments + 1)
					{
						// The user wants to set floor sound obstruction
						if (bObstructs)
						{
							m_state |= Cry::AreaManager::EAreaState::ObstructFloor;
						}
						else
						{
							m_state &= ~Cry::AreaManager::EAreaState::ObstructFloor;
						}
					}
				}
			}
		}
		break;
	}
}

//////////////////////////////////////////////////////////////////////////
void CArea::SetAreaType(EEntityAreaType type)
{
	CRY_PROFILE_FUNCTION(PROFILE_ENTITY);
	m_areaType = type;

	// to prevent gravity volumes being evaluated in the
	// AreaManager::UpdatePlayer function, that caused problems.
	if (m_areaType == ENTITY_AREA_TYPE_GRAVITYVOLUME)
		m_pAreaManager->Unregister(this);

}

// resets area - clears all segments in area
//////////////////////////////////////////////////////////////////////////
void CArea::ClearPoints()
{
	CRY_PROFILE_FUNCTION(PROFILE_ENTITY);

	if (!m_areaSegments.empty())
	{
		for (a2DSegment* const pSegment : m_areaSegments)
		{
			delete pSegment;
		}

		m_areaSegments.clear();
	}
	m_areaPoints.clear();
	m_triIndices.clear();
	m_extents.Clear();
	m_area = 0.0f;

	m_state &= ~Cry::AreaManager::EAreaState::Initialized;
}

//////////////////////////////////////////////////////////////////////////
size_t CArea::MemStat()
{
	size_t memSize = sizeof(*this);

	memSize += m_areaSegments.size() * (sizeof(a2DSegment) + sizeof(a2DSegment*));
	return memSize;
}

//adds segment to area, calculates line parameters y=kx+b, sets horizontal/vertical flags
//////////////////////////////////////////////////////////////////////////
void CArea::AddSegment(const a2DPoint& p0, const a2DPoint& p1, bool const bObstructSound)
{
	CRY_PROFILE_FUNCTION(PROFILE_ENTITY);
	a2DSegment* const pSegment = new a2DSegment;
	pSegment->bObstructSound = bObstructSound;
	UpdateSegment(*pSegment, p0, p1);
	m_areaSegments.push_back(pSegment);
}

//////////////////////////////////////////////////////////////////////////
void CArea::UpdateSegment(a2DSegment& segment, a2DPoint const& p0, a2DPoint const& p1)
{
	// If this is horizontal line set flag. This segment is needed only for distance calculations.
	segment.isHorizontal = p1.y == p0.y;

	if (p0.x < p1.x)
	{
		segment.bbox.min.x = p0.x;
		segment.bbox.max.x = p1.x;
	}
	else
	{
		segment.bbox.min.x = p1.x;
		segment.bbox.max.x = p0.x;
	}

	if (p0.y < p1.y)
	{
		segment.bbox.min.y = p0.y;
		segment.bbox.max.y = p1.y;
	}
	else
	{
		segment.bbox.min.y = p1.y;
		segment.bbox.max.y = p0.y;
	}

	if (!segment.isHorizontal)
	{
		//if this is vertical line - special case
		if (p1.x == p0.x)
		{
			segment.k = 0.0f;
			segment.b = p0.x;
		}
		else
		{
			segment.k = (p1.y - p0.y) / (p1.x - p0.x);
			segment.b = p0.y - segment.k * p0.x;
		}
	}
	else
	{
		segment.k = 0.0f;
		segment.b = 0.0f;
	}
	segment.normal = Vec2(p1.y - p0.y, p0.x - p1.x).GetNormalized();
}

// calculates min distance from point within area to the border of area
// returns fade coefficient: Distance/m_Proximity
// [0 - on the very border of area,	1 - inside area, distance to border is more than m_Proximity]
//////////////////////////////////////////////////////////////////////////
float CArea::CalcDistToPoint(a2DPoint const& point) const
{
	CRY_PROFILE_FUNCTION(PROFILE_ENTITY);
	if ((m_state & Cry::AreaManager::EAreaState::Initialized) == 0)
		return -1;

	if (m_proximity == 0.0f)
		return 1.0f;

	float distMin = m_proximity * m_proximity;
	float curDist;
	a2DBBox proximityBox;

	proximityBox.max.x = point.x + m_proximity;
	proximityBox.max.y = point.y + m_proximity;
	proximityBox.min.x = point.x - m_proximity;
	proximityBox.min.y = point.y - m_proximity;

	for (auto const pAreaSegment : m_areaSegments)
	{
		if (!pAreaSegment->bbox.BBoxOutBBox2D(proximityBox))
		{
			if (pAreaSegment->isHorizontal)
			{
				if (point.x < pAreaSegment->bbox.min.x)
					curDist = pAreaSegment->bbox.min.GetSquaredDistance(point);
				else if (point.x > pAreaSegment->bbox.max.x)
					curDist = pAreaSegment->bbox.max.GetSquaredDistance(point);
				else
					curDist = fabsf(point.y - pAreaSegment->bbox.max.y);
				curDist *= curDist;
			}
			else
			{
				if (pAreaSegment->k == 0.0f)
				{
					if (point.y < pAreaSegment->bbox.min.y)
						curDist = pAreaSegment->bbox.min.GetSquaredDistance(point);
					else if (point.y > pAreaSegment->bbox.max.y)
						curDist = pAreaSegment->bbox.max.GetSquaredDistance(point);
					else
						curDist = fabsf(point.x - pAreaSegment->b);
					curDist *= curDist;
				}
				else
				{
					a2DPoint intersection;
					float b2, k2;
					k2 = -1.0f / pAreaSegment->k;
					b2 = point.y - k2 * point.x;
					intersection.x = (b2 - pAreaSegment->b) / (pAreaSegment->k - k2);
					intersection.y = k2 * intersection.x + b2;

					if (intersection.x < pAreaSegment->bbox.min.x)
						curDist = point.GetSquaredDistance(pAreaSegment->GetStart());
					else if (intersection.x > pAreaSegment->bbox.max.x)
						curDist = point.GetSquaredDistance(pAreaSegment->GetEnd());
					else
						curDist = intersection.GetSquaredDistance(point);
				}
				if (curDist < distMin)
					distMin = curDist;
			}
		}
	}

	return sqrt_tpl(distMin) / m_proximity; // TODO: Check if "distmin/m_fProximity*m_fProximity" is faster
}

// check if the point is within the area
// first BBox check, then count number of intersections for horizontal ray from point and area segments
// if the number is odd - the point is inside
//////////////////////////////////////////////////////////////////////////
bool CArea::CalcPointWithin(EntityId const nEntityID, Vec3 const& point3d, bool const bIgnoreHeight /* = false */, bool const bCacheResult /* = true */)
{
	CRY_PROFILE_FUNCTION(PROFILE_ENTITY);
	bool bResult = false;

	if ((m_state & Cry::AreaManager::EAreaState::Initialized) != 0)
	{
		SCachedAreaData* pCachedData = nullptr;

		if (nEntityID != INVALID_ENTITYID)
		{
			TEntityCachedAreaDataMap::iterator Iter(m_mapEntityCachedAreaData.find(nEntityID));

			if (Iter != m_mapEntityCachedAreaData.end())
			{
				pCachedData = &(Iter->second);
			}
		}

		if (pCachedData != nullptr && (pCachedData->eFlags & eCachedAreaData_PointWithinValid) != 0)
		{
			bResult = GetCachedPointWithin(nEntityID);
		}
		else
		{
			bResult = CalcPointWithinNonCached(point3d, bIgnoreHeight);

			// Set the flags and put the result into the data cache.
			if (pCachedData != nullptr && bCacheResult)
			{
				if (bIgnoreHeight)
				{
					pCachedData->eFlags = (ECachedAreaData)(pCachedData->eFlags | eCachedAreaData_PointWithinValid | eCachedAreaData_PointWithinValidHeightIgnored);
				}
				else
				{
					pCachedData->eFlags = (ECachedAreaData)(pCachedData->eFlags | eCachedAreaData_PointWithinValid);
				}

				pCachedData->bPointWithin = bResult;
			}
		}
	}

	return bResult;
}

// helper function to figure out if given point is contained within the area
// does not make use of any cached data
//////////////////////////////////////////////////////////////////////////
bool CArea::CalcPointWithinNonCached(Vec3 const& point3d, bool const bIgnoreHeight) const
{
	bool bResult = false;

	switch (m_areaType)
	{
	case ENTITY_AREA_TYPE_SPHERE:
		{
			Vec3 oPoint(point3d - m_sphereCenter);

			if (bIgnoreHeight)
			{
				oPoint.z = 0.0f;
			}

			bResult = (oPoint.GetLengthSquared() < m_sphereRadius2);

			break;
		}
	case ENTITY_AREA_TYPE_BOX:
		{
			Vec3 p3d = m_invMatrix.TransformPoint(point3d);

			if (bIgnoreHeight)
				p3d.z = m_boxMax.z;

			// And put the result into the data cache
			if ((p3d.x < m_boxMin.x) ||
				(p3d.y < m_boxMin.y) ||
				(p3d.z < m_boxMin.z) ||
				(p3d.x > m_boxMax.x) ||
				(p3d.y > m_boxMax.y) ||
				(p3d.z > m_boxMax.z))
			{
				bResult = false;
			}
			else
			{
				bResult = true;
			}

			break;
		}
	case ENTITY_AREA_TYPE_SOLID:
		{
			if (point3d.IsValid())
			{
				Vec3 localPoint3D = m_invMatrix.TransformPoint(point3d);
				bResult = m_pAreaSolid->IsInside(localPoint3D);
			}

			break;
		}
	case ENTITY_AREA_TYPE_SHAPE:
		{
			if (!m_bClosed)
			{
				bResult = false;
				break;
			}

			bResult = true;

			if (!bIgnoreHeight)
			{
				if (m_height > 0.0f)
				{
					if (point3d.z < m_origin || point3d.z > m_origin + m_height)
					{
						bResult = false;
					}
				}
			}

			if (bResult)
			{
				a2DPoint const* const point = (CArea::a2DPoint*)(&point3d);

				bResult = !m_areaBBox.PointOutBBox2D(*point);

				if (bResult)
				{
					size_t cntr = 0;
					size_t const nSegmentCount = m_areaSegments.size();

					for (size_t sIdx = 0; sIdx < nSegmentCount; ++sIdx)
					{
						if (!m_areaSegments[sIdx]->isHorizontal && !m_areaSegments[sIdx]->bbox.PointOutBBox2DVertical(*point))
						{
							if (m_areaSegments[sIdx]->IntersectsXPosVertical(*point) || m_areaSegments[sIdx]->IntersectsXPos(*point))
							{
								++cntr;
							}
						}
					}

					bResult = ((cntr & 1) != 0);
				}
			}

			break;
		}
	default:
		{
			CryFatalError("Unknown area type during CArea::CalcPointWithin");

			break;
		}
	}

	return bResult;
}

//	for editor use - if point is within - returns min horizontal distance to border
//	if point out - returns -1
//////////////////////////////////////////////////////////////////////////
float CArea::CalcPointWithinDist(EntityId const nEntityID, Vec3 const& point3d, bool const bIgnoreSoundObstruction /* = true */, bool const bCacheResult /* = true */)
{
	CRY_PROFILE_FUNCTION(PROFILE_ENTITY);
	float fMinDist = -1.0f;

	if ((m_state & Cry::AreaManager::EAreaState::Initialized) != 0)
	{
		float fDistanceWithinSq = 0.0f;
		SCachedAreaData* pCachedData = nullptr;

		if (nEntityID != INVALID_ENTITYID)
		{
			TEntityCachedAreaDataMap::iterator Iter(m_mapEntityCachedAreaData.find(nEntityID));

			if (Iter != m_mapEntityCachedAreaData.end())
			{
				pCachedData = &(Iter->second);
			}
		}

		// Only computes distance if point is inside the area.
		bool bGoAheadAndCompute = false;

		if (pCachedData != nullptr)
		{
			bGoAheadAndCompute = (pCachedData->eFlags & eCachedAreaData_DistWithinSqValid) == 0 && pCachedData->ePosType == AREA_POS_TYPE_2DINSIDE_ZINSIDE;
		}
		else
		{
			// Unfortunately we need to compute the position type.
			bGoAheadAndCompute = CalcPosType(nEntityID, point3d) == AREA_POS_TYPE_2DINSIDE_ZINSIDE;
		}

		if (bGoAheadAndCompute)
		{
			switch (m_areaType)
			{
			case ENTITY_AREA_TYPE_SPHERE:
				{
					Vec3 const sPnt = point3d - m_sphereCenter;
					float fPointLengthSq = sPnt.GetLengthSquared();

					fMinDist = m_sphereRadius - sPnt.GetLength();
					fDistanceWithinSq = m_sphereRadius2 - fPointLengthSq;

					break;
				}
			case ENTITY_AREA_TYPE_BOX:
				{
					if (!bIgnoreSoundObstruction)
					{
						Vec3 v3ClosestPos(ZERO);
						CalcClosestPointToObstructedBox(v3ClosestPos, fDistanceWithinSq, point3d);
						fMinDist = (fDistanceWithinSq > 0.0f) ? sqrt_tpl(fDistanceWithinSq) : fMinDist;
					}
					else
					{
						Vec3 oTemp(ZERO);
						fDistanceWithinSq = ClosestPointOnHullDistSq(nEntityID, point3d, oTemp, true);
						fMinDist = (fDistanceWithinSq > 0.0f) ? sqrt_tpl(fDistanceWithinSq) : fMinDist;
					}

					break;
				}
			case ENTITY_AREA_TYPE_SOLID:
				{
					Vec3 const localPoint3D(m_invMatrix.TransformPoint(point3d));
					int queryFlag = bIgnoreSoundObstruction ? CAreaSolid::eSegmentQueryFlag_Obstruction : CAreaSolid::eSegmentQueryFlag_All;
					Vec3 vOnHull;

					if (m_pAreaSolid->QueryNearest(localPoint3D, queryFlag, vOnHull, fDistanceWithinSq))
					{
						fMinDist = (fDistanceWithinSq > 0.0f) ? sqrt_tpl(fDistanceWithinSq) : fMinDist;
					}

					break;
				}
			case ENTITY_AREA_TYPE_SHAPE:
				{
					if (!bIgnoreSoundObstruction)
					{
						Vec3 v3ClosestPos;
						CalcClosestPointToObstructedShape(nEntityID, v3ClosestPos, fDistanceWithinSq, point3d);
						fMinDist = (fDistanceWithinSq > 0.0f) ? sqrt_tpl(fDistanceWithinSq) : fMinDist;
					}
					else
					{
						// check distance to every line segment, remember the closest
						size_t const nSegmentSize = m_areaSegments.size();

						for (size_t sIdx = 0; sIdx < nSegmentSize; sIdx++)
						{
							float fT;
							a2DSegment* curSg = m_areaSegments[sIdx];

							Vec3 startSeg(curSg->GetStart().x, curSg->GetStart().y, point3d.z);
							Vec3 endSeg(curSg->GetEnd().x, curSg->GetEnd().y, point3d.z);
							Lineseg line(startSeg, endSeg);

							// Returns distance from a point to a line segment, ignoring the z coordinates
							float fDist = Distance::Point_Lineseg2D(point3d, line, fT);

							if (fMinDist == -1)
								fMinDist = fDist;

							fMinDist = min(fDist, fMinDist);
						}

						if (m_height > 0.0f)
						{
							float fDistToRoof = fMinDist + 1.0f;
							float fDistToFloor = fMinDist + 1.0f;

							if ((m_state & Cry::AreaManager::EAreaState::ObstructFloor) == 0)
							{
								fDistToFloor = point3d.z - m_origin;
							}

							if ((m_state & Cry::AreaManager::EAreaState::ObstructRoof) == 0)
							{
								fDistToRoof = m_origin + m_height - point3d.z;
							}

							float fZDist = min(fDistToFloor, fDistToRoof);
							fMinDist = min(fMinDist, fZDist);
						}

						fDistanceWithinSq = (fDistanceWithinSq > 0.0f) ? fMinDist * fMinDist : 0.0f;
					}

					break;
				}
			default:
				{
					CryFatalError("Unknown area type during CArea::CalcPointWithinDist");

					break;
				}
			}
		}

		// Set the flags and put the shortest distance into the data cache also when not computing.
		if (pCachedData != nullptr && bCacheResult)
		{
			if (bIgnoreSoundObstruction)
			{
				pCachedData->eFlags = (ECachedAreaData)(pCachedData->eFlags | eCachedAreaData_DistWithinSqValid | eCachedAreaData_DistWithinSqValidNotObstructed);
			}
			else
			{
				pCachedData->eFlags = (ECachedAreaData)(pCachedData->eFlags | eCachedAreaData_DistWithinSqValid);
			}

			pCachedData->fDistanceWithinSq = fDistanceWithinSq;
		}
	}

	return fMinDist;
}

//////////////////////////////////////////////////////////////////////////
float CArea::ClosestPointOnHullDistSq(EntityId const nEntityID, Vec3 const& Point3d, Vec3& OnHull3d, bool const bIgnoreSoundObstruction /* = true */, bool const bCacheResult /* = true */)
{
	CRY_PROFILE_FUNCTION(PROFILE_ENTITY);
	float fClosestDistance = -1.0f;

	if ((m_state & Cry::AreaManager::EAreaState::Initialized) != 0)
	{
		SCachedAreaData* pCachedData = nullptr;

		if (nEntityID != INVALID_ENTITYID)
		{
			TEntityCachedAreaDataMap::iterator Iter(m_mapEntityCachedAreaData.find(nEntityID));

			if (Iter != m_mapEntityCachedAreaData.end())
			{
				pCachedData = &(Iter->second);
			}
		}

		if (pCachedData != nullptr && (pCachedData->eFlags & eCachedAreaData_PosOnHullValid) != 0 &&
		    (pCachedData->eFlags & eCachedAreaData_DistWithinSqValid) != 0)
		{
			fClosestDistance = GetCachedPointWithinDistSq(nEntityID);
			OnHull3d = GetCachedPointOnHull(nEntityID);
		}
		else
		{
			Vec3 Closest3d(ZERO);

			switch (m_areaType)
			{
			case ENTITY_AREA_TYPE_SOLID:
				{
					CalcClosestPointToSolid(Point3d, bIgnoreSoundObstruction, fClosestDistance, &OnHull3d);

					break;
				}
			case ENTITY_AREA_TYPE_SHAPE:
				{
					if (!bIgnoreSoundObstruction)
					{
						CalcClosestPointToObstructedShape(nEntityID, OnHull3d, fClosestDistance, Point3d);
					}
					else
					{
						float fDistToRoof = 0.0f;
						float fDistToFloor = 0.0f;
						float fZDistTemp = 0.0f;

						if (m_height)
						{
							// negative means from inside to hull
							fDistToRoof = Point3d.z - (m_origin + m_height);
							fDistToFloor = m_origin - Point3d.z;

							if (fabsf(fDistToFloor) < fabsf(fDistToRoof))
							{
								// below
								if ((m_state & Cry::AreaManager::EAreaState::ObstructFloor) != 0)
								{
									fDistToFloor = 0.0f;
									fZDistTemp = fDistToRoof;
								}
								else
								{
									fDistToRoof = 0.0f;
									fZDistTemp = fDistToFloor;
								}
							}
							else
							{
								// above
								if ((m_state & Cry::AreaManager::EAreaState::ObstructRoof) != 0)
								{
									fDistToRoof = 0.0f;
									fZDistTemp = fDistToFloor;
								}
								else
								{
									fDistToFloor = 0.0f;
									fZDistTemp = fDistToRoof;
								}
							}
						}

						float fZDistSq = fZDistTemp * fZDistTemp;
						float fXDistSq = 0.0f;

						bool bIsIn2DShape = false;

						if (pCachedData && (pCachedData->eFlags & eCachedAreaData_PosTypeValid) != 0)
						{
							bIsIn2DShape = (pCachedData->ePosType == AREA_POS_TYPE_2DINSIDE_ZABOVE)
							               || (pCachedData->ePosType == AREA_POS_TYPE_2DINSIDE_ZINSIDE)
							               || (pCachedData->ePosType == AREA_POS_TYPE_2DINSIDE_ZBELOW);
						}
						else
						{
							bIsIn2DShape = (CalcPosType(nEntityID, Point3d) == AREA_POS_TYPE_2DINSIDE_ZINSIDE);
						}

						//// point is not under or above area shape, so approach from the side
						// Find the line segment that is closest to the 2d point.
						size_t const nSegmentSize = m_areaSegments.size();

						for (size_t sIdx = 0; sIdx < nSegmentSize; sIdx++)
						{
							float fT;
							a2DSegment* curSg = m_areaSegments[sIdx];

							Vec3 startSeg(curSg->GetStart().x, curSg->GetStart().y, Point3d.z);
							Vec3 endSeg(curSg->GetEnd().x, curSg->GetEnd().y, Point3d.z);
							Lineseg line(startSeg, endSeg);

							// Returns distance from a point to a line segment, ignoring the z coordinates
							fXDistSq = Distance::Point_Lineseg2DSq(Point3d, line, fT);

							float fThisDistance = 0.0f;

							if (bIsIn2DShape && fZDistSq)
								fThisDistance = min(fXDistSq, fZDistSq);
							else
								fThisDistance = fXDistSq + fZDistSq;

							// is this closer than the previous one?
							if (fThisDistance < fClosestDistance)
							{
								fClosestDistance = fThisDistance;
								// find closest point
								if (fZDistSq && fZDistSq < fXDistSq)
								{
									Closest3d = Point3d;
									Closest3d.z = Point3d.z + fDistToFloor - fDistToRoof;
								}
								else
									Closest3d = (line.GetPoint(fT));

							}
						}

						OnHull3d = Closest3d;
					}

					break;
				}

			case ENTITY_AREA_TYPE_SPHERE:
				{
					Vec3 Temp = Point3d - m_sphereCenter;
					OnHull3d = Temp.normalize() * m_sphereRadius;
					OnHull3d += m_sphereCenter;
					fClosestDistance = OnHull3d.GetSquaredDistance(Point3d);

					break;
				}
			case ENTITY_AREA_TYPE_BOX:
				{
					if (!bIgnoreSoundObstruction)
					{
						CalcClosestPointToObstructedBox(OnHull3d, fClosestDistance, Point3d);
					}
					else
					{
						Vec3 const p3d = m_invMatrix.TransformPoint(Point3d);
						AABB const myAABB(m_boxMin, m_boxMax);
						fClosestDistance = Distance::Point_AABBSq(p3d, myAABB, OnHull3d);

						if (m_boxSideObstruction[4].bObstructed && OnHull3d.z == myAABB.max.z)
						{
							// Point is on the roof plane, but may be on the edge already
							Vec2 vTop(OnHull3d.x, myAABB.max.y);
							Vec2 vLeft(myAABB.min.x, OnHull3d.y);
							Vec2 vLow(OnHull3d.x, myAABB.min.y);
							Vec2 vRight(myAABB.max.x, OnHull3d.y);

							float fDistanceToTop = p3d.GetSquaredDistance2D(vTop);
							float fDistanceToLeft = p3d.GetSquaredDistance2D(vLeft);
							float fDistanceToLow = p3d.GetSquaredDistance2D(vLow);
							float fDistanceToRight = p3d.GetSquaredDistance2D(vRight);
							float fTempMinDistance = fDistanceToTop;

							OnHull3d = vTop;

							if (fDistanceToLeft < fTempMinDistance)
							{
								OnHull3d = vLeft;
								fTempMinDistance = fDistanceToLeft;
							}

							if (fDistanceToLow < fTempMinDistance)
							{
								OnHull3d = vLow;
								fTempMinDistance = fDistanceToLow;
							}

							if (fDistanceToRight < fTempMinDistance)
							{
								OnHull3d = vRight;
							}

							OnHull3d.z = min(myAABB.max.z, p3d.z);
							fClosestDistance = OnHull3d.GetSquaredDistance(p3d);

						}

						if (m_boxSideObstruction[5].bObstructed && OnHull3d.z == myAABB.min.z)
						{
							// Point is on the roof plane, but may be on the edge already
							Vec2 vTop(OnHull3d.x, myAABB.max.y);
							Vec2 vLeft(myAABB.min.x, OnHull3d.y);
							Vec2 vLow(OnHull3d.x, myAABB.min.y);
							Vec2 vRight(myAABB.max.x, OnHull3d.y);

							float fDistanceToTop = p3d.GetSquaredDistance2D(vTop);
							float fDistanceToLeft = p3d.GetSquaredDistance2D(vLeft);
							float fDistanceToLow = p3d.GetSquaredDistance2D(vLow);
							float fDistanceToRight = p3d.GetSquaredDistance2D(vRight);
							float fTempMinDistance = fDistanceToTop;

							OnHull3d = vTop;

							if (fDistanceToLeft < fTempMinDistance)
							{
								OnHull3d = vLeft;
								fTempMinDistance = fDistanceToLeft;
							}

							if (fDistanceToLow < fTempMinDistance)
							{
								OnHull3d = vLow;
								fTempMinDistance = fDistanceToLow;
							}

							if (fDistanceToRight < fTempMinDistance)
							{
								OnHull3d = vRight;
							}

							OnHull3d.z = max(myAABB.min.z, p3d.z);
							fClosestDistance = OnHull3d.GetSquaredDistance(p3d);
						}

						OnHull3d = m_worldTM.TransformPoint(OnHull3d);
					}

					// TODO transform point back to world ... (why?)
					break;
				}
			default:
				{
					CryFatalError("Unknown area type during CArea::ClosestPointOnHullDistSq");

					break;
				}
			}

			// Put the shortest distance and point into the data cache.
			if (pCachedData != nullptr && bCacheResult)
			{
				if (bIgnoreSoundObstruction)
				{
					pCachedData->eFlags = (ECachedAreaData)(pCachedData->eFlags | eCachedAreaData_PosOnHullValid | eCachedAreaData_DistWithinSqValid | eCachedAreaData_DistWithinSqValidNotObstructed);
				}
				else
				{
					pCachedData->eFlags = (ECachedAreaData)(pCachedData->eFlags | eCachedAreaData_PosOnHullValid | eCachedAreaData_DistWithinSqValid);
				}

				pCachedData->oPos = OnHull3d;
				pCachedData->fDistanceWithinSq = fClosestDistance;
			}
		}
	}

	return fClosestDistance;
}

//////////////////////////////////////////////////////////////////////////
float CArea::CalcPointNearDistSq(EntityId const nEntityID, Vec3 const& Point3d, Vec3& OnHull3d, bool const bIgnoreSoundObstruction /* = true */, bool const bCacheResult /* = true */)
{
	CRY_PROFILE_FUNCTION(PROFILE_ENTITY);
	float fClosestDistance = -1.0f;

	if ((m_state & Cry::AreaManager::EAreaState::Initialized) != 0)
	{
		SCachedAreaData* pCachedData = nullptr;

		if (nEntityID != INVALID_ENTITYID)
		{
			TEntityCachedAreaDataMap::iterator Iter(m_mapEntityCachedAreaData.find(nEntityID));

			if (Iter != m_mapEntityCachedAreaData.end())
			{
				pCachedData = &(Iter->second);
			}
		}

		if (pCachedData != nullptr && (pCachedData->eFlags & eCachedAreaData_PosOnHullValid) != 0 &&
		    (pCachedData->eFlags & eCachedAreaData_DistNearSqValid) != 0)
		{
			fClosestDistance = GetCachedPointNearDistSq(nEntityID);
			OnHull3d = GetCachedPointOnHull(nEntityID);
		}
		else
		{
			Vec3 Closest3d(ZERO);

			switch (m_areaType)
			{
			case ENTITY_AREA_TYPE_SOLID:
				{
					CalcClosestPointToSolid(Point3d, bIgnoreSoundObstruction, fClosestDistance, &OnHull3d);

					break;
				}
			case ENTITY_AREA_TYPE_SHAPE:
				{
					if (!bIgnoreSoundObstruction)
					{
						CalcClosestPointToObstructedShape(nEntityID, OnHull3d, fClosestDistance, Point3d);
					}
					else
					{
						float fZDistSq = 0.0f;
						float fXDistSq = FLT_MAX;

						// first find the closest edge
						size_t const nSegmentSize = m_areaSegments.size();

						for (size_t sIdx = 0; sIdx < nSegmentSize; sIdx++)
						{
							float fT = 0.0f;
							a2DSegment const* const curSg = m_areaSegments[sIdx];

							Vec3 startSeg(curSg->GetStart().x, curSg->GetStart().y, Point3d.z);
							Vec3 endSeg(curSg->GetEnd().x, curSg->GetEnd().y, Point3d.z);
							Lineseg line(startSeg, endSeg);

							// Returns distance from a point to a line segment, ignoring the z coordinates
							float fThisXDistSq = Distance::Point_Lineseg2DSq(Point3d, line, fT);

							if (fThisXDistSq < fXDistSq)
							{
								// find closest point
								fXDistSq = fThisXDistSq;
								Closest3d = (line.GetPoint(fT));
							}
						}

						// now we have Closest3d being the point on the 2D hull of the shape
						if (m_height)
						{
							// negative means from inside to hull
							float fDistToRoof = Point3d.z - (m_origin + m_height);
							float fDistToFloor = m_origin - Point3d.z;

							float fZRoofSq = fDistToRoof * fDistToRoof;
							float fZFloorSq = fDistToFloor * fDistToFloor;

							bool bIsIn2DShape = false;
							if (pCachedData != nullptr && (pCachedData->eFlags & eCachedAreaData_PosTypeValid) != 0)
							{
								bIsIn2DShape = (pCachedData->ePosType == AREA_POS_TYPE_2DINSIDE_ZABOVE)
								               || (pCachedData->ePosType == AREA_POS_TYPE_2DINSIDE_ZINSIDE)
								               || (pCachedData->ePosType == AREA_POS_TYPE_2DINSIDE_ZBELOW);
							}
							else
							{
								bIsIn2DShape = (CalcPosType(nEntityID, Point3d) == AREA_POS_TYPE_2DINSIDE_ZINSIDE);
							}

							if (bIsIn2DShape)
							{
								// point is below, in, or above the area

								if ((Point3d.z < m_origin + m_height && Point3d.z > m_origin))
								{
									// Point is inside z-boundary
									if (((m_state & Cry::AreaManager::EAreaState::ObstructRoof) == 0) && (fZRoofSq < fXDistSq) && (fZRoofSq < fZFloorSq))
									{
										// roof is closer than side
										fZDistSq = fZRoofSq;
										Closest3d = Point3d;
										fXDistSq = 0.0f;
									}

									if (((m_state & Cry::AreaManager::EAreaState::ObstructFloor) == 0) && (fZFloorSq < fXDistSq) && (fZFloorSq < fZRoofSq))
									{
										// floor is closer than side
										fZDistSq = fZFloorSq;
										Closest3d = Point3d;
										fXDistSq = 0.0f;
									}

									// correcting z-axis value
									if (fZRoofSq < fZFloorSq)
										Closest3d.z = Point3d.z - fDistToRoof;
									else
										Closest3d.z = Point3d.z - fDistToFloor;
								}
								else
								{
									// point is above or below area
									if (fabsf(fDistToRoof) < fabsf(fDistToFloor))
									{
										// being above
										if ((m_state & Cry::AreaManager::EAreaState::ObstructRoof) == 0)
										{
											// perpendicular point to Roof
											fXDistSq = 0.0f;
											Closest3d = Point3d;
										}
										// correcting z axis value
										Closest3d.z = m_origin + m_height;
										fZDistSq = fZRoofSq;
									}
									else
									{
										// being below
										if ((m_state & Cry::AreaManager::EAreaState::ObstructFloor) == 0)
										{
											// perpendicular point to Floor
											fXDistSq = 0.0f;
											Closest3d = Point3d;
										}

										// correcting z axis value
										Closest3d.z = m_origin;
										fZDistSq = fZFloorSq;
									}
								}
							}
							else
							{
								// outside of 2D Shape, so diagonal or only to the side
								if ((Point3d.z > m_origin + m_height || Point3d.z < m_origin))
								{
									// Point is outside z-boundary
									// point is above or below area
									if (fabsf(fDistToRoof) < fabsf(fDistToFloor))
									{
										// being above
										fZDistSq = fZRoofSq;
										Closest3d.z = m_origin + m_height;
									}
									else
									{
										// being below
										fZDistSq = fZFloorSq;
										Closest3d.z = m_origin;
									}
								}
								else
								{
									// on the side and outside of the area, so point is on the face
									// correct z-Value
									Closest3d.z = Point3d.z;
								}
							}
						}
						else
						{
							// infinite high area
							// Closest is on an edge, ZDistance is 0, so nothing really to do here
						}

						fClosestDistance = fXDistSq + fZDistSq;
						OnHull3d = Closest3d;
					}

					break;
				}
			case ENTITY_AREA_TYPE_SPHERE:
				{
					Vec3 Temp = Point3d - m_sphereCenter;
					OnHull3d = Temp.normalize() * m_sphereRadius;
					OnHull3d += m_sphereCenter;
					fClosestDistance = OnHull3d.GetSquaredDistance(Point3d);

					break;
				}
			case ENTITY_AREA_TYPE_BOX:
				{
					if (!bIgnoreSoundObstruction)
					{
						CalcClosestPointToObstructedBox(OnHull3d, fClosestDistance, Point3d);
					}
					else
					{
						Vec3 const p3d = m_invMatrix.TransformPoint(Point3d);
						AABB const myAABB(m_boxMin, m_boxMax);

						fClosestDistance = Distance::Point_AABBSq(p3d, myAABB, OnHull3d);

						if (m_boxSideObstruction[4].bObstructed && OnHull3d.z == myAABB.max.z)
						{
							// Point is on the roof plane, but may be on the edge already
							Vec2 vTop(OnHull3d.x, myAABB.max.y);
							Vec2 vLeft(myAABB.min.x, OnHull3d.y);
							Vec2 vLow(OnHull3d.x, myAABB.min.y);
							Vec2 vRight(myAABB.max.x, OnHull3d.y);

							float fDistanceToTop = p3d.GetSquaredDistance2D(vTop);
							float fDistanceToLeft = p3d.GetSquaredDistance2D(vLeft);
							float fDistanceToLow = p3d.GetSquaredDistance2D(vLow);
							float fDistanceToRight = p3d.GetSquaredDistance2D(vRight);
							float fTempMinDistance = fDistanceToTop;

							OnHull3d = vTop;

							if (fDistanceToLeft < fTempMinDistance)
							{
								OnHull3d = vLeft;
								fTempMinDistance = fDistanceToLeft;
							}

							if (fDistanceToLow < fTempMinDistance)
							{
								OnHull3d = vLow;
								fTempMinDistance = fDistanceToLow;
							}

							if (fDistanceToRight < fTempMinDistance)
							{
								OnHull3d = vRight;
							}

							OnHull3d.z = min(myAABB.max.z, p3d.z);
							fClosestDistance = OnHull3d.GetSquaredDistance(p3d);
						}

						if (m_boxSideObstruction[5].bObstructed && OnHull3d.z == myAABB.min.z)
						{
							// Point is on the roof plane, but may be on the edge already
							Vec2 vTop(OnHull3d.x, myAABB.max.y);
							Vec2 vLeft(myAABB.min.x, OnHull3d.y);
							Vec2 vLow(OnHull3d.x, myAABB.min.y);
							Vec2 vRight(myAABB.max.x, OnHull3d.y);

							float fDistanceToTop = p3d.GetSquaredDistance2D(vTop);
							float fDistanceToLeft = p3d.GetSquaredDistance2D(vLeft);
							float fDistanceToLow = p3d.GetSquaredDistance2D(vLow);
							float fDistanceToRight = p3d.GetSquaredDistance2D(vRight);
							float fTempMinDistance = fDistanceToTop;

							OnHull3d = vTop;

							if (fDistanceToLeft < fTempMinDistance)
							{
								OnHull3d = vLeft;
								fTempMinDistance = fDistanceToLeft;
							}

							if (fDistanceToLow < fTempMinDistance)
							{
								OnHull3d = vLow;
								fTempMinDistance = fDistanceToLow;
							}

							if (fDistanceToRight < fTempMinDistance)
							{
								OnHull3d = vRight;
							}

							OnHull3d.z = max(myAABB.min.z, p3d.z);
							fClosestDistance = OnHull3d.GetSquaredDistance(p3d);
						}

						OnHull3d = m_worldTM.TransformPoint(OnHull3d);
					}

					break;
				}
			default:
				{
					CryFatalError("Unknown area type during CArea::CalcPointNearDistSq (1)");

					break;
				}
			}

			// Put the shortest distance and point into the data cache.
			if (pCachedData != nullptr && bCacheResult)
			{
				if (bIgnoreSoundObstruction)
				{
					pCachedData->eFlags = (ECachedAreaData)(pCachedData->eFlags | eCachedAreaData_PosOnHullValid | eCachedAreaData_DistNearSqValid | eCachedAreaData_DistNearSqValidNotObstructed);
				}
				else
				{
					pCachedData->eFlags = (ECachedAreaData)(pCachedData->eFlags | eCachedAreaData_PosOnHullValid | eCachedAreaData_DistNearSqValid);
				}

				pCachedData->oPos = OnHull3d;
				pCachedData->fDistanceNearSq = fClosestDistance;
			}
		}
	}

	return fClosestDistance;
}

//////////////////////////////////////////////////////////////////////////
float CArea::CalcPointNearDistSq(EntityId const nEntityID, Vec3 const& Point3d, bool const bIgnoreHeight /* = false */, bool const bIgnoreSoundObstruction /* = true */, bool const bCacheResult /* = true */)
{
	CRY_PROFILE_FUNCTION(PROFILE_ENTITY);
	float fClosestDistance = -1.0f;

	if ((m_state & Cry::AreaManager::EAreaState::Initialized) != 0)
	{
		SCachedAreaData* pCachedData = nullptr;

		if (nEntityID != INVALID_ENTITYID)
		{
			TEntityCachedAreaDataMap::iterator Iter(m_mapEntityCachedAreaData.find(nEntityID));

			if (Iter != m_mapEntityCachedAreaData.end())
			{
				pCachedData = &(Iter->second);
			}
		}

		if (pCachedData != nullptr && (pCachedData->eFlags & eCachedAreaData_DistNearSqValid) != 0)
		{
			fClosestDistance = GetCachedPointNearDistSq(nEntityID);
		}
		else
		{
			switch (m_areaType)
			{
			case ENTITY_AREA_TYPE_SOLID:
				{
					CalcClosestPointToSolid(Point3d, bIgnoreSoundObstruction, fClosestDistance, nullptr);

					break;
				}
			case ENTITY_AREA_TYPE_SHAPE:
				{
					if (!bIgnoreSoundObstruction)
					{
						Vec3 oTemp(ZERO);
						CalcClosestPointToObstructedShape(nEntityID, oTemp, fClosestDistance, Point3d);
					}
					else
					{
						float fZDistSq = 0.0f;
						float fXDistSq = FLT_MAX;

						for (AreaSegments::const_iterator it = m_areaSegments.begin(), itEnd = m_areaSegments.end(); it != itEnd; ++it)
						{
							a2DSegment const* const curSg = *it;
							a2DPoint const curSgStart = curSg->GetStart(), curSgEnd = curSg->GetEnd();

							Lineseg line;
							line.start.x = curSgStart.x;
							line.start.y = curSgStart.y;
							line.end.x = curSgEnd.x;
							line.end.y = curSgEnd.y;

							/// Returns distance from a point to a line segment, ignoring the z coordinates
							float fThisXDistSq = Distance::Point_Lineseg2DSq(Point3d, line);
							fXDistSq = min(fXDistSq, fThisXDistSq);
						}

						if (m_height)
						{
							// negative means from inside to hull
							float const fDistToRoof = Point3d.z - (m_origin + m_height);
							float const fDistToFloor = m_origin - Point3d.z;

							float const fZRoofSq = fDistToRoof * fDistToRoof;
							float const fZFloorSq = fDistToFloor * fDistToFloor;

							bool bIsIn2DShape = false;

							if (pCachedData != nullptr && (pCachedData->eFlags & eCachedAreaData_PosTypeValid) != 0)
							{
								bIsIn2DShape = (pCachedData->ePosType == AREA_POS_TYPE_2DINSIDE_ZABOVE)
								               || (pCachedData->ePosType == AREA_POS_TYPE_2DINSIDE_ZINSIDE)
								               || (pCachedData->ePosType == AREA_POS_TYPE_2DINSIDE_ZBELOW);
							}
							else
							{
								bIsIn2DShape = (CalcPosType(nEntityID, Point3d) == AREA_POS_TYPE_2DINSIDE_ZINSIDE);
							}

							if (bIsIn2DShape)
							{
								// point is below, in, or above the area
								if ((Point3d.z < m_origin + m_height && Point3d.z > m_origin))
								{
									// Point is inside z-boundary
									if (((m_state & Cry::AreaManager::EAreaState::ObstructRoof) == 0) && (fZRoofSq < fXDistSq) && (fZRoofSq < fZFloorSq))
									{
										// roof is closer than side
										fZDistSq = fZRoofSq;
										fXDistSq = 0.0f;
									}

									if (((m_state & Cry::AreaManager::EAreaState::ObstructFloor) == 0) && (fZFloorSq < fXDistSq) && (fZFloorSq < fZRoofSq))
									{
										// floor is closer than side
										fZDistSq = fZFloorSq;
										fXDistSq = 0.0f;
									}
								}
								else
								{
									// point is above or below area
									if (fabsf(fDistToRoof) < fabsf(fDistToFloor))
									{
										// being above
										fZDistSq = fZRoofSq;

										if ((m_state & Cry::AreaManager::EAreaState::ObstructRoof) == 0)
										{
											// perpendicular point to Roof
											fXDistSq = 0.0f;
										}
									}
									else
									{
										// being below
										fZDistSq = fZFloorSq;

										if ((m_state & Cry::AreaManager::EAreaState::ObstructFloor) == 0)
										{
											// perpendicular point to Floor
											fXDistSq = 0.0f;
										}
									}
								}
							}
							else
							{
								// outside of 2D Shape, so diagonal or only to the side
								if ((Point3d.z < m_origin + m_height && Point3d.z > m_origin))
								{
									// Point is inside z-boundary
								}
								else
								{
									// point is above or below area
									if (fabsf(fDistToRoof) < fabsf(fDistToFloor))
									{
										// being above
										fZDistSq = fZRoofSq;
									}
									else
									{
										// being below
										fZDistSq = fZFloorSq;
									}
								}
							}
						}
						else
						{
							// infinite high area
							// Closest is on an edge, ZDistance is 0, so nothing really to do here
						}

						fClosestDistance = fXDistSq;

						if (!bIgnoreHeight)
							fClosestDistance += fZDistSq;
					}

					break;
				}
			case ENTITY_AREA_TYPE_SPHERE:
				{
					Vec3 vTemp(Point3d);

					if (bIgnoreHeight)
						vTemp.z = m_sphereCenter.z;

					float const fLength = m_sphereCenter.GetDistance(vTemp) - m_sphereRadius;
					fClosestDistance = fLength * fLength;

					break;
				}
			case ENTITY_AREA_TYPE_BOX:
				{
					if (!bIgnoreSoundObstruction)
					{
						Vec3 oTemp(ZERO);
						CalcClosestPointToObstructedBox(oTemp, fClosestDistance, Point3d);
					}
					else
					{
						Vec3 p3d = m_invMatrix.TransformPoint(Point3d);
						Vec3 OnHull3d;

						if (bIgnoreHeight)
							p3d.z = m_boxMin.z;

						AABB myAABB(m_boxMin, m_boxMax);

						fClosestDistance = Distance::Point_AABBSq(p3d, myAABB, OnHull3d);

						if (m_boxSideObstruction[4].bObstructed && OnHull3d.z == myAABB.max.z)
						{
							// Point is on the roof plane, but may be on the edge already
							Vec2 vTop(OnHull3d.x, myAABB.max.y);
							Vec2 vLeft(myAABB.min.x, OnHull3d.y);
							Vec2 vLow(OnHull3d.x, myAABB.min.y);
							Vec2 vRight(myAABB.max.x, OnHull3d.y);

							float fDistanceToTop = p3d.GetSquaredDistance2D(vTop);
							float fDistanceToLeft = p3d.GetSquaredDistance2D(vLeft);
							float fDistanceToLow = p3d.GetSquaredDistance2D(vLow);
							float fDistanceToRight = p3d.GetSquaredDistance2D(vRight);
							float fTempMinDistance = fDistanceToTop;

							OnHull3d = vTop;

							if (fDistanceToLeft < fTempMinDistance)
							{
								OnHull3d = vLeft;
								fTempMinDistance = fDistanceToLeft;
							}

							if (fDistanceToLow < fTempMinDistance)
							{
								OnHull3d = vLow;
								fTempMinDistance = fDistanceToLow;
							}

							if (fDistanceToRight < fTempMinDistance)
							{
								OnHull3d = vRight;
							}

							OnHull3d.z = min(myAABB.max.z, p3d.z);
							fClosestDistance = OnHull3d.GetSquaredDistance(p3d);

						}

						if (m_boxSideObstruction[5].bObstructed && OnHull3d.z == myAABB.min.z)
						{
							// Point is on the floor plane, but may be on the edge already
							Vec2 vTop(OnHull3d.x, myAABB.max.y);
							Vec2 vLeft(myAABB.min.x, OnHull3d.y);
							Vec2 vLow(OnHull3d.x, myAABB.min.y);
							Vec2 vRight(myAABB.max.x, OnHull3d.y);

							float fDistanceToTop = p3d.GetSquaredDistance2D(vTop);
							float fDistanceToLeft = p3d.GetSquaredDistance2D(vLeft);
							float fDistanceToLow = p3d.GetSquaredDistance2D(vLow);
							float fDistanceToRight = p3d.GetSquaredDistance2D(vRight);
							float fTempMinDistance = fDistanceToTop;

							OnHull3d = vTop;

							if (fDistanceToLeft < fTempMinDistance)
							{
								OnHull3d = vLeft;
								fTempMinDistance = fDistanceToLeft;
							}

							if (fDistanceToLow < fTempMinDistance)
							{
								OnHull3d = vLow;
								fTempMinDistance = fDistanceToLow;
							}

							if (fDistanceToRight < fTempMinDistance)
							{
								OnHull3d = vRight;
							}

							OnHull3d.z = max(myAABB.min.z, p3d.z);
							fClosestDistance = OnHull3d.GetSquaredDistance(p3d);
						}
					}

					// TODO transform point back to world ... (why?)
					break;
				}
			default:
				{
					CryFatalError("Unknown area type during CArea::CalcPointNearDistSq (2)");

					break;
				}
			}

			// Put the shortest distance into the data cache
			if (pCachedData != nullptr && bCacheResult)
			{
				if (bIgnoreSoundObstruction)
				{
					if (bIgnoreHeight)
					{
						pCachedData->eFlags = (ECachedAreaData)(pCachedData->eFlags | eCachedAreaData_DistNearSqValid | eCachedAreaData_DistNearSqValidNotObstructed | eCachedAreaData_DistNearSqValidHeightIgnored);
					}
					else
					{
						pCachedData->eFlags = (ECachedAreaData)(pCachedData->eFlags | eCachedAreaData_DistNearSqValid | eCachedAreaData_DistNearSqValidNotObstructed);
					}
				}
				else
				{
					if (bIgnoreHeight)
					{
						pCachedData->eFlags = (ECachedAreaData)(pCachedData->eFlags | eCachedAreaData_DistNearSqValid | eCachedAreaData_DistNearSqValidHeightIgnored);
					}
					else
					{
						pCachedData->eFlags = (ECachedAreaData)(pCachedData->eFlags | eCachedAreaData_DistNearSqValid);
					}
				}

				pCachedData->fDistanceNearSq = fClosestDistance;
			}
		}
	}

	return fClosestDistance;
}

//////////////////////////////////////////////////////////////////////////
EAreaPosType CArea::CalcPosType(EntityId const nEntityID, Vec3 const& rPos, bool const bCacheResult /* = true */)
{
	CRY_PROFILE_FUNCTION(PROFILE_ENTITY);
	EAreaPosType eTempPosType = AREA_POS_TYPE_COUNT;

	if ((m_state & Cry::AreaManager::EAreaState::Initialized) != 0)
	{
		SCachedAreaData* pCachedData = nullptr;

		if (nEntityID != INVALID_ENTITYID)
		{
			TEntityCachedAreaDataMap::iterator Iter(m_mapEntityCachedAreaData.find(nEntityID));

			if (Iter != m_mapEntityCachedAreaData.end())
			{
				pCachedData = &(Iter->second);
			}
		}

		if (pCachedData != nullptr && (pCachedData->eFlags & eCachedAreaData_PosTypeValid) != 0)
		{
			eTempPosType = GetCachedPointPosTypeWithin(nEntityID);
		}
		else
		{
			bool const bIsIn2DShape = CalcPointWithin(nEntityID, rPos, true, false);

			switch (m_areaType)
			{
			case ENTITY_AREA_TYPE_SOLID:
				{
					Vec3 const ov3PosLocalSpace(m_invMatrix.TransformPoint(rPos));
					AABB const boundbox(m_pAreaSolid->GetBoundBox());

					if (ov3PosLocalSpace.z < boundbox.min.z)
						eTempPosType = AREA_POS_TYPE_2DINSIDE_ZBELOW;
					else if (ov3PosLocalSpace.z > boundbox.max.z)
						eTempPosType = AREA_POS_TYPE_2DINSIDE_ZABOVE;
					else
					{
						if (bIsIn2DShape)
							eTempPosType = AREA_POS_TYPE_2DINSIDE_ZINSIDE;
					}

					break;
				}
			case ENTITY_AREA_TYPE_SHAPE:
				{
					if (m_height > 0.0f)
					{
						// Negative means from inside to hull
						float const fDistToRoof = rPos.z - (m_origin + m_height);
						float const fDistToFloor = m_origin - rPos.z;

						if (bIsIn2DShape)
						{
							// Point is below, in, or above the area
							if ((rPos.z < m_origin + m_height && rPos.z > m_origin))
								eTempPosType = AREA_POS_TYPE_2DINSIDE_ZINSIDE;
							else
							{
								// Point is above or below area
								if (fabsf(fDistToRoof) < fabsf(fDistToFloor))
									eTempPosType = AREA_POS_TYPE_2DINSIDE_ZABOVE;
								else
									eTempPosType = AREA_POS_TYPE_2DINSIDE_ZBELOW;
							}
						}
						else
						{
							// Outside of 2D Shape, so diagonal or only to the side
							if ((rPos.z > m_origin + m_height || rPos.z < m_origin))
							{
								// Point is outside z-boundary
								// point is above or below area
								if (fabsf(fDistToRoof) < fabsf(fDistToFloor))
									eTempPosType = AREA_POS_TYPE_2DOUTSIDE_ZABOVE;
								else
									eTempPosType = AREA_POS_TYPE_2DOUTSIDE_ZBELOW;
							}
							else
								eTempPosType = AREA_POS_TYPE_2DOUTSIDE_ZINSIDE;
						}
					}
					else
					{
						// No height means infinite Z boundaries (no roof, no floor)
						if (bIsIn2DShape)
							eTempPosType = AREA_POS_TYPE_2DINSIDE_ZINSIDE;
						else
							eTempPosType = AREA_POS_TYPE_2DOUTSIDE_ZINSIDE;
					}

					break;
				}
			case ENTITY_AREA_TYPE_BOX:
				{
					if (m_boxMax.z > 0.0f)
					{
						Vec3 const ov3PosLocalSpace(m_invMatrix.TransformPoint(rPos));
						if (bIsIn2DShape)
						{
							if (ov3PosLocalSpace.z < m_boxMin.z) // Warning : Fix for boxes not centered around the base of an entity. Please check before back integrating.
								eTempPosType = AREA_POS_TYPE_2DINSIDE_ZBELOW;
							else if (ov3PosLocalSpace.z > m_boxMax.z)
								eTempPosType = AREA_POS_TYPE_2DINSIDE_ZABOVE;
							else
								eTempPosType = AREA_POS_TYPE_2DINSIDE_ZINSIDE;
						}
						else
						{
							if (ov3PosLocalSpace.z < m_boxMin.z) // Warning : Fix for boxes not centered around the base of an entity. Please check before back integrating.
								eTempPosType = AREA_POS_TYPE_2DOUTSIDE_ZBELOW;
							else if (ov3PosLocalSpace.z > m_boxMax.z)
								eTempPosType = AREA_POS_TYPE_2DOUTSIDE_ZABOVE;
							else
								eTempPosType = AREA_POS_TYPE_2DOUTSIDE_ZINSIDE;
						}
					}
					else
					{
						// No height means infinite Z boundaries (no roof, no floor)
						if (bIsIn2DShape)
							eTempPosType = AREA_POS_TYPE_2DINSIDE_ZINSIDE;
						else
							eTempPosType = AREA_POS_TYPE_2DOUTSIDE_ZINSIDE;
					}

					break;
				}
			case ENTITY_AREA_TYPE_SPHERE:
				{
					float const fSphereMostTop = m_sphereCenter.z + m_sphereRadius;
					float const fSphereMostBottom = m_sphereCenter.z - m_sphereRadius;
					bool const bAbove = rPos.z > fSphereMostTop;
					bool const bBelow = rPos.z < fSphereMostBottom;
					bool const bIsIn3DShape = CalcPointWithin(nEntityID, rPos, false);

					if (bIsIn3DShape)
						eTempPosType = AREA_POS_TYPE_2DINSIDE_ZINSIDE;
					else
					{
						if (bIsIn2DShape)
						{
							// We're inside of the sphere 2D silhouette but not inside the sphere itself
							// now check if we're above its max z or below its min z
							if (bAbove)
								eTempPosType = AREA_POS_TYPE_2DINSIDE_ZABOVE;

							if (bBelow)
								eTempPosType = AREA_POS_TYPE_2DINSIDE_ZBELOW;
						}
						else
						{
							// We're outside of the sphere 2D silhouette and outside of the sphere itself
							// now check if we're above its max z or below its min z
							eTempPosType = AREA_POS_TYPE_2DOUTSIDE_ZINSIDE;

							if (bAbove)
								eTempPosType = AREA_POS_TYPE_2DOUTSIDE_ZABOVE;

							if (bBelow)
								eTempPosType = AREA_POS_TYPE_2DOUTSIDE_ZBELOW;
						}
					}

					break;
				}
			default:
				{
					CryFatalError("Unknown area type during CArea::CalcPosType");

					break;
				}
			}

			if (pCachedData != nullptr && bCacheResult)
			{
				pCachedData->eFlags = (ECachedAreaData)(pCachedData->eFlags | eCachedAreaData_PosTypeValid);
				pCachedData->ePosType = eTempPosType;
			}
		}
	}

	return eTempPosType;
}

//////////////////////////////////////////////////////////////////////////
void CArea::CalcClosestPointToObstructedShape(EntityId const nEntityID, Vec3& outClosest, float& outClosestDistSq, Vec3 const& sourcePos)
{
	CRY_PROFILE_FUNCTION(PROFILE_ENTITY);
	size_t const numSegments = m_areaSegments.size();
	Lineseg oLine;
	Vec3 closest(ZERO);
	float closestDistSq(FLT_MAX);

	// If this area got a height
	if (m_height)
	{
		EAreaPosType const ePosType = CalcPosType(nEntityID, sourcePos);
		float const fRoofWorldPosZ = m_origin + m_height;

		// Find the closest point
		// First of all check if we're either right above a non-obstructing roof or below a non-obstructing floor
		// if so, just use this data since we have the shortest distance right there
		if (ePosType == AREA_POS_TYPE_2DINSIDE_ZABOVE && ((m_state & Cry::AreaManager::EAreaState::ObstructRoof) == 0))
		{
			closestDistSq = (sourcePos.z - fRoofWorldPosZ) * (sourcePos.z - fRoofWorldPosZ);
			closest.x = sourcePos.x;
			closest.y = sourcePos.y;
			closest.z = fRoofWorldPosZ;
		}
		else if (ePosType == AREA_POS_TYPE_2DINSIDE_ZBELOW && ((m_state & Cry::AreaManager::EAreaState::ObstructFloor) == 0))
		{
			closestDistSq = (m_origin - sourcePos.z) * (m_origin - sourcePos.z);
			closest.x = sourcePos.x;
			closest.y = sourcePos.y;
			closest.z = m_origin;
		}
		else
		{
			PrefetchLine(&m_areaSegments[0], 0);

			for (size_t nIdx = 0; nIdx < numSegments; ++nIdx)
			{
				a2DSegment const* const curSg = m_areaSegments[nIdx];
				float const fCurrSegStart[2] = { curSg->GetStart().x, curSg->GetStart().y };
				float const fCurrSegEnd[2] = { curSg->GetEnd().x, curSg->GetEnd().y };

				// If the segment is not obstructed get the closest point to it
				if (!curSg->bObstructSound)
				{
					float fPosZ = sourcePos.z;
					bool bAdjusted = false;

					// Adjust Z position if we're outside the boundaries
					if (fPosZ > fRoofWorldPosZ)
					{
						fPosZ = fRoofWorldPosZ;
						bAdjusted = true;
					}
					else if (fPosZ < m_origin)
					{
						fPosZ = m_origin;
						bAdjusted = true;
					}

					oLine.start = Vec3(fCurrSegStart[0], fCurrSegStart[1], fPosZ);
					oLine.end = Vec3(fCurrSegEnd[0], fCurrSegEnd[1], fPosZ);

					// If we're outside the Z boundaries we need to include Z on our test
					float fT;
					float const fTempDistToLineSq = bAdjusted ?
					                                Distance::Point_LinesegSq(sourcePos, oLine, fT) :
					                                Distance::Point_Lineseg2DSq(sourcePos, oLine, fT);

					if (fTempDistToLineSq < closestDistSq)
					{
						closestDistSq = fTempDistToLineSq;
						closest = oLine.GetPoint(fT);
					}
				}
				else
				{
					// Otherwise we need to check the roof and the floor if we're not inside the area
					if (ePosType != AREA_POS_TYPE_2DINSIDE_ZINSIDE)
					{
						// Roof
						if ((m_state & Cry::AreaManager::EAreaState::ObstructRoof) == 0)
						{
							oLine.start = Vec3(fCurrSegStart[0], fCurrSegStart[1], fRoofWorldPosZ);
							oLine.end = Vec3(fCurrSegEnd[0], fCurrSegEnd[1], fRoofWorldPosZ);
							float fT;
							float const fTempDistToLineSq = Distance::Point_LinesegSq(sourcePos, oLine, fT);

							if (fTempDistToLineSq < closestDistSq)
							{
								closestDistSq = fTempDistToLineSq;
								closest = oLine.GetPoint(fT);
							}
						}

						// Floor
						if ((m_state & Cry::AreaManager::EAreaState::ObstructFloor) == 0)
						{
							oLine.start = Vec3(fCurrSegStart[0], fCurrSegStart[1], m_origin);
							oLine.end = Vec3(fCurrSegEnd[0], fCurrSegEnd[1], m_origin);

							float fT;
							float const fTempDistToLineSq = Distance::Point_LinesegSq(sourcePos, oLine, fT);

							if (fTempDistToLineSq < closestDistSq)
							{
								closestDistSq = fTempDistToLineSq;
								closest = oLine.GetPoint(fT);
							}
						}
					}
				}
			}
		}

		// If we're inside an area we always need to check a non-obstructing roof and floor
		// this needs to be done only once right after the loop
		if (ePosType == AREA_POS_TYPE_2DINSIDE_ZINSIDE)
		{
			// Roof
			if ((m_state & Cry::AreaManager::EAreaState::ObstructRoof) == 0)
			{
				float const fTempDistToLineSq = (sourcePos.z - fRoofWorldPosZ) * (sourcePos.z - fRoofWorldPosZ);

				if (fTempDistToLineSq < closestDistSq)
				{
					closestDistSq = fTempDistToLineSq;
					closest.x = sourcePos.x;
					closest.y = sourcePos.y;
					closest.z = fRoofWorldPosZ;
				}
			}

			// Floor
			if ((m_state & Cry::AreaManager::EAreaState::ObstructFloor) == 0)
			{
				float const fTempDistToLineSq = (m_origin - sourcePos.z) * (m_origin - sourcePos.z);

				if (fTempDistToLineSq < closestDistSq)
				{
					closestDistSq = fTempDistToLineSq;
					closest.x = sourcePos.x;
					closest.y = sourcePos.y;
					closest.z = m_origin;
				}
			}
		}
	}
	else // This area has got infinite height
	{
		// Find the closest distance to non-obstructing segments, at source pos height
		for (size_t nIdx = 0; nIdx < numSegments; ++nIdx)
		{
			a2DSegment const* const curSg = m_areaSegments[nIdx];
			float const fCurrSegStart[2] = { curSg->GetStart().x, curSg->GetStart().y };
			float const fCurrSegEnd[2] = { curSg->GetEnd().x, curSg->GetEnd().y };

			// If the segment is not obstructed get the closest point and continue
			if (!curSg->bObstructSound)
			{
				oLine.start = Vec3(fCurrSegStart[0], fCurrSegStart[1], sourcePos.z);
				oLine.end = Vec3(fCurrSegEnd[0], fCurrSegEnd[1], sourcePos.z);

				float fT;
				float const fTempDistToLineSq = Distance::Point_Lineseg2DSq(sourcePos, oLine, fT); // Ignore Z

				if (fTempDistToLineSq < closestDistSq)
				{
					closestDistSq = fTempDistToLineSq;
					closest = oLine.GetPoint(fT);
				}
			}
		}
	}

	// If there was no calculation, most likely all sides + roof + floor are obstructed, return the source position
	if (closest.IsZeroFast() && closestDistSq == FLT_MAX)
		closest = sourcePos;

	outClosest = closest;
	outClosestDistSq = closestDistSq;
}

//////////////////////////////////////////////////////////////////////////
void CArea::CalcClosestPointToObstructedBox(Vec3& outClosest, float& outClosestDistSq, Vec3 const& sourcePos) const
{
	CRY_PROFILE_FUNCTION(PROFILE_ENTITY);
	Vec3 source(sourcePos);
	Vec3 closest(sourcePos);
	outClosestDistSq = FLT_MAX;

	if (!m_bAllObstructed)
	{
		Vec3 const posLocalSpace(m_invMatrix.TransformPoint(source)), boxMin = m_boxMin, boxMax = m_boxMax;

		// Determine the closest side and check for obstruction
		int closestSideIndex = -1;
		bool bAllObstructedExceptBackside = false;
		Vec3 boxBackSideEdges[4];

		if (posLocalSpace.x > boxMax.x)
		{
			// Side 2 is closest, 4 is backside
			closestSideIndex = 1;

			if (m_numObstructed == 5 && !m_boxSideObstruction[3].bObstructed)
			{
				// Describe possible snapping positions on back side 4
				boxBackSideEdges[0] = Vec3(boxMin.x, boxMin.y, posLocalSpace.z);
				boxBackSideEdges[1] = Vec3(boxMin.x, posLocalSpace.y, boxMax.z);
				boxBackSideEdges[2] = Vec3(boxMin.x, boxMax.y, posLocalSpace.z);
				boxBackSideEdges[3] = Vec3(boxMin.x, posLocalSpace.y, boxMin.z);

				bAllObstructedExceptBackside = true;
			}
		}
		else if (posLocalSpace.x < boxMin.x)
		{
			// Side 4 is closest, 2 is backside
			closestSideIndex = 3;

			if (m_numObstructed == 5 && !m_boxSideObstruction[1].bObstructed)
			{
				// Describe possible snapping positions on back side 2
				boxBackSideEdges[0] = Vec3(boxMax.x, boxMax.y, posLocalSpace.z);
				boxBackSideEdges[1] = Vec3(boxMax.x, posLocalSpace.y, boxMax.z);
				boxBackSideEdges[2] = Vec3(boxMax.x, boxMin.y, posLocalSpace.z);
				boxBackSideEdges[3] = Vec3(boxMax.x, posLocalSpace.y, boxMin.z);

				bAllObstructedExceptBackside = true;
			}
		}
		else
		{
			if (posLocalSpace.y < boxMin.y)
			{
				// Side 0 is closest, 2 is backside
				closestSideIndex = 0;

				if (m_numObstructed == 5 && !m_boxSideObstruction[2].bObstructed)
				{
					// Describe possible snapping positions on back side 2
					boxBackSideEdges[0] = Vec3(boxMin.x, boxMax.y, posLocalSpace.z);
					boxBackSideEdges[1] = Vec3(posLocalSpace.x, boxMax.y, boxMax.z);
					boxBackSideEdges[2] = Vec3(boxMax.x, boxMax.y, posLocalSpace.z);
					boxBackSideEdges[3] = Vec3(posLocalSpace.x, boxMax.y, boxMin.z);

					bAllObstructedExceptBackside = true;
				}
			}
			else if (posLocalSpace.y > boxMax.y)
			{
				// Side 2 is closest, 0 is backside
				closestSideIndex = 2;

				if (m_numObstructed == 5 && !m_boxSideObstruction[0].bObstructed)
				{
					// Describe possible snapping positions on back side 0
					boxBackSideEdges[0] = Vec3(boxMax.x, boxMin.y, posLocalSpace.z);
					boxBackSideEdges[1] = Vec3(posLocalSpace.x, boxMin.y, boxMax.z);
					boxBackSideEdges[2] = Vec3(boxMin.x, boxMin.y, posLocalSpace.z);
					boxBackSideEdges[3] = Vec3(posLocalSpace.x, boxMin.y, boxMin.z);

					bAllObstructedExceptBackside = true;
				}
			}
			else
			{
				if (posLocalSpace.z < boxMin.z)
				{
					// Side 5 is closest, 4 is backside
					closestSideIndex = 5;

					if (m_numObstructed == 5 && !m_boxSideObstruction[4].bObstructed)
					{
						// Describe possible snapping positions on back side 4
						boxBackSideEdges[0] = Vec3(posLocalSpace.x, boxMin.y, boxMax.z);
						boxBackSideEdges[1] = Vec3(boxMin.x, posLocalSpace.y, boxMax.z);
						boxBackSideEdges[2] = Vec3(posLocalSpace.x, boxMax.y, boxMax.z);
						boxBackSideEdges[3] = Vec3(boxMax.x, posLocalSpace.y, boxMax.z);

						bAllObstructedExceptBackside = true;
					}
				}
				else if (posLocalSpace.z > boxMax.z)
				{
					// Side 4 is closest, 5 is backside
					closestSideIndex = 4;

					if (m_numObstructed == 5 && !m_boxSideObstruction[5].bObstructed)
					{
						// Describe possible snapping positions on back side 5
						boxBackSideEdges[0] = Vec3(posLocalSpace.x, boxMin.y, boxMin.z);
						boxBackSideEdges[1] = Vec3(boxMin.x, posLocalSpace.y, boxMin.z);
						boxBackSideEdges[2] = Vec3(posLocalSpace.x, boxMax.y, boxMin.z);
						boxBackSideEdges[3] = Vec3(boxMax.x, posLocalSpace.y, boxMin.z);

						bAllObstructedExceptBackside = true;
					}
				}
				else
				{
					// We're inside the box
					float const distanceSide[6] =
					{
						fabs(m_boxSideObstruction[0].bObstructed ? FLT_MAX : boxMin.y - posLocalSpace.y),
						fabs(m_boxSideObstruction[1].bObstructed ? FLT_MAX : boxMax.x - posLocalSpace.x),
						fabs(m_boxSideObstruction[2].bObstructed ? FLT_MAX : boxMax.y - posLocalSpace.y),
						fabs(m_boxSideObstruction[3].bObstructed ? FLT_MAX : boxMin.x - posLocalSpace.x),
						fabs(m_boxSideObstruction[4].bObstructed ? FLT_MAX : boxMax.z - posLocalSpace.z),
						fabs(m_boxSideObstruction[5].bObstructed ? FLT_MAX : boxMin.z - posLocalSpace.z)
					};

					float const closestDistance = std::min<float>(std::min<float>(std::min<float>(distanceSide[0], distanceSide[1]), min(distanceSide[2], distanceSide[3])), min(distanceSide[4], distanceSide[5]));

					for (unsigned int i = 0; i < 6; ++i)
					{
						if (distanceSide[i] == closestDistance)
						{
							switch (i)
							{
							case 0:
								closest = Vec3(posLocalSpace.x, posLocalSpace.y - closestDistance, posLocalSpace.z);
								break;
							case 1:
								closest = Vec3(posLocalSpace.x + closestDistance, posLocalSpace.y, posLocalSpace.z);
								break;
							case 2:
								closest = Vec3(posLocalSpace.x, posLocalSpace.y + closestDistance, posLocalSpace.z);
								break;
							case 3:
								closest = Vec3(posLocalSpace.x - closestDistance, posLocalSpace.y, posLocalSpace.z);
								break;
							case 4:
								closest = Vec3(posLocalSpace.x, posLocalSpace.y, posLocalSpace.z + closestDistance);
								break;
							case 5:
								closest = Vec3(posLocalSpace.x, posLocalSpace.y, posLocalSpace.z - closestDistance);
								break;
							}

							// Transform it all back to world coordinates
							outClosest = m_worldTM.TransformPoint(closest);
							outClosestDistSq = Vec3(source - outClosest).GetLengthSquared();
							return;
						}
					}
				}
			}
		}

		// We're done determining the us facing side and we're not inside the box,
		// now check if the side not obstructed and return the closest position on it
		if (closestSideIndex > -1 && !m_boxSideObstruction[closestSideIndex].bObstructed)
		{
			// The shortest side is not obstructed
			closest.x = std::min<float>(std::max<float>(posLocalSpace.x, boxMin.x), boxMax.x);
			closest.y = std::min<float>(std::max<float>(posLocalSpace.y, boxMin.y), boxMax.y);
			closest.z = std::min<float>(std::max<float>(posLocalSpace.z, boxMin.z), boxMax.z);

			// Transform the result back to world values
			outClosest = m_worldTM.TransformPoint(closest);
			outClosestDistSq = Vec3(source - outClosest).GetLengthSquared();
			return;
		}

		// If we get here the closest side was obstructed
		// Now describe the 6 sides by applying min and max coordinate values
		SBoxSide const boxSides[6] =
		{
			SBoxSide(boxMin,        Vec3(boxMax.x, boxMin.y,  boxMax.z)),
			SBoxSide(Vec3(boxMax.x, boxMin.y,      boxMin.z), boxMax),
			SBoxSide(Vec3(boxMin.x, boxMax.y,      boxMin.z), boxMax),
			SBoxSide(boxMin,        Vec3(boxMin.x, boxMax.y,  boxMax.z)),
			SBoxSide(Vec3(boxMin.x, boxMin.y,      boxMax.z), boxMax),
			SBoxSide(boxMin,        Vec3(boxMax.x, boxMax.y,  boxMin.z))
		};

		// Check all non-obstructing sides, get the closest position on them
		float closestDistance = FLT_MAX;
		Vec3 closestPoint(ZERO);

		for (unsigned int i = 0; i < 6; ++i)
		{
			if (!m_boxSideObstruction[i].bObstructed)
			{
				if (bAllObstructedExceptBackside)
				{
					// At least 2 axis must be within boundaries, which means we're facing directly a side and snapping is necessary
					bool const bWithinX = posLocalSpace.x > boxMin.x && posLocalSpace.x < boxMax.x;
					bool const bWithinY = posLocalSpace.y > boxMin.y && posLocalSpace.y < boxMax.y;
					bool const bWithinZ = posLocalSpace.z > boxMin.z && posLocalSpace.z < boxMax.z;

					if (bWithinX && (bWithinY || bWithinZ) ||
					    bWithinY && (bWithinX || bWithinZ) ||
					    bWithinZ && (bWithinX || bWithinY))
					{
						// Get the distance to the edges and choose the shortest one
						float const distanceToEdges[4] =
						{
							Vec3(posLocalSpace - boxBackSideEdges[0]).GetLengthSquared(),
							Vec3(posLocalSpace - boxBackSideEdges[1]).GetLengthSquared(),
							Vec3(posLocalSpace - boxBackSideEdges[2]).GetLengthSquared(),
							Vec3(posLocalSpace - boxBackSideEdges[3]).GetLengthSquared()
						};

						float const closestDistanceToEdges = std::min<float>(std::min<float>(distanceToEdges[0], distanceToEdges[1]), std::min<float>(distanceToEdges[2], distanceToEdges[3]));

						for (unsigned int j = 0; j < 4; ++j)
						{
							if (distanceToEdges[j] == closestDistanceToEdges)
							{
								// Snap to it
								closest = boxBackSideEdges[j];
								break;
							}
						}

						break;
					}
				}

				closestPoint.x = std::min<float>(std::max<float>(posLocalSpace.x, boxSides[i].minValues.x), boxSides[i].maxValues.x);
				closestPoint.y = std::min<float>(std::max<float>(posLocalSpace.y, boxSides[i].minValues.y), boxSides[i].maxValues.y);
				closestPoint.z = std::min<float>(std::max<float>(posLocalSpace.z, boxSides[i].minValues.z), boxSides[i].maxValues.z);

				float const tempClosestDistance = Vec3(posLocalSpace - closestPoint).GetLengthSquared();

				if (tempClosestDistance < closestDistance)
				{
					closestDistance = tempClosestDistance;
					closest = closestPoint;
				}
			}
		}

		// Transform the result back to world values
		outClosest = m_worldTM.TransformPoint(closest);
		outClosestDistSq = Vec3(source - outClosest).GetLengthSquared();
	}
	else
	{
		outClosest = sourcePos;
	}
}

//////////////////////////////////////////////////////////////////////////
void CArea::CalcClosestPointToSolid(Vec3 const& rv3SourcePos, bool bIgnoreSoundObstruction, float& rfClosestDistSq, Vec3* rv3ClosestPos) const
{
	CRY_PROFILE_FUNCTION(PROFILE_ENTITY);
	Vec3 localPoint3D = m_invMatrix.TransformPoint(rv3SourcePos);
	int queryFlag = bIgnoreSoundObstruction ? CAreaSolid::eSegmentQueryFlag_All : CAreaSolid::eSegmentQueryFlag_Open;
	if (m_pAreaSolid->IsInside(localPoint3D))
		queryFlag |= CAreaSolid::eSegmentQueryFlag_UsingReverseSegment;
	Vec3 closestPos(0, 0, 0);
	m_pAreaSolid->QueryNearest(localPoint3D, queryFlag, closestPos, rfClosestDistSq);
	if (rv3ClosestPos)
		*rv3ClosestPos = m_worldTM.TransformPoint(closestPos);
}

//////////////////////////////////////////////////////////////////////////
void CArea::InvalidateCachedAreaData(EntityId const nEntityID)
{
	CRY_PROFILE_FUNCTION(PROFILE_ENTITY);
	TEntityCachedAreaDataMap::iterator Iter(m_mapEntityCachedAreaData.find(nEntityID));

	if (Iter != m_mapEntityCachedAreaData.end())
	{
		SCachedAreaData& rCachedData = Iter->second;
		rCachedData.eFlags = eCachedAreaData_None;
		rCachedData.ePosType = AREA_POS_TYPE_COUNT;
		rCachedData.fDistanceWithinSq = FLT_MAX;
		rCachedData.fDistanceNearSq = FLT_MAX;
		rCachedData.bPointWithin = false;
		rCachedData.oPos.zero();
	}
}

//////////////////////////////////////////////////////////////////////////
void CArea::SetPoints(Vec3 const* const pPoints, bool const* const pSoundObstructionSegments, size_t const numLocalPoints, bool const bClosed)
{
	CRY_PROFILE_FUNCTION(PROFILE_ENTITY);
	m_areaType = ENTITY_AREA_TYPE_SHAPE;

	// at least two points needed to create closed shape
	if (numLocalPoints > 1)
	{
		if (pPoints != nullptr)
		{
			// Set potentially both, points and sound obstruction.
			ReleaseAreaData();
			m_bClosed = bClosed;
			m_areaPoints.resize(numLocalPoints);
			m_areaSegments.resize(numLocalPoints - !bClosed);
			for (size_t i = 0; i < m_areaSegments.size(); ++i)
			{
				m_areaSegments[i] = new a2DSegment;
				m_areaSegments[i]->bObstructSound = pSoundObstructionSegments && pSoundObstructionSegments[i];
			}

			MovePoints(pPoints, numLocalPoints);

			if (pSoundObstructionSegments != nullptr)
			{
				// Set "Roof" and "Floor" sound obstruction.
				SetSoundObstructionOnAreaFace(numLocalPoints, pSoundObstructionSegments[numLocalPoints]);
				SetSoundObstructionOnAreaFace(numLocalPoints + 1, pSoundObstructionSegments[numLocalPoints + 1]);
			}
		}
		else if (pSoundObstructionSegments != nullptr)
		{
			// Set sound obstruction only.
			size_t const numAudioPoints = numLocalPoints + 2; // Adding "Roof" and "Floor"

			for (size_t i = 0; i < numAudioPoints; ++i)
			{
				// We assume that segments exist.
				SetSoundObstructionOnAreaFace(i, pSoundObstructionSegments[i]);
			}
		}

		m_state |= Cry::AreaManager::EAreaState::Initialized;
		m_pAreaManager->SetAreaDirty(this);
	}
}

//////////////////////////////////////////////////////////////////////////
void CArea::MovePoints(Vec3 const* const pPoints, size_t const numLocalPoints)
{
	if (!m_areaSegments.empty() && numLocalPoints > 0)
	{
		assert(numLocalPoints == m_areaPoints.size());

		m_area = 0.0f;
		m_origin = pPoints[0].z;
		for (size_t i = 0; i < numLocalPoints; ++i)
		{
			m_areaPoints[i] = pPoints[i];
			m_origin = min(m_origin, pPoints[i].z);
			if (m_bClosed)
			{
				size_t j = NextPoint(i);
				m_area += (pPoints[i].x * pPoints[j].y - pPoints[i].y * pPoints[j].x);
			}
		}
		m_area *= 0.5f;

		// Reverse points if closed and CW
		if (m_area < 0.0f)
		{
			for (size_t i = 0; i < numLocalPoints / 2; ++i)
				std::swap(m_areaPoints[i], m_areaPoints[numLocalPoints - 1 - i]);
			m_area = -m_area;
		}

		// Update segments
		for (size_t i = 0; i < m_areaSegments.size(); ++i)
		{
			size_t j = NextPoint(i);
			UpdateSegment(*m_areaSegments[i], m_areaPoints[i], m_areaPoints[j]);
		}

		CalcBBox();
		m_pAreaManager->SetAreaDirty(this);
	}
}

//////////////////////////////////////////////////////////////////////////
void CArea::SetBox(const Vec3& min, const Vec3& max, const Matrix34& tm)
{
	ReleaseAreaData();

	m_areaType = ENTITY_AREA_TYPE_BOX;
	m_state |= Cry::AreaManager::EAreaState::Initialized;
	m_boxMin = min;
	m_boxMax = max;
	m_worldTM = tm;
	m_invMatrix = tm.GetInverted();

	m_pAreaManager->SetAreaDirty(this);
}

//////////////////////////////////////////////////////////////////////////
void CArea::BeginSettingSolid(const Matrix34& worldTM)
{
	ReleaseAreaData();

	m_areaType = ENTITY_AREA_TYPE_SOLID;
	m_state |= Cry::AreaManager::EAreaState::Initialized;
	m_worldTM = worldTM;
	m_invMatrix = m_worldTM.GetInverted();
	m_pAreaSolid = new CAreaSolid;
}

//////////////////////////////////////////////////////////////////////////
void CArea::AddConvexHullToSolid(const Vec3* verticesOfConvexHull, bool bObstruction, int numberOfVertices)
{
	if (!m_pAreaSolid || m_areaType != ENTITY_AREA_TYPE_SOLID)
		return;

	m_pAreaSolid->AddSegment(verticesOfConvexHull, bObstruction, numberOfVertices);
}

//////////////////////////////////////////////////////////////////////////
void CArea::EndSettingSolid()
{
	if (!m_pAreaSolid || m_areaType != ENTITY_AREA_TYPE_SOLID)
		return;

	m_pAreaSolid->BuildBSP();
	m_pAreaManager->SetAreaDirty(this);
}

//////////////////////////////////////////////////////////////////////////
void CArea::SetMatrix(const Matrix34& tm)
{
	m_invMatrix = tm.GetInverted();
	m_worldTM = tm;

	m_pAreaManager->SetAreaDirty(this);
}

//////////////////////////////////////////////////////////////////////////
void CArea::GetMatrix(Matrix34& tm) const
{
	tm = m_worldTM;
}

//////////////////////////////////////////////////////////////////////////
void CArea::GetWorldBox(Vec3& rMin, Vec3& rMax) const
{
	switch (m_areaType)
	{
	case ENTITY_AREA_TYPE_SHAPE:
		{
			rMin.x = m_areaBBox.min.x;
			rMin.y = m_areaBBox.min.y;
			rMin.z = m_origin;
			rMax.x = m_areaBBox.max.x;
			rMax.y = m_areaBBox.max.y;
			rMax.z = m_origin + m_height;

			break;
		}
	case ENTITY_AREA_TYPE_BOX:
		{
			rMin = m_worldTM.TransformPoint(m_boxMin);
			rMax = m_worldTM.TransformPoint(m_boxMax);

			break;
		}
	case ENTITY_AREA_TYPE_SPHERE:
		{
			rMin = m_sphereCenter - Vec3(m_sphereRadius, m_sphereRadius, m_sphereRadius);
			rMax = m_sphereCenter + Vec3(m_sphereRadius, m_sphereRadius, m_sphereRadius);

			break;
		}
	case ENTITY_AREA_TYPE_GRAVITYVOLUME:
		{
			rMin.zero();
			rMax.zero();

			break;
		}
	case ENTITY_AREA_TYPE_SOLID:
		{
			AABB oAABB;
			GetSolidBoundBox(oAABB);

			rMin = oAABB.min;
			rMax = oAABB.max;

			break;
		}
	default:
		{
			rMin.zero();
			rMax.zero();

			break;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CArea::SetSphere(const Vec3& center, float fRadius)
{
	ReleaseAreaData();

	m_state |= Cry::AreaManager::EAreaState::Initialized;
	m_areaType = ENTITY_AREA_TYPE_SPHERE;
	m_sphereCenter = center;
	m_sphereRadius = fRadius;
	m_sphereRadius2 = m_sphereRadius * m_sphereRadius;

	m_pAreaManager->SetAreaDirty(this);
}

//////////////////////////////////////////////////////////////////////////
void CArea::CalcBBox()
{
	CRY_PROFILE_FUNCTION(PROFILE_ENTITY);
	a2DBBox& areaBBox = m_areaBBox;
	areaBBox.min.x = m_areaSegments[0]->bbox.min.x;
	areaBBox.min.y = m_areaSegments[0]->bbox.min.y;
	areaBBox.max.x = m_areaSegments[0]->bbox.max.x;
	areaBBox.max.y = m_areaSegments[0]->bbox.max.y;

	for (size_t sIdx = 1; sIdx < m_areaSegments.size(); ++sIdx)
	{
		if (areaBBox.min.x > m_areaSegments[sIdx]->bbox.min.x)
			areaBBox.min.x = m_areaSegments[sIdx]->bbox.min.x;
		if (areaBBox.min.y > m_areaSegments[sIdx]->bbox.min.y)
			areaBBox.min.y = m_areaSegments[sIdx]->bbox.min.y;
		if (areaBBox.max.x < m_areaSegments[sIdx]->bbox.max.x)
			areaBBox.max.x = m_areaSegments[sIdx]->bbox.max.x;
		if (areaBBox.max.y < m_areaSegments[sIdx]->bbox.max.y)
			areaBBox.max.y = m_areaSegments[sIdx]->bbox.max.y;
	}
	GetBBox() = areaBBox;
}

//////////////////////////////////////////////////////////////////////////
void CArea::ResolveEntityIds()
{
	if ((m_state & Cry::AreaManager::EAreaState::EntityIdsResolved) == 0)
	{
		size_t index = 0;

		for (auto const& guid : m_entityGuids)
		{
			m_entityIds[index++] = g_pIEntitySystem->FindEntityByGuid(guid);
		}

		m_state |= Cry::AreaManager::EAreaState::EntityIdsResolved;
	}
}

//////////////////////////////////////////////////////////////////////////
void CArea::ReleaseCachedAreaData()
{
	stl::free_container(m_mapEntityCachedAreaData);
}

//////////////////////////////////////////////////////////////////////////
float CArea::GetFadeDistance()
{
	CRY_PROFILE_FUNCTION(PROFILE_ENTITY);

	if (m_fadeDistance < 0.0f || gEnv->IsEditor())
	{
		m_fadeDistance = 0.0f;

		for (auto const entityId : m_entityIds)
		{
			CEntity const* const pIEntity = g_pIEntitySystem->GetEntityFromID(entityId);

			if (pIEntity != nullptr)
			{
				auto pIEntityAudioComponent = pIEntity->GetComponent<IEntityAudioComponent>();

				if (pIEntityAudioComponent != nullptr)
				{
					m_fadeDistance = std::max<float>(m_fadeDistance, pIEntityAudioComponent->GetFadeDistance());
				}
			}
		}
	}

	return m_fadeDistance;
}

//////////////////////////////////////////////////////////////////////////
float CArea::GetEnvironmentFadeDistance()
{
	if (m_environmentFadeDistance < 0.0f || gEnv->IsEditor())
	{
		m_environmentFadeDistance = 0.0f;

		for (auto const entityId : m_entityIds)
		{
			CEntity const* const pIEntity = g_pIEntitySystem->GetEntityFromID(entityId);

			if (pIEntity != nullptr)
			{
				auto pIEntityAudioComponent = pIEntity->GetComponent<IEntityAudioComponent>();

				if (pIEntityAudioComponent != nullptr)
				{
					m_environmentFadeDistance = std::max<float>(m_environmentFadeDistance, pIEntityAudioComponent->GetEnvironmentFadeDistance());
				}
			}
		}
	}

	return m_environmentFadeDistance;
}

//////////////////////////////////////////////////////////////////////////
float CArea::GetGreatestFadeDistance()
{
	CRY_PROFILE_FUNCTION(PROFILE_ENTITY);

	if (m_greatestFadeDistance < 0.0f || gEnv->IsEditor())
	{
		m_greatestFadeDistance = 0.0f;

		for (auto const entityId : m_entityIds)
		{
			CEntity const* const pIEntity = g_pIEntitySystem->GetEntityFromID(entityId);

			if (pIEntity != nullptr)
			{
				auto pIEntityAudioComponent = pIEntity->GetComponent<IEntityAudioComponent>();

				if (pIEntityAudioComponent != nullptr)
				{
					m_greatestFadeDistance = std::max<float>(m_greatestFadeDistance, pIEntityAudioComponent->GetGreatestFadeDistance());
				}
			}
		}
	}

	return m_greatestFadeDistance;
}

//////////////////////////////////////////////////////////////////////////
void CArea::GetBBox(Vec2& vMin, Vec2& vMax) const
{
	// Only valid for shape areas.
	const a2DBBox& areaBox = GetBBox();
	vMin = Vec2(areaBox.min.x, areaBox.min.y);
	vMax = Vec2(areaBox.max.x, areaBox.max.y);
}

//////////////////////////////////////////////////////////////////////////
const CArea::a2DBBox& CArea::GetBBox() const
{
	return s_areaBoxes[m_bbox_holder].box;
}

//////////////////////////////////////////////////////////////////////////
CArea::a2DBBox& CArea::GetBBox()
{
	return s_areaBoxes[m_bbox_holder].box;
}

//////////////////////////////////////////////////////////////////////////
void CArea::GetSolidBoundBox(AABB& outBoundBox) const
{
	if (!m_pAreaSolid)
		return;

	outBoundBox = m_pAreaSolid->GetBoundBox();
}

//////////////////////////////////////////////////////////////////////////
void CArea::AddEntity(const EntityId entId)
{
	if (entId != INVALID_ENTITYID)
	{
		m_fadeDistance = -1.0f;
		m_greatestFadeDistance = -1.0f;

		if (CVar::pDrawAreaDebug->GetIVal() == 2)
		{
			CryLog("<AreaManager> Area %u Direct Event: %s", m_entityId, "ATTACH_THIS");
		}

		// Always add as the entity might not exist yet.
		stl::push_back_unique(m_entityIds, entId);

		CEntity* const pIEntity = g_pIEntitySystem->GetEntityFromID(entId);

		if (pIEntity != nullptr)
		{
			SEntityEvent event;
			event.event = ENTITY_EVENT_ATTACH_THIS;
			event.nParam[0] = m_entityId;
			event.nParam[1] = m_areaId;
			event.nParam[2] = 0;
			pIEntity->SendEvent(event);

			if ((pIEntity->GetFlags() & ENTITY_FLAG_VOLUME_SOUND) > 0)
			{
				m_pAreaManager->TriggerAudioListenerUpdate(this);
				GetGreatestFadeDistance();
			}
		}

		m_pAreaManager->SetAreaDirty(this);
	}
}

//////////////////////////////////////////////////////////////////////////
void CArea::AddEntity(const EntityGUID entGuid)
{
	stl::push_back_unique(m_entityGuids, entGuid);
	AddEntity(g_pIEntitySystem->FindEntityByGuid(entGuid));
}

//////////////////////////////////////////////////////////////////////////
void CArea::AddEntities(const EntityIdVector& entIDs)
{
	EntityIdVector::const_iterator Iter(entIDs.begin());
	EntityIdVector::const_iterator const IterEnd(entIDs.end());

	for (; Iter != IterEnd; ++Iter)
	{
		AddEntity(*Iter);
	}
}

//////////////////////////////////////////////////////////////////////////
void CArea::RemoveEntity(EntityId const entId)
{
	if (entId != INVALID_ENTITYID)
	{
		m_fadeDistance = -1.0f;
		m_greatestFadeDistance = -1.0f;

		if (CVar::pDrawAreaDebug->GetIVal() == 2)
		{
			CryLog("<AreaManager> Area %u Direct Event: %s", m_entityId, "DETACH_THIS");
		}

		// Always remove as the entity might be already gone.
		stl::find_and_erase(m_entityIds, entId);

		CEntity* const pIEntity = g_pIEntitySystem->GetEntityFromID(entId);

		if (pIEntity != nullptr)
		{
			SEntityEvent event;
			event.event = ENTITY_EVENT_DETACH_THIS;
			event.nParam[0] = m_entityId;
			event.nParam[1] = m_areaId;
			event.nParam[2] = 0;
			pIEntity->SendEvent(event);

			if ((pIEntity->GetFlags() & ENTITY_FLAG_VOLUME_SOUND) > 0)
			{
				m_pAreaManager->TriggerAudioListenerUpdate(this);
				GetGreatestFadeDistance();
			}
		}

		m_pAreaManager->SetAreaDirty(this);
	}
}

//////////////////////////////////////////////////////////////////////////
void CArea::RemoveEntity(EntityGUID const entGuid)
{
	stl::find_and_erase(m_entityGuids, entGuid);
	RemoveEntity(g_pIEntitySystem->FindEntityByGuid(entGuid));
}

//////////////////////////////////////////////////////////////////////////
void CArea::RemoveEntities()
{
	LOADING_TIME_PROFILE_SECTION
	// Inform all attached entities that they have been disconnected to prevent lost entities.
	EntityIdVector const tmpVec(std::move(m_entityIds));
	EntityIdVector::const_iterator Iter(tmpVec.begin());
	EntityIdVector::const_iterator const IterEnd(tmpVec.end());

	for (; Iter != IterEnd; ++Iter)
	{
		RemoveEntity(*Iter);
	}

	m_entityGuids.clear();
}

//////////////////////////////////////////////////////////////////////////
void CArea::AddCachedEvent(const SEntityEvent& event)
{
	m_cachedEvents.push_back(event);
}

//////////////////////////////////////////////////////////////////////////
void CArea::ClearCachedEventsFor(EntityId const nEntityID)
{
	for (CachedEvents::iterator it = m_cachedEvents.begin(); it != m_cachedEvents.end(); )
	{
		SEntityEvent& event = *it;

		if (event.nParam[0] == nEntityID)
		{
			it = m_cachedEvents.erase(it);
			continue;
		}

		++it;
	}
}

//////////////////////////////////////////////////////////////////////////
void CArea::ClearCachedEvents()
{
	m_cachedEvents.clear();
}

//////////////////////////////////////////////////////////////////////////
void CArea::SendCachedEventsFor(EntityId const nEntityID)
{
	CRY_PROFILE_FUNCTION(PROFILE_ENTITY);
	size_t const nCountCachedEvents = m_cachedEvents.size();

	if (((m_state & Cry::AreaManager::EAreaState::Initialized) != 0) && nCountCachedEvents > 0)
	{
		for (size_t i = 0; i < nCountCachedEvents; ++i)
		{
			SEntityEvent cachedEvent = m_cachedEvents[i]; // copy to avoid invalidation if vector re-allocates

			if (cachedEvent.nParam[0] == nEntityID)
			{
				if (CVar::pDrawAreaDebug->GetIVal() == 2)
				{
					string sState;
					if (cachedEvent.event == ENTITY_EVENT_ENTERNEARAREA)
						sState = "ENTERNEAR";
					if (cachedEvent.event == ENTITY_EVENT_MOVENEARAREA)
						sState = "MOVENEAR";
					if (cachedEvent.event == ENTITY_EVENT_ENTERAREA)
						sState = "ENTER";
					if (cachedEvent.event == ENTITY_EVENT_MOVEINSIDEAREA)
						sState = "MOVEINSIDE";
					if (cachedEvent.event == ENTITY_EVENT_LEAVEAREA)
						sState = "LEAVE";
					if (cachedEvent.event == ENTITY_EVENT_LEAVENEARAREA)
						sState = "LEAVENEAR";

					CryLog("<AreaManager> Area %u Queued Event: %s", m_entityId, sState.c_str());
				}

				cachedEvent.nParam[1] = m_areaId;
				cachedEvent.nParam[2] = m_entityId;

				SendEvent(cachedEvent, false);
			}
		}

		ClearCachedEventsFor(nEntityID);
	}
}

//////////////////////////////////////////////////////////////////////////
void CArea::SendEvent(SEntityEvent& newEvent, bool bClearCachedEvents /* = true */)
{
	m_pAreaManager->OnEvent(newEvent.event, (EntityId)newEvent.nParam[0], this);

	size_t const nCountEntities = m_entityIds.size();

	for (size_t eIdx = 0; eIdx < nCountEntities; ++eIdx)
	{
		if (CEntity* pAreaAttachedEntity = g_pIEntitySystem->GetEntityFromID(m_entityIds[eIdx]))
		{
			pAreaAttachedEntity->SendEvent(newEvent);

			if (bClearCachedEvents)
				ClearCachedEventsFor((EntityId)newEvent.nParam[0]);
		}
	}
}

// do enter area - player was outside, now is inside
// calls entity OnEnterArea which calls script OnEnterArea( player, AreaID )
//////////////////////////////////////////////////////////////////////////
void CArea::EnterArea(EntityId const entityId)
{
	CRY_PROFILE_FUNCTION(PROFILE_ENTITY);

	if ((m_state & Cry::AreaManager::EAreaState::Initialized) != 0)
	{
		TEntityCachedAreaDataMap::iterator Iter(m_mapEntityCachedAreaData.find(entityId));

		if (Iter == m_mapEntityCachedAreaData.end())
		{
			// If we get here it means "OnAddedToAreaCache" did not get called, the entity was probably spawned within this area.
			m_mapEntityCachedAreaData.insert(std::make_pair(entityId, SCachedAreaData())).first;
		}

		if (CVar::pDrawAreaDebug->GetIVal() == 2)
		{
			CryLog("<AreaManager> Area %u Direct Event: %s", m_entityId, "ENTER");
		}

		SEntityEvent event;
		event.event = ENTITY_EVENT_ENTERAREA;
		event.nParam[0] = entityId;
		event.nParam[1] = m_areaId;
		event.nParam[2] = m_entityId;

		SendEvent(event);
	}
}

// do leave area - player was inside, now is outside
// calls entity OnLeaveArea which calls script OnLeaveArea( player, AreaID )
//////////////////////////////////////////////////////////////////////////
void CArea::LeaveArea(EntityId const entityId)
{
	CRY_PROFILE_FUNCTION(PROFILE_ENTITY);

	if ((m_state & Cry::AreaManager::EAreaState::Initialized) != 0)
	{
		if (CVar::pDrawAreaDebug->GetIVal() == 2)
		{
			CryLog("<AreaManager> Area %u Direct Event: %s", m_entityId, "LEAVE");
		}

		SEntityEvent event;
		event.event = ENTITY_EVENT_LEAVEAREA;
		event.nParam[0] = entityId;
		event.nParam[1] = m_areaId;
		event.nParam[2] = m_entityId;

		SendEvent(event);
	}
}

// do enter near area - entity was "far", now is "near"
// calls entity OnEnterNearArea which calls script OnEnterNearArea( entity(player), AreaID )
//////////////////////////////////////////////////////////////////////////
void CArea::EnterNearArea(
  EntityId const entityId,
  Vec3 const& closestPointToArea,
  float const distance)
{
	CRY_PROFILE_FUNCTION(PROFILE_ENTITY);

	if ((m_state & Cry::AreaManager::EAreaState::Initialized) != 0)
	{
		TEntityCachedAreaDataMap::iterator Iter(m_mapEntityCachedAreaData.find(entityId));

		if (Iter == m_mapEntityCachedAreaData.end())
		{
			// If we get here it means "OnAddedToAreaCache" did not get called, the entity was probably spawned within the near region of this area.
			m_mapEntityCachedAreaData.insert(std::make_pair(entityId, SCachedAreaData())).first;
		}

		if (CVar::pDrawAreaDebug->GetIVal() == 2)
		{
			CryLog("<AreaManager> Area %u Direct Event: %s", m_entityId, "ENTERNEAR");
		}

		SEntityEvent event(entityId, m_areaId, m_entityId, 0, distance, 0.0f, 0.0f, closestPointToArea);
		event.event = ENTITY_EVENT_ENTERNEARAREA;
		SendEvent(event);
	}
}

// do leave near area - entity was "near", now is "far"
// calls entity OnLeaveNearArea which calls script OnLeaveNearArea( entity(player), AreaID )
//////////////////////////////////////////////////////////////////////////
void CArea::LeaveNearArea(EntityId const entityId)
{
	CRY_PROFILE_FUNCTION(PROFILE_ENTITY);

	if ((m_state & Cry::AreaManager::EAreaState::Initialized) != 0)
	{
		if (CVar::pDrawAreaDebug->GetIVal() == 2)
		{
			CryLog("<AreaManager> Area %u Direct Event: %s", m_entityId, "LEAVENEAR");
		}

		SEntityEvent event;
		event.event = ENTITY_EVENT_LEAVENEARAREA;
		event.nParam[0] = entityId;
		event.nParam[1] = m_areaId;
		event.nParam[2] = m_entityId;
		SendEvent(event);
	}
}

//////////////////////////////////////////////////////////////////////////
void CArea::OnAddedToAreaCache(EntityId const entityId)
{
	CRY_PROFILE_FUNCTION(PROFILE_ENTITY);

	if ((m_state & Cry::AreaManager::EAreaState::Initialized) != 0)
	{
		TEntityCachedAreaDataMap::iterator Iter(m_mapEntityCachedAreaData.find(entityId));

		if (Iter == m_mapEntityCachedAreaData.end())
		{
			m_mapEntityCachedAreaData.insert(std::make_pair(entityId, SCachedAreaData())).first;
		}
		else
		{
			// Cannot yet exist, if so figure out why it wasn't removed properly or why this is called more than once!
			CRY_ASSERT(false);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CArea::OnRemovedFromAreaCache(EntityId const entityId)
{
	CRY_PROFILE_FUNCTION(PROFILE_ENTITY);
	m_mapEntityCachedAreaData.erase(entityId);
}

// pEntity moves in an area, which is controlled by an area with a higher priority
//////////////////////////////////////////////////////////////////////////
void CArea::ExclusiveUpdateAreaInside(
  EntityId const entityId,
  EntityId const higherAreaId,
  float const fade)
{
	CRY_PROFILE_FUNCTION(PROFILE_ENTITY);

	if ((m_state & Cry::AreaManager::EAreaState::Initialized) != 0)
	{
		if (CVar::pDrawAreaDebug->GetIVal() == 2)
		{
			CryLog("<AreaManager> Area %u Direct Event: %s", m_entityId, "MOVEINSIDE");
		}

		size_t const numAttachedEntities = m_entityIds.size();

		for (size_t index = 0; index < numAttachedEntities; ++index)
		{
			CEntity* const pEntity = g_pIEntitySystem->GetEntityFromID(m_entityIds[index]);

			if (pEntity != nullptr)
			{
				SEntityEvent event;
				event.event = ENTITY_EVENT_MOVEINSIDEAREA;
				event.nParam[0] = entityId;
				event.nParam[1] = m_areaId;
				event.nParam[2] = m_entityId; // AreaLowEntityID
				event.nParam[3] = higherAreaId;
				event.fParam[0] = fade;
				pEntity->SendEvent(event);
			}
		}
	}
}

// pEntity moves near an area, which is controlled by an area with a higher priority
//////////////////////////////////////////////////////////////////////////
void CArea::ExclusiveUpdateAreaNear(
  EntityId const entityId,
  EntityId const higherAreaId,
  float const distance,
  Vec3 const& position)
{
	CRY_PROFILE_FUNCTION(PROFILE_ENTITY);

	if ((m_state & Cry::AreaManager::EAreaState::Initialized) != 0)
	{
		if (CVar::pDrawAreaDebug->GetIVal() == 2)
		{
			CryLogAlways("<AreaManager> Area %u Direct Event: %s", m_entityId, "MOVENEAR");
		}

		size_t const numAttachedEntities = m_entityIds.size();

		for (size_t index = 0; index < numAttachedEntities; ++index)
		{
			CEntity* const pEntity = g_pIEntitySystem->GetEntityFromID(m_entityIds[index]);

			if (pEntity != nullptr)
			{
				SEntityEvent event;
				event.event = ENTITY_EVENT_MOVENEARAREA;
				event.nParam[0] = entityId;
				event.nParam[1] = m_areaId;
				event.nParam[2] = m_entityId; // AreaLowEntityID
				event.nParam[3] = higherAreaId;
				event.fParam[0] = distance;
				event.vec = position;
				pEntity->SendEvent(event);
			}
		}
	}
}

//calculate distance to area. player is inside of the area
//////////////////////////////////////////////////////////////////////////
float CArea::CalculateFade(const Vec3& pos3D)
{
	CRY_PROFILE_FUNCTION(PROFILE_ENTITY);
	float fadeCoeff = 0.0f;

	if ((m_state & Cry::AreaManager::EAreaState::Initialized) != 0)
	{
		a2DPoint const pos = CArea::a2DPoint(pos3D);

		switch (m_areaType)
		{
		case ENTITY_AREA_TYPE_SOLID:
			{
				if (m_proximity <= 0.0f)
				{
					fadeCoeff = 1.0f;
					break;
				}
				Vec3 PosOnHull(ZERO);
				float squaredDistance;
				if (!m_pAreaSolid->QueryNearest(pos3D, CAreaSolid::eSegmentQueryFlag_All, PosOnHull, squaredDistance))
					break;
				fadeCoeff = sqrt_tpl(squaredDistance) / m_proximity;
			}
			break;
		case ENTITY_AREA_TYPE_SHAPE:
			fadeCoeff = CalcDistToPoint(pos);
			break;
		case ENTITY_AREA_TYPE_SPHERE:
			{
				if (m_proximity <= 0.0f)
				{
					fadeCoeff = 1.0f;
					break;
				}
				Vec3 Delta = pos3D - m_sphereCenter;
				fadeCoeff = (m_sphereRadius - Delta.GetLength()) / m_proximity;
				if (fadeCoeff > 1.0f)
					fadeCoeff = 1.0f;
				break;
			}
		case ENTITY_AREA_TYPE_BOX:
			{
				if (m_proximity <= 0.0f)
				{
					fadeCoeff = 1.0f;
					break;
				}
				Vec3 p3D = m_invMatrix.TransformPoint(pos3D);
				Vec3 MinDelta = p3D - m_boxMin;
				Vec3 MaxDelta = m_boxMax - p3D;
				Vec3 EdgeDist = (m_boxMax - m_boxMin) / 2.0f;
				if ((!EdgeDist.x) || (!EdgeDist.y) || (!EdgeDist.z))
				{
					fadeCoeff = 1.0f;
					break;
				}

				float fFadeScale = m_proximity / 100.0f;
				EdgeDist *= fFadeScale;

				float fMinFade = MinDelta.x / EdgeDist.x;

				for (int k = 0; k < 3; k++)
				{
					float fFade1 = MinDelta[k] / EdgeDist[k];
					float fFade2 = MaxDelta[k] / EdgeDist[k];
					fMinFade = min(fMinFade, min(fFade1, fFade2));
				} //k

				fadeCoeff = fMinFade;
				if (fadeCoeff > 1.0f)
					fadeCoeff = 1.0f;
				break;
			}
		}
	}

	return fadeCoeff;
}

//////////////////////////////////////////////////////////////////////////
void CArea::Draw(size_t const idx)
{
	CRY_PROFILE_FUNCTION(PROFILE_ENTITY);
	I3DEngine* p3DEngine = gEnv->p3DEngine;
	IRenderAuxGeom* pRC = gEnv->pRenderer->GetIRenderAuxGeom();
	pRC->SetRenderFlags(e_Def3DPublicRenderflags);

	ColorB colorsArray[] = {
		ColorB(255, 0,   0,   255),
		ColorB(0,   255, 0,   255),
		ColorB(0,   0,   255, 255),
		ColorB(255, 255, 0,   255),
		ColorB(255, 0,   255, 255),
		ColorB(0,   255, 255, 255),
		ColorB(255, 255, 255, 255),
	};
	ColorB color = colorsArray[idx % (sizeof(colorsArray) / sizeof(ColorB))];
	ColorB color1 = colorsArray[(idx + 1) % (sizeof(colorsArray) / sizeof(ColorB))];
	// ColorB color2 = colorsArray[(idx+2)%(sizeof(colorsArray)/sizeof(ColorB))];

	switch (m_areaType)
	{
	case ENTITY_AREA_TYPE_SOLID:
		m_pAreaSolid->Draw(m_worldTM, color, color1);
		break;
	case ENTITY_AREA_TYPE_SHAPE:
		{
			const float deltaZ = 0.1f;
			size_t const nSize = m_areaPoints.size();

			for (size_t i = 0; i < nSize; ++i)
			{
				Vec3 v0 = m_areaPoints[i];
				Vec3 v1 = m_areaPoints[NextPoint(i)];
				v0.z = max(v0.z, p3DEngine->GetTerrainElevation(v0.x, v0.y) + deltaZ);
				v1.z = max(v1.z, p3DEngine->GetTerrainElevation(v1.x, v1.y) + deltaZ);

				// draw lower line segments
				if (i < nSize - !m_bClosed)
					pRC->DrawLine(v0, color, v1, color);

				// Draw upper line segments and vertical edges
				if (m_height > 0.0f)
				{
					Vec3 v0Z = Vec3(v0.x, v0.y, v0.z + m_height);
					Vec3 v1Z = Vec3(v1.x, v1.y, v1.z + m_height);

					if (i < nSize - !m_bClosed)
						pRC->DrawLine(v0Z, color, v1Z, color);
					pRC->DrawLine(v0, color, v0Z, color);
				}
			}
			break;
		}
	case ENTITY_AREA_TYPE_SPHERE:
		{
			ColorB color3 = color;
			color3.a = 64;

			pRC->SetRenderFlags(e_Def3DPublicRenderflags | e_AlphaBlended);
			pRC->DrawSphere(m_sphereCenter, m_sphereRadius, color3);
			break;
		}
	case ENTITY_AREA_TYPE_BOX:
		{
			float fLength = m_boxMax.x - m_boxMin.x;
			float fWidth = m_boxMax.y - m_boxMin.y;
			float fHeight = m_boxMax.z - m_boxMin.z;

			Vec3 v0 = m_boxMin;
			Vec3 v1 = Vec3(m_boxMin.x + fLength, m_boxMin.y, m_boxMin.z);
			Vec3 v2 = Vec3(m_boxMin.x + fLength, m_boxMin.y + fWidth, m_boxMin.z);
			Vec3 v3 = Vec3(m_boxMin.x, m_boxMin.y + fWidth, m_boxMin.z);
			Vec3 v4 = Vec3(m_boxMin.x, m_boxMin.y, m_boxMin.z + fHeight);
			Vec3 v5 = Vec3(m_boxMin.x + fLength, m_boxMin.y, m_boxMin.z + fHeight);
			Vec3 v6 = Vec3(m_boxMin.x + fLength, m_boxMin.y + fWidth, m_boxMin.z + fHeight);
			Vec3 v7 = Vec3(m_boxMin.x, m_boxMin.y + fWidth, m_boxMin.z + fHeight);

			v0 = m_worldTM.TransformPoint(v0);
			v1 = m_worldTM.TransformPoint(v1);
			v2 = m_worldTM.TransformPoint(v2);
			v3 = m_worldTM.TransformPoint(v3);
			v4 = m_worldTM.TransformPoint(v4);
			v5 = m_worldTM.TransformPoint(v5);
			v6 = m_worldTM.TransformPoint(v6);
			v7 = m_worldTM.TransformPoint(v7);

			// draw lower half of box
			pRC->DrawLine(v0, color1, v1, color1);
			pRC->DrawLine(v1, color1, v2, color1);
			pRC->DrawLine(v2, color1, v3, color1);
			pRC->DrawLine(v3, color1, v0, color1);

			// draw upper half of box
			pRC->DrawLine(v4, color1, v5, color1);
			pRC->DrawLine(v5, color1, v6, color1);
			pRC->DrawLine(v6, color1, v7, color1);
			pRC->DrawLine(v7, color1, v4, color1);

			// draw vertical edges
			pRC->DrawLine(v0, color1, v4, color1);
			pRC->DrawLine(v1, color1, v5, color1);
			pRC->DrawLine(v2, color1, v6, color1);
			pRC->DrawLine(v3, color1, v7, color1);

			break;
		}
	default:
		break;
	}
}

//////////////////////////////////////////////////////////////////////////
void CArea::ReleaseAreaData()
{
	CRY_PROFILE_FUNCTION(PROFILE_ENTITY);
	ClearPoints();

	stl::free_container(m_mapEntityCachedAreaData);
}

//////////////////////////////////////////////////////////////////////////
void CArea::GetMemoryUsage(ICrySizer* pSizer) const
{
	SIZER_COMPONENT_NAME(pSizer, "CArea");
	if (m_pAreaSolid)
	{
		m_pAreaSolid->GetMemoryUsage(pSizer);
	}

	for (auto const pAreaSegment : m_areaSegments)
	{
		pSizer->AddObject(pAreaSegment, sizeof(*pAreaSegment));
	}

	pSizer->AddObject(this, sizeof(*this));
}

//////////////////////////////////////////////////////////////////////////
float CArea::GetCachedPointWithinDistSq(EntityId const nEntityID) const
{
	CRY_PROFILE_FUNCTION(PROFILE_ENTITY);
	float fValue = 0.0f;
	TEntityCachedAreaDataMap::const_iterator const Iter(m_mapEntityCachedAreaData.find(nEntityID));

	if (Iter != m_mapEntityCachedAreaData.end())
	{
		fValue = Iter->second.fDistanceWithinSq;
	}

	return fValue;
}

//////////////////////////////////////////////////////////////////////////
bool CArea::GetCachedPointWithin(EntityId const nEntityID) const
{
	CRY_PROFILE_FUNCTION(PROFILE_ENTITY);
	bool bValue = false;
	TEntityCachedAreaDataMap::const_iterator const Iter(m_mapEntityCachedAreaData.find(nEntityID));

	if (Iter != m_mapEntityCachedAreaData.end())
	{
		bValue = Iter->second.bPointWithin;
	}

	return bValue;
}

//////////////////////////////////////////////////////////////////////////
EAreaPosType CArea::GetCachedPointPosTypeWithin(EntityId const nEntityID) const
{
	CRY_PROFILE_FUNCTION(PROFILE_ENTITY);
	EAreaPosType eValue = AREA_POS_TYPE_COUNT;
	TEntityCachedAreaDataMap::const_iterator const Iter(m_mapEntityCachedAreaData.find(nEntityID));

	if (Iter != m_mapEntityCachedAreaData.end())
	{
		eValue = Iter->second.ePosType;
	}

	return eValue;
}

//////////////////////////////////////////////////////////////////////////
float CArea::GetCachedPointNearDistSq(EntityId const nEntityID) const
{
	CRY_PROFILE_FUNCTION(PROFILE_ENTITY);
	float fValue = 0.0f;
	TEntityCachedAreaDataMap::const_iterator const Iter(m_mapEntityCachedAreaData.find(nEntityID));

	if (Iter != m_mapEntityCachedAreaData.end())
	{
		fValue = Iter->second.fDistanceNearSq;
	}

	return fValue;
}

//////////////////////////////////////////////////////////////////////////
Vec3 const& CArea::GetCachedPointOnHull(EntityId const nEntityID) const
{
	CRY_PROFILE_FUNCTION(PROFILE_ENTITY);
	TEntityCachedAreaDataMap::const_iterator const Iter(m_mapEntityCachedAreaData.find(nEntityID));

	if (Iter != m_mapEntityCachedAreaData.end())
	{
		return Iter->second.oPos;
	}

	return m_nullVec;
}

//////////////////////////////////////////////////////////////////////////
const CArea::TAreaBoxes& CArea::GetBoxHolders()
{
	return s_areaBoxes;
}


//////////////////////////////////////////////////////////////////////////
AABB CArea::GetAABB() const
{
	switch (m_areaType)
	{
	case ENTITY_AREA_TYPE_SPHERE:
		return AABB(m_sphereCenter, m_sphereRadius);
	case ENTITY_AREA_TYPE_BOX:
		return AABB::CreateTransformedAABB(m_worldTM, AABB(m_boxMin, m_boxMax));
	case ENTITY_AREA_TYPE_SHAPE:
		return AABB(
			Vec3(m_areaBBox.min.x, m_areaBBox.min.y, m_origin),
			Vec3(m_areaBBox.max.x, m_areaBBox.max.y, m_origin + m_height));
	case ENTITY_AREA_TYPE_SOLID:
		return AABB::CreateTransformedAABB(m_worldTM, m_pAreaSolid->GetBoundBox());
	default:
		return AABB(AABB::RESET);
	}
}

float CArea::GetExtent(EGeomForm eForm)
{
	switch (m_areaType)
	{
	case ENTITY_AREA_TYPE_SPHERE:
		return SphereExtent(eForm, m_sphereRadius);
	case ENTITY_AREA_TYPE_BOX:
		return BoxExtent(eForm, (m_boxMax - m_boxMin) * 0.5f) * ScaleExtent(eForm, m_worldTM);
	case ENTITY_AREA_TYPE_SHAPE:
	{
		if (eForm == GeomForm_Vertices)
		{
			return m_areaPoints.size() * (m_height > 0.0f ? 2.0f : 1.0f);
		}

		CGeomExtent& ext = m_extents.Make(eForm);
		if (!ext)
		{
			float zscale = m_height > 0.0f ? (eForm == GeomForm_Edges ? 2.0f : m_height) : 1.0f;
			if (eForm == GeomForm_Volume)
			{
				if (!m_bClosed)
					return 0.0f;

				TPolygon2D<Vec3> polygon(m_areaPoints);
				polygon.Triangulate(m_triIndices);
				int nTris = m_triIndices.size() / 3;
				ext.ReserveParts(nTris);
				for (uint index = 0; index < m_triIndices.size(); index += 3)
				{
					Vec2 a = m_areaPoints[m_triIndices[index]],
					     b = m_areaPoints[m_triIndices[index + 1]],
					     c = m_areaPoints[m_triIndices[index + 2]];

					float extent = ((b - a) ^ (c - a)) * 0.5f;
					extent *= zscale;
					ext.AddPart(extent);
				}
			}
			else
			{
				if (eForm == GeomForm_Edges && m_height > 0.0f)
				{
					// Add vertical edges
					ext.ReserveParts(m_areaPoints.size() + m_areaSegments.size());
					for (auto const& point : m_areaPoints)
						ext.AddPart(m_height);
				}

				// Add horizontal edges/surfaces
				ext.ReserveParts(m_areaSegments.size());
				for (const auto& segment : m_areaSegments)
				{
					float extent = (segment->bbox.min - segment->bbox.max).GetLength();
					extent *= zscale;
					ext.AddPart(extent);
				}
			}
		}
		return ext.TotalExtent();
	}
	case ENTITY_AREA_TYPE_SOLID:
		// To do
		// m_pAreaSolid->GetExtent(eForm) * ScaleExtent(eForm, m_worldTM);
	default:
		return 0.0f;
	}
}

void CArea::GetRandomPoints(Array<PosNorm> points, CRndGen seed, EGeomForm eForm) const
{
	CRY_PROFILE_FUNCTION(PROFILE_ENTITY)
	switch (m_areaType)
	{
	case ENTITY_AREA_TYPE_SPHERE:
		SphereRandomPoints(points, seed, eForm, m_sphereRadius);
		for (auto& point : points)
			point.vPos += m_sphereCenter;
		return;
	case ENTITY_AREA_TYPE_BOX:
	{
		BoxRandomPoints(points, seed, eForm, (m_boxMax - m_boxMin) * 0.5f);
		Matrix34 tm = m_worldTM;
		tm.SetTranslation(tm.GetTranslation() + (m_boxMax + m_boxMin) * 0.5f);
		for (auto& point : points)
			point <<= tm;
		return;
	}
	case ENTITY_AREA_TYPE_SHAPE:
	{
		const CGeomExtent& ext = m_extents[eForm];

		auto SetVertexPoint = [=](PosNorm& point, int n, float tv)
		{
			point.vPos = m_areaPoints[n];
			point.vPos.z += m_height * tv;
			if (!m_bClosed && (n == 0 || n == m_areaPoints.size() - 1))
				point.vNorm = Vec3(m_areaSegments[n]->normal);
			else
				point.vNorm = Vec3(m_areaSegments[n]->normal + m_areaSegments[PrevPoint(n)]->normal).GetNormalized();
		};
		auto SetSegmentPoint = [=](PosNorm& point, int n, float th, float tv)
		{
			point.vPos = Lerp(m_areaPoints[n], m_areaPoints[NextPoint(n)], th);
			point.vPos.z += m_height * tv;
			point.vNorm = Vec3(m_areaSegments[n]->normal);
		};

		if (eForm == GeomForm_Vertices)
		{
			for (auto& point : points)
			{
				int part = seed.GetRandom(0, (int)m_areaPoints.size() - 1);
				SetVertexPoint(point, part, (float)seed.GetRandom(0, 1));
			}
		}
		else if (eForm == GeomForm_Edges)
		{
			for (auto& point : points)
			{
				int part = ext.RandomPart(seed);
				if (m_height > 0.0f && part < (int)m_areaPoints.size())
				{
					// Vertical edge
					SetVertexPoint(point, part, seed.GenerateFloat());
				}
				else
				{
					// Horizontal edge
					if (m_height > 0.0f)
						part -= m_areaPoints.size();
					SetSegmentPoint(point, part, seed.GenerateFloat(), (float)seed.GetRandom(0, 1));
				}
			}
		}
		else if (eForm == GeomForm_Surface)
		{
			for (auto& point : points)
			{
				int part = ext.RandomPart(seed);
				SetSegmentPoint(point, part, seed.GenerateFloat(), seed.GenerateFloat());
			}
		}
		else if (eForm == GeomForm_Volume)
		{
			if (!m_bClosed)
			{
				return points.fill(PosNorm(ZERO));
			}
			for (auto& point : points)
			{
				int part = ext.RandomPart(seed);
				int index = part * 3;
				Vec3 a = m_areaPoints[m_triIndices[index]],
				     b = m_areaPoints[m_triIndices[index + 1]],
				     c = m_areaPoints[m_triIndices[index + 2]];

				float t[3];
				RandomSplit3(seed, t);

				point.vPos = Vec3(a * t[0] + b * t[1] + c * t[2]);
				point.vPos.z += seed.GetRandom(0.0f, m_height);
				point.vNorm = Vec3(0, 0, 1);
			}
		}
		return;
	}
	case ENTITY_AREA_TYPE_SOLID:
		// To do
	default:
		return;
	}
}

bool CArea::IsPointInside(Vec3 const& pointToTest) const
{
	return CalcPointWithinNonCached(pointToTest, false);
}


#if defined(INCLUDE_ENTITYSYSTEM_PRODUCTION_CODE)
//////////////////////////////////////////////////////////////////////////
char const* const CArea::GetAreaEntityName() const
{
	if (g_pIEntitySystem != nullptr)
	{
		CEntity const* const pIEntity = g_pIEntitySystem->GetEntityFromID(m_entityId);

		if (pIEntity != nullptr)
		{
			return pIEntity->GetName();
		}
	}

	return nullptr;
}
#endif // INCLUDE_ENTITYSYSTEM_PRODUCTION_CODE
