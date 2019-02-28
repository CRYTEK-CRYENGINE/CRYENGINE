// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "Navigation.h"

#include <CryCore/TypeInfo_impl.h>
#include <CryAISystem/INavigationSystem.h>

#include "AICollision.h"

#include "Util/GeometryUtil.h"

static const int maxForbiddenNameLen = 1024;

CNavigation::CNavigation(ISystem* pSystem) :
	m_pSystem(pSystem)
{
	AIInitLog(pSystem);
}

CNavigation::~CNavigation()
{

}

bool CNavigation::Init()
{
	return true;
}

void CNavigation::ShutDown()
{
	FlushAllAreas();
}

void CNavigation::FlushAllAreas()
{
	m_mapGenericShapes.clear();
	m_mapOcclusionPlanes.clear();
	m_mapPerceptionModifiers.clear();

	m_mapDesignerPaths.clear();
}

//====================================================================
// OnMissionLoaded
//====================================================================
void CNavigation::OnMissionLoaded()
{
}

void CNavigation::Serialize(TSerialize ser)
{
}

//
//-----------------------------------------------------------------------------------------------------------
void CNavigation::FlushSystemNavigation(void)
{
}

void CNavigation::ValidateNavigation()
{
}

// NOTE Jun 7, 2007: <pvl> not const anymore because ForbiddenAreasOverlap()
// isn't const anymore
//===================================================================
// ValidateAreas
//===================================================================
bool CNavigation::ValidateAreas()
{
	bool result = true;
	for (ShapeMap::const_iterator it = m_mapDesignerPaths.begin(); it != m_mapDesignerPaths.end(); ++it)
	{
		if (it->second.shape.size() < 2)
		{
			AIWarning("AI Path %s has only %d points", it->first.c_str(), it->second.shape.size());
			result = false;
		}
	}
	for (ShapeMap::const_iterator it = m_mapOcclusionPlanes.begin(); it != m_mapOcclusionPlanes.end(); ++it)
	{
		if (it->second.shape.size() < 2)
		{
			AIWarning("AI Occlusion Plane %s has only %d points", it->first.c_str(), it->second.shape.size());
			result = false;
		}
	}
	for (PerceptionModifierShapeMap::const_iterator it = m_mapPerceptionModifiers.begin(); it != m_mapPerceptionModifiers.end(); ++it)
	{
		if (it->second.shape.size() < 2)
		{
			AIWarning("AI Perception Modifier %s has only %d points", it->first.c_str(), it->second.shape.size());
			result = false;
		}
	}
	return result;
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
	if (di == m_mapDesignerPaths.end())
		return false;
	path = di->second;
	return true;
}

//====================================================================
// WritePolygonArea
//====================================================================
void CNavigation::WritePolygonArea(CCryFile& file, const string& name, const ListPositions& pts)
{
	unsigned nameLen = name.length();
	if (nameLen >= maxForbiddenNameLen)
		nameLen = maxForbiddenNameLen - 1;
	file.Write(&nameLen, sizeof(nameLen));
	file.Write((char*) name.c_str(), nameLen * sizeof(char));
	unsigned ptsSize = pts.size();
	file.Write(&ptsSize, sizeof(ptsSize));
	ListPositions::const_iterator it;
	for (it = pts.begin(); it != pts.end(); ++it)
	{
		// only works so long as *it is a contiguous object
		const Vec3& pt = *it;
		file.Write((void*) &pt, sizeof(pt));
	}
}


void CNavigation::WriteAreasIntoFile(const char* fileName)
{
	CCryFile file;
	if (false != file.Open(fileName, "wb"))
	{
		int fileVersion = BAI_AREA_FILE_VERSION_WRITE;
		file.Write(&fileVersion, sizeof(fileVersion));

		// Write designer paths
		{
			unsigned numDesignerPaths = m_mapDesignerPaths.size();
			file.Write(&numDesignerPaths, sizeof(numDesignerPaths));
			ShapeMap::const_iterator it;
			for (it = m_mapDesignerPaths.begin(); it != m_mapDesignerPaths.end(); ++it)
			{
				WritePolygonArea(file, it->first, it->second.shape);
				int navType = it->second.navType;
				file.Write(&navType, sizeof(navType));
				file.Write(&it->second.type, sizeof(int));
				file.Write(&it->second.closed, sizeof(bool));
			}
		}

		//----------------------------------------------------------
		// End of navigation-related shapes, start of other shapes
		//----------------------------------------------------------

		// Write generic shapes
		{
			unsigned numGenericShapes = m_mapGenericShapes.size();
			file.Write(&numGenericShapes, sizeof(numGenericShapes));
			ShapeMap::const_iterator it;
			for (it = m_mapGenericShapes.begin(); it != m_mapGenericShapes.end(); ++it)
			{
				WritePolygonArea(file, it->first, it->second.shape);

				int navType = it->second.navType;
				file.Write(&navType, sizeof(navType));
				file.Write(&it->second.type, sizeof(int));
				float height = it->second.height;
				int lightLevel = (int)it->second.lightLevel;
				file.Write(&height, sizeof(height));
				file.Write(&lightLevel, sizeof(lightLevel));
			}
		}
	}
	else
	{
		AIWarning("Unable to open areas file %s", fileName);
	}
}

//
//-----------------------------------------------------------------------------------------------------------
void CNavigation::ExportData(
	const char* navFileName, const char* areasFileName, const char* roadsFileName,
	const char* vertsFileName, const char* volumeFileName, const char* flightFileName)
{
	if (areasFileName)
	{
		AILogProgress("[AISYSTEM] Exporting Areas to %s", areasFileName);
		WriteAreasIntoFile(areasFileName);
	}
	AILogProgress("[AISYSTEM] Finished Exporting data");
}

