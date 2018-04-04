// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

// -------------------------------------------------------------------------
//  File name:   SegmentedWorld.cpp
//  Version:     v1.00
//  Created:     4/11/2011 by Allen Chen
//  Compilers:   Visual Studio.NET
//  Description:
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "SegmentedWorld.h"
#include "IVehicleSystem.h"
#include <CryRenderer/IRenderAuxGeom.h>
#include "CryActionCVars.h"

#define MINIMUM_GRID_SIZE 3
#define MAXIMUM_GRID_SIZE 32

string CSegmentedWorld::s_levelName;
XmlNodeRef CSegmentedWorld::s_levelSurfaceTypes;
XmlNodeRef CSegmentedWorld::s_levelObjects;

CSegmentedWorld::CSegmentedWorld()
	: m_segmentSizeMeters(0)
	, m_invSegmentSizeMeters(0.0f)
	, m_pPoolAllocator(nullptr)
	, m_bInitialWorldReady(false)
{
	if (gEnv->pGameFramework)
	{
		CRY_ASSERT_MESSAGE(gEnv->pGameFramework->GetILevelSystem(), "Unable to register as levelsystem listener!");
		if (gEnv->pGameFramework->GetILevelSystem())
		{
			gEnv->pGameFramework->GetILevelSystem()->AddListener(this);
		}
	}
}

CSegmentedWorld::~CSegmentedWorld()
{
	SAFE_DELETE(m_pPoolAllocator);

	if (gEnv->pGameFramework)
	{
		if (gEnv->pGameFramework->GetILevelSystem())
			gEnv->pGameFramework->GetILevelSystem()->RemoveListener(this);
	}
}

void CSegmentedWorld::OnLoadingStart(ILevelInfo* pLevel)
{
	if (!pLevel)
		return;

	Reset();

	string levelPath = string(pLevel->GetPath()) + "/";
	XmlNodeRef levelInfoNode = GetISystem()->LoadXmlFromFile(levelPath + "segmentedlevelinfo.xml");
	if (levelInfoNode)
	{
		bool bEntitiesUseGUIDs = false;
		levelInfoNode->getAttr("SegmentedWorldSizeMeters", m_segmentSizeMeters);
		levelInfoNode->getAttr("SegmentedWorldMinX", m_segmentsMin.x);
		levelInfoNode->getAttr("SegmentedWorldMinY", m_segmentsMin.y);
		levelInfoNode->getAttr("SegmentedWorldMaxX", m_segmentsMax.x);
		levelInfoNode->getAttr("SegmentedWorldMaxY", m_segmentsMax.y);
		levelInfoNode->getAttr("EntitiesUseGUIDs", bEntitiesUseGUIDs);

		bool bInvalidInput = m_segmentsMin.x > m_segmentsMax.x || m_segmentsMin.y > m_segmentsMax.y;
#ifdef DEDICATED_SERVER
		bInvalidInput |= bEntitiesUseGUIDs;
#else
		if (gEnv->bMultiplayer)
			bInvalidInput |= bEntitiesUseGUIDs;
#endif

		if (bInvalidInput)
		{
			CRY_ASSERT(false);
			GameWarning("Invalid segmented world bounds info");
			gEnv->p3DEngine->SetSegmentsManager(0);
			return;
		}

		s_levelName = pLevel->GetName();
		s_levelName.MakeLower();

		XmlNodeRef xmlLevelData = gEnv->pSystem->LoadXmlFromFile(levelPath + "leveldata.xml");
		s_levelSurfaceTypes = xmlLevelData->findChild("SurfaceTypes");

		XmlNodeRef xmlMission = gEnv->pSystem->LoadXmlFromFile(levelPath + pLevel->GetDefaultGameType()->xmlFile);
		s_levelObjects = xmlMission->findChild("Objects");

		gEnv->p3DEngine->SetSegmentsManager(this);
		gEnv->pEntitySystem->SetEntitiesUseGUIDs(bEntitiesUseGUIDs);

		m_invSegmentSizeMeters = 1.0f / m_segmentSizeMeters;

		m_neededSegmentsMin.x = 0;
		m_neededSegmentsMin.y = 0;

		int gridSize = CCryActionCVars::Get().sw_gridSize;
		m_neededSegmentsMax.x = gridSize;
		m_neededSegmentsMax.y = gridSize;

		m_neededSegmentsHalfSizeMeters.x = (m_neededSegmentsMax.x - m_neededSegmentsMin.x) * m_segmentSizeMeters * 0.5f;
		m_neededSegmentsHalfSizeMeters.y = (m_neededSegmentsMax.y - m_neededSegmentsMin.y) * m_segmentSizeMeters * 0.5f;

		assert(m_pPoolAllocator->GetTotalMemory().nAlloc == 0);
	}
	else
	{
		gEnv->p3DEngine->SetSegmentsManager(0);
		gEnv->pEntitySystem->SetEntitiesUseGUIDs(false);
	}
}

