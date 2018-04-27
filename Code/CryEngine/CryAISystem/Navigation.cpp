// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/********************************************************************
   CryGame Source File.
   -------------------------------------------------------------------------
   File name:   Navigation.cpp
   Version:     v1.00
   Description:

   -------------------------------------------------------------------------
   History:
   - ?
   - 4 May 2009  : Evgeny Adamenkov: Replaced IRenderer with CDebugDrawContext

 *********************************************************************/

#include "StdAfx.h"

#include "Navigation.h"
#include "PolygonSetOps/Polygon2d.h"

#include "DebugDrawContext.h"
#include "CalculationStopper.h"
#include <CrySystem/File/CryBufferedFileReader.h>

static const int maxForbiddenNameLen = 1024;

// flag used for debugging/profiling the improvement obtained from using a
// QuadTree for the forbidden shapes. Currently it's actually faster not
// using quadtree - need to experiment more
// kirill - enabling quadtree - is faster on low-spec
bool useForbiddenQuadTree = true;//false;

CNavigation::CNavigation(ISystem* pSystem)
{
}

CNavigation::~CNavigation()
{
}

void CNavigation::ShutDown()
{
	FlushAllAreas();
}

// // loads the triangulation for this level and mission
//
//-----------------------------------------------------------------------------------------------------------
void CNavigation::LoadNavigationData(const char* szLevel, const char* szMission)
{
	LOADING_TIME_PROFILE_SECTION(GetISystem());

	CTimeValue startTime = gEnv->pTimer->GetAsyncCurTime();
	
	char fileNameAreas[1024];
	cry_sprintf(fileNameAreas, "%s/areas%s.bai", szLevel, szMission);

	GetAISystem()->ReadAreasFromFile(fileNameAreas);

	CTimeValue endTime = gEnv->pTimer->GetAsyncCurTime();
	AILogLoading("Navigation Data Loaded in %5.2f sec", (endTime - startTime).GetSeconds());
}

void CNavigation::Update(CTimeValue frameStartTime, float frameDeltaTime)
{
	// Update path devalues.
	for (ShapeMap::iterator it = m_mapDesignerPaths.begin(); it != m_mapDesignerPaths.end(); ++it)
		it->second.devalueTime = max(0.0f, it->second.devalueTime - frameDeltaTime);
}

void CNavigation::Reset(IAISystem::EResetReason reason)
{
	// Reset path devalue stuff.
	for (ShapeMap::iterator it = m_mapDesignerPaths.begin(); it != m_mapDesignerPaths.end(); ++it)
		it->second.devalueTime = 0;
}

// copies a designer path into provided list if a path of such name is found
//
//-----------------------------------------------------------------------------------------------------------
bool CNavigation::GetDesignerPath(const char* szName, SShape& path) const
{
	ShapeMap::const_iterator di = m_mapDesignerPaths.find(szName);
	if (di != m_mapDesignerPaths.end())
	{
		path = di->second;
		return true;
	}

	return false;
}

//====================================================================
// ReadPolygonArea
//====================================================================
bool ReadPolygonArea(CCryBufferedFileReader& file, int version, string& name, ListPositions& pts)
{
	unsigned nameLen = maxForbiddenNameLen;
	file.ReadType(&nameLen);
	if (nameLen >= maxForbiddenNameLen)
	{
		AIWarning("Excessive forbidden area name length - AI loading failed");
		return false;
	}
	char tmpName[maxForbiddenNameLen];
	file.ReadRaw(&tmpName[0], nameLen);
	tmpName[nameLen] = '\0';
	name = tmpName;

	unsigned ptsSize;
	file.ReadType(&ptsSize);

	for (unsigned iPt = 0; iPt < ptsSize; ++iPt)
	{
		Vec3 pt;
		file.ReadType(&pt);
		pts.push_back(pt);
	}
	return true;
}

//
// Reads (designer paths) areas from file. clears the existing areas
void CNavigation::ReadAreasFromFile(CCryBufferedFileReader& file, int fileVersion)
{
	FlushAllAreas();

	unsigned numAreas;

	// Read designer paths
	{
		file.ReadType(&numAreas);
		// vague sanity check
		AIAssert(numAreas < 1000000);
		for (unsigned iArea = 0; iArea < numAreas; ++iArea)
		{
			ListPositions lp;
			string name;
			ReadPolygonArea(file, fileVersion, name, lp);

			int navType(0), type(0);
			bool closed(false);
			file.ReadType(&navType);
			file.ReadType(&type);

			if (fileVersion >= 22)
			{
				file.ReadType(&closed);
			}

			if (m_mapDesignerPaths.find(name) != m_mapDesignerPaths.end())
			{
				AIError("CNavigation::ReadAreasFromFile: Designer path '%s' already exists, please rename the path and reexport.", name.c_str());
				//@TODO do something about AI Paths in multiple area.bai files when in Segmented World
			}
			else
				m_mapDesignerPaths.insert(ShapeMap::iterator::value_type(name, SShape(lp, false, (IAISystem::ENavigationType)navType, type, closed)));
		}
	}
}

