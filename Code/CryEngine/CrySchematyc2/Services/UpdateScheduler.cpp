// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "UpdateScheduler.h"
#include "CVars.h"

#include <Cry3DEngine/I3DEngine.h>
#include <CryRenderer/IRenderAuxGeom.h>

#include <numeric>

#if DEBUG_RELEVANCE_GRID
//#define DEBUG_RELEVANCE_GRID_EXTRA_LOG (1)
#endif
#define DEBUG_CHECK_SLOW (0)

namespace Schematyc2
{
	namespace
	{
		static const size_t s_defaultBucketSize      = 128;
		static const size_t s_gridCellReserveSize    = 0;
	}

	////////////////////////////////////////////////////////////////
	CRelevanceGrid::CRelevanceGrid()
		: m_pUpdateScheduler(nullptr)
		, m_gridSize(0)
		, m_gridSizeMinusOne(0)
		, m_updateHalfDistance(0)
	{
		gEnv->pSystem->GetISystemEventDispatcher()->RegisterListener(this, "CRelevanceGrid");
		m_relevantEntitiesWorldPos.reserve(10);
	}

	////////////////////////////////////////////////////////////////
	CRelevanceGrid::~CRelevanceGrid()
	{
		gEnv->pSystem->GetISystemEventDispatcher()->RemoveListener(this);
	}

	////////////////////////////////////////////////////////////////
	void CRelevanceGrid::Construct()
	{
		MEMSTAT_CONTEXT(EMemStatContextType::Other, "Schematyc2::CRelevanceGrid::Construct");

		const float terrainSize = static_cast<float>(gEnv->p3DEngine->GetTerrainSize());

		m_gridSize = int(terrainSize / CVars::sc2_RelevanceGridCellSize);
		m_gridSizeMinusOne = m_gridSize - 1;
		m_updateHalfDistance = 0;

		m_dynamicObjects.clear();
		m_relevantCells.clear();
		m_grid.clear();

		m_grid.resize(m_gridSize * m_gridSize);
		if (s_gridCellReserveSize > 0)
		{
			for (SGridCell& cell : m_grid)
			{
				cell.globalUpdateCallbacks.reserve(s_gridCellReserveSize);
				cell.localGridUpdateCallbacks.reserve(s_gridCellReserveSize);
			}
		}
	}