void CSegmentedWorld::OnLoadingComplete(ILevelInfo* pLevel)
{
	if (!pLevel || !s_levelObjects)
		return;

#ifndef DEDICATED_SERVER
	// set all the global entities invisible
	EntityId id = 0;
	EntityGUID guid;
	IEntity* pEntity = NULL;

	for (int i = 0; i < s_levelObjects->getChildCount(); i++)
	{
		XmlNodeRef xmlEntity = s_levelObjects->getChild(i);
		if (xmlEntity->getAttr("EntityGuid", guid))
		{
			id = gEnv->pEntitySystem->FindEntityByGuid(guid);
			pEntity = gEnv->pEntitySystem->GetEntity(id);

			if (pEntity)
			{
				pEntity->Invisible(true);
			}
		}
	}
#endif
}

void CSegmentedWorld::OnUnloadComplete(ILevelInfo* pLevel)
{
	if (!pLevel)
		return;

	gEnv->p3DEngine->SetSegmentsManager(0);
}

void CSegmentedWorld::Reset()
{
	int& gridSize = CCryActionCVars::Get().sw_gridSize;
	gridSize = CLAMP(gridSize, MINIMUM_GRID_SIZE, MAXIMUM_GRID_SIZE);

	m_arrSegments.clear();

	m_bInitialWorldReady = false;
	m_accSegmentOffset = Vec2i(ZERO);
	m_segmentsMin = Vec2i(ZERO);
	m_segmentsMax = Vec2i(ZERO);

	SAFE_DELETE(m_pPoolAllocator);
	int poolSize = gridSize * gridSize * 2;
	m_pPoolAllocator = new stl::TPoolAllocator<CSegmentNode>(stl::FHeap().PageSize(poolSize));

	s_levelSurfaceTypes = NULL;
	s_levelObjects = NULL;
}

CSegmentNode* CSegmentedWorld::PreloadSegment(int wx, int wy)
{
	CSegmentNode* pSeg = NULL;

	if (pSeg = FindSegmentByWorldCoords(wx, wy))
		return pSeg;

	assert(m_pPoolAllocator);
	void* pAlloc = m_pPoolAllocator->Allocate();

	pSeg = new(pAlloc) CSegmentNode(this, wx, wy, wx - m_neededSegmentsMin.x, wy - m_neededSegmentsMin.y);

	assert(pSeg);
	m_arrSegments.push_back(pSeg);

	return pSeg;
}

Vec2i CalcSWMove(int lx, int ly, const Vec2i& vMin, const Vec2i& vMax, int iThreshold)
{
	Vec2i vCenter((vMax.x - vMin.x) / 2, (vMax.y - vMin.y) / 2);
	Vec2i vThreshold(iThreshold, iThreshold);
	Vec2i vMinOK = vCenter - vThreshold;
	Vec2i vMaxOK = vCenter + vThreshold;

	Vec2i vOfs;

	// handle odd sized rectangles as well
	bool xOdd = (vMax.x - vMin.x) % 2 == 1;
	bool yOdd = (vMax.y - vMin.y) % 2 == 1;

	if (xOdd && lx <= vMinOK.x)
		vOfs.x = lx - vMinOK.x - 1; // negative, move left
	else if (!xOdd && lx < vMinOK.x)
		vOfs.x = lx - vMinOK.x; // negative, move left
	else if (lx >= vMaxOK.x)
		vOfs.x = lx - vMaxOK.x + 1; // positive, move right
	else
		vOfs.x = 0;

	if (yOdd && ly <= vMinOK.y)
		vOfs.y = ly - vMinOK.y - 1; // negative, move left
	else if (!yOdd && ly < vMinOK.y)
		vOfs.y = ly - vMinOK.y; // negative, move up
	else if (ly >= vMaxOK.y)
		vOfs.y = ly - vMaxOK.y + 1; // positive, move down
	else
		vOfs.y = 0;

	return vOfs;
}