// Offsets all areas when segmented world shifts
void CNavigation::OffsetAllAreas(const Vec3& additionalOffset)
{
	for (ShapeMap::iterator it = m_mapDesignerPaths.begin(); it != m_mapDesignerPaths.end(); ++it)
		it->second.OffsetShape(additionalOffset);
}

//====================================================================
// GetNearestPointOnPath
//====================================================================
float CNavigation::GetNearestPointOnPath(const char* szPathName, const Vec3& vPos, Vec3& vResult, bool& bLoopPath, float& totalLength) const
{
	// calculate a point on a path which has the nearest distance from the given point.
	// Also returns segno it means ( 0.0 start point 100.0 end point always)
	// strict even if t == 0 or t == 1.0

	// return values
	// return value : segno;
	// vResult : the point on path
	// bLoopPath : true if the path is looped

	vResult = ZERO;
	bLoopPath = false;

	float result = -1.0;

	SShape pathShape;
	if (!GetDesignerPath(szPathName, pathShape) || pathShape.shape.empty())
		return result;

	float dist = FLT_MAX;
	bool bFound = false;
	float segNo = 0.0f;
	float howmanypath = 0.0f;

	Vec3 vPointOnLine;

	ListPositions::const_iterator cur = pathShape.shape.begin();
	ListPositions::const_reverse_iterator last = pathShape.shape.rbegin();
	ListPositions::const_iterator next(cur);
	++next;

	float lengthTmp = 0.0f;
	while (next != pathShape.shape.end())
	{
		Lineseg seg(*cur, *next);

		lengthTmp += (*cur - *next).GetLength();

		float t;
		float d = Distance::Point_Lineseg(vPos, seg, t);
		if (d < dist)
		{
			Vec3 vSeg = seg.GetPoint(1.0f) - seg.GetPoint(0.0f);
			Vec3 vTmp = vPos - seg.GetPoint(t);

			vSeg.NormalizeSafe();
			vTmp.NormalizeSafe();

			dist = d;
			bFound = true;
			result = segNo + t;
			vPointOnLine = seg.GetPoint(t);
		}
		cur = next;
		++next;
		segNo += 1.0f;
		howmanypath += 1.0f;
	}
	if (howmanypath == 0.0f)
		return result;

	if (bFound == false)
	{
		segNo = 0.0f;
		cur = pathShape.shape.begin();
		while (cur != pathShape.shape.end())
		{
			Vec3 vTmp = vPos - *cur;
			float d = vTmp.GetLength();
			if (d < dist)
			{
				dist = d;
				bFound = true;
				result = segNo;
				vPointOnLine = *cur;
			}
			++cur;
			segNo += 1.0f;
		}
	}

	vResult = vPointOnLine;

	cur = pathShape.shape.begin();

	Vec3 vTmp = *cur - *last;
	if (vTmp.GetLength() < 0.0001f)
		bLoopPath = true;

	totalLength = lengthTmp;

	return result * 100.0f / howmanypath;
}

//====================================================================
// GetPointOnPathBySegNo
//====================================================================
void CNavigation::GetPointOnPathBySegNo(const char* szPathName, Vec3& vResult, float segNo) const
{
	vResult = ZERO;

	SShape pathShape;
	if ((segNo < 0.f) || (segNo > 100.f) || !GetDesignerPath(szPathName, pathShape))
		return;

	ListPositions& shape = pathShape.shape;
	size_t size = shape.size();
	if (size == 0)
		return;

	if (size == 1)
	{
		vResult = *(shape.begin());
		return;
	}

	float totalLength = 0.0f;
	for (ListPositions::const_iterator cur = shape.begin(), next = cur; ++next != shape.end(); cur = next)
	{
		totalLength += (*next - *cur).GetLength();
	}

	float segLength = totalLength * segNo / 100.0f;
	float currentLength = 0.0f;
	float currentSegLength = 0.0f;

	ListPositions::const_iterator cur, next;
	for (cur = shape.begin(), next = cur; ++next != shape.end(); cur = next)
	{
		const Vec3& curPoint = *cur;
		Vec3 currentSeg = *next - curPoint;
		currentSegLength = currentSeg.GetLength();

		if (currentLength + currentSegLength > segLength)
		{
			vResult = curPoint;
			if (currentSegLength > 0.0003f)
			{
				vResult += ((segLength - currentLength) / currentSegLength) * currentSeg;
			}
			return;
		}

		currentLength += currentSegLength;
	}

	vResult = *cur;
}

void CNavigation::FlushAllAreas()
{
	m_mapDesignerPaths.clear();
}

//
//-----------------------------------------------------------------------------------------------------------
bool CNavigation::DoesNavigationShapeExists(const char* szName, EnumAreaType areaType, bool road)
{
	if (areaType == AREATYPE_PATH && !road)
	{
		return m_mapDesignerPaths.find(szName) != m_mapDesignerPaths.end();
	}
	return false;
}