	////////////////////////////////////////////////////////////////
	void CRelevanceGrid::OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam)
	{
		switch (event)
		{
		case ESYSTEM_EVENT_LEVEL_LOAD_START:
			{
				CRY_PROFILE_SECTION(PROFILE_LOADING_ONLY, "ESYSTEM_EVENT_LEVEL_LOAD_START");
				if (gEnv->IsEditor())
				{
					Construct();
				}
				break;
			}
		case ESYSTEM_EVENT_EDITOR_GAME_MODE_CHANGED:
		case ESYSTEM_EVENT_EDITOR_SIMULATION_MODE_CHANGED:
			if (wparam)
			{
				// We are jumping ingame or starting simulation so we have to clear any previous state about the grid
				m_relevantCells.clear();
				for (SGridCell& gridCell : m_grid)
				{
					gridCell.relevanceCounter = 0;
				}
			}
			break;
		case ESYSTEM_EVENT_GAME_FRAMEWORK_INIT_DONE:
			gEnv->pGameFramework->GetILevelSystem()->AddListener(this);
			break;

		case ESYSTEM_EVENT_GAME_FRAMEWORK_ABOUT_TO_SHUTDOWN:
			gEnv->pGameFramework->GetILevelSystem()->RemoveListener(this);
			break;

		default:
			break;
		}
	}

	////////////////////////////////////////////////////////////////
	void CRelevanceGrid::OnLoadingLevelEntitiesStart(ILevelInfo* pLevel)
	{
		CRY_PROFILE_FUNCTION(PROFILE_GAME);

		// NOTE pavloi 2017.10.02: terrain should be loaded at this point, so we can reconstruct the grid with the new level size
		Construct();
	}

	////////////////////////////////////////////////////////////////
	bool CRelevanceGrid::Register(CUpdateScope* pScope, const UpdateCallback& callback, UpdateFrequency frequency, UpdatePriority priority, const UpdateFilter& filter)
	{
		CRY_ASSERT(!pScope->m_bInStaticRelevanceGrid);
		CRY_ASSERT(!pScope->m_bInDynamicRelevanceGrid);
		if (pScope->m_bInStaticRelevanceGrid)
		{
			return false;
		}

		if (const IEntity* pEntity = pScope->GetEntity())
		{
			ushort x = 0;
			ushort y = 0;
			ushort cellIdx = 0;
			GetCellCoordinates(pEntity->GetWorldPos(), x, y, cellIdx);
			
			CRelevanceGrid::TUpdateCallbacks* pUpdateSlots = nullptr;
			if (pScope->IsOnlyLocalGrid())
			{
				pUpdateSlots = &m_grid[cellIdx].localGridUpdateCallbacks;
			}
			else
			{
				pUpdateSlots = &m_grid[cellIdx].globalUpdateCallbacks;
			}

			pUpdateSlots->emplace_back(pScope, callback, frequency, priority, filter); 
			pScope->m_bInStaticRelevanceGrid = true;
			pScope->m_relevanceGridIndex = cellIdx;

			if (m_grid[cellIdx].relevanceCounter > 0)
			{
				m_pUpdateScheduler->ConnectInternal(*pScope, callback, frequency, priority, filter);
			}

			return true;
		}

		return false;
	}

	////////////////////////////////////////////////////////////////
	void CRelevanceGrid::Unregister(CUpdateScope* pScope)
	{
		CRY_ASSERT(!pScope->m_bInDynamicRelevanceGrid);
		CRY_ASSERT(pScope->m_bInStaticRelevanceGrid);
	
		pScope->m_bInStaticRelevanceGrid = false;
		const uint16 cellIdx = pScope->m_relevanceGridIndex;
		pScope->m_relevanceGridIndex = -1;

		CRY_ASSERT(cellIdx < m_grid.size());
		if (cellIdx < m_grid.size())
		{
			CRelevanceGrid::TUpdateCallbacks* pUpdateSlots = nullptr;
			if (pScope->IsOnlyLocalGrid())
			{
				pUpdateSlots = &m_grid[cellIdx].localGridUpdateCallbacks;
			}
			else
			{
				pUpdateSlots = &m_grid[cellIdx].globalUpdateCallbacks;
			}

#if DEBUG_RELEVANCE_GRID
			const bool erased = stl::find_and_erase_if(*pUpdateSlots, [&pScope](const CRelevanceGrid::SUpdateCallback& cb) { return pScope == cb.pScope; });
			CRY_ASSERT_MESSAGE(erased, "Unable to find scope for entity %x at cell %u", pScope->GetEntity() ? pScope->GetEntity()->GetId() : INVALID_ENTITYID, cellIdx);
#else
			stl::find_and_erase_if(*pUpdateSlots, [&pScope](const CRelevanceGrid::SUpdateCallback& cb) { return pScope == cb.pScope; });
#endif
		}
	}

	////////////////////////////////////////////////////////////////
	CRelevanceGrid::SDynamicObject::SDynamicObject(CUpdateScope* pScope, const UpdateCallback& callback, UpdateFrequency frequency, UpdatePriority priority, const UpdateFilter& filter)
		: updateCallback(pScope, callback, frequency, priority, filter)
	{}

	////////////////////////////////////////////////////////////////
	bool CRelevanceGrid::RegisterDynamicObject(CUpdateScope* pScope, const UpdateCallback& callback, UpdateFrequency frequency, UpdatePriority priority, const UpdateFilter& filter)
	{
		CRY_ASSERT(!pScope->m_bInStaticRelevanceGrid);
		CRY_ASSERT(!pScope->m_bInDynamicRelevanceGrid);
		if (!pScope->m_bInDynamicRelevanceGrid)
		{
#if DEBUG_CHECK_SLOW
			CRY_ASSERT(m_dynamicObjects.end() == std::find_if(m_dynamicObjects.begin(), m_dynamicObjects.end(), [&pScope](const CRelevanceGrid::SDynamicObject& dynObj) { return pScope == dynObj.updateCallback.pScope; }));
#endif
			m_dynamicObjects.emplace_back(pScope, callback, frequency, priority, filter);
			pScope->m_bInDynamicRelevanceGrid = true;
			return true;
		}
		return false;
	}

	////////////////////////////////////////////////////////////////
	void CRelevanceGrid::UnregisterDynamicObject(CUpdateScope* pScope)
	{
		CRY_ASSERT(!pScope->m_bInStaticRelevanceGrid);
		CRY_ASSERT(pScope->m_bInDynamicRelevanceGrid);
		pScope->m_bInDynamicRelevanceGrid = false;

#if defined(USE_CRY_ASSERT)
		const bool erased = stl::find_and_erase_if(m_dynamicObjects, [&pScope](const CRelevanceGrid::SDynamicObject& dynObj) { return pScope == dynObj.updateCallback.pScope; });
		CRY_ASSERT(erased);
#else
		stl::find_and_erase_if(m_dynamicObjects, [&pScope](const CRelevanceGrid::SDynamicObject& dynObj) { return pScope == dynObj.updateCallback.pScope; });
#endif
	}

	////////////////////////////////////////////////////////////////
	void CRelevanceGrid::Update(CUpdateRelevanceContext* pRelevanceContext)
	{
		if (!pRelevanceContext)
		{
			return;
		}

		if (!m_updateHalfDistance)
		{
			m_updateHalfDistance = int(pRelevanceContext->GetDefaultGridDistanceUpdate() / Schematyc2::CVars::sc2_RelevanceGridCellSize);
		}

		size_t relevantEntitiesCount(0);
		const SRelevantEntity* relevantEntities = pRelevanceContext->GetRelevantEntities(relevantEntitiesCount);

		m_relevantEntitiesWorldPos.clear();

		// Fill the relevant entities position
		for (size_t i = 0; i < relevantEntitiesCount; ++i)
		{
			const SRelevantEntity& relevantEntity = relevantEntities[i];
			if (relevantEntity.GetIsLocalEntity())
			{
				m_relevantEntitiesWorldPos.emplace_back(relevantEntity.GetEntityWorldPos());
			}
		}

		for (const SDynamicObject& dynamicObject : m_dynamicObjects)
		{
			const CRelevanceGrid::SUpdateCallback& updateCallback = dynamicObject.updateCallback;
			const Vec3 dynamicEntityWorldPos = updateCallback.pScope->GetEntity()->GetWorldPos();
			bool insideRange = false;

			for (const Vec3& relevantEntityWorldPos : m_relevantEntitiesWorldPos)
			{
				const Vec3 entityToDynamicObjectVec = dynamicEntityWorldPos - relevantEntityWorldPos;
				if (entityToDynamicObjectVec.GetLengthSquared() <= pRelevanceContext->GetDynamicEntitiesUpdateRangeSqr())
				{
					insideRange = true;

					if (updateCallback.pScope->IsBound())
					{
						break;
					}

					m_pUpdateScheduler->ConnectInternal(*updateCallback.pScope, updateCallback.callback, updateCallback.frequency, updateCallback.priority, updateCallback.filter);
					break;
				}
			}

			// TODO: Maybe not disconnect immediately on the hard distance check
			if (!insideRange && updateCallback.pScope->IsBound())
			{
				m_pUpdateScheduler->DisconnectInternal(*updateCallback.pScope);
			}
		}

		for (size_t i = 0; i < relevantEntitiesCount; ++i)
		{
			const SRelevantEntity& relevantEntity = relevantEntities[i];

			ushort cellX = 0;
			ushort cellY = 0;
			ushort cellIdx = 0;
			GetCellCoordinates(relevantEntity.GetEntityWorldPos(), cellX, cellY, cellIdx);
			SRelevantCell relevantCell(cellX, cellY, cellIdx, relevantEntity.GetEntityId(), relevantEntity.GetIsLocalEntity());
			SRelevantCell* pChangedCell(nullptr);
			ECellChangedResult result = DidRelevantCellIndexChange(relevantCell, pChangedCell);
			if (result == eCellChangedResult_Changed)
			{
#if DEBUG_RELEVANCE_GRID_EXTRA_LOG
				CryLogAlways("CRelevanceGrid::Update relevant cell change for ent %u old coord %u:%u(%u) new coord %u:%u(%u), local %d",
					relevantCell.id,
					pChangedCell->x, pChangedCell->y, pChangedCell->cellIdx,
					cellX, cellY, cellIdx,
					pChangedCell->bIsLocal);
#endif // DEBUG_RELEVANCE_GRID_EXTRA_LOG

				// Connect & disconnect the relevant entities
				// Connect
				SetupUpdateCallbacksAroundCell(cellX, cellY, pChangedCell->bIsLocal, true);

				// Disconnect
				SetupUpdateCallbacksAroundCell(pChangedCell->x, pChangedCell->y, pChangedCell->bIsLocal, false);

				// Update new cell info
				pChangedCell->x = cellX;
				pChangedCell->y = cellY;
				pChangedCell->cellIdx = cellIdx;
			}
			else if (result == eCellChangedResult_NotFound)
			{
#if DEBUG_RELEVANCE_GRID_EXTRA_LOG
				CryLogAlways("CRelevanceGrid::Update relevant cell new for ent %u new coord %u:%u(%u), local %d",
					relevantCell.id,
					cellX, cellY, cellIdx,
					relevantCell.bIsLocal);
#endif // DEBUG_RELEVANCE_GRID_EXTRA_LOG

				// Register relevant cell
				m_relevantCells.emplace_back(cellX, cellY, cellIdx, relevantEntity.GetEntityId(), relevantEntity.GetIsLocalEntity());

				// Connect the cells
				SetupUpdateCallbacksAroundCell(cellX, cellY, relevantCell.bIsLocal, true);
			}
		}


#if DEBUG_RELEVANCE_GRID
		// TODO pavloi 2017.10.02: add debug draw of m_dynamicObjects
		if (CVars::sc2_RelevanceGridDebugStatic)
		{
			DebugDrawStatic(pRelevanceContext);
		}
#endif // #if DEBUG_RELEVANCE_GRID
	}

	////////////////////////////////////////////////////////////////
	void CRelevanceGrid::SetupUpdateCallbacksAroundCell(ushort cellX, ushort cellY, bool bIsLocalGrid, bool bConnect)
	{
		ushort startX = (ushort)clamp_tpl(cellX - m_updateHalfDistance, 0, m_gridSizeMinusOne);
		ushort endX   = (ushort)clamp_tpl(cellX + m_updateHalfDistance, 0, m_gridSizeMinusOne);
		ushort startY = (ushort)clamp_tpl(cellY - m_updateHalfDistance, 0, m_gridSizeMinusOne);
		ushort endY   = (ushort)clamp_tpl(cellY + m_updateHalfDistance, 0, m_gridSizeMinusOne);
		
		for (ushort y = startY; y <= endY; ++y)
		{
			for (ushort x = startX; x <= endX; ++x)
			{
				const ushort idx = GetCellIndex(x, y);
				if (bConnect)
				{
					ConnectCell(idx, bIsLocalGrid);
				}
				else
				{
					DisconnectCell(idx, bIsLocalGrid);
				}
			}
		}
	}

	////////////////////////////////////////////////////////////////
	void CRelevanceGrid::ConnectCell(ushort idx, bool bIsLocal)
	{
		SGridCell& gridCell = m_grid[idx];
		++gridCell.relevanceCounter;
		if (gridCell.relevanceCounter == 1)
		{
#if DEBUG_RELEVANCE_GRID_EXTRA_LOG
			CryLogAlways("CRelevanceGrid::ConnectCell (%u) local %u", idx, bIsLocal);
#endif // DEBUG_RELEVANCE_GRID_EXTRA_LOG

			for (const CRelevanceGrid::SUpdateCallback& updateCallback : gridCell.globalUpdateCallbacks)
			{
				m_pUpdateScheduler->ConnectInternal(*updateCallback.pScope, updateCallback.callback, updateCallback.frequency, updateCallback.priority, updateCallback.filter);
			}

			if (bIsLocal)
			{
				for (const CRelevanceGrid::SUpdateCallback& localUpdateCallback : gridCell.localGridUpdateCallbacks)
				{
					m_pUpdateScheduler->ConnectInternal(*localUpdateCallback.pScope, localUpdateCallback.callback, localUpdateCallback.frequency, localUpdateCallback.priority, localUpdateCallback.filter);
				}
			}
		}
	}

	////////////////////////////////////////////////////////////////
	void CRelevanceGrid::DisconnectCell(ushort idx, bool bIsLocal)
	{
		SGridCell& gridCell = m_grid[idx];
		--gridCell.relevanceCounter;
		if (gridCell.relevanceCounter == 0)
		{
#if DEBUG_RELEVANCE_GRID_EXTRA_LOG
			CryLogAlways("CRelevanceGrid::DisconnectCell (%u) local %u", idx, bIsLocal);
#endif // DEBUG_RELEVANCE_GRID_EXTRA_LOG

			for (const CRelevanceGrid::SUpdateCallback& updateCallback : gridCell.globalUpdateCallbacks)
			{
				m_pUpdateScheduler->DisconnectInternal(*updateCallback.pScope);
			}

			if (bIsLocal)
			{
				for (const CRelevanceGrid::SUpdateCallback& localUpdateCB : gridCell.localGridUpdateCallbacks)
				{
					m_pUpdateScheduler->DisconnectInternal(*localUpdateCB.pScope);
				}
			}
		}
	}

	////////////////////////////////////////////////////////////////
	CRelevanceGrid::ECellChangedResult CRelevanceGrid::DidRelevantCellIndexChange(const SRelevantCell& cellToFind, SRelevantCell*& pCellInfo)
	{
		TRelevantCells::iterator it = std::find_if(m_relevantCells.begin(), m_relevantCells.end(), [&cellToFind](const SRelevantCell& a) { return cellToFind.id == a.id; });
		if (it != m_relevantCells.end())
		{
			if (it->cellIdx != cellToFind.cellIdx)
			{
				pCellInfo = &(*it);
				return eCellChangedResult_Changed;
			}
			else
			{
				return eCellChangedResult_Same;
			}
		}

		return eCellChangedResult_NotFound;
	}

	////////////////////////////////////////////////////////////////
	void CRelevanceGrid::GetCellCoordinates(const Vec3 worldPos, ushort &x, ushort& y, ushort& cellIdx) const
	{
		x = (ushort)clamp_tpl(int(worldPos.x / CVars::sc2_RelevanceGridCellSize), 0, m_gridSizeMinusOne);
		y = (ushort)clamp_tpl(int(worldPos.y / CVars::sc2_RelevanceGridCellSize), 0, m_gridSizeMinusOne);
		cellIdx = y * m_gridSize + x;
	}

	////////////////////////////////////////////////////////////////
	ushort CRelevanceGrid::GetCellIndex(ushort x, ushort y) const
	{
		return y * m_gridSize + x;
	}

	////////////////////////////////////////////////////////////////
	void CRelevanceGrid::SetUpdateScheduler(CUpdateScheduler* pScheduler)
	{
		m_pUpdateScheduler = pScheduler;
	}

	////////////////////////////////////////////////////////////////