void CSegmentedWorld::UpdateActiveSegmentBound(const Vec2i& offset)
{
	m_neededSegmentsMin += offset;
	m_neededSegmentsMax += offset;
	m_accSegmentOffset += offset;

	Vec2i activeSegmentSize = m_neededSegmentsMax - m_neededSegmentsMin;
	int xAdj = (activeSegmentSize.x % 2 == 1) ? 0 : 1;
	int yAdj = (activeSegmentSize.y % 2 == 1) ? 0 : 1;
	m_neededSegmentsCenter.x = m_neededSegmentsMin.x + activeSegmentSize.x / 2 - xAdj;
	m_neededSegmentsCenter.y = m_neededSegmentsMin.y + activeSegmentSize.y / 2 - yAdj;
}

bool CSegmentedWorld::LoadInitialWorld(const Vec3& pos)
{
	if (m_bInitialWorldReady)
		return true;

	Vec2i posSS = WorldToGridPosition(pos);
	Vec2i vOfs = CalcSWMove(posSS.x, posSS.y, m_neededSegmentsMin, m_neededSegmentsMax, 1);

	UpdateActiveSegmentBound(vOfs);

	ForceLoadSegments(m_neededSegmentsMin, m_neededSegmentsMax, ISegmentsManager::slfAll);
	AdjustGlobalObjects();

	return true;
}

void CSegmentedWorld::ForceLoadSegments(const Vec2i& segmentsMin, const Vec2i& segmentsMax, unsigned int flags)
{
	assert(segmentsMin.x < segmentsMax.x && segmentsMin.y < segmentsMax.y);

	gEnv->p3DEngine->SetSegmentOperationInProgress(true);

	for (int wy = segmentsMin.y; wy < segmentsMax.y; ++wy)
	{
		for (int wx = segmentsMin.x; wx < segmentsMax.x; ++wx)
		{
			CSegmentNode* pSeg = PreloadSegment(wx, wy);
			pSeg->ForceLoad(flags);
		}
	}

	// CS - update connections..
	gEnv->p3DEngine->GetIVisAreaManager()->UpdateConnections();
	gEnv->p3DEngine->SetSegmentOperationInProgress(false);
}

void CSegmentedWorld::DeleteSegment(int wx, int wy)
{
	for (int i = 0; i < m_arrSegments.size(); ++i)
	{
		CSegmentNode* pSeg = m_arrSegments[i];
		if (pSeg->m_wx == wx && pSeg->m_wy == wy)
		{
			m_arrSegments.erase(m_arrSegments.begin() + i);

			pSeg->~CSegmentNode();
			m_pPoolAllocator->Deallocate(pSeg);
			return;
		}
	}
}

bool CSegmentedWorld::NeedToUpdateSegment(int wx, int wy)
{
	CSegmentNode* pSeg = FindSegmentByWorldCoords(wx, wy);
	if (!pSeg) return true;

	return !pSeg->StreamingFinished();
}

bool CSegmentedWorld::MoveToSegment(int x, int y)
{
	Vec3 wPos = GridToWorldPosition(x, y);
	char str[256];
	cry_sprintf(str, "goto %f %f %f", wPos.x, wPos.y, wPos.z);
	gEnv->pConsole->ExecuteString(str, true);

	return true;
}

