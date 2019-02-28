// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include <CryAISystem/IAISystem.h>
#include <CryAISystem/INavigationSystem.h>

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace StdLib
	{

		void CStdLibRegistration::InstantiateGeneratorFactoriesForRegistration()
		{
			{
				Client::CGeneratorFactory<CGenerator_PointsOnPureGrid>::SCtorParams ctorParams;

				ctorParams.szName = "std::PointsOnPureGrid";
				ctorParams.guid = "498bce51-a2b9-4e77-b0f9-e127e8a5cc72"_cry_guid;
				ctorParams.szDescription =
					"Generates points on a grid.\n"
					"The grid is specified by a center, size (of one edge) and a spacing between the points.\n"
					"Notice: this generator is very limited and doesn't work well if you intend to use it for things like uneven terrain.";

				static const Client::CGeneratorFactory<CGenerator_PointsOnPureGrid> generatorFactory_PointsOnPureGrid(ctorParams);
			}

			{
				Client::CGeneratorFactory<CGenerator_PointsOnNavMesh>::SCtorParams ctorParams;

				ctorParams.szName = "std::PointsOnNavMesh";
				ctorParams.guid = "70b13c46-e012-4fde-bba5-70ccd4f8b321"_cry_guid;
				ctorParams.szDescription =
					"Generates Pos3s on the navigation mesh limited by an AABB.\n"
					"The AABB is defined by a pivot and local mins and maxs.\n"
					"The pivot has to reside within the enclosing volume of a NavMesh or else no points will be generated.\n"
					"Each of the resulting points will reside on the center of the corresponding NavMesh triangle.";

				static const Client::CGeneratorFactory<CGenerator_PointsOnNavMesh> generatorFactory_PointsOnNavMesh(ctorParams);
			}

			{
				Client::CGeneratorFactory<CGenerator_PointsOnGridProjectedOntoNavMesh>::SCtorParams ctorParams;

				ctorParams.szName = "std::PointsOnGridProjectedOntoNavMesh";
				ctorParams.guid = "91486c28-ff91-4bf2-86e4-7b0c63e30e54"_cry_guid;
				ctorParams.szDescription =
					"Generates points on a grid which are then projected onto the navigation mesh.\n"
					"The grid is specified by a center, size (of one edge) and a spacing between the points.\n"
					"This generator works quite well slopes and hills on the terrain.";

				static const Client::CGeneratorFactory<CGenerator_PointsOnGridProjectedOntoNavMesh> generatorFactory_PointsOnGridProjectedOntoNavMesh(ctorParams);
			}
		}

		//===================================================================================
		//
		// CGenerator_PointsOnPureGrid
		//
		//===================================================================================

		CGenerator_PointsOnPureGrid::CGenerator_PointsOnPureGrid(const SParams& params)
			: m_params(params)
		{
			// nothing
		}

		Client::IGenerator::EUpdateStatus CGenerator_PointsOnPureGrid::DoUpdate(const SUpdateContext& updateContext, Client::CItemListProxy_Writable<Pos3>& itemListToPopulate)
		{
			if (m_params.size <= 0.0f)
			{
				updateContext.error.Format("Bad 'size' param: %f (should be > 0.0)", m_params.size);
				return EUpdateStatus::ExceptionOccurred;
			}

			if (m_params.spacing <= 0.0f)
			{
				updateContext.error.Format("Bad 'spacing' param: %f (should be > 0.0)", m_params.spacing);
				return EUpdateStatus::ExceptionOccurred;
			}

			const float halfSize = (float)m_params.size * 0.5f;

			const float minX = m_params.center.value.x - halfSize;
			const float maxX = m_params.center.value.x + halfSize;

			const float minY = m_params.center.value.y - halfSize;
			const float maxY = m_params.center.value.y + halfSize;

			std::vector<Vec3> tmpItems;

			for (float x = minX; x <= maxX; x += m_params.spacing)
			{
				for (float y = minY; y <= maxY; y += m_params.spacing)
				{
					tmpItems.emplace_back(x, y, m_params.center.value.z);
				}
			}

			itemListToPopulate.CreateItemsByItemFactory(tmpItems.size());
			for (size_t i = 0; i < tmpItems.size(); ++i)
			{
				itemListToPopulate.GetItemAtIndex(i).value = tmpItems[i];
			}

			return EUpdateStatus::FinishedGeneratingItems;
		}

		//===================================================================================
		//
		// CGenerator_PointsOnNavMesh
		//
		//===================================================================================

		CGenerator_PointsOnNavMesh::CGenerator_PointsOnNavMesh(const SParams& params)
			: m_params(params)
		{
			// nothing
		}

		Client::IGenerator::EUpdateStatus CGenerator_PointsOnNavMesh::DoUpdate(const SUpdateContext& updateContext, Client::CItemListProxy_Writable<Pos3>& itemListToPopulate)
		{
			INavigationSystem* pNavigationSystem = gEnv->pAISystem->GetNavigationSystem();

			if (const NavigationMeshID meshID = pNavigationSystem->GetEnclosingMeshID(m_params.navigationAgentTypeID, m_params.pivot.value))
			{
				static const size_t maxTrianglesToLookFor = 1024;

				const AABB aabb(m_params.pivot.value + m_params.localAABBMins.value, m_params.pivot.value + m_params.localAABBMaxs.value);
				Vec3 triangleCenters[maxTrianglesToLookFor];

				//TODO: Get Navmesh query filter from somewhere
				INavMeshQueryFilter* pQueryFilter = nullptr;
				const size_t numTrianglesFound = pNavigationSystem->GetTriangleCenterLocationsInMesh(meshID, m_params.pivot.value, aabb, triangleCenters, maxTrianglesToLookFor, pQueryFilter);

				if (numTrianglesFound > 0)
				{
					// warn if we hit the triangle limit (we might produce "incomplete" data)
					if (numTrianglesFound == maxTrianglesToLookFor)
					{
						CryWarning(VALIDATOR_MODULE_UNKNOWN, VALIDATOR_WARNING, "UQS: CGenerator_PointsOnNavMesh::DoUpdate: reached the triangle limit of %i (there might be more triangles that we cannot use)", (int)maxTrianglesToLookFor);
					}

					//
					// populate the given item-list and install an item-monitor that checks for NavMesh changes
					//

					std::unique_ptr<CItemMonitor_NavMeshChangesInAABB> pItemMonitorNavMeshChanges(new CItemMonitor_NavMeshChangesInAABB(m_params.navigationAgentTypeID));
					itemListToPopulate.CreateItemsByItemFactory(numTrianglesFound);

					for (size_t i = 0; i < numTrianglesFound; ++i)
					{
						itemListToPopulate.GetItemAtIndex(i).value = triangleCenters[i];
						pItemMonitorNavMeshChanges->AddPointToMonitoredArea(triangleCenters[i]);
					}

					Core::IHubPlugin::GetHub().GetQueryManager().AddItemMonitorToQuery(updateContext.queryID, std::move(pItemMonitorNavMeshChanges));
				}
			}

			// debug-persist the AABB
			IF_UNLIKELY(updateContext.blackboard.pDebugRenderWorldPersistent)
			{
				updateContext.blackboard.pDebugRenderWorldPersistent->AddAABB(AABB(m_params.pivot.value + m_params.localAABBMins.value, m_params.pivot.value + m_params.localAABBMaxs.value), Col_White);
			}

			return EUpdateStatus::FinishedGeneratingItems;
		}

		//===================================================================================
		//
		// CGenerator_PointsOnGridProjectedOntoNavMesh
		//
		//===================================================================================

		CGenerator_PointsOnGridProjectedOntoNavMesh::CGenerator_PointsOnGridProjectedOntoNavMesh(const SParams& params)
			: m_params(params)
			, m_bSetupPending(true)
			, m_numCellsOnOneAxis(0)
			, m_startPosX(0.0f)
			, m_startPosY(0.0f)
			, m_debugRunawayCounter(0)
		{
		}

		bool CGenerator_PointsOnGridProjectedOntoNavMesh::Setup(const SUpdateContext& updateContext)
		{
			if (m_params.size <= 0.0f)
			{
				updateContext.error.Format("Bad 'size' param: %f (should be > 0.0)", m_params.size);
				return false;
			}

			if (m_params.spacing <= 0.0f)
			{
				updateContext.error.Format("Bad 'spacing' param: %f (should be > 0.0)", m_params.spacing);
				return false;
			}

			if (m_params.verticalTolerance <= 0.0f)
			{
				updateContext.error.Format("Bad 'verticalTolerance' param: %f (should be > 0.0)", m_params.verticalTolerance);
				return false;
			}

			m_numCellsOnOneAxis = std::max((int)(m_params.size / m_params.spacing), 1);
			m_startPosX = m_params.center.value.x - (m_numCellsOnOneAxis / 2) * m_params.spacing;
			m_startPosY = m_params.center.value.y - (m_numCellsOnOneAxis / 2) * m_params.spacing;
			m_grid.resize(m_numCellsOnOneAxis * m_numCellsOnOneAxis);
			m_visited.resize(m_numCellsOnOneAxis * m_numCellsOnOneAxis, false);
			m_parents.resize(m_numCellsOnOneAxis * m_numCellsOnOneAxis, -1);

			//
			// start flooding from the center of the grid
			//

			const int centerIndexX = m_numCellsOnOneAxis / 2;
			const int centerIndexY = m_numCellsOnOneAxis / 2;
			const int compressedCenterIndex = centerIndexY * m_numCellsOnOneAxis + centerIndexX;

			m_openList.push_back(compressedCenterIndex);
			m_visited[compressedCenterIndex] = true;

			return true;
		}

		Client::IGenerator::EUpdateStatus CGenerator_PointsOnGridProjectedOntoNavMesh::Flood(const SUpdateContext& updateContext, Client::CItemListProxy_Writable<Pos3>& itemListToPopulate)
		{
			while (!m_openList.empty())
			{
				PerformOneFloodStep(updateContext);

				if (updateContext.blackboard.timeBudget.IsExhausted())
					break;
			}

			if (m_openList.empty())
			{
				itemListToPopulate.CreateItemsByItemFactory(m_successfullyProjectedPoints.size());
				for (size_t i = 0; i < m_successfullyProjectedPoints.size(); i++)
				{
					itemListToPopulate.GetItemAtIndex(i) = m_successfullyProjectedPoints[i];
				}

				return EUpdateStatus::FinishedGeneratingItems;
			}
			else
			{
				return EUpdateStatus::StillGeneratingItems;
			}
		}

		void CGenerator_PointsOnGridProjectedOntoNavMesh::PerformOneFloodStep(const SUpdateContext& updateContext)
		{
			assert(!m_openList.empty());

			const int compressedGridIndex = m_openList.front();
			const int gridIndexX = compressedGridIndex % m_numCellsOnOneAxis;
			const int gridIndexY = compressedGridIndex / m_numCellsOnOneAxis;

			m_openList.pop_front();

			// compute the x/y position of this cell (the z-position will be computed via a hint from our potential parent)

			Pos3& cellPos = m_grid[compressedGridIndex];
			cellPos.value.x = m_startPosX + gridIndexX * m_params.spacing;
			cellPos.value.y = m_startPosY + gridIndexY * m_params.spacing;

			// if we have a parent (i. e. if we're not the center grid cell), then use the parent's z-pos as a hint for projecting the point onto the navmesh

			const int compressedParentIndex = m_parents[compressedGridIndex];

			if (compressedParentIndex != -1)
			{
				cellPos.value.z = m_grid[compressedParentIndex].value.z;
			}
			else
			{
				cellPos.value.z = m_params.center.value.z;
			}

			// DEBUG
			if (updateContext.blackboard.pDebugRenderWorldPersistent)
			{
				// draw debug counter as text (to visualize the order of visiting all cells)
				updateContext.blackboard.pDebugRenderWorldPersistent->AddText(cellPos.value + Vec3(0.0f, 0.0f, 0.5f), 1.5f, Col_Cyan, "%i", m_debugRunawayCounter++);

				// draw arrow from child cell to parent cell
				if (compressedParentIndex != -1)
				{
					Vec3 dir = (m_grid[compressedParentIndex].value - cellPos.value);
					float len = dir.NormalizeSafe(Vec3(0, 0, 0));
					Vec3 start = cellPos.value;
					Vec3 end = m_grid[compressedParentIndex].value;
					updateContext.blackboard.pDebugRenderWorldPersistent->AddDirection(start, end, 0.2f, len * 0.5f, Col_Cyan);
				}
			}

			//
			// attempt to project the cell position onto navmesh
			//

			//TODO: Get Navmesh query filter from somewhere
			INavMeshQueryFilter* pQueryFilter = nullptr;
			Vec3 projectdPosOnNavMesh;
			if (gEnv->pAISystem->GetNavigationSystem()->GetClosestPointInNavigationMesh(m_params.navigationAgentTypeID, cellPos.value, m_params.verticalTolerance, 0.0f, &projectdPosOnNavMesh, pQueryFilter))
			{
				cellPos.value = projectdPosOnNavMesh;
				m_successfullyProjectedPoints.push_back(cellPos);
			}

			//
			// inspect all 8 neighbors
			//

			static const struct
			{
				int x, y;
			} neighborOffsets[8] =
			{
				{ -1, -1 },
				{  0, -1 },
				{  1, -1 },

				{ -1,  0 },
				{  1,  0 },

				{ -1,  1 },
				{  0,  1 },
				{  1,  1 },
			};

			for (int neighbor = 0; neighbor < 8; neighbor++)
			{
				const int neighboredGridIndexX = gridIndexX + neighborOffsets[neighbor].x;
				const int neighboredGridIndexY = gridIndexY + neighborOffsets[neighbor].y;

				// skip this neighbor if outside the grid

				if (neighboredGridIndexX < 0)
					continue;

				if (neighboredGridIndexY < 0)
					continue;

				if (neighboredGridIndexX >= m_numCellsOnOneAxis)
					continue;

				if (neighboredGridIndexY >= m_numCellsOnOneAxis)
					continue;

				// skip this neighbor if already visited

				const int compressedNeighborGridIndex = neighboredGridIndexY * m_numCellsOnOneAxis + neighboredGridIndexX;

				if (m_visited[compressedNeighborGridIndex])
					continue;

				// put this neighbor on the open list

				m_openList.push_back(compressedNeighborGridIndex);

				// and mark it as visited

				m_visited[compressedNeighborGridIndex] = true;
				m_parents[compressedNeighborGridIndex] = compressedGridIndex;
			}
		}

		Client::IGenerator::EUpdateStatus CGenerator_PointsOnGridProjectedOntoNavMesh::DoUpdate(const SUpdateContext& updateContext, Client::CItemListProxy_Writable<Pos3>& itemListToPopulate)
		{
			if (m_bSetupPending)
			{
				if (!Setup(updateContext))
					return Client::IGenerator::EUpdateStatus::ExceptionOccurred;
				m_bSetupPending = false;
			}

			return Flood(updateContext, itemListToPopulate);
		}

	}
}