//
//-----------------------------------------------------------------------------------------------------------
bool CNavigation::CreateNavigationShape(const SNavigationShapeParams& params)
{
	std::vector<Vec3> vecPts(params.points, params.points + params.nPoints);

	if (params.areaType == AREATYPE_PATH && params.pathIsRoad == false)
	{
		//designer path need to preserve directions
	}
	else
	{
		if (params.closed)
			EnsureShapeIsWoundAnticlockwise<std::vector<Vec3>, float>(vecPts);
	}

	ListPositions listPts(vecPts.begin(), vecPts.end());

	if (params.areaType == AREATYPE_PATH)
	{
		if (listPts.size() < 2)
			return true; // Do not report too few points as errors.

		if (m_mapDesignerPaths.find(params.szPathName) != m_mapDesignerPaths.end())
		{
			AIError("CAISystem::CreateNavigationShape: Designer path '%s' already exists, please rename the path.", params.szPathName);
			return false;
		}

		if (params.closed)
		{
			if (Distance::Point_Point(listPts.front(), listPts.back()) > 0.1f)
				listPts.push_back(listPts.front());
		}
		m_mapDesignerPaths.insert(ShapeMap::iterator::value_type(params.szPathName, SShape(listPts, false, (IAISystem::ENavigationType)params.nNavType, params.nAuxType, params.closed)));
	}
	return true;
}

// deletes designer created path
//-----------------------------------------------------------------------------------------------------------
void CNavigation::DeleteNavigationShape(const char* szName)
{
	ShapeMap::iterator di;
	di = m_mapDesignerPaths.find(szName);
	if (di != m_mapDesignerPaths.end())
		m_mapDesignerPaths.erase(di);
}

const char* CNavigation::GetNearestPathOfTypeInRange(IAIObject* requester, const Vec3& reqPos, float range, int type, float devalue, bool useStartNode)
{
	AIAssert(requester);
	float rangeSq(sqr(range));

	ShapeMap::iterator closestShapeIt(m_mapDesignerPaths.end());
	float closestShapeDist(rangeSq);
	CAIActor* pReqActor = requester->CastToCAIActor();

	for (ShapeMap::iterator it = m_mapDesignerPaths.begin(); it != m_mapDesignerPaths.end(); ++it)
	{
		const SShape& path = it->second;

		// Skip paths which we cannot travel
		if ((path.navType & pReqActor->GetMovementAbility().pathfindingProperties.navCapMask) == 0)
			continue;
		// Skip wrong type
		if (path.type != type)
			continue;
		// Skip locked paths
		if (path.devalueTime > 0.01f)
			continue;

		// Skip paths too far away
		Vec3 tmp;
		if (Distance::Point_AABBSq(reqPos, path.aabb, tmp) > rangeSq)
			continue;

		float d;
		if (useStartNode)
		{
			// Disntance to start node.
			d = Distance::Point_PointSq(reqPos, path.shape.front());
		}
		else
		{
			// Distance to nearest point on path.
			ListPositions::const_iterator nearest = path.NearestPointOnPath(reqPos, false, d, tmp);
		}

		if (d < closestShapeDist)
		{
			closestShapeIt = it;
			closestShapeDist = d;
		}
	}

	if (closestShapeIt != m_mapDesignerPaths.end())
	{
		closestShapeIt->second.devalueTime = devalue;
		return closestShapeIt->first.c_str();
	}

	return 0;
}

//====================================================================
// CheckNavigationType
// When areas are nested there is an ordering - make this explicit by
// ordering the search over the navigation types
//====================================================================
IAISystem::ENavigationType CNavigation::CheckNavigationType(const Vec3& pos, int& nBuildingID, IAISystem::tNavCapMask navCapMask) const
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	if (navCapMask & IAISystem::NAV_TRIANGULAR)
		return IAISystem::NAV_TRIANGULAR;

	if (navCapMask & IAISystem::NAV_FREE_2D)
		return IAISystem::NAV_FREE_2D;

	return IAISystem::NAV_UNSET;
}

void CNavigation::GetMemoryStatistics(ICrySizer* pSizer)
{
	{
		SIZER_SUBCOMPONENT_NAME(pSizer, "NavRegions");
	}

	size_t size = 0;
	for (ShapeMap::iterator itr = m_mapDesignerPaths.begin(); itr != m_mapDesignerPaths.end(); ++itr)
	{
		size += (itr->first).capacity();
		size += itr->second.MemStats();
	}
	pSizer->AddObject(&m_mapDesignerPaths, size);
}

size_t CNavigation::GetMemoryUsage() const
{
	size_t mem = 0;
	return mem;
}

#ifdef CRYAISYSTEM_DEBUG
//-----------------------------------------------------------------------------------------------------------
void CNavigation::DebugDraw() const
{
	CDebugDrawContext dc;

	// Draw occupied designer paths
	for (ShapeMap::const_iterator it = m_mapDesignerPaths.begin(); it != m_mapDesignerPaths.end(); ++it)
	{
		const SShape& path = it->second;
		if (path.devalueTime < 0.01f)
			continue;
		dc->Draw3dLabel(path.shape.front(), 1, "%.1f", path.devalueTime);
	}
}

#endif //CRYAISYSTEM_DEBUG