Vec2i CSegmentedWorld::SelectSegmentToUpdate(const Vec2i& focalPointWC)
{
	// Select the segment closest to the player to update
	Vec2i segmentToUpdateWC = Vec2i(0, 0);
	int segmentToUpdateDistance = INT_MAX;
	for (int x = m_neededSegmentsMin.x; x < m_neededSegmentsMax.x; ++x)
	{
		for (int y = m_neededSegmentsMin.y; y < m_neededSegmentsMax.y; ++y)
		{
			if (!NeedToUpdateSegment(x, y)) continue;
			Vec2i wc = Vec2i(x, y);
			int distance = (focalPointWC - wc).GetLength2();
			if (distance < segmentToUpdateDistance)
			{
				segmentToUpdateWC = wc;
				segmentToUpdateDistance = distance;
			}
		}
		// Also consider all segments outside the boundaries
		for (int i = 0; i < m_arrSegments.size(); ++i)
		{
			Vec2i wc = Vec2i(m_arrSegments[i]->m_wx, m_arrSegments[i]->m_wy);
			if (wc.x < m_neededSegmentsMin.x || wc.x >= m_neededSegmentsMax.x ||
			    wc.y < m_neededSegmentsMin.y || wc.y >= m_neededSegmentsMax.y)
			{
				int distance = (focalPointWC - wc).GetLength2();
				if (distance < segmentToUpdateDistance)
				{
					segmentToUpdateWC = wc;
					segmentToUpdateDistance = distance;
				}
			}
		}
	}
	return segmentToUpdateWC;
}

void CSegmentedWorld::AdjustGlobalObjects()
{
	EntityId id = 0;
	EntityGUID guid;
	IEntity* pEntity = NULL;
	Vec3 offset = GridToWorldPosition(-m_neededSegmentsMin);

	for (int i = 0; i < s_levelObjects->getChildCount(); i++)
	{
		XmlNodeRef xmlEntity = s_levelObjects->getChild(i);
		if (xmlEntity->getAttr("EntityGuid", guid))
		{
			id = gEnv->pEntitySystem->FindEntityByGuid(guid);
			pEntity = gEnv->pEntitySystem->GetEntity(id);

			if (pEntity)
			{
				pEntity->SetPos(pEntity->GetPos() + offset);
			}
		}
	}
}

void CSegmentedWorld::PostUpdate()
{
	if (gEnv->p3DEngine->GetSegmentsManager() != this)
		return;

	gEnv->p3DEngine->GetIVisAreaManager()->ReleaseInactiveSegments();
	gEnv->p3DEngine->GetITerrain()->ReleaseInactiveSegments();

	// correct the player's position after specified by flownode
	if (!m_bInitialWorldReady)
	{
		IActor* pClientPlayer = gEnv->pGameFramework->GetClientActor();
		if (!pClientPlayer || pClientPlayer->IsDead())
			return;

		// set player position as our world focal point
		m_worldFocalPoint = pClientPlayer->GetEntity()->GetWorldPos();

		GetISystem()->GetISystemEventDispatcher()->OnSystemEvent(ESYSTEM_EVENT_SW_FORCE_LOAD_START, 0, 0);

		LoadInitialWorld(m_worldFocalPoint);

		Vec3 offset = GridToWorldPosition(-m_neededSegmentsMin);
		pClientPlayer->GetEntity()->SetPos(m_worldFocalPoint + offset);

		GetISystem()->GetISystemEventDispatcher()->OnSystemEvent(ESYSTEM_EVENT_SW_FORCE_LOAD_END, 0, 0);

		m_bInitialWorldReady = true;
	}
}

