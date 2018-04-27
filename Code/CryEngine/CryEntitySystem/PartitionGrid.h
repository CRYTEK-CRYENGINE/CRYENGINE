// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryMemory/PoolAllocator.h>

#define SECTORS_PER_GROUP 16
#define METERS_PER_SECTOR 2

#define SECTOR_TO_GROUP_COORD(x) ((x) / SECTORS_PER_GROUP)
//#define SECTOR_TO_GROUP_COORD(x)  ((x)>>4)
#define SECTOR_TO_LOCAL_COORD(x) ((x) & 0xF)

class CEntity;
struct IEntityClass;

//////////////////////////////////////////////////////////////////////////
struct SGridLocation
{
	SGridLocation* next;         // Next entry.
	SGridLocation* prev;         // Previous entry.
	int            nSectorIndex; // Index of the sector.

	CEntity*       pEntity;
	IEntityClass*  pEntityClass;
	uint32         nEntityFlags;

	//////////////////////////////////////////////////////////////////////////
	// Custom new/delete.
	//////////////////////////////////////////////////////////////////////////
	void* operator new(size_t nSize);
	void  operator delete(void* ptr);
};

//////////////////////////////////////////////////////////////////////////
struct SPartitionGridQuery
{
	AABB                   aabb;
	IEntityClass*          pEntityClass;
	uint32                 nEntityFlags;
	std::vector<IEntity*>* pEntities;

	SPartitionGridQuery()
	{
		pEntities = 0;
		pEntityClass = 0;
		nEntityFlags = 0;
	}
};

//////////////////////////////////////////////////////////////////////////
class CPartitionGrid
{
public:
	//////////////////////////////////////////////////////////////////////////
	CPartitionGrid();
	~CPartitionGrid();

	void           AllocateGrid(float fMetersX, float fMetersY);
	void           DeallocateGrid();
	SGridLocation* Rellocate(SGridLocation* obj, const Vec3& newPos, CEntity* pEntity);
	void           FreeLocation(SGridLocation* obj);

	// Here some processing is done.
	void Update();
	void Reset();

	void BeginReset();

	// Get entities in box.
	void GetEntitiesInBox(SPartitionGridQuery& query);

	// Gather grid memory usage statistics.
	void GetMemoryUsage(ICrySizer* pSizer) const;

public:
	friend class CUT_PartitionGrid; // For unit testing.

	//////////////////////////////////////////////////////////////////////////
	struct Sector
	{
		SGridLocation* first;
	};
	struct SectorGroup
	{
		int    nLocationCount;
		Sector sectors[SECTORS_PER_GROUP][SECTORS_PER_GROUP];

		//////////////////////////////////////////////////////////////////////////
		// Custom new/delete.
		//////////////////////////////////////////////////////////////////////////
		void* operator new(size_t nSize)
		{
			void* ptr = CPartitionGrid::g_SectorGroupPoolAlloc->Allocate();
			if (ptr)
				memset(ptr, 0, nSize); // Clear objects memory.
			return ptr;
		}
		void operator delete(void* ptr)
		{
			if (ptr)
				CPartitionGrid::g_SectorGroupPoolAlloc->Deallocate(ptr);
		}
	};
	struct LocationInfo
	{
		Sector*      sector;
		SectorGroup* group;
		int          x, y; // Location x/y in sector space.

		LocationInfo() : sector(0), group(0), x(0), y(0) {}
	};
	//////////////////////////////////////////////////////////////////////////
	// Pool Allocator.
	typedef stl::PoolAllocatorNoMT<sizeof(SectorGroup)>   SectorGroup_PoolAlloc;
	static SectorGroup_PoolAlloc*  g_SectorGroupPoolAlloc;
	typedef stl::PoolAllocatorNoMT<sizeof(SGridLocation)> GridLocation_PoolAlloc;
	static GridLocation_PoolAlloc* g_GridLocationPoolAlloc;
	//////////////////////////////////////////////////////////////////////////
private:
	typedef std::vector<IEntity*> EntityArray;

