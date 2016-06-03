// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

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
#include "CTriangulator.h"

struct SpecialArea
{
	// (MATT) Note that this must correspond exactly to the enum in the Editor's Navigation.h {2009/06/17}
	enum EType
	{
		TYPE_WAYPOINT_HUMAN,
		TYPE_VOLUME,
		TYPE_FLIGHT,
		TYPE_WATER,
		TYPE_WAYPOINT_3DSURFACE,
		TYPE_FREE_2D,
		TYPE_TRIANGULATION,
		TYPE_LAYERED_NAV_MESH,
		TYPE_FLIGHT2,
	};

	void SetPolygon(const ListPositions& polygon)
	{
		lstPolygon = polygon;
		CalcAABB();
	}
	const ListPositions& GetPolygon() const
	{
		return lstPolygon;
	}

	const AABB& GetAABB() const
	{
		return aabb;
	}

	SpecialArea::EType   type                : 6;
	EWaypointConnections waypointConnections : 6;
	EAILightLevel        lightLevel          : 4;
	int16                nBuildingID;

	float                fMinZ;
	float                fMaxZ;
	float                fHeight;
	float                fNodeAutoConnectDistance;

	bool                 bAltered     : 1;
	bool                 bCritterOnly : 1;

	SpecialArea()
		: nBuildingID(-1)
		, type(SpecialArea::TYPE_WAYPOINT_HUMAN)
		, waypointConnections(WPCON_DESIGNER_NONE)
		, fNodeAutoConnectDistance(.0f)
		, bAltered(false)
		, bCritterOnly(false)
		, lightLevel(AILL_NONE)
	{
		fMinZ = FLT_MAX;
		fMaxZ = -FLT_MAX;
		fHeight = 0.0f;

		aabb.Reset();
	}

private:
	ListPositions lstPolygon;
	AABB          aabb;

	void          CalcAABB()
	{
		aabb.Reset();
		for (ListPositions::const_iterator it = lstPolygon.begin(); it != lstPolygon.end(); ++it)
			aabb.Add(*it);
	}
};

typedef std::map<string, int>    SpecialAreaNames;
typedef std::vector<SpecialArea> SpecialAreas;
typedef std::vector<int16>       FreeSpecialAreaIDs;

struct SExtraLinkCostShape
{
	SExtraLinkCostShape(const ListPositions& shape, const AABB& aabb, float costFactor) : shape(shape), aabb(aabb), costFactor(costFactor), origCostFactor(costFactor) {}
	SExtraLinkCostShape(const ListPositions& shape, float costFactor) : shape(shape), costFactor(costFactor), origCostFactor(costFactor)
	{
		aabb.Reset();
		for (ListPositions::const_iterator it = shape.begin(); it != shape.end(); ++it)
			aabb.Add(*it);
	}
	ListPositions shape;
	AABB          aabb;
	// the cost factor can get modified at run-time - it will get reset at the same time
	// as graph links get reset etc
	float costFactor;
	float origCostFactor;
};

typedef std::map<string, SExtraLinkCostShape> ExtraLinkCostShapeMap;

struct CutEdgeIdx
{
	int idx1;
	int idx2;

	CutEdgeIdx(int i1, int i2)
	{
		idx1 = i1;
		idx2 = i2;
	}
};

typedef std::vector<CutEdgeIdx> NewCutsVector;

class CCryBufferedFileReader;
// FIXME Jan 30, 2008: <pvl> these should probably be members of their respective
// classes they save, they need to be visible both in Navigation.cpp
// CAISystem.cpp so let's put them here until refactored further.
void ReadArea(CCryBufferedFileReader& file, int version, string& name, SpecialArea& sa);
bool ReadForbiddenArea(CCryBufferedFileReader& file, int version, CAIShape* shape);
#if defined(SEG_WORLD)
bool ReadPolygonArea(CCryBufferedFileReader& file, int version, string& name, ListPositions& pts, const Vec3& vSegmentOffset);
#else
bool ReadPolygonArea(CCryBufferedFileReader& file, int version, string& name, ListPositions& pts);
#endif
void ReadExtraLinkCostArea(CCryBufferedFileReader& file, int version, string& name, SExtraLinkCostShape& shape);

class CNavRegion;
class CFlightNavRegion;
class CFlightNavRegion2;
class CVolumeNavRegion;
class CRoadNavRegion;
class CFree2DNavRegion;
class CSmartObjectNavRegion;
class CCompositeLayeredNavMeshRegion;
class CCustomNavRegion;

class CNavigation : public INavigation
{
public:
	explicit CNavigation(ISystem* pSystem);
	~CNavigation();

