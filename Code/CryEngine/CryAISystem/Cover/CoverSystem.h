// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "Cover.h"
#include "CoverSurface.h"
#include "CoverUser.h"
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

// notifies whenever cover-surfaces change or get removed
struct ICoverSurfaceListener
{
	virtual ~ICoverSurfaceListener() {}
	virtual void OnCoverSurfaceChangedOrRemoved(const CoverSurfaceID& affectedSurfaceID) = 0;
};

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
	virtual ICoverUser*    RegisterEntity(const EntityId entityId, const ICoverUser::Params& params) override;
	virtual void           UnregisterEntity(const EntityId entityId) override;
	virtual ICoverUser*    GetRegisteredCoverUser(const EntityId entityId) const override;

	virtual ICoverSampler* CreateCoverSampler(const char* samplerName = "default") override;

	virtual void           Clear() override;
	virtual bool           ReadSurfacesFromFile(const char* fileName) override;

	virtual CoverSurfaceID AddSurface(const SurfaceInfo& surfaceInfo) override;
	virtual void           RemoveSurface(const CoverSurfaceID& surfaceID) override;
	virtual void           UpdateSurface(const CoverSurfaceID& surfaceID, const SurfaceInfo& surfaceInfo) override;

	virtual uint32         GetSurfaceCount() const override;
	virtual bool           GetSurfaceInfo(const CoverSurfaceID& surfaceID, SurfaceInfo* surfaceInfo) const override;

	virtual uint32         GetCover(const Vec3& center, float range, const Vec3* eyes, uint32 eyeCount, float distanceToCover,
	                                Vec3* locations, uint32 maxLocationCount, uint32 maxLocationsPerSurface) const override;

	virtual void DrawSurface(const CoverSurfaceID& surfaceID) override;
	//~ICoverSystem

	bool                      ReloadConfig();
	void                      Reset();

	void                      BreakEvent(const Vec3& position, float radius);
	void                      AddCoverEntity(EntityId entityID);
	void                      RemoveCoverEntity(EntityId entityID);

	void                      SetCoverOccupied(const CoverID& coverID, bool occupied, const CoverUser& occupant);
	bool                      IsCoverOccupied(const CoverID& coverID) const;
	EntityId                  GetCoverOccupant(const CoverID& coverID) const;
	bool                      IsCoverPhysicallyOccupiedByAnyOtherCoverUser(const CoverID& coverID, const ICoverUser& coverUserSearchingForEmptySpace) const;

	void                      Update(float updateTime);
	void                      DebugDraw();
	void                      DebugDrawSurfaceEffectiveHeight(const CoverSurface& surface, const Vec3* eyes, uint32 eyeCount);
	void                      DebugDrawCoverUser(const EntityId entityId);

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

	ILINE void RegisterCoverSurfaceListener(ICoverSurfaceListener* pCoverSurfaceListener)
	{
		stl::push_back_unique(m_coverSurfaceListeners, pCoverSurfaceListener);
	}

	ILINE void UnregisterCoverSurfaceListener(ICoverSurfaceListener* pCoverSurfaceListener)
	{
		stl::find_and_erase(m_coverSurfaceListeners, pCoverSurfaceListener);
	}

private:
	void             AddLocations(const CoverSurfaceID& surfaceID, const CoverSurface& surface);
	void             RemoveLocations(const CoverSurfaceID& surfaceID, const CoverSurface& surface);
	void             AddDynamicSurface(const CoverSurfaceID& surfaceID, const CoverSurface& surface);
	void             ResetDynamicSurface(const CoverSurfaceID& surfaceID, CoverSurface& surface);
	void             ResetDynamicCover();
	void             NotifyCoverUsers(const CoverSurfaceID& surfaceID);
	void             NotifyCoverSurfaceListeners(const CoverSurfaceID& surfaceID);

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

	typedef std::unordered_map<EntityId, std::shared_ptr<CoverUser>> CoverUsersMap;
	CoverUsersMap m_coverUsers;

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

	struct OccupantInfo
	{
		EntityId entityId;  // entity (CoverUser) that is occupying the cover (or has reserved it for later occupation)
		Vec3 pos;           // center of the *occupant*, *not* the cover! (the occupant will back off a bit from the actual cover location via its ICoverUser::Params::distanceToCover property)
		float radius;       // size of the occupied space
	};

	typedef std::unordered_map<CoverID, OccupantInfo, stl::hash_uint32> OccupiedCover;
	OccupiedCover m_occupied;

	typedef std::vector<CoverSurfaceID> FreeIDs;
	FreeIDs m_freeIDs;

	typedef std::vector<IEntityClass*> DynamicSurfaceEntityClasses;
	DynamicSurfaceEntityClasses m_dynamicSurfaceEntityClasses;

	DynamicCoverManager         m_dynamicCoverManager;

	string                      m_configName;

	mutable CoverLocationCache  m_coverLocationCache;

	typedef std::list<ICoverSurfaceListener*> CoverSurfaceListeners; // using a std::list<> to allow for un-registering during NotifyCoverSurfaceListeners()
	CoverSurfaceListeners       m_coverSurfaceListeners;
};