void CSegmentedWorld::Update()
{
	if (gEnv->p3DEngine->GetSegmentsManager() != this)
		return;

	IActor* pClientPlayer = gEnv->pGameFramework->GetClientActor();
	if (!pClientPlayer || pClientPlayer->IsDead() || !m_bInitialWorldReady)
		return;

	// set player position as our world focal point
	m_worldFocalPoint = pClientPlayer->GetEntity()->GetWorldPos();

	// player position in grid space
#if USE_RELATIVE_COORD
	m_gridFocalPointLC = WorldToGridPosition(m_worldFocalPoint);
	m_gridFocalPointWC = m_neededSegmentsMin + m_gridFocalPointLC;
#else
	m_gridFocalPointWC = WorldToGridPosition(m_worldFocalPoint);
	m_gridFocalPointLC = m_gridFocalPointWC - m_accSegmentOffset;
#endif

	int iThreshold = 1;//gEnv->pConsole->GetCVar("sw_autoMove")->GetIVal();
	if (iThreshold > 0)
	{
		Vec2i vOfs = CalcSWMove(m_gridFocalPointLC.x, m_gridFocalPointLC.y, m_neededSegmentsMin, m_neededSegmentsMax, iThreshold);

		if (vOfs.x < -1) vOfs.x = -1;
		if (vOfs.x > +1) vOfs.x = +1;
		if (vOfs.y < -1) vOfs.y = -1;
		if (vOfs.y > +1) vOfs.y = +1;

		UpdateActiveSegmentBound(vOfs);

#if USE_RELATIVE_COORD
		// offset position for existing objects
		Vec3 offset((float)(-vOfs.x * m_segmentSizeMeters), (float)(-vOfs.y * m_segmentSizeMeters), 0);
		gEnv->pRenderer->OffsetPosition(offset);
		if (vOfs.x != 0 || vOfs.y != 0)
		{
			for (int i = 0; i < m_arrSegments.size(); i++)
			{
				CSegmentNode* pSeg = m_arrSegments[i];
				pSeg->SetLocalCoords(pSeg->m_wx - m_neededSegmentsMin.x, pSeg->m_wy - m_neededSegmentsMin.y);
			}

			gEnv->p3DEngine->GetIVisAreaManager()->OffsetPosition(offset);

			Vec3 entPos;
			IEntityItPtr pIt = gEnv->pEntitySystem->GetEntityIterator();
			while (IEntity* pEnt = pIt->Next())
			{
				if (pEnt->GetParent() == 0)
				{
					entPos = pEnt->GetPos() + offset;
					float xdiff = entPos.x - m_neededSegmentsHalfSizeMeters.x;
					float ydiff = entPos.y - m_neededSegmentsHalfSizeMeters.y;
					if (pEnt->GetPhysics() && (xdiff > m_neededSegmentsHalfSizeMeters.x || xdiff < -m_neededSegmentsHalfSizeMeters.x
					                           || ydiff > m_neededSegmentsHalfSizeMeters.y || ydiff < -m_neededSegmentsHalfSizeMeters.y))
					{
						pEnt->Invisible(true);
					}
					pEnt->SetPos(entPos, 0, true);
				}

				if (IVehicle* pVehicle = gEnv->pGameFramework->GetIVehicleSystem()->GetVehicle(pEnt->GetId()))
					pVehicle->OffsetPosition(offset);
			}

			gEnv->p3DEngine->OffsetPosition(offset);

			// Update focal points
			m_worldFocalPoint = pClientPlayer->GetEntity()->GetWorldPos();
			m_gridFocalPointLC = WorldToGridPosition(m_worldFocalPoint);
			m_gridFocalPointWC = m_neededSegmentsMin + m_gridFocalPointLC;

			GetISystem()->GetISystemEventDispatcher()->OnSystemEvent(ESYSTEM_EVENT_SW_SHIFT_WORLD, (UINT_PTR)&offset, 0);
		}
#endif
	}

	gEnv->p3DEngine->SetSegmentOperationInProgress(true);
	Vec2i wc = SelectSegmentToUpdate(m_gridFocalPointWC);
	UpdateSegment(wc.x, wc.y);
	gEnv->p3DEngine->SetSegmentOperationInProgress(false);

	// display debug info on screen
	DrawDebugInfo();
}

void CSegmentedWorld::UpdateSegment(int wx, int wy)
{
	CSegmentNode* pSeg = FindSegmentByWorldCoords(wx, wy);

	// check if segment is out of active boundaries
	if (wx < m_neededSegmentsMin.x || wx >= m_neededSegmentsMax.x ||
	    wy < m_neededSegmentsMin.y || wy >= m_neededSegmentsMax.y)
	{
		if (!pSeg)
			return;

		DeleteSegment(wx, wy);
		pSeg->SetStatus(ESEG_IDLE);
	}
	else
	{
		if (!pSeg)
		{
			// allocate segment if not exit
			pSeg = PreloadSegment(wx, wy);
			pSeg->SetStatus(ESEG_INIT);
		}
		else
		{
			// request to start streaming
			pSeg->SetStatus(ESEG_STREAM);
		}
	}
	assert(pSeg);
	pSeg->Update();
}

