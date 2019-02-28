// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "UpdateScheduler.h"
#include "CVars.h"

#include <Cry3DEngine/I3DEngine.h>
#include <CryRenderer/IRenderAuxGeom.h>

#if DEBUG_RELEVANCE_GRID
//#define DEBUG_RELEVANCE_GRID_EXTRA_LOG (1)
#endif

namespace Schematyc2
{
	namespace
	{
		static const size_t s_defaultBucketSize      = 128;
		static const uint32 s_invalidBucketIdx       = ~0;
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
		gEnv->pGameFramework->GetILevelSystem()->RemoveListener(this);
		gEnv->pSystem->GetISystemEventDispatcher()->RemoveListener(this);
	}

	////////////////////////////////////////////////////////////////
	void CRelevanceGrid::Construct()
	{
		const float terrainSize = static_cast<float>(gEnv->p3DEngine->GetTerrainSize());

		m_gridSize = int(terrainSize / CVars::sc_RelevanceGridCellSize);
		m_gridSizeMinusOne = m_gridSize - 1;
		m_updateHalfDistance = 0;

		m_dynamicObjects.clear();
		m_relevantCells.clear();
		m_grid.clear();

		m_grid.resize(m_gridSize * m_gridSize);
		for (SGridCell& cell : m_grid)
		{
			cell.globalUpdateCallbacks.reserve(16);
			cell.localGridUpdateCallbacks.reserve(16);
		}
	}

