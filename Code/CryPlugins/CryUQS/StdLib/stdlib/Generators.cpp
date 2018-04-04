// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

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
				const size_t numTrianglesFound = pNavigationSystem->GetTriangleCenterLocationsInMesh(meshID, m_params.pivot.value, aabb, triangleCenters, maxTrianglesToLookFor);

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

	}
}