//
//-----------------------------------------------------------------------------------------------------------
bool CNavigation::DoesNavigationShapeExists(const char* szName, EnumAreaType areaType, bool road)
{
	//////////////////////////////////////////////////////////////////////////
	/*
	if (m_pSystem->GetAISystem())
	{
		m_pSystem->GetAISystem()->DoesNavigationShapeExists(szName, areaType);
	}
	*/
	//////////////////////////////////////////////////////////////////////////
	
	switch (areaType)
	{
	case AREATYPE_PATH:
		return m_mapDesignerPaths.find(szName) != m_mapDesignerPaths.end();
	case AREATYPE_OCCLUSION_PLANE:
		return m_mapOcclusionPlanes.find(szName) != m_mapOcclusionPlanes.end();
	case AREATYPE_EXTRALINKCOST:
		break;
	case AREATYPE_GENERIC:
		return m_mapGenericShapes.find(szName) != m_mapGenericShapes.end();
	case AREATYPE_PERCEPTION_MODIFIER:
	case AREATYPE_FORBIDDEN:
	case AREATYPE_FORBIDDENBOUNDARY:
	case AREATYPE_NAVIGATIONMODIFIER:
	default:
		break;
	}
	return false;
}

//
//-----------------------------------------------------------------------------------------------------------
bool CNavigation::CreateNavigationShape(const SNavigationShapeParams& params)
{
	//////////////////////////////////////////////////////////////////////////
	if (m_pSystem->GetAISystem())
	{
		m_pSystem->GetAISystem()->CreateNavigationShape(params);
	}
	//////////////////////////////////////////////////////////////////////////
	
	// need at least one point in a path. Some paths need more than one (areas need 3)
	if (params.nPoints == 0)
		return true; // Do not report too few points as errors.

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
			AIError("CNavigation::CreateNavigationShape: Designer path '%s' already exists, please rename the path.", params.szPathName);
			return false;
		}

		if (params.closed)
		{
			if (Distance::Point_Point(listPts.front(), listPts.back()) > 0.1f)
				listPts.push_back(listPts.front());
		}
		m_mapDesignerPaths.insert(ShapeMap::iterator::value_type(params.szPathName, SShape(listPts, false, (IAISystem::ENavigationType)params.nNavType, params.nAuxType, params.closed)));
	}
	else if (params.areaType == AREATYPE_OCCLUSION_PLANE)
	{
		if (listPts.size() < 3)
			return true; // Do not report too few points as errors.

		if (m_mapOcclusionPlanes.find(params.szPathName) != m_mapOcclusionPlanes.end())
		{
			AIError("CNavigation::CreateNavigationShape: Occlusion plane '%s' already exists, please rename the shape.", params.szPathName);
			return false;
		}

		m_mapOcclusionPlanes.insert(ShapeMap::iterator::value_type(params.szPathName, SShape(listPts)));
	}
	else if (params.areaType == AREATYPE_PERCEPTION_MODIFIER)
	{
		if (listPts.size() < 2)
			return false;

		PerceptionModifierShapeMap::iterator di;
		di = m_mapPerceptionModifiers.find(params.szPathName);

		if (di == m_mapPerceptionModifiers.end())
		{
			SPerceptionModifierShape pms(listPts, params.fReductionPerMetre, params.fReductionMax, params.fHeight, params.closed);
			m_mapPerceptionModifiers.insert(PerceptionModifierShapeMap::iterator::value_type(params.szPathName, pms));
		}
		else
			return false;
	}
	else if (params.areaType == AREATYPE_GENERIC)
	{
		if (listPts.size() < 3)
			return true; // Do not report too few points as errors.

		if (m_mapGenericShapes.find(params.szPathName) != m_mapGenericShapes.end())
		{
			AIError("CNavigation::CreateNavigationShape: Shape '%s' already exists, please rename the shape.", params.szPathName);
			return false;
		}

		m_mapGenericShapes.insert(ShapeMap::iterator::value_type(params.szPathName,
		                                                         SShape(listPts, false, IAISystem::NAV_UNSET, params.nAuxType, params.closed, params.fHeight, params.lightLevel)));
	}
	return true;
}

// deletes designer created path
//
//-----------------------------------------------------------------------------------------------------------
void CNavigation::DeleteNavigationShape(const char* szName)
{
	//////////////////////////////////////////////////////////////////////////
	if (m_pSystem->GetAISystem())
	{
		m_pSystem->GetAISystem()->DeleteNavigationShape(szName);
	}
	//////////////////////////////////////////////////////////////////////////
	
	ShapeMap::iterator di;

	di = m_mapDesignerPaths.find(szName);
	if (di != m_mapDesignerPaths.end())
		m_mapDesignerPaths.erase(di);

	di = m_mapGenericShapes.find(szName);
	if (di != m_mapGenericShapes.end())
	{
		m_mapGenericShapes.erase(di);
	}

	di = m_mapOcclusionPlanes.find(szName);
	if (di != m_mapOcclusionPlanes.end())
		m_mapOcclusionPlanes.erase(di);

	PerceptionModifierShapeMap::iterator pmsi = m_mapPerceptionModifiers.find(szName);
	if (pmsi != m_mapPerceptionModifiers.end())
		m_mapPerceptionModifiers.erase(pmsi);
}

void CNavigation::Update() const
{
}

void CNavigation::GetMemoryStatistics(ICrySizer* pSizer)
{
	size_t size = 0;
	for (ShapeMap::iterator itr = m_mapDesignerPaths.begin(); itr != m_mapDesignerPaths.end(); itr++)
	{
		size += (itr->first).capacity();
		size += itr->second.MemStats();
	}
	pSizer->AddObject(&m_mapDesignerPaths, size);
}

