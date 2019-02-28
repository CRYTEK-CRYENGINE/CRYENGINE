// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "PartitionGrid.h"
#include "Entity.h"
#include <CrySystem/CryUnitTest.h>

// Pool Allocator.
CPartitionGrid::SectorGroup_PoolAlloc* CPartitionGrid::g_SectorGroupPoolAlloc = NULL;
CPartitionGrid::GridLocation_PoolAlloc* CPartitionGrid::g_GridLocationPoolAlloc = NULL;

//////////////////////////////////////////////////////////////////////////
CPartitionGrid::CPartitionGrid()
{
	m_nWidth = 0;
	m_nHeight = 0;
	m_pSectorGroups = 0;
	m_worldToSector = 1;
	m_bResetting = 0;

	m_entityCache.reserve(1024);
	g_SectorGroupPoolAlloc = new SectorGroup_PoolAlloc;
	g_GridLocationPoolAlloc = new GridLocation_PoolAlloc;
}

//////////////////////////////////////////////////////////////////////////
CPartitionGrid::~CPartitionGrid()
{
	DeallocateGrid();
	delete g_SectorGroupPoolAlloc;
	delete g_GridLocationPoolAlloc;
}

//////////////////////////////////////////////////////////////////////////
void CPartitionGrid::DeallocateGrid()
{
	if (m_pSectorGroups)
	{
		delete[] m_pSectorGroups;
	}
	m_pSectorGroups = NULL;

	g_SectorGroupPoolAlloc->FreeMemoryForce();
	g_GridLocationPoolAlloc->FreeMemoryForce();
}

//////////////////////////////////////////////////////////////////////////
void CPartitionGrid::AllocateGrid(float fMetersX, float fMetersY)
{
	if (m_pSectorGroups)
		delete[] m_pSectorGroups;
	m_worldToSector = 1.0f / METERS_PER_SECTOR;
	m_nWidth = (int)(fMetersX / (SECTORS_PER_GROUP * METERS_PER_SECTOR));
	m_nHeight = (int)(fMetersY / (SECTORS_PER_GROUP * METERS_PER_SECTOR));
	m_numSectorsX = m_nWidth * (SECTORS_PER_GROUP);
	m_numSectorsY = m_nHeight * (SECTORS_PER_GROUP);
	m_pSectorGroups = new SectorGroup*[m_nWidth * m_nHeight];
	memset(m_pSectorGroups, 0, sizeof(SectorGroup*) * m_nWidth * m_nHeight);
}

//////////////////////////////////////////////////////////////////////////
CPartitionGrid::SectorGroup* CPartitionGrid::AllocateGroup()
{
	SectorGroup* pSectorGroup = new SectorGroup; // From pool
	memset(pSectorGroup, 0, sizeof(*pSectorGroup));
	return pSectorGroup;
}

//////////////////////////////////////////////////////////////////////////
SGridLocation* CPartitionGrid::AllocateLocation()
{
	SGridLocation* pLoc = new SGridLocation; // From Pool
	memset(pLoc, 0, sizeof(SGridLocation));
	return pLoc;
}

//////////////////////////////////////////////////////////////////////////
void CPartitionGrid::FreeLocation(SGridLocation* obj)
{
	if (obj && !m_bResetting)
	{
		LocationInfo locInfo;
		GetSector(obj->nSectorIndex, locInfo);
		if (locInfo.sector)
		{
			SectorUnlink(obj, locInfo);
			if (locInfo.group->nLocationCount <= 0)
				FreeGroup(locInfo);
		}
		delete obj;
	}
}

//////////////////////////////////////////////////////////////////////////
void CPartitionGrid::FreeGroup(LocationInfo& locInfo)
{
	delete locInfo.group;
	int nGroupIndex = GetGroupIndex(SECTOR_TO_GROUP_COORD(locInfo.x), SECTOR_TO_GROUP_COORD(locInfo.y));
	m_pSectorGroups[nGroupIndex] = 0;
}

//////////////////////////////////////////////////////////////////////////
void CPartitionGrid::SectorLink(SGridLocation* obj, LocationInfo& locInfo)
{
	if (locInfo.sector->first)
	{
		// Add to the beginning of the sector list.
		SGridLocation* head = locInfo.sector->first;
		obj->prev = 0;
		obj->next = head;
		head->prev = obj;
		locInfo.sector->first = obj;
	}
	else
	{
		locInfo.sector->first = obj;
		obj->prev = 0;
		obj->next = 0;
	}
	obj->nSectorIndex = GetSectorIndex(locInfo.x, locInfo.y);
	locInfo.group->nLocationCount++;
}