#if DEBUG_RELEVANCE_GRID
	void CRelevanceGrid::DebugDrawStatic(CUpdateRelevanceContext* pRelevanceContext)
	{
		if (gEnv->pRenderer == nullptr)
		{
			return;
		}

		if (gEnv->pRenderer->GetFrameID() == m_debugUpdateFrame)
		{
			return;
		}
		m_debugUpdateFrame = gEnv->pRenderer->GetFrameID();

		IRenderAuxGeom* pRenderAuxGeom = gEnv->pRenderer->GetIRenderAuxGeom();
		if (pRenderAuxGeom == nullptr)
		{
			return;
		}

		SAuxGeomRenderFlags flags(e_Def3DPublicRenderflags);
		flags.SetDepthWriteFlag(e_DepthWriteOff);
		flags.SetDepthTestFlag(e_DepthTestOff);
		pRenderAuxGeom->SetRenderFlags(flags);

		const float cellSize = CVars::sc2_RelevanceGridCellSize;
		const float halfCellSize = cellSize * 0.5f - 0.05f;
		auto buildCellAabb = [cellSize, halfCellSize](int cellX, int cellY)
		{
			Vec3 center = Vec3((float)cellX, (float)cellY, 0.0f) * cellSize + Vec3(halfCellSize, halfCellSize, 0.0f);

			const float z = crymath::floor(gEnv->p3DEngine->GetTerrainZ(center.x, center.y));
			center.z = z + halfCellSize * 0.5f;

			return AABB(center, halfCellSize);
		};

		const ColorB colorRelevantCell = Col_Red;
		const ColorB colorConnectedCell = Col_Yellow;
		const ColorB colorConnectedCellEmpty = Col_White;

		string textBuf;
		SDrawTextInfo ti;
		IRenderAuxText::AColor textColor = Col_White;
		ti.color[0] = textColor.rgba[0];
		ti.color[1] = textColor.rgba[1];
		ti.color[2] = textColor.rgba[2];
		ti.color[3] = textColor.rgba[3];
		ti.flags = eDrawText_FixedSize | eDrawText_Center;
		ti.scale = IRenderAuxText::ASize(1.25f).val;

		auto printCallback = [](const SUpdateCallback& callback, string& textBuf)
		{
			if (const IEntity* pEnt = callback.pScope->GetEntity())
			{
				const EntityId entId = pEnt->GetId();
				const char* szName = pEnt->GetName();
				// NOTE pavloi 2017.10.02: print in red callbacks which are not bound for some reason
				const char* szFormat = callback.pScope->IsBound() ? "\n%u:%s - %u" : "\n$4%u:%s - %u$o";
				textBuf.AppendFormat(szFormat, entId, szName, (uint32)callback.priority);
			}
		};

		const bool isDrawAllCells = (CVars::sc2_RelevanceGridDebugStatic & 2) != 0;
		const bool isPrintExtraInfo = (CVars::sc2_RelevanceGridDebugStatic & 4) != 0;
		const bool isLogMemoryStats = (CVars::sc2_RelevanceGridDebugStatic & 8) != 0;
		CVars::sc2_RelevanceGridDebugStatic &= ~8;

		if (isLogMemoryStats)
		{
			DebugLogStaticMemoryStats();
		}

		for (const SRelevantCell& relevantCell : m_relevantCells)
		{
			ushort cellX = relevantCell.x;
			ushort cellY = relevantCell.y;
			{
				AABB aabb = buildCellAabb(cellX, cellY);
				aabb.Expand(Vec3(0.02f));
				pRenderAuxGeom->DrawAABB(aabb, false, colorRelevantCell, eBBD_Faceted);
			}


			ushort startX = (ushort)clamp_tpl(cellX - m_updateHalfDistance, 0, m_gridSizeMinusOne);
			ushort endX = (ushort)clamp_tpl(cellX + m_updateHalfDistance, 0, m_gridSizeMinusOne);
			ushort startY = (ushort)clamp_tpl(cellY - m_updateHalfDistance, 0, m_gridSizeMinusOne);
			ushort endY = (ushort)clamp_tpl(cellY + m_updateHalfDistance, 0, m_gridSizeMinusOne);
			for (ushort y = startY; y <= endY; ++y)
			{
				for (ushort x = startX; x <= endX; ++x)
				{
					const ushort idx = GetCellIndex(x, y);
					const SGridCell& gridCell = m_grid[idx];

					const bool isEmpty = gridCell.globalUpdateCallbacks.empty() && gridCell.localGridUpdateCallbacks.empty();

					if (isEmpty && !isDrawAllCells)
					{
						continue;
					}

					const AABB aabb = buildCellAabb(x, y);
					pRenderAuxGeom->DrawAABB(aabb, false, isEmpty ? colorConnectedCellEmpty : colorConnectedCell, eBBD_Faceted);

					textBuf.clear();
					
					textBuf.Format("%u:%u(%u) - %" PRISIZE_T" , %" PRISIZE_T" - relCnt %d"
						, x, y, idx
						, gridCell.globalUpdateCallbacks.size()
						, gridCell.localGridUpdateCallbacks.size()
						, gridCell.relevanceCounter);

					if (isPrintExtraInfo)
					{
						for (const SUpdateCallback& callback : gridCell.globalUpdateCallbacks)
						{
							printCallback(callback, textBuf);
						}

						for (const SUpdateCallback& callback : gridCell.localGridUpdateCallbacks)
						{
							printCallback(callback, textBuf);
						}
					}

					pRenderAuxGeom->RenderTextQueued(aabb.GetCenter(), ti, textBuf.c_str());
				}
			}
		}
	}

	void CRelevanceGrid::DebugLogStaticMemoryStats()
	{
		CryLogAlways("gridSize %d", m_gridSize);
		CryLogAlways("grid size %zu, capacity %zu, mem %zu", m_grid.size(), m_grid.capacity(), m_grid.capacity() * sizeof(m_grid[0]));

		struct SMemStat
		{
			size_t capVecMem = 0;
			size_t useVecMem = 0;
			size_t minCap = -1, maxCap = 0, avgCap = 0;
			size_t minSize = -1, maxSize = 0, avgSize = 0;

			void Update(const TUpdateCallbacks& v)
			{
				const size_t cap = v.capacity();
				const size_t size = v.size();

				minCap = std::min(cap, minCap);
				maxCap = std::max(cap, maxCap);
				avgCap += cap;

				minSize = std::min(size, minSize);
				maxSize = std::max(size, maxSize);
				avgSize += size;

				capVecMem += cap * sizeof(v[0]);
				useVecMem += size * sizeof(v[0]);
			}

			void Log(const size_t count)
			{
				CryLogAlways("capacity min %zu max %zu avg %.2lf", minCap, maxCap, double(avgCap) / count);
				CryLogAlways("size     min %zu max %zu avg %.2lf", minSize, maxSize, double(avgSize) / count);
				CryLogAlways("capacity mem %zu", capVecMem);
				CryLogAlways("used mem     %zu", useVecMem);
				CryLogAlways("diff mem     %zu", capVecMem - useVecMem);
			}
		};

		SMemStat global, local;
		for (const SGridCell& cell : m_grid)
		{
			global.Update(cell.globalUpdateCallbacks);
			local.Update(cell.localGridUpdateCallbacks);
		}
		CryLogAlways("global");
		global.Log(m_grid.size());
		CryLogAlways("local");
		local.Log(m_grid.size());
	}
