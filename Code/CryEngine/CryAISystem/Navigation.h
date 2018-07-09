// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/********************************************************************
   -------------------------------------------------------------------------
   File name:   Navigation.h
   $Id$
   Description: interface for the CGraph class.

   -------------------------------------------------------------------------
   History:
   - ?
   - 4 May 2009   : Evgeny Adamenkov: Removed IRenderer

 *********************************************************************/

#ifndef NAVIGATION_H
#define NAVIGATION_H

#include <CryAISystem/INavigation.h>

class CCryBufferedFileReader;

bool ReadPolygonArea(CCryBufferedFileReader& file, int version, string& name, ListPositions& pts);

class CNavigation : public INavigation
{
public:
	explicit CNavigation(ISystem* pSystem);
	~CNavigation();

	// INavigation
	virtual float  GetNearestPointOnPath(const char* szPathName, const Vec3& vPos, Vec3& vResult, bool& bLoopPath, float& totalLength) const;
	virtual void   GetPointOnPathBySegNo(const char* szPathName, Vec3& vResult, float segNo) const;
	//~INavigation

	const ShapeMap& GetDesignerPaths() const { return m_mapDesignerPaths; }

	// copies a designer path into provided list if a path of such name is found
	bool  GetDesignerPath(const char* szName, SShape& path) const;

	void  Reset(IAISystem::EResetReason reason);
	void  ShutDown();
	
	// // loads the triangulation for this level and mission
	void  LoadNavigationData(const char* szLevel, const char* szMission);

	// reads (designer paths) areas from file. clears the existing areas
	void ReadAreasFromFile(CCryBufferedFileReader&, int fileVersion);

	void Update(CTimeValue currentTime, float frameTime);

	/// This is just for debugging
	const char* GetNavigationShapeName(int nBuildingID) const;
	bool        DoesNavigationShapeExists(const char* szName, EnumAreaType areaType, bool road = false);
	bool        CreateNavigationShape(const SNavigationShapeParams& params);
	void        DeleteNavigationShape(const char* szName);

	void FlushAllAreas();

	// Offsets all areas when segmented world shifts
	void OffsetAllAreas(const Vec3& additionalOffset);

	/// Returns nearest designer created path/shape.
	/// The devalue parameter specifies how long the path will be unusable by others after the query.
	/// If useStartNode is true the start point of the path is used to select nearest path instead of the nearest point on path.
	virtual const char* GetNearestPathOfTypeInRange(IAIObject* requester, const Vec3& pos, float range, int type, float devalue, bool useStartNode);

	IAISystem::ENavigationType CheckNavigationType(const Vec3& pos, int& nBuildingID, IAISystem::tNavCapMask navCapMask) const;

	void                       GetMemoryStatistics(ICrySizer* pSizer);
	size_t                     GetMemoryUsage() const;

#ifdef CRYAISYSTEM_DEBUG
	void DebugDraw() const;
#endif //CRYAISYSTEM_DEBUG

private:
	// <AI SHAPE STUFF>
	ShapeMap           m_mapDesignerPaths;
};

#endif // NAVIGATION_H