//////////////////////////////////////////////////////////////////////////
void CPartitionGrid::SectorUnlink(SGridLocation* obj, LocationInfo& locInfo)
{
	bool bRemoved = false;
	if (locInfo.sector && obj == locInfo.sector->first) // if head of list.
	{
		bRemoved = true;
		locInfo.sector->first = obj->next;
		if (locInfo.sector->first)
		{
			locInfo.sector->first->prev = 0;
		}
	}
	else
	{
		//assert( obj->prev != 0 );
		if (obj->prev)
		{
			obj->prev->next = obj->next;
			bRemoved = true;
		}
		if (obj->next)
		{
			bRemoved = true;
			obj->next->prev = obj->prev;
		}
	}
	if (bRemoved)
	{
		locInfo.group->nLocationCount--;
		assert(locInfo.group->nLocationCount >= 0);
	}
	obj->prev = 0;
	obj->next = 0;
}

//////////////////////////////////////////////////////////////////////////
SGridLocation* CPartitionGrid::Rellocate(SGridLocation* obj, const Vec3& newPos, CEntity* pEntity)
{
	CRY_PROFILE_FUNCTION(PROFILE_ENTITY);

	if (m_bResetting)
	{
		return 0;
	}

	if (obj)
	{
		LocationInfo from, to;
		GetSector(obj->nSectorIndex, from);
		GetSector(newPos, to);
		if (from.sector != to.sector)
		{
			// Relink object between sectors.
			SectorUnlink(obj, from);
			if (to.sector)
			{
				SectorLink(obj, to);
				if (from.group && from.group->nLocationCount <= 0)
					FreeGroup(from);
			}
			else
			{
				FreeLocation(obj);
				obj = 0;
			}
		}
	}
	else if (fabs(newPos.x) > 0.1f || fabs(newPos.y) > 0.1f)
	{
		// Allocate a new sector location.
		LocationInfo to;
		GetSector(newPos, to);
		if (to.sector)
		{
			obj = AllocateLocation();
			obj->pEntity = pEntity;
			if (pEntity)
			{
				obj->pEntityClass = pEntity->GetClass();
				obj->nEntityFlags = pEntity->GetFlags();
			}
			SectorLink(obj, to);
		}
	}
	return obj;
}