	// INavigation
	virtual uint32 GetPath(const char* szPathName, Vec3* points, uint32 maxpoints) const;
	virtual float  GetNearestPointOnPath(const char* szPathName, const Vec3& vPos, Vec3& vResult, bool& bLoopPath, float& totalLength) const;
	virtual void   GetPointOnPathBySegNo(const char* szPathName, Vec3& vResult, float segNo) const;
	virtual bool   IsSegmentValid(IAISystem::tNavCapMask navCap, float rad, const Vec3& posFrom, Vec3& posTo, IAISystem::ENavigationType& navTypeFrom) const;
	//~INavigation

	ILINE const SpecialAreas& GetSpecialAreas() const
	{
		return m_specialAreas;
	}

	const SpecialArea* GetSpecialArea(const Vec3& pos, SpecialArea::EType areaType);
	// Gets the special area with a particular building ID - may return 0 if it cannot be found
	const SpecialArea* GetSpecialArea(int buildingID) const;
	const SpecialArea* GetSpecialArea(const char* name) const;
	const SpecialArea* GetSpecialAreaNearestPos(const Vec3& pos, SpecialArea::EType areaType);
	const char*        GetSpecialAreaName(int buildingID) const;

	typedef std::vector<std::pair<string, const SpecialArea*>> VolumeRegions;
	/// Fills in the container of special areas that relate to 3D volumes
	void            GetVolumeRegions(VolumeRegions& volumeRegions) const;
	const ShapeMap& GetDesignerPaths() const { return m_mapDesignerPaths; }

	// copies a designer path into provided list if a path of such name is found
	bool  GetDesignerPath(const char* szName, SShape& path) const;

	bool  Init();
	void  Reset(IAISystem::EResetReason reason);
	void  ShutDown();
	void  FlushSystemNavigation(bool bDeleteAll);
	// Gets called after loading the mission
	void  OnMissionLoaded();
	// // loads the triangulation for this level and mission
	void  LoadNavigationData(const char* szLevel, const char* szMission);

	float GetDynamicLinkConnectionTimeModifier() const;
	void  BumpDynamicLinkConnectionUpdateTime(float modifier, size_t durationFrames);

	void  Serialize(TSerialize ser);

	// reads (designer paths) areas from file. clears the existing areas
#if defined(SEG_WORLD)
	// SEG_WORLD: adds offset to the areas read, and doesn't clear existing areas.
	void ReadAreasFromFile(CCryBufferedFileReader&, int fileVersion, const Vec3& vSegmentOffset);
#else
	void ReadAreasFromFile(CCryBufferedFileReader&, int fileVersion);
#endif
	void ReadAreasFromFile_Old(CCryBufferedFileReader&, int fileVersion);

	// Offsets all areas when segmented world shifts
	void OffsetAllAreas(const Vec3& additionalOffset);

	void Update(CTimeValue currentTime, float frameTime);
	void UpdateNavRegions();

	enum ENavDataState {NDS_UNSET, NDS_OK, NDS_BAD};
	ENavDataState GetNavDataState() const { return m_navDataState; }

	void          FlushAllAreas();
	void          FlushSpecialAreas();

	void          InsertSpecialArea(const char* name, const SpecialArea& sa);
	void          EraseSpecialArea(const char* name);

	/// This is just for debugging
	const char* GetNavigationShapeName(int nBuildingID) const;
	/// Checks if navigation shape exists - called by editor
	bool        DoesNavigationShapeExists(const char* szName, EnumAreaType areaType, bool road = false);
	bool        CreateNavigationShape(const SNavigationShapeParams& params);
	/// Deletes designer created path/shape - called by editor
	void        DeleteNavigationShape(const char* szName);
	void        DisableModifier(const char* name);

	bool        IsPointInForbiddenRegion(const Vec3& pos, bool checkAutoGenRegions = true) const                    { return false; }
	bool        IsPointInForbiddenRegion(const Vec3& pos, const CAIShape** ppShape, bool checkAutoGenRegions) const { return false; }
	/// Returns true if the point is inside the nav modifiers marked out as containing water
	bool        IsPointInWaterAreas(const Vec3& pt) const;
	/// Indicates if a point is in a special area, checking the height too
	static bool IsPointInSpecialArea(const Vec3& pos, const SpecialArea& sa);
	/// returns true if pos is inside a TRIANGULATION nav modifier, or if there are no
	/// such modifiers.
	/// NOTE this must only be called in editor (since it shouldn't be needed in game) - will assert that
	/// this is the case!
	bool IsPointInTriangulationAreas(const Vec3& pos) const;
	/// if there's intersection vClosestPoint indicates the intersection point, and the edge normal
	/// is optionally returned. If bForceNormalOutwards is set then in the case of forbidden
	/// boundaries this normal is chosen to point (partly) towards vStart.
	/// nameToSkip can optionally point to a string indicating a forbidden area area to not check
	/// mode indicates if areas and/or boundaries should be checked
	virtual bool IntersectsForbidden(const Vec3& vStart, const Vec3& vEnd, Vec3& vClosestPoint, const string* nameToSkip = 0, Vec3* pNormal = NULL,
	                                 INavigation::EIFMode mode = INavigation::IF_AREASBOUNDARIES, bool bForceNormalOutwards = false) const { return false; }
	virtual bool IntersectsForbidden(const Vec3& vStart, const Vec3& vEnd, float radius, INavigation::EIFMode mode = INavigation::IF_AREASBOUNDARIES) const { return false; }
	/// Checks for intersection with special areas of given type
	/// if there's intersection vClosestPoint indicates the intersection point
	bool         IntersectsSpecialArea(const Vec3& start, const Vec3& end, Vec3& closestPoint, SpecialArea::EType type) const;