CSegmentNode* CSegmentedWorld::FindSegNodeByWorldPos(const Vec3& pos)
{
	Vec2i ptSS = WorldToGridPosition(pos);
#if USE_RELATIVE_COORD
	CSegmentNode* pSeg = FindSegmentByLocalCoords(ptSS.x, ptSS.y);
#else
	CSegmentNode* pSeg = FindSegmentByWorldCoords(ptSS.x, ptSS.y);
#endif

	return pSeg;
}

bool CSegmentedWorld::FindSegment(ITerrain* pTerrain, const Vec3& pt, int& nSID)
{
#if USE_RELATIVE_COORD
	Vec2i ptSS = WorldToGridPosition(pt);
	CSegmentNode* pSeg = FindSegmentByLocalCoords(ptSS.x, ptSS.y);
	if (pSeg && pSeg->OffsetApplied(pSeg->m_wx - m_neededSegmentsMin.x, pSeg->m_wy - m_neededSegmentsMin.y))
	{
		nSID = pSeg->m_nSID;
	}
	else
	{
		nSID = -1;
	}
#else
	Vec2i ptSS = WorldToGridPosition(pt);
	CSegmentNode* pSeg = FindSegmentByWorldCoords(ptSS.x, ptSS.y);
	if (pSeg)
	{
		nSID = pSeg->m_nSID;
	}
	else
	{
		nSID = -1;
	}
#endif
	return true;
}

bool CSegmentedWorld::FindSegmentCoordByID(int nSID, int& x, int& y)
{
	for (int i = 0; i < m_arrSegments.size(); ++i)
		if (m_arrSegments[i]->m_nSID == nSID)
		{
			x = m_arrSegments[i]->m_wx;
			y = m_arrSegments[i]->m_wy;
			return true;
		}
	return false;
}

bool CSegmentedWorld::IsInsideSegmentedWorld(const Vec3& pt)
{
	return pt.x >= 0 && pt.y >= 0 &&
	       pt.y < (m_neededSegmentsMax.x - m_neededSegmentsMin.x) * m_segmentSizeMeters &&
	       pt.x < (m_neededSegmentsMax.y - m_neededSegmentsMin.y) * m_segmentSizeMeters;
}

void CSegmentedWorld::GetTerrainSizeInMeters(int& x, int& y)
{
	x = (m_neededSegmentsMax.x - m_neededSegmentsMin.x) * m_segmentSizeMeters;
	y = (m_neededSegmentsMax.y - m_neededSegmentsMin.y) * m_segmentSizeMeters;
}

CSegmentNode* CSegmentedWorld::FindSegmentByID(int nSID)
{
	for (int i = 0; i < m_arrSegments.size(); ++i)
		if (m_arrSegments[i]->m_nSID == nSID)
			return m_arrSegments[i];
	return 0;
}

CSegmentNode* CSegmentedWorld::FindSegmentByWorldCoords(int wx, int wy)
{
	for (int i = 0; i < m_arrSegments.size(); ++i)
	{
		CSegmentNode* pSeg = m_arrSegments[i];
		if (pSeg->m_wx == wx && pSeg->m_wy == wy)
			return pSeg;
	}
	return 0;
}

CSegmentNode* CSegmentedWorld::FindSegmentByLocalCoords(int lx, int ly)
{
	for (int i = 0; i < m_arrSegments.size(); ++i)
	{
		CSegmentNode* pSeg = m_arrSegments[i];
		if (pSeg->m_lx == lx && pSeg->m_ly == ly)
			return pSeg;
	}
	return 0;
}

bool CSegmentedWorld::PushEntityToSegment(unsigned int id, bool bLocal)
{
	bool bRet = false;
	IEntity* pEntity = gEnv->pEntitySystem->GetEntity(id);
	if (pEntity)
	{
		Vec3 pos = pEntity->GetWorldPos();
		CSegmentNode* pSegNode = FindSegNodeByWorldPos(pos);
		if (pSegNode)
		{
			if (bLocal)
				pSegNode->m_localEntities.push_back(pEntity);
			else
				pSegNode->m_globalEntities.push_back(pEntity);

			bRet = true;
		}

	}

	return bRet;
}