	////////////////////////////////////////////////////////////////
	void CRelevanceGrid::OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam)
	{
		switch (event)
		{
		case ESYSTEM_EVENT_LEVEL_LOAD_START:
			{
				LOADING_TIME_PROFILE_SECTION_NAMED("ESYSTEM_EVENT_LEVEL_LOAD_START");
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
		if (pScope->GetEntity())
		{
			ushort x = 0;
			ushort y = 0;
			ushort cellIdx = 0;
			GetCellCoordinates(pScope->GetEntity()->GetWorldPos(), x, y, cellIdx);
			
			CRelevanceGrid::TUpdateCallbacks* pUpdateSlots = nullptr;
			if (pScope->IsOnlyLocalGrid())
			{
				pUpdateSlots = &m_grid[cellIdx].localGridUpdateCallbacks;
			}
			else
			{
				pUpdateSlots = &m_grid[cellIdx].globalUpdateCallbacks;
			}

			m_entityToGridCellIndexLookup.insert(std::make_pair(pScope->GetEntity()->GetId(), cellIdx));
			pUpdateSlots->emplace_back(pScope, callback, frequency, priority, filter); 

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
		if (pScope->GetEntity())
		{
			TEntityToGridCell::iterator it = m_entityToGridCellIndexLookup.find(pScope->GetEntity()->GetId());

			if (it != m_entityToGridCellIndexLookup.end())
			{
				ushort cellIdx = it->second;

				CRelevanceGrid::TUpdateCallbacks* pUpdateSlots = nullptr;
				if (pScope->IsOnlyLocalGrid())
				{
					pUpdateSlots = &m_grid[cellIdx].localGridUpdateCallbacks;
				}
				else
				{
					pUpdateSlots = &m_grid[cellIdx].globalUpdateCallbacks;
				}

				CRelevanceGrid::TUpdateCallbacks::iterator it = std::find_if(pUpdateSlots->begin(), pUpdateSlots->end(), [&pScope](const CRelevanceGrid::SUpdateCallback& cb) { return pScope == cb.pScope; });
				if (it != pUpdateSlots->end())
				{
					pUpdateSlots->erase(it);
				}
			}
		}
	}

	////////////////////////////////////////////////////////////////
	CRelevanceGrid::SDynamicObject::SDynamicObject(CUpdateScope* pScope, const UpdateCallback& callback, UpdateFrequency frequency, UpdatePriority priority, const UpdateFilter& filter)
		: updateCallback(pScope, callback, frequency, priority, filter)
	{}

	////////////////////////////////////////////////////////////////
	bool CRelevanceGrid::RegisterDynamicObject(CUpdateScope* pScope, const UpdateCallback& callback, UpdateFrequency frequency, UpdatePriority priority, const UpdateFilter& filter)
	{
		CRelevanceGrid::TDynamicObjects::iterator it = std::find_if(m_dynamicObjects.begin(), m_dynamicObjects.end(), [&pScope](const CRelevanceGrid::SDynamicObject& dynObj) { return pScope == dynObj.updateCallback.pScope; });
		if (it == m_dynamicObjects.end())
		{
			m_dynamicObjects.emplace_back(pScope, callback, frequency, priority, filter);
			return true;
		}
		return false;
	}

	////////////////////////////////////////////////////////////////
	void CRelevanceGrid::UnregisterDynamicObject(CUpdateScope* pScope)
	{
		CRelevanceGrid::TDynamicObjects::iterator it = std::find_if(m_dynamicObjects.begin(), m_dynamicObjects.end(), [&pScope](const CRelevanceGrid::SDynamicObject& dynObj) { return pScope == dynObj.updateCallback.pScope; });
		if (it != m_dynamicObjects.end())
		{
			m_dynamicObjects.erase(it);
		}
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
			m_updateHalfDistance = int(pRelevanceContext->GetDefaultGridDistanceUpdate() / Schematyc2::CVars::sc_RelevanceGridCellSize);
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
		if (CVars::sc_RelevanceGridDebugStatic)
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
		x = (ushort)clamp_tpl(int(worldPos.x / CVars::sc_RelevanceGridCellSize), 0, m_gridSizeMinusOne);
		y = (ushort)clamp_tpl(int(worldPos.y / CVars::sc_RelevanceGridCellSize), 0, m_gridSizeMinusOne);
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

		size_t relevantEntitiesCount(0);
		const SRelevantEntity* relevantEntities = pRelevanceContext->GetRelevantEntities(relevantEntitiesCount);

		IRenderAuxGeom* pRenderAuxGeom = gEnv->pRenderer->GetIRenderAuxGeom();
		if (pRenderAuxGeom == nullptr)
		{
			return;
		}

		SAuxGeomRenderFlags flags(e_Def3DPublicRenderflags);
		flags.SetDepthWriteFlag(e_DepthWriteOff);
		flags.SetDepthTestFlag(e_DepthTestOff);
		pRenderAuxGeom->SetRenderFlags(flags);

		const float cellSize = CVars::sc_RelevanceGridCellSize;
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
		ti.flags = eDrawText_FixedSize | eDrawText_Center | eDrawText_800x600;
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

		const bool isDrawAllCells = CVars::sc_RelevanceGridDebugStatic > 1;
		const bool isPrintExtraInfo = CVars::sc_RelevanceGridDebugStatic > 2;

		for (const SRelevantCell& relevantCell : m_relevantCells)
		{
			ushort cellX = relevantCell.x;
			ushort cellY = relevantCell.y;
			ushort cellIdx = relevantCell.cellIdx;
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
		: m_currentBucketIdx(0)
		, m_bInFrame(false)
	{
		m_relevanceGrid.SetUpdateScheduler(this);
		for(size_t bucketIdx = 0; bucketIdx < s_bucketCount; ++ bucketIdx)
		{
			m_frameTimes[bucketIdx] = 0.0f;
		}
	}

	////////////////////////////////////////////////////////////////
	CUpdateScheduler::~CUpdateScheduler()
	{
		for(SSlot& slot : m_slots)
		{
			slot.pScope->m_bIsBound = false;
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
		ScopeDestroyed(scope);
		DisconnectInternal(scope);
	}

	//////////////////////////////////////////////////////////////////////////
	bool CUpdateScheduler::ScopeDestroyed(CUpdateScope& scope)
	{
		if (scope.GetSchematycObj())
		{
			const bool shouldUseGrid = m_useRelevanceGrid && m_useRelevanceGrid(*scope.GetSchematycObj());
			const bool isDynamicObject = m_isDynamicObject && m_isDynamicObject(*scope.GetSchematycObj());

			if (shouldUseGrid)
			{
				if (isDynamicObject)
				{
					m_relevanceGrid.UnregisterDynamicObject(&scope);
					return true;
				}
				else
				{
					m_relevanceGrid.Unregister(&scope);
					return true;
				}
			}
		}

		return false;
	}

	//////////////////////////////////////////////////////////////////////////
	void CUpdateScheduler::DisconnectInternal(CUpdateScope &scope)
	{
		if (scope.m_bIsBound == true)
		{
			SlotVector::iterator itSlot = std::find_if(m_slots.begin(), m_slots.end(), SFindSlot(&scope));
			CRY_ASSERT(itSlot != m_slots.end());
			if (itSlot != m_slots.end())
			{
				// Disconnect from appropriate buckets.
				const size_t         bucketStride = size_t(1) << itSlot->frequency;
				const UpdatePriority priority = itSlot->priority;
				for (CBucket* pBucket = m_buckets + itSlot->firstBucketIdx, *pEndBucket = m_buckets + s_bucketCount; pBucket < pEndBucket; pBucket += bucketStride)
				{
					pBucket->Disconnect(scope, priority);
				}
				m_slots.erase(itSlot);
			}
			scope.m_bIsBound = false;
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
			m_buckets[m_currentBucketIdx].BeginUpdate();
			m_frameTimes[m_currentBucketIdx] = frameTime;
			m_bInFrame                       = true;

			m_stats.Reset();

			return true;
		}
		return false;
	}

	////////////////////////////////////////////////////////////////
	bool CUpdateScheduler::Update(UpdatePriority beginPriority, UpdatePriority endPriority, CUpdateRelevanceContext* pRelevanceContext)
	{
		CRY_PROFILE_FUNCTION(PROFILE_GAME);
		if(m_bInFrame)
		{
			m_relevanceGrid.Update(pRelevanceContext);

			UpdateSchedulerStats::SUpdateStageStats updateStats(beginPriority, endPriority);
			const int64 startTimeTicks = CryGetTicks();

			// Calculate cumulative frame times.
			SUpdateContext frequencyUpdateContexts[EUpdateFrequency::Count];
			for(UpdateFrequency frequency = EUpdateFrequency::EveryFrame; frequency < EUpdateFrequency::Count; ++ frequency)
			{
				const size_t frameCount = size_t(1) << frequency;
				for(size_t frameIdx = 0; frameIdx < frameCount; ++ frameIdx)
				{
					frequencyUpdateContexts[frequency].cumulativeFrameTime += m_frameTimes[(m_currentBucketIdx - frameIdx) % s_bucketCount];
				}
				frequencyUpdateContexts[frequency].frameTime = m_frameTimes[m_currentBucketIdx];
			}
			// Update contents of current bucket.
			m_buckets[m_currentBucketIdx].Update(frequencyUpdateContexts, beginPriority, endPriority, updateStats);

			const int64 endTimeTicks = CryGetTicks();

			if (gEnv->pTimer)
			{
				updateStats.updateTime = CTimeValue(gEnv->pTimer->TicksToSeconds(endTimeTicks - startTimeTicks));
			}

			m_stats.Add(updateStats);
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
			m_buckets[m_currentBucketIdx].EndUpdate();
			m_currentBucketIdx = (m_currentBucketIdx + 1) % s_bucketCount;
			m_bInFrame         = false;
			return true;
		}
		return false;
	}

	////////////////////////////////////////////////////////////////
	void CUpdateScheduler::VerifyCleanup() {}

	////////////////////////////////////////////////////////////////
	void CUpdateScheduler::Reset()
	{
		for(size_t bucketIdx = 0; bucketIdx < s_bucketCount; ++ bucketIdx)
		{
			m_buckets[bucketIdx].Reset();
			m_frameTimes[bucketIdx] = 0.0f;
		}
		m_currentBucketIdx = 0;
		m_bInFrame         = false;
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

	//////////////////////////////////////////////////////////////////////////
	bool CUpdateScheduler::ConnectInternal(CUpdateScope& scope, const UpdateCallback& callback, UpdateFrequency frequency /*= EUpdateFrequency::EveryFrame*/, UpdatePriority priority /*= EUpdatePriority::Default*/, const UpdateFilter& filter /*= UpdateFilter()*/)
	{
		DisconnectInternal(scope);
		if ((frequency >= EUpdateFrequency::EveryFrame) && (frequency < EUpdateFrequency::Count))
		{
			// Find group of buckets containing least observers. 
			const size_t bucketStride = size_t(1) << frequency;
			size_t       firstBucketIdx = 0;
			if (frequency > EUpdateFrequency::EveryFrame)
			{
				size_t bucketGroupSize[s_bucketCount] = {};
				for (size_t bucketIdx = 0; bucketIdx < s_bucketCount; ++bucketIdx)
				{
					bucketGroupSize[bucketIdx % bucketStride] += m_buckets[bucketIdx].GetSize();
				}
				for (size_t bucketGroupIdx = 1; bucketGroupIdx < bucketStride; ++bucketGroupIdx)
				{
					if (bucketGroupSize[bucketGroupIdx] < bucketGroupSize[firstBucketIdx])
					{
						firstBucketIdx = bucketGroupIdx;
					}
				}
			}
			// Connect to slot and to appropriate buckets.
			m_slots.push_back(SSlot(&scope, static_cast<uint32>(firstBucketIdx), frequency, priority));
			for (CBucket* pBucket = m_buckets + firstBucketIdx, *pEndBucket = m_buckets + s_bucketCount; pBucket < pEndBucket; pBucket += bucketStride)
			{
				//CryLogAlways("Entity name %s: Pos:%f, %f, %f", filter.)
				pBucket->Connect(scope, callback, frequency, priority, filter);
			}
			scope.m_bIsBound = true;
			return true;
		}
		return false;
	}

	////////////////////////////////////////////////////////////////
	CUpdateScheduler::SSlot::SSlot()
		: pScope(NULL)
		, firstBucketIdx(s_invalidBucketIdx)
		, frequency(EUpdateFrequency::Invalid)
		, priority(EUpdatePriority::Default)
	{}

	////////////////////////////////////////////////////////////////
	CUpdateScheduler::SSlot::SSlot(CUpdateScope* _pScope, uint32 _firstBucketIdx, UpdateFrequency _frequency, UpdatePriority _priority)
		: pScope(_pScope)
		, firstBucketIdx(_firstBucketIdx)
		, frequency(_frequency)
		, priority(_priority)
	{}

	////////////////////////////////////////////////////////////////
	CUpdateScheduler::SObserver::SObserver(const CUpdateScope* _pScope, const UpdateCallback& _callback, UpdateFrequency _frequency, UpdatePriority _priority, const UpdateFilter& _filter)
		: pScope(_pScope)
		, callback(_callback)
		, frequency(_frequency)
		, currentPriority(_priority)
		, newPriority(_priority)
		, filter(_filter)
	{}

	//////////////////////////////////////////////////////////////////////////
	CUpdateScheduler::SFrameUpdateStats::SFrameUpdateStats()
		: count(0)
	{}

	//////////////////////////////////////////////////////////////////////////
	void CUpdateScheduler::SFrameUpdateStats::Add(const UpdateSchedulerStats::SUpdateStageStats& stats)
	{
		if (count < k_maxStagesCount)
		{
			stagesStats[count] = stats;
			++count;
		}
	}

	//////////////////////////////////////////////////////////////////////////
	void CUpdateScheduler::SFrameUpdateStats::Reset()
	{
		count = 0;
	}

	//////////////////////////////////////////////////////////////////////////
	const UpdateSchedulerStats::SUpdateStageStats* CUpdateScheduler::SFrameUpdateStats::Get(size_t& outCount) const
	{
		outCount = count;
		return stagesStats;
	}

	////////////////////////////////////////////////////////////////
	CUpdateScheduler::CBucket::CBucket()
		: m_dirtyCount(0)
		, m_garbageCount(0)
		, m_pos(0)
	{
		m_observers.reserve(s_defaultBucketSize);
	}

	////////////////////////////////////////////////////////////////
	void CUpdateScheduler::CBucket::Connect(CUpdateScope& scope, const UpdateCallback& callback, UpdateFrequency frequency, UpdatePriority priority, const UpdateFilter& filter)
	{
		m_observers.push_back(SObserver(&scope, callback, frequency, priority, filter));
		++ m_dirtyCount;
	}

	////////////////////////////////////////////////////////////////
	void CUpdateScheduler::CBucket::Disconnect(CUpdateScope& scope, UpdatePriority priority)
	{
		SObserver* pObserver = FindObserver(scope, priority);
		CRY_ASSERT(pObserver);
		if(pObserver)
		{
			pObserver->pScope      = nullptr;
			pObserver->callback    = UpdateCallback();
			pObserver->frequency   = EUpdateFrequency::Invalid;
			pObserver->newPriority = EUpdatePriority::Dirty;
			pObserver->filter      = UpdateFilter();
			++ m_garbageCount;
		}
	}

	////////////////////////////////////////////////////////////////
	void CUpdateScheduler::CBucket::BeginUpdate()
	{
		// Sort observer bindings in order of priority and memory address.
		// The sort process automatically pushes dirty observer bindings to the back so they can be erased in a single block.
		if((m_dirtyCount > 0) || (m_garbageCount > 0))
		{
			ObserverVector::iterator itBeginObserver = m_observers.begin();
			ObserverVector::iterator itEndObserver = m_observers.end();
			for(ObserverVector::iterator itObserver = itBeginObserver; itObserver != itEndObserver; ++ itObserver)
			{
				SObserver &observer = *itObserver;
				observer.currentPriority = observer.newPriority;
			}
			std::sort(itBeginObserver, itEndObserver, SSortObservers());
			if(m_garbageCount > 0)
			{
				m_observers.erase(itEndObserver - m_garbageCount, itEndObserver);
			}
			m_dirtyCount   = 0;
			m_garbageCount = 0;
		}
	}

	////////////////////////////////////////////////////////////////
	void CUpdateScheduler::CBucket::Update(const SUpdateContext (&frequencyUpdateContexts)[EUpdateFrequency::Count], UpdatePriority beginPriority, UpdatePriority endPriority, UpdateSchedulerStats::SUpdateStageStats& outUpdateStats)
	{
		const size_t startPos = m_pos;
		size_t updatedCount = 0;

		// Update all remaining observers if valid and not filtered.
		const size_t endPos = m_observers.size() - m_dirtyCount;
		for( ; m_pos != endPos; ++ m_pos)
		{
			const SObserver &observer = m_observers[m_pos];
			if(observer.currentPriority <= endPriority)
			{
				break;
			}
			else if(observer.currentPriority <= beginPriority)
			{
				if(observer.callback && (!observer.filter || observer.filter()))
				{
					observer.callback(frequencyUpdateContexts[observer.frequency]);
					updatedCount++;
				}
			}
		}

		outUpdateStats.itemsToUpdateCount += (m_pos - startPos);
		outUpdateStats.updatedItemsCount += updatedCount;
	}

	////////////////////////////////////////////////////////////////
	void CUpdateScheduler::CBucket::EndUpdate()
	{
		m_pos = 0;
	}

	////////////////////////////////////////////////////////////////
	void CUpdateScheduler::CBucket::Reset()
	{
		m_observers.clear();
		m_dirtyCount   = 0;
		m_garbageCount = 0;
		m_pos          = 0;
	}

	////////////////////////////////////////////////////////////////
	CUpdateScheduler::SObserver* CUpdateScheduler::CBucket::FindObserver(CUpdateScope& scope, UpdatePriority priority)
	{
		ObserverVector::iterator itBeginObserver = m_observers.begin();
		ObserverVector::iterator itEndObserver = m_observers.end();
		ObserverVector::iterator itFirstDirtyObserver = itEndObserver - m_dirtyCount;
		ObserverVector::iterator itObserver = std::lower_bound(itBeginObserver, itFirstDirtyObserver, priority, SLowerBoundObserver());
		if(itObserver != itFirstDirtyObserver)
		{
			for( ; itObserver->currentPriority == priority; ++ itObserver)
			{
				SObserver &observer = *itObserver;
				if(observer.pScope == &scope)
				{
					return &observer;
				}
			}
		}
		for(itObserver = itFirstDirtyObserver; itObserver != itEndObserver; ++ itObserver)
		{
			SObserver &observer = *itObserver;
			if(observer.pScope == &scope)
			{
				return &observer;
			}
		}
		return nullptr;
	}
}