//////////////////////////////////////////////////////////////////////////
void CPartitionGrid::AddSectorsToQuery(int groupX, int groupY, int lx1, int ly1, int lx2, int ly2, const SPartitionGridQuery& query, EntityArray& entities)
{
	int nGroupIndex = GetGroupIndex(groupX, groupY);
	assert(nGroupIndex >= 0 && nGroupIndex < m_nWidth * m_nHeight);
	SectorGroup* pGroup = m_pSectorGroups[nGroupIndex];
	if (pGroup && pGroup->nLocationCount > 0)
	{
		for (int y = ly1; y <= ly2; y++)
		{
			for (int x = lx1; x <= lx2; x++)
			{
				for (SGridLocation* pLoc = pGroup->sectors[x][y].first; pLoc != NULL; pLoc = pLoc->next)
				{
					if (((query.nEntityFlags & pLoc->nEntityFlags) == query.nEntityFlags) &&
					    (query.pEntityClass == 0 || (query.pEntityClass == pLoc->pEntityClass)))
					{
						entities.push_back(pLoc->pEntity);
					}
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CPartitionGrid::GetEntitiesInBox(SPartitionGridQuery& query)
{
	m_entityCache.resize(0);
	query.pEntities = &m_entityCache;

	if (m_bResetting)
	{
		return;
	}

	int x1, y1, x2, y2;
	PositionToSectorCoords(query.aabb.min, x1, y1);
	PositionToSectorCoords(query.aabb.max, x2, y2);

	if (!m_pSectorGroups)
		return;

	x1 = clamp_tpl(x1, 0, m_numSectorsX - 1);
	y1 = clamp_tpl(y1, 0, m_numSectorsY - 1);
	x2 = clamp_tpl(x2, 0, m_numSectorsX - 1);
	y2 = clamp_tpl(y2, 0, m_numSectorsY - 1);

	// Calculate group rectangle.
	int gx1, gy1, gx2, gy2;
	gx1 = SECTOR_TO_GROUP_COORD(x1);
	gy1 = SECTOR_TO_GROUP_COORD(y1);
	gx2 = SECTOR_TO_GROUP_COORD(x2);
	gy2 = SECTOR_TO_GROUP_COORD(y2);

	// calculate local coordinates inside the group.
	int inGroupX1 = SECTOR_TO_LOCAL_COORD(x1);
	int inGroupY1 = SECTOR_TO_LOCAL_COORD(y1);
	int inGroupX2 = SECTOR_TO_LOCAL_COORD(x2);
	int inGroupY2 = SECTOR_TO_LOCAL_COORD(y2);

	for (int gy = gy1; gy <= gy2; gy++)
	{
		for (int gx = gx1; gx <= gx2; gx++)
		{
			int lx1 = 0;
			int ly1 = 0;
			int lx2 = SECTORS_PER_GROUP - 1;
			int ly2 = SECTORS_PER_GROUP - 1;

			if (gx == gx1)
				lx1 = inGroupX1;
			if (gx == gx2)
				lx2 = inGroupX2;
			if (gy == gy1)
				ly1 = inGroupY1;
			if (gy == gy2)
				ly2 = inGroupY2;

			// Add all entities of the whole group.
			AddSectorsToQuery(gx, gy, lx1, ly1, lx2, ly2, query, m_entityCache);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void CPartitionGrid::GetMemoryUsage(ICrySizer* pSizer) const
{
	SIZER_COMPONENT_NAME(pSizer, "Partition Grid");
	pSizer->AddObject(m_pSectorGroups, sizeof(SectorGroup*) * m_nWidth * m_nHeight);
	pSizer->AddObject(CPartitionGrid::g_SectorGroupPoolAlloc);
	pSizer->AddObject(CPartitionGrid::g_GridLocationPoolAlloc);
}

//////////////////////////////////////////////////////////////////////////
void CPartitionGrid::Reset()
{
	stl::free_container(m_entityCache);
	DeallocateGrid();
	m_bResetting = false;
}

void CPartitionGrid::BeginReset()
{
	// When we beging reset we ignore all calls to the proximity system until call to the Reset.
	m_bResetting = true;
}

//////////////////////////////////////////////////////////////////////////
// Unit Testing of Patritition grid
//////////////////////////////////////////////////////////////////////////
#ifdef CRY_UNIT_TESTING

CRY_UNIT_TEST(CUT_PartitionGrid)
{
	CPartitionGrid* pGrid = GetIEntitySystem()->GetPartitionGrid();
	float fMetersX = 1000;
	float fMetersY = 1000;
	pGrid->AllocateGrid(fMetersX, fMetersY);

	CRY_UNIT_TEST_ASSERT(pGrid->m_nWidth == (int)(fMetersX / (SECTORS_PER_GROUP * METERS_PER_SECTOR)));
	CRY_UNIT_TEST_ASSERT(pGrid->m_nHeight == (int)(fMetersY / (SECTORS_PER_GROUP * METERS_PER_SECTOR)));

	SGridLocation* obj[5];
	ZeroStruct(obj);
	obj[0] = pGrid->Rellocate(obj[0], Vec3(500, 500, 0), 0);
	obj[1] = pGrid->Rellocate(obj[1], Vec3(501, 501, 0), 0);
	obj[2] = pGrid->Rellocate(obj[2], Vec3(503, 503, 0), 0);
	obj[3] = pGrid->Rellocate(obj[3], Vec3(600, 600, 0), 0);
	obj[4] = pGrid->Rellocate(obj[4], Vec3(700, 500, 0), 0);

	SPartitionGridQuery query;
	query.aabb = AABB(Vec3(400, 400, -1), Vec3(620, 620, 1));

	pGrid->GetEntitiesInBox(query);
	CRY_UNIT_TEST_ASSERT(query.pEntities && query.pEntities->size() == 4);   // Must find 4 objects inside.

	obj[0] = pGrid->Rellocate(obj[0], Vec3(-100, -100, 0), 0);
	pGrid->GetEntitiesInBox(query);
	CRY_UNIT_TEST_ASSERT(query.pEntities && query.pEntities->size() == 3);   // Must find 3 objects inside.

	obj[1] = pGrid->Rellocate(obj[1], Vec3(10000, 10000, 0), 0);
	pGrid->GetEntitiesInBox(query);
	CRY_UNIT_TEST_ASSERT(query.pEntities && query.pEntities->size() == 2);   // Must find 2 objects inside.

	for (int i = 0; i < 5; i++)
		pGrid->FreeLocation(obj[i]);

	pGrid->DeallocateGrid();
}

#endif //CRY_UNIT_TESTING