void CSegmentedWorld::DrawDebugInfo()
{
	int debugInfo = CCryActionCVars::Get().sw_debugInfo;
	if (!debugInfo || gEnv->IsDedicated())
		return;

	int width = gEnv->pRenderer->GetWidth();
	int height = gEnv->pRenderer->GetHeight();
	GetISystem()->GetIRenderer()->Set2DMode(true, width, height);

	static float debugGridSize = 16;
	if (m_segmentsMax.x > 32)
		debugGridSize = 8;

	Vec2 debugTextOrigin(50, 50);
	Vec2 debugGridOrigin(debugTextOrigin.x, debugTextOrigin.y + m_segmentsMax.y * debugGridSize);

	DrawSegmentGrid(debugGridOrigin, debugGridSize);
	if (debugInfo == 2)
		PrintSegmentInfo(debugTextOrigin, debugGridSize);
	else if (debugInfo == 3)
		PrintMemoryInfo(debugTextOrigin, debugGridSize);

	GetISystem()->GetIRenderer()->Set2DMode(false, width, height);
}

void CSegmentedWorld::DrawSegmentGrid(const Vec2& origin, float size)
{
	IRenderAuxGeom* pAux = GetISystem()->GetIRenderer()->GetIRenderAuxGeom();

	float sx, sy, thickness = 1;
	ColorB color;
	for (int x = m_segmentsMin.x; x <= m_segmentsMax.x; ++x)
	{
		for (int y = m_segmentsMin.y; y <= m_segmentsMax.y; ++y)
		{
			CSegmentNode* pSeg = FindSegmentByWorldCoords(x, y);
			color =
			  pSeg == 0 ? Col_Gray :
			  !pSeg->StreamingSucceeded() ? Col_Yellow :
			  Col_Green;

			sx = origin.x + (float)(x - m_segmentsMin.x) * size;
			sy = origin.y - (float)(y - m_segmentsMin.y) * size;

			pAux->DrawLine(Vec3(sx, sy, 0), color, Vec3(sx + size - 1, sy, 0), color, thickness);
			pAux->DrawLine(Vec3(sx + size - 1, sy, 0), color, Vec3(sx + size - 1, sy - (size - 1), 0), color, thickness);
			pAux->DrawLine(Vec3(sx + size - 1, sy - (size - 1), 0), color, Vec3(sx, sy - (size - 1), 0), color, thickness);
			pAux->DrawLine(Vec3(sx, sy - (size - 1), 0), color, Vec3(sx, sy, 0), color, thickness);
		}
	}

	bool xOdd = (m_neededSegmentsMax.x - m_neededSegmentsMin.x) % 2 == 1;
	bool yOdd = (m_neededSegmentsMax.y - m_neededSegmentsMin.y) % 2 == 1;
	int xThreshold = xOdd ? 1 : 2;
	int yThreshold = yOdd ? 1 : 2;
	color = Col_Blue;
	thickness = 2;
	sx = origin.x + (float)(m_neededSegmentsCenter.x - m_segmentsMin.x) * size;
	sy = origin.y - (float)(m_neededSegmentsCenter.y - m_segmentsMin.y) * size;
	pAux->DrawLine(Vec3(sx, sy, 0), color, Vec3(sx + size * xThreshold - 1, sy, 0), color, thickness);
	pAux->DrawLine(Vec3(sx + size * xThreshold - 1, sy, 0), color, Vec3(sx + size * xThreshold - 1, sy - (size * yThreshold - 1), 0), color, thickness);
	pAux->DrawLine(Vec3(sx + size * xThreshold - 1, sy - (size * yThreshold - 1), 0), color, Vec3(sx, sy - (size * yThreshold - 1), 0), color, thickness);
	pAux->DrawLine(Vec3(sx, sy - (size * xThreshold - 1), 0), color, Vec3(sx, sy, 0), color, 2);

	Vec3 playerGridPos = WorldToScreenPosition(m_worldFocalPoint);
	playerGridPos.x -= m_segmentsMin.x;
	playerGridPos.y -= m_segmentsMin.y;
#if USE_RELATIVE_COORD
	playerGridPos.x += m_neededSegmentsMin.x;
	playerGridPos.y += m_neededSegmentsMin.y;
#endif
	playerGridPos *= size;
	playerGridPos.y = -playerGridPos.y;

	playerGridPos += Vec3(origin.x, origin.y, 0);
	pAux->DrawLine(playerGridPos - Vec3(2, 0, 0), Col_Red, playerGridPos + Vec3(2, 0, 0), Col_Red, 5);

	const CCamera& camera = gEnv->pRenderer->GetCamera();
	{
		Vec3 lookDir = camera.GetViewdir();
		lookDir.y = -lookDir.y;
		lookDir.z = 0;
		lookDir.Normalize();

		pAux->DrawLine(playerGridPos, Col_Red, playerGridPos + 10 * lookDir, Col_Red, 2);
	}
}