	virtual bool IsPathForbidden(const Vec3& start, const Vec3& end) const             { return false; }
	virtual bool IsPointForbidden(const Vec3& pos, float tol, Vec3* pNormal = 0) const { return false; }
	/// Get the best point outside any forbidden region given the input point,
	/// and optionally a start position to stay close to
	virtual Vec3 GetPointOutsideForbidden(Vec3& pos, float distance, const Vec3* startPos = 0) const;

	bool         GetBuildingInfo(int nBuildingID, IAISystem::SBuildingInfo& info) const;
	bool         IsPointInBuilding(const Vec3& pos, int nBuildingID) const;

	/// Returns nearest designer created path/shape.
	/// The devalue parameter specifies how long the path will be unusable by others after the query.
	/// If useStartNode is true the start point of the path is used to select nearest path instead of the nearest point on path.
	virtual const char* GetNearestPathOfTypeInRange(IAIObject* requester, const Vec3& pos, float range, int type, float devalue, bool useStartNode);

	void                DisableNavigationInBrokenRegion(std::vector<Vec3>& outline);

	virtual void        ModifyNavCostFactor(const char* navModifierName, float factor) {}
	/// returns the names of the region files generated during volume generation
	virtual void        GetVolumeRegionFiles(const char* szLevel, const char* szMission, DynArray<CryStringT<char>>& filenames) const;

	/// Returns the base-class nav region - needs the graph only if type is waypoint (so you can
	/// pass in 0 if you know it's not... but be careful!)
	CNavRegion*                GetNavRegion(IAISystem::ENavigationType type, const CGraph* pGraph) const;
	const std::vector<float>&  Get3DPassRadii() const { return m_3DPassRadii; }
	IAISystem::ENavigationType CheckNavigationType(const Vec3& pos, int& nBuildingID, IAISystem::tNavCapMask navCapMask) const;

	void                       GetMemoryStatistics(ICrySizer* pSizer);
	size_t                     GetMemoryUsage() const;

#ifdef CRYAISYSTEM_DEBUG
	void DebugDraw() const;
#endif //CRYAISYSTEM_DEBUG

private:
	/// Indicates if the navigation data is sufficiently valid after loading that
	/// we should continue
	ENavDataState m_navDataState;

	// <NAV REGION STUFF>
	/// used during 3D nav generation - the pass radii of the entities that will use the navigation
	std::vector<float> m_3DPassRadii;
	// </NAV REGION STUFF>

	// <AI SHAPE STUFF>
	ShapeMap           m_mapDesignerPaths;
	SpecialAreas       m_specialAreas; // define where to disable automatic AI processing.
	FreeSpecialAreaIDs m_freeSpecialAreaIDs;
	SpecialAreaNames   m_specialAreaNames; // to speed up GetSpecialArea() in game mode

	unsigned int       m_nNumBuildings;
	// </NAV MODIFIER STUFF>

	struct SValidationErrorMarker
	{
		SValidationErrorMarker(const string& msg, const Vec3& pos, const OBB& obb, ColorB col) : msg(msg), pos(pos), obb(obb), col(col) {}
		SValidationErrorMarker(const string& msg, const Vec3& pos, ColorB col) : msg(msg), pos(pos), col(col)
		{
			obb.SetOBBfromAABB(Matrix33(IDENTITY), AABB(Vec3(-0.1f, -0.1f, -0.1f), Vec3(0.1f, 0.1f, 0.1f)));
		}
		Vec3   pos;
		OBB    obb;
		string msg;
		ColorB col;
	};

	std::vector<SValidationErrorMarker> m_validationErrorMarkers;

	//====================================================================
	// Used during graph generation - should subsequently be empty
	//====================================================================
	CTriangulator*    m_pTriangulator;
	std::vector<Tri*> m_vTriangles;
	VARRAY            m_vVertices;

	float             m_dynamicLinkUpdateTimeBump;
	size_t            m_dynamicLinkUpdateTimeBumpDuration;
	size_t            m_dynamicLinkUpdateTimeBumpElapsed;
};

#endif // NAVIGATION_H