#endif // DEBUG_RELEVANCE_GRID

	////////////////////////////////////////////////////////////////
	CRelevanceGrid::SUpdateCallback::SUpdateCallback(CUpdateScope* pScope, const UpdateCallback& callback, UpdateFrequency frequency, UpdatePriority priority, const UpdateFilter& filter)
		: pScope(pScope)
		, callback(callback)
		, frequency(frequency)
		, priority(priority)
		, filter(filter)
	{}

	////////////////////////////////////////////////////////////////
	CUpdateScheduler::CUpdateScheduler()
		: m_updatePosition(0)
		, m_frameIdx(0)
		, m_bInFrame(false)
	{
		m_relevanceGrid.SetUpdateScheduler(this);
		for (size_t bucketIdx = 0; bucketIdx < EUpdateFrequency::Count; ++bucketIdx)
		{
			m_buckets[bucketIdx].SetFrequency(bucketIdx);
		}
		for (float& t : m_frameTimes)
		{
			t = 0.0f;
		}
	}

	////////////////////////////////////////////////////////////////
	CUpdateScheduler::~CUpdateScheduler()
	{
		for (int i = 0; i < EUpdateFrequency::Count; ++i)
		{
			m_buckets[i].CleanUp();
		}
	}

	////////////////////////////////////////////////////////////////
	bool CUpdateScheduler::Connect(CUpdateScope& scope, const UpdateCallback& callback, UpdateFrequency frequency, UpdatePriority priority, const UpdateFilter& filter)
	{
		if (scope.GetSchematycObj())
		{
			const bool shouldUseGrid = m_useRelevanceGrid && m_useRelevanceGrid(*scope.GetSchematycObj());
			const bool isDynamicObject = m_isDynamicObject && m_isDynamicObject(*scope.GetSchematycObj());

			if (shouldUseGrid)
			{
				if (isDynamicObject)
				{
					return m_relevanceGrid.RegisterDynamicObject(&scope, callback, frequency, priority, filter);
				}
				else
				{
					return m_relevanceGrid.Register(&scope, callback, frequency, priority, filter);
				}
			}
		}

		return ConnectInternal(scope, callback, frequency, priority, filter);
	}

	////////////////////////////////////////////////////////////////
	void CUpdateScheduler::Disconnect(CUpdateScope& scope)
	{
		UnregisterFromRelevanceGrid(scope);
		DisconnectInternal(scope);
	}

	//////////////////////////////////////////////////////////////////////////
	void CUpdateScheduler::UnregisterFromRelevanceGrid(CUpdateScope& scope)
	{
		if (scope.m_bInDynamicRelevanceGrid)
		{
			m_relevanceGrid.UnregisterDynamicObject(&scope);
		}
		else if (scope.m_bInStaticRelevanceGrid)
		{
			m_relevanceGrid.Unregister(&scope);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CUpdateScheduler::DisconnectInternal(CUpdateScope &scope)
	{
		if (scope.m_bIsBound)
		{
			const int64 startTicks = CryGetTicks();

			m_buckets[scope.m_frequency].Disconnect(scope, scope.m_priority);

			scope.m_bIsBound = false;
			scope.m_bucketIndex = -1;
			scope.m_priority = 0;
			scope.m_frequency = 0;

			const int64 endTicks = CryGetTicks();
			m_stats.AddDisconnected(endTicks - startTicks);
		}
	}

	////////////////////////////////////////////////////////////////
	bool CUpdateScheduler::InFrame() const
	{
		return m_bInFrame;
	}

	////////////////////////////////////////////////////////////////
	bool CUpdateScheduler::BeginFrame(float frameTime)
	{
		CRY_PROFILE_FUNCTION(PROFILE_GAME);
		CRY_ASSERT(!m_bInFrame);
		if(!m_bInFrame)
		{
			bool updateOrderDirty = false;

			for (size_t frequency = 0; frequency < EUpdateFrequency::Count; ++frequency)
			{
				CRY_ASSERT(m_pendingPriorities.empty());
				m_buckets[frequency].BeginUpdate(m_pendingPriorities);

				if (!m_pendingPriorities.empty())
				{
					for (UpdatePriority priority : m_pendingPriorities)
					{
						m_updateOrder.emplace_back(priority, frequency);
					}
					updateOrderDirty = true;
					m_pendingPriorities.clear();
				}
			}

			if (updateOrderDirty)
			{
				std::sort(m_updateOrder.begin(), m_updateOrder.end());
			}

			m_updatePosition = 0;

			{
				m_frameIdx = gEnv->nMainFrameID;
				const uint32 idx = m_frameIdx % s_maxFrameStride;
				m_frameTimes[idx] = frameTime;
			}

			m_stats.Reset();

			m_bInFrame = true;
			return true;
		}
		return false;
	}

	////////////////////////////////////////////////////////////////
	bool CUpdateScheduler::Update(UpdatePriority beginPriority, UpdatePriority endPriority, CUpdateRelevanceContext* pRelevanceContext)
	{
		CRY_PROFILE_FUNCTION(PROFILE_GAME);
		MEMSTAT_CONTEXT(EMemStatContextType::Other, "Schematyc2::CUpdateScheduler::Update");

		if(m_bInFrame)
		{
			m_relevanceGrid.Update(pRelevanceContext);

			static_assert(CRY_ARRAY_COUNT(m_stats.bucketStats) == CRY_ARRAY_COUNT(m_buckets), "Bucket count mismatch");
			UpdateSchedulerStats::SUpdateBucketStats* bucketUpdateStats = m_stats.bucketStats;
			
			UpdateSchedulerStats::SUpdateStageStats stageUpdateStats(beginPriority, endPriority);
			const int64 startTimeTicks = CryGetTicks();

			// Calculate cumulative frame times.
			const uint32 curFrameId = m_frameIdx;

			SUpdateContext frequencyUpdateContexts[EUpdateFrequency::Count];
			for (UpdateFrequency frequency = EUpdateFrequency::EveryFrame; frequency < EUpdateFrequency::Count; ++frequency)
			{
				const size_t frameCount = size_t(1) << frequency;
				for (size_t frameIdx = 0; frameIdx < frameCount; ++frameIdx)
				{
					frequencyUpdateContexts[frequency].cumulativeFrameTime += m_frameTimes[(curFrameId - frameIdx) % s_maxFrameStride];
				}
				frequencyUpdateContexts[frequency].frameTime = m_frameTimes[curFrameId % s_maxFrameStride];
			}

			for (size_t updateSize = m_updateOrder.size(); m_updatePosition < updateSize; ++m_updatePosition)
			{
				SUpdateSlot& slot = m_updateOrder[m_updatePosition];
				if (slot.priority <= endPriority)
				{
					break;
				}
				else if (slot.priority <= beginPriority)
				{
					UpdateSchedulerStats::SUpdateBucketStats& stats = bucketUpdateStats[slot.bucketIndex];
					CBucket& bucket = m_buckets[slot.bucketIndex];
					SUpdateContext& context = frequencyUpdateContexts[slot.bucketIndex];
					if (slot.bucketIndex == EUpdateFrequency::EveryFrame)
					{
						bucket.Update<false>(context, slot.priority, curFrameId, stats, m_debugNameMap);
					}
					else
					{
						bucket.Update<true>(context, slot.priority, curFrameId, stats, m_debugNameMap);
					}
				}
			}

			// Finish update stats
			{
				const int64 endTimeTicks = CryGetTicks();
				stageUpdateStats.updateTime = CTimeValue(gEnv->pTimer->TicksToSeconds(endTimeTicks - startTimeTicks));

				for (int index = 0; index < EUpdateFrequency::Count; ++index)
				{
					UpdateSchedulerStats::SUpdateBucketStats& stats = bucketUpdateStats[index];
					stats.frequency = index;
					stageUpdateStats.itemsToUpdateCount += stats.itemsToUpdateCount;
					stageUpdateStats.updatedItemsCount += stats.updatedItemsCount;
				}

				m_stats.Add(stageUpdateStats);
			}
			return true;
		}
		return false;
	}

	////////////////////////////////////////////////////////////////
	bool CUpdateScheduler::EndFrame()
	{
		CRY_ASSERT(m_bInFrame);
		if(m_bInFrame)
		{
			m_bInFrame = false;
			return true;
		}
		return false;
	}

	////////////////////////////////////////////////////////////////
	void CUpdateScheduler::VerifyCleanup() {}

	////////////////////////////////////////////////////////////////
	void CUpdateScheduler::Reset()
	{
		for(size_t bucketIdx = 0; bucketIdx < EUpdateFrequency::Count; ++ bucketIdx)
		{
			m_buckets[bucketIdx].Reset();
		}
		m_bInFrame = false;
	}

	//////////////////////////////////////////////////////////////////////////
	const UpdateSchedulerStats::IFrameUpdateStats* CUpdateScheduler::GetFrameUpdateStats() const
	{
		return &m_stats;
	}

	//////////////////////////////////////////////////////////////////////////
	void CUpdateScheduler::SetShouldUseRelevanceGridCallback(IUpdateScheduler::UseRelevanceGridPredicate useRelevanceGrid)
	{
		m_useRelevanceGrid = useRelevanceGrid;
	}

	void CUpdateScheduler::SetIsDynamicObjectCallback(IsDynamicObjectPredicate isDynamicObject)
	{
		m_isDynamicObject = isDynamicObject;
	}

	void CUpdateScheduler::SetDebugPriorityNames(const DebugPriorityNameArray& debugNames)
	{
		DebugPriorityNameArray::const_iterator it = debugNames.begin();

		for (; it < debugNames.end(); ++it)
		{
			m_debugNameMap.insert(*it);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	bool CUpdateScheduler::ConnectInternal(CUpdateScope& scope, const UpdateCallback& callback, UpdateFrequency frequency /*= EUpdateFrequency::EveryFrame*/, UpdatePriority priority /*= EUpdatePriority::Default*/, const UpdateFilter& filter /*= UpdateFilter()*/)
	{
		CRY_PROFILE_FUNCTION(PROFILE_GAME);
		DisconnectInternal(scope);
		if ((frequency >= EUpdateFrequency::EveryFrame) && (frequency < EUpdateFrequency::Count))
		{
			const int64 startTicks = CryGetTicks();

			m_buckets[frequency].Connect(scope, callback, frequency, priority, filter);

			scope.m_bIsBound = true;
			scope.m_priority = priority;
			scope.m_frequency = frequency;

			const int64 endTicks = CryGetTicks();
			m_stats.AddConnected(endTicks - startTicks);

			return true;
		}
		return false;
	}

	////////////////////////////////////////////////////////////////
	CUpdateScheduler::SUpdateSlot::SUpdateSlot()
		: priority(0)
		, bucketIndex(0)
	{ }

	CUpdateScheduler::SUpdateSlot::SUpdateSlot(const UpdatePriority priority, const UpdateFrequency frequency)
		: priority(priority)
		, bucketIndex(frequency)
	{}

	////////////////////////////////////////////////////////////////
	CUpdateScheduler::SObserver::SObserver()
		: pScope(nullptr)
		, frequency(0)
		, stride(0)
	{}

	CUpdateScheduler::SObserver::SObserver(CUpdateScope& scope, const UpdateCallback& callback, const UpdateFilter& filter, UpdateFrequency frequency, UpdateFrequency stride)
		: pScope(&scope)
		, callback(callback)
		, filter(filter)
		, frequency(frequency)
		, stride(stride)
	{}


	CUpdateScheduler::SPendingObserver::SPendingObserver()
		: priority(0)
	{}

	CUpdateScheduler::SPendingObserver::SPendingObserver(CUpdateScope& scope, const UpdateCallback& callback, const UpdateFilter& filter, UpdateFrequency frequency, UpdateFrequency stride, UpdatePriority priority)
		: SObserver(scope, callback, filter, frequency, stride)
		, priority(priority)
	{}

	//////////////////////////////////////////////////////////////////////////
	CUpdateScheduler::SFrameUpdateStats::SFrameUpdateStats()
		: stagesCount(0)
	{}

	//////////////////////////////////////////////////////////////////////////
	void CUpdateScheduler::SFrameUpdateStats::Add(const UpdateSchedulerStats::SUpdateStageStats& stats)
	{
		if (stagesCount < k_maxStagesCount)
		{
			stagesStats[stagesCount] = stats;
			++stagesCount;
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CUpdateScheduler::SFrameUpdateStats::AddConnected(int64 timeTicks)
	{
		changeStats.connectedCount++;
		changeStats.connectionTimeTicks += timeTicks;
	}

	//////////////////////////////////////////////////////////////////////////
	void CUpdateScheduler::SFrameUpdateStats::AddDisconnected(int64 timeTicks)
	{
		changeStats.disconnectedCount++;
		changeStats.disconnectionTimeTicks += timeTicks;
	}

	//////////////////////////////////////////////////////////////////////////
	void CUpdateScheduler::SFrameUpdateStats::Reset()
	{
		stagesCount = 0;
		::memset(bucketStats, 0, sizeof(bucketStats[0]) * k_maxBucketsCount);
		::memset(&changeStats, 0, sizeof(changeStats));
	}

	//////////////////////////////////////////////////////////////////////////
	const UpdateSchedulerStats::SUpdateStageStats* CUpdateScheduler::SFrameUpdateStats::GetStageStats(size_t& outCount) const
	{
		outCount = stagesCount;
		return stagesStats;
	}

	//////////////////////////////////////////////////////////////////////////
	const UpdateSchedulerStats::SUpdateBucketStats* CUpdateScheduler::SFrameUpdateStats::GetBucketStats(size_t& outCount) const
	{
		outCount = k_maxBucketsCount;
		return bucketStats;
	}

	////////////////////////////////////////////////////////////////
	CUpdateScheduler::SObserverGroup::SObserverGroup()
	{
		observers.reserve(s_defaultBucketSize);
	}

	////////////////////////////////////////////////////////////////
	CUpdateScheduler::CBucket::CBucket()
		: m_totalObserverCount(0)
		, m_totalDirtyCount(0)
		, m_frequency(0)
	{
	}

	////////////////////////////////////////////////////////////////
	void CUpdateScheduler::CBucket::Connect(CUpdateScope& scope, const UpdateCallback& callback, UpdateFrequency frequency, UpdatePriority priority, const UpdateFilter& filter)
	{
		CRY_PROFILE_FUNCTION(PROFILE_GAME);
		CRY_ASSERT(frequency == m_frequency);

		TObserverMap::iterator it = m_observers.find(priority);
		if (it == m_observers.end())
		{
			const UpdateFrequency stride = 0;
			m_pendingNewPriorityObservers.emplace_back(scope, callback, filter, frequency, stride, priority);
			scope.m_bUpdateBindPending = true;
		}
		else
		{
			AddObserver(it->second.observers, scope, callback, filter);
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CUpdateScheduler::CBucket::AddObserver(TObserverVector& observers, CUpdateScope& scope, const UpdateCallback& callback, const UpdateFilter& filter)
	{
		const UpdateFrequency stride = SelectNewStride();
		observers.emplace_back(scope, callback, filter, m_frequency, stride);
		scope.m_bucketIndex = observers.size() - 1;

		m_frameStrideBuckets[stride]++;
		m_totalObserverCount++;
	}

	////////////////////////////////////////////////////////////////
	void CUpdateScheduler::CBucket::Disconnect(CUpdateScope& scope, UpdatePriority priority)
	{
		CRY_PROFILE_FUNCTION(PROFILE_GAME);

		if (scope.m_bUpdateBindPending)
		{
			scope.m_bUpdateBindPending = false;
#if defined(USE_CRY_ASSERT)
			bool erased = stl::find_and_erase_if(m_pendingNewPriorityObservers,
				[&scope](SPendingObserver& observer) { return observer.pScope == &scope; });
			CRY_ASSERT(erased);
#else
			stl::find_and_erase_if(m_pendingNewPriorityObservers,
				[&scope](SPendingObserver& observer) { return observer.pScope == &scope; });
#endif
		}
		else
		{
			TObserverMap::iterator it = m_observers.find(priority);
			CRY_ASSERT(it != m_observers.end());
			if (it != m_observers.end())
			{
#if defined(USE_CRY_ASSERT)
				bool res = stl::push_back_unique(it->second.dirtyIndices, scope.m_bucketIndex);
				CRY_ASSERT(res);
#else
				stl::push_back_unique(it->second.dirtyIndices, scope.m_bucketIndex);
#endif

				SObserver& observer = it->second.observers[scope.m_bucketIndex];
				observer.pScope = nullptr;
				observer.callback = UpdateCallback();
				observer.filter = UpdateFilter();
				m_frameStrideBuckets[observer.stride]--;
			}
		}
	}

	////////////////////////////////////////////////////////////////
	void CUpdateScheduler::CBucket::BeginUpdate(TPendingPriorities& pendingPriorities)
	{
		DeleteDirtyObservers();
		AddPendingObservers(pendingPriorities);
	}

	//////////////////////////////////////////////////////////////////////////
	void CUpdateScheduler::CBucket::DeleteDirtyObservers()
	{
		CRY_PROFILE_FUNCTION(PROFILE_GAME);
		for (auto& prioObserverPair : m_observers)
		{
			SObserverGroup& group = prioObserverPair.second;
			const size_t dirtyIndicesSize = group.dirtyIndices.size();

			// Delete disconnected observers in reverse to guarantee correct order
			if (dirtyIndicesSize > 0)
			{
				if (group.dirtyIndices.size() == group.observers.size())
				{
					group.observers.clear();
				}
				else
				{
					std::sort(group.dirtyIndices.begin(), group.dirtyIndices.end());

					for (TDirtyIndicesVector::reverse_iterator it = group.dirtyIndices.rbegin(), itEnd = group.dirtyIndices.rend(); it != itEnd; ++it)
					{
						//Remove swap
						const size_t index = *it;
						const size_t observerSize = group.observers.size();

						CRY_ASSERT(observerSize > 1);
						if (index != (observerSize - 1))
						{
							TObserverVector::iterator newPos = group.observers.begin() + index;
							std::iter_swap(newPos, group.observers.end() - 1);

							newPos->pScope->m_bucketIndex = index;
						}

						group.observers.pop_back();
					}
				}

				m_totalObserverCount -= dirtyIndicesSize;
				group.dirtyIndices.clear();
			}

#if DEBUG_CHECK_SLOW
			for (size_t i = 0; i < group.observers.size(); ++i)
			{
				CRY_ASSERT(group.observers[i].pScope);
				CRY_ASSERT(group.observers[i].pScope->m_bucketIndex == i);
			}
#endif
		}
		m_totalDirtyCount = 0;
	}

	//////////////////////////////////////////////////////////////////////////
	void CUpdateScheduler::CBucket::AddPendingObservers(TPendingPriorities& pendingPriorities)
	{
		if (!m_pendingNewPriorityObservers.empty())
		{
			CRY_PROFILE_FUNCTION(PROFILE_GAME);

			for (SPendingObserver& newObserver : m_pendingNewPriorityObservers)
			{
				TObserverVector& observers = m_observers[newObserver.priority].observers;

#if defined(USE_CRY_ASSERT)
				bool res = stl::push_back_unique(pendingPriorities, newObserver.priority);
				CRY_ASSERT(!res || observers.empty());
#else
				stl::push_back_unique(pendingPriorities, newObserver.priority);
#endif

				AddObserver(observers, *newObserver.pScope, newObserver.callback, newObserver.filter);
				newObserver.pScope->m_bUpdateBindPending = false;
			}

			m_pendingNewPriorityObservers.clear();

			CRY_ASSERT(std::accumulate(m_frameStrideBuckets.begin(), m_frameStrideBuckets.end(), 0) == m_totalObserverCount);
		}
	}

	////////////////////////////////////////////////////////////////
	const char* CUpdateScheduler::CBucket::GetDebugName(UpdatePriority priority, const TPriorityDebugNameMap& debugNames)
	{
		stack_string debugName;

		TPriorityDebugNameMap::const_iterator it = debugNames.find(priority);
		if (it != debugNames.end())
		{
			debugName.Format("Priority: %s", it->second);
		}
		else
		{
			debugName = "Priority: Unknown";
		}

		switch (m_frequency)
		{
		case EUpdateFrequency::EveryFrame:
			debugName += ", Update Freq: Every Frame";
			break;
		case EUpdateFrequency::EveryTwoFrames:
			debugName += ", Update Freq: Every Two Frames";
			break;
		case EUpdateFrequency::EveryFourFrames:
			debugName += ", Update Freq: Every Four Frames";
			break;
		case EUpdateFrequency::EveryEightFrames:
			debugName += ", Update Freq: Every Eight Frames";
			break;
		default:
			debugName += ", Update Freq: Unknown";
			break;
		}

		static_assert((int)EUpdateFrequency::Count == 4, "Unexpected Update Frequency Enum Count");

		return debugName.c_str();
	}

	////////////////////////////////////////////////////////////////
	template <bool UseStride>
	void CUpdateScheduler::CBucket::Update(const SUpdateContext context, UpdatePriority priority, uint32 frameId, UpdateSchedulerStats::SUpdateBucketStats& outUpdateStats, const TPriorityDebugNameMap& debugNames)
	{
#if DEBUG_BUCKETS_NAMES
		CRY_PROFILE_FUNCTION_ARG(PROFILE_GAME, GetDebugName(priority, debugNames));
#else
		CRY_PROFILE_FUNCTION(PROFILE_GAME);
#endif

		size_t updatedCount = 0;

		const UpdateFrequency strideMask = (UpdateFrequency(1) << m_frequency) - 1;
		const UpdateFrequency currentStride = frameId & strideMask;

		const int64 startTicks = CryGetTicks();
		
		TObserverMap::iterator it = m_observers.find(priority);
		if (it != m_observers.end())
		{
			SObserverGroup& observerGroup = it->second;
			const size_t observerSize = observerGroup.observers.size();

			for (size_t k = 0; k < observerSize; ++k)
			{
				SObserver& observer = observerGroup.observers[k];
				if (!UseStride || observer.stride == currentStride)
				{
					if (observer.callback && (!observer.filter || observer.filter()))
					{
						observer.callback(context);
						updatedCount++;
					}
				}
			}

			outUpdateStats.itemsToUpdateCount += observerSize;
			outUpdateStats.updatedItemsCount += updatedCount;
		}

		const int64 endTicks = CryGetTicks();
		outUpdateStats.updateTimeTicks += endTicks - startTicks;
	}

	////////////////////////////////////////////////////////////////
	void CUpdateScheduler::CBucket::Reset()
	{
		for (TObserverMap::iterator it = m_observers.begin(), itEnd = m_observers.end(); it != itEnd; ++it)
		{
			it->second.observers.clear();
			it->second.dirtyIndices.clear();
		}
	}

	////////////////////////////////////////////////////////////////
	void CUpdateScheduler::CBucket::CleanUp()
	{
		DeleteDirtyObservers();

		for (TObserverMap::iterator it = m_observers.begin(), itEnd = m_observers.end(); it != itEnd; ++it)
		{
			for (SObserver& observer : it->second.observers)
			{
				observer.pScope->m_bIsBound = false;
			}
			it->second.observers.clear();
		}
		
		for (SPendingObserver& observer : m_pendingNewPriorityObservers)
		{
			observer.pScope->m_bUpdateBindPending = false;
			observer.pScope->m_bIsBound = false;
		}
		m_pendingNewPriorityObservers.clear();
	}

	//////////////////////////////////////////////////////////////////////////
	void CUpdateScheduler::CBucket::SetFrequency(UpdateFrequency frequency)
	{
		m_frequency = frequency;
		m_frameStrideBuckets.resize(size_t(1) << frequency);
	}

	//////////////////////////////////////////////////////////////////////////
	UpdateFrequency CUpdateScheduler::CBucket::SelectNewStride() const
	{
		auto iter = std::min_element(m_frameStrideBuckets.begin(), m_frameStrideBuckets.end());
		return UpdateFrequency(iter - m_frameStrideBuckets.begin());
	}

}