void CSegmentedWorld::PrintSegmentInfo(const Vec2& origin, float size)
{
	SDrawTextInfo ti;
	int flags = eDrawText_FixedSize | eDrawText_2D | eDrawText_Monospace;

	Vec3 step(0, 16, 0);
	Vec3 pos(origin.x, origin.y, 1);
	pos.x += m_segmentsMax.x * size + 50;

	Vec3 worldPosLC, worldPosWC;
#if USE_RELATIVE_COORD
	float scale = 1.5f;
	IRenderAuxText::DrawText(pos, scale, NULL, flags, string("Use Relative Coordinates"));
	pos += step;
	scale = 1.2f;
	worldPosLC = m_worldFocalPoint;
	worldPosWC = worldPosLC + GridToWorldPosition(m_neededSegmentsMin);
#else
	scale = 1.5f;
	IRenderAuxText::DrawText(pos, scale, NULL, flags, string("Use Absolute Coordinates"));
	pos += step;
	ti.xscale = ti.yscale = 1.2f;
	worldPosWC = m_worldFocalPoint;
	worldPosLC = worldPosWC - GridToWorldPosition(m_neededSegmentsMin);
#endif
	pos += step;
	IRenderAuxText::DrawText(pos, scale, NULL, flags, string().Format("Player Global World Position: %.2f, %.2f, %.2f", worldPosWC.x, worldPosWC.y, worldPosWC.z).c_str());
	pos += step;
	IRenderAuxText::DrawText(pos, scale, NULL, flags, string().Format("Player Global Grid Position: %d, %d", m_gridFocalPointWC.x, m_gridFocalPointWC.y).c_str());
	pos += step;
	IRenderAuxText::DrawText(pos, scale, NULL, flags, string().Format("Global Segments: (%d, %d), (%d,%d)", m_segmentsMin.x, m_segmentsMin.y, m_segmentsMax.x, m_segmentsMax.y).c_str());
#ifdef _DEBUG
	pos += step;
	pos += step;
	IRenderAuxText::DrawText(pos, scale, NULL, flags, string().Format("Player Local World Position: %.2f, %.2f, %.2f", worldPosLC.x, worldPosLC.y, worldPosLC.z).c_str());
	pos += step;
	IRenderAuxText::DrawText(pos, scale, NULL, flags, string().Format("Player Local Grid Position: %d, %d", m_gridFocalPointLC.x, m_gridFocalPointLC.y).c_str());
	pos += step;
	IRenderAuxText::DrawText(pos, scale, NULL, flags, string().Format("Active Segments: (%d, %d), (%d,%d)", m_neededSegmentsMin.x, m_neededSegmentsMin.y, m_neededSegmentsMax.x, m_neededSegmentsMax.y).c_str());
	pos += step;
	IRenderAuxText::DrawText(pos, scale, NULL, flags, string().Format("Active Segments Center: (%d, %d)", m_neededSegmentsCenter.x, m_neededSegmentsCenter.y).c_str());
#endif
}

void CSegmentedWorld::PrintMemoryInfo(const Vec2& origin, float size)
{
	Vec3 step(0, 16, 0);
	Vec3 pos(origin.x, origin.y, 1);
	pos.x += m_segmentsMax.x * size + 50;

	const stl::SPoolMemoryUsage memUsage = m_pPoolAllocator->GetTotalMemory();
	IRenderAuxText::DrawText(pos, 1.2f, NULL, eDrawText_FixedSize | eDrawText_2D | eDrawText_Monospace, string().Format("Segment Heap: %d KB Used, %d KB Freed, %d KB Unused",
	                                                         (int)(memUsage.nUsed >> 10), (int)(memUsage.nPoolFree() >> 10), (int)(memUsage.nNonPoolFree() >> 10)).c_str());
}
