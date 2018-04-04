// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#ifndef __CoverSystem_h__
#define __CoverSystem_h__

#pragma once

#include "Cover.h"
#include "CoverSurface.h"
#include "DynamicCoverManager.h"

#include <CryAISystem/HashGrid.h>

struct CachedCoverLocationValues
{
	CachedCoverLocationValues()
		: location(ZERO)
		, normal(ZERO)
		, height(.0f)
	{};

	Vec3  location;
	Vec3  normal;
	float height;
};

typedef VectorMap<CoverID, CachedCoverLocationValues> CoverLocationCache;

class CCoverSystem
	: public ICoverSystem
{
	enum
	{
		PreallocatedDynamicCount = 64,
	};
public:
	CCoverSystem(const char* configFileName);
	~CCoverSystem();

	typedef std::vector<CoverID> CoverCollection;

	// ICoverSystem
	virtual ICoverSampler* CreateCoverSampler(const char* samplerName = "default");

	virtual bool           ReloadConfig();

	virtual void           Reset();
	virtual void           Clear();
	virtual bool           ReadSurfacesFromFile(const char* fileName);

	virtual void           BreakEvent(const Vec3& position, float radius);
	virtual void           AddCoverEntity(EntityId entityID);
	virtual void           RemoveCoverEntity(EntityId entityID);

	virtual CoverSurfaceID AddSurface(const SurfaceInfo& surfaceInfo);
	virtual void           RemoveSurface(const CoverSurfaceID& surfaceID);
	virtual void           UpdateSurface(const CoverSurfaceID& surfaceID, const SurfaceInfo& surfaceInfo);

	virtual uint32         GetSurfaceCount() const;
	virtual bool           GetSurfaceInfo(const CoverSurfaceID& surfaceID, SurfaceInfo* surfaceInfo) const;

	virtual void           SetCoverOccupied(const CoverID& coverID, bool occupied, const tAIObjectID& occupant);
	virtual bool           IsCoverOccupied(const CoverID& coverID) const;
	virtual tAIObjectID    GetCoverOccupant(const CoverID& coverID) const;

	virtual uint32         GetCover(const Vec3& center, float range, const Vec3* eyes, uint32 eyeCount, float distanceToCover,
	                                Vec3* locations, uint32 maxLocationCount, uint32 maxLocationsPerSurface) const;

	virtual void DrawSurface(const CoverSurfaceID& surfaceID);
	//~ICoverSystem

	void                      Update(float updateTime);
	void                      DebugDraw();

	bool                      IsDynamicSurfaceEntity(IEntity* entity) const;

	ILINE const CoverSurface& GetCoverSurface(const CoverID& coverID) const
	{
		return m_surfaces[(coverID >> CoverIDSurfaceIDShift) - 1];
	}

	ILINE const CoverSurface& GetCoverSurface(const CoverSurfaceID& surfaceID) const
	{
		return m_surfaces[surfaceID - 1];
	}

	ILINE CoverSurface& GetCoverSurface(const CoverSurfaceID& surfaceID)
	{
		return m_surfaces[surfaceID - 1];
	}

	ILINE const CoverPath& GetCoverPath(const CoverSurfaceID& surfaceID, float distanceToCover) const
	{
		return CacheCoverPath(surfaceID, m_surfaces[surfaceID - 1], distanceToCover);
	}

	ILINE uint32 GetCover(const Vec3& center, float radius, CoverCollection& locations) const
	{
		return m_locations.query_sphere(center, radius, locations);
	}

	ILINE Vec3 GetCoverLocation(const CoverID& coverID, float offset = 0.0f, float* height = 0, Vec3* normal = 0) const
	{
		return GetAndCacheCoverLocation(coverID, offset, height, normal);
	}

	ILINE CoverID GetCoverID(const CoverSurfaceID& surfaceID, uint16 location) const
	{
		return CoverID((surfaceID << CoverIDSurfaceIDShift) | location);
	}

	ILINE uint16 GetLocationID(const CoverID& coverID) const
	{
		return (coverID & CoverIDLocationIDMask);
	}

	ILINE CoverSurfaceID GetSurfaceID(const CoverID& coverID) const
	{
		return CoverSurfaceID(coverID >> CoverIDSurfaceIDShift);
	}

private:
	void             AddLocations(const CoverSurfaceID& surfaceID, const CoverSurface& surface);
	void             RemoveLocations(const CoverSurfaceID& surfaceID, const CoverSurface& surface);
	void             AddDynamicSurface(const CoverSurfaceID& surfaceID, const CoverSurface& surface);
	void             ResetDynamicSurface(const CoverSurfaceID& surfaceID, CoverSurface& surface);
	void             ResetDynamicCover();
	void             NotifyCoverUsers(const CoverSurfaceID& surfaceID);

	Vec3             GetAndCacheCoverLocation(const CoverID& coverID, float offset = 0.0f, float* height = 0, Vec3* normal = 0) const;
	void             ClearAndReserveCoverLocationCache();

	const CoverPath& CacheCoverPath(const CoverSurfaceID& surfaceID, const CoverSurface& surface, float distanceToCover) const;

	typedef std::vector<CoverSurface> Surfaces;
	Surfaces m_surfaces;

	struct location_position
	{
		Vec3 operator()(const CoverID& coverID) const
		{
			return gAIEnv.pCoverSystem->GetCoverLocation(coverID, 0.0f);
		}
	};

	typedef hash_grid<256, CoverID, hash_grid_2d<Vec3, Vec3i>, location_position> Locations;
	Locations m_locations;

	typedef std::map<float, CoverPath> Paths;

	struct PathCacheEntry
	{
		CoverSurfaceID surfaceID;
		Paths          paths;
	};

	typedef std::deque<PathCacheEntry> PathCache;
	mutable PathCache       m_pathCache;

	mutable CoverCollection m_externalQueryBuffer;

	typedef std::unordered_map<CoverID, tAIObjectID, stl::hash_uint32> OccupiedCover;
	OccupiedCover m_occupied;

	typedef std::vector<CoverSurfaceID> FreeIDs;
	FreeIDs m_freeIDs;

	typedef std::vector<IEntityClass*> DynamicSurfaceEntityClasses;
	DynamicSurfaceEntityClasses m_dynamicSurfaceEntityClasses;

	DynamicCoverManager         m_dynamicCoverManager;

	string                      m_configName;

	mutable CoverLocationCache  m_coverLocationCache;
};

#endif //__CoverSystem_h__