	SectorGroup*   AllocateGroup();
	SGridLocation* AllocateLocation();
	void           FreeGroup(LocationInfo& locInfo);
	// Converts world position into the sector grid coordinates.
	void           PositionToSectorCoords(const Vec3 worldPos, int& x, int& y);
	int            GetSectorIndex(int x, int y) const  { return y * m_numSectorsX + x; }
	int            GetGroupIndex(int gx, int gy) const { return gy * m_nWidth + gx; }
	SectorGroup*   GetSectorGroup(int x, int y);
	void           GetSector(const Vec3& worldPos, LocationInfo& locInfo);
	void           GetSector(int nSectorIndex, LocationInfo& locInfo);
	void           GetSector(int x, int y, LocationInfo& locInfo);
	void           SectorLink(SGridLocation* obj, LocationInfo& locInfo);
	void           SectorUnlink(SGridLocation* obj, LocationInfo& locInfo);

	//////////////////////////////////////////////////////////////////////////
	//void AddSectorGroupToQuery( int gx,int gy,int lx1,int ly1,const SPartitionGridQuery &query,EntityArray &entities );
	void AddSectorsToQuery(int groupX, int groupY, int lx1, int ly1, int lx2, int ly2, const SPartitionGridQuery& query, EntityArray& entities);

	//////////////////////////////////////////////////////////////////////////
	float         m_worldToSector;
	int           m_nWidth;
	int           m_nHeight;
	int           m_numSectorsX;
	int           m_numSectorsY;
	SectorGroup** m_pSectorGroups;

	bool          m_bResetting;

	// This is used to return entity list to caller in response to the query events.
	EntityArray m_entityCache;
};

//////////////////////////////////////////////////////////////////////////
inline void CPartitionGrid::PositionToSectorCoords(const Vec3 worldPos, int& x, int& y)
{
	float fx = worldPos.x;
	float fy = worldPos.y;
	if (fx < 0)
		fx = 0;
	if (fy < 0)
		fy = 0;
	//float fx = (worldPos.x + fabsf(worldPos.x))*0.5f
	x = fastftol_positive(fx * m_worldToSector);
	y = fastftol_positive(fy * m_worldToSector);
}

//////////////////////////////////////////////////////////////////////////
inline CPartitionGrid::SectorGroup* CPartitionGrid::GetSectorGroup(int x, int y)
{
	if (!m_pSectorGroups)
		return 0;

	x = SECTOR_TO_GROUP_COORD(x);
	y = SECTOR_TO_GROUP_COORD(y);
	assert(x >= 0 && x < m_nWidth && y >= 0 && y < m_nHeight);
	int nIndex = GetGroupIndex(x, y);
	SectorGroup* pGroup = m_pSectorGroups[nIndex];
	if (!pGroup)
	{
		pGroup = AllocateGroup();
		m_pSectorGroups[nIndex] = pGroup;
	}
	return pGroup;
}

//////////////////////////////////////////////////////////////////////////
inline void CPartitionGrid::GetSector(int x, int y, LocationInfo& locInfo)
{
	if (x >= 0 && x < m_numSectorsX && y >= 0 && y < m_numSectorsY)
	{
		SectorGroup* pGroup = GetSectorGroup(x, y);
		if (pGroup)
		{
			locInfo.group = pGroup;
			int lx = SECTOR_TO_LOCAL_COORD(x);
			int ly = SECTOR_TO_LOCAL_COORD(y);
			locInfo.sector = &(locInfo.group->sectors[lx][ly]);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
inline void CPartitionGrid::GetSector(int nSectorIndex, LocationInfo& locInfo)
{
	locInfo.x = nSectorIndex % m_numSectorsX;
	locInfo.y = nSectorIndex / m_numSectorsX;
	GetSector(locInfo.x, locInfo.y, locInfo);
}

//////////////////////////////////////////////////////////////////////////
inline void CPartitionGrid::GetSector(const Vec3& worldPos, LocationInfo& locInfo)
{
	int x, y;
	PositionToSectorCoords(worldPos, x, y);
	locInfo.x = x;
	locInfo.y = y;
	return GetSector(locInfo.x, locInfo.y, locInfo);
}

//////////////////////////////////////////////////////////////////////////
inline void* SGridLocation::operator new(size_t nSize)
{
	void* ptr = CPartitionGrid::g_GridLocationPoolAlloc->Allocate();
	if (ptr)
		memset(ptr, 0, nSize); // Clear objects memory.
	return ptr;
}
//////////////////////////////////////////////////////////////////////////
inline void SGridLocation::operator delete(void* ptr)
{
	if (ptr)
		CPartitionGrid::g_GridLocationPoolAlloc->Deallocate(ptr);
}
