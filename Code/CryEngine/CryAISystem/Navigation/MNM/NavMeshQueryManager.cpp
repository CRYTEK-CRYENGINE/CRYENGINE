// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "NavMeshQueryManager.h"
#include "NavMeshQuery.h"
#include "Navigation/NavigationSystem/NavigationSystem.h"
#include <CrySystem/ConsoleRegistration.h>

namespace MNM
{
	//====================================================================
	// CNavMeshQueryManager
	//====================================================================

	const char* s_szFilterByCallerCvarName = "ai_DebugDrawNavigationQueryListFilterByCaller";
	const char* s_szFilterByCallerCvarNameDefaultValue = "None";

	CNavMeshQueryManager::CNavMeshQueryManager()
		: m_queryIdCount(0)
	{
		RegisterCVars();
	}

	CNavMeshQueryManager::~CNavMeshQueryManager()
	{
		UnregisterCVars();
	}

	void CNavMeshQueryManager::Clear()
	{
#ifdef NAV_MESH_QUERY_DEBUG
		m_queriesDebug.clear();
#endif // NAV_MESH_QUERY_DEBUG
	}

	INavMeshQueryPtr CNavMeshQueryManager::CreateQueryBatch(const INavMeshQuery::SNavMeshQueryConfigBatch& queryConf)
	{
		INavMeshQueryPtr pQuery = std::shared_ptr<CNavMeshQueryBatch>(
			new CNavMeshQueryBatch(m_queryIdCount++, queryConf),
			[&](CNavMeshQueryBatch* pNavMeshQuery)
		{
#ifdef NAV_MESH_QUERY_DEBUG
			m_queriesDebug.erase(pNavMeshQuery);
#endif // NAV_MESH_QUERY_DEBUG
			pNavMeshQuery->~CNavMeshQueryBatch();
		});
#ifdef NAV_MESH_QUERY_DEBUG
		m_queriesDebug.insert(pQuery.get());
#endif // NAV_MESH_QUERY_DEBUG

		return pQuery;
	}

	INavMeshQuery::EQueryStatus CNavMeshQueryManager::RunInstantQuery(const INavMeshQuery::SNavMeshQueryConfigInstant& queryConf, INavMeshQueryProcessing& queryProcessing, const size_t processingBatchSize /*= 1024 */)  
	{
		CNavMeshQueryInstant queryInstant(++m_queryIdCount, queryConf);
		const INavMeshQuery::EQueryStatus result = queryInstant.Run(queryProcessing, processingBatchSize);
		return result;
	}

#ifdef NAV_MESH_QUERY_DEBUG
	const INavMeshQueryManager::INavMeshQueriesPtrSet& CNavMeshQueryManager::GetQueriesDebug() const
	{
		return m_queriesDebug;
	}

	void CNavMeshQueryManager::DebugDrawQueriesList() const
	{
		if (!gAIEnv.CVars.navigation.DebugDrawNavigationQueriesList)
		{
			return;
		}

		// Setup
		const ColorF titleColor = Col_Red;
		const ColorF queryColor = Col_White;
		const ColorF batchColor = Col_Grey;
		const ColorF invalidationColor = Col_Red;


		const float spacingQuery = 20.0f;
		const float spacingBatch = 15.0f;

		const float headerSize = 1.6f;
		const float textQuerySize = 1.4f;
		const float textBatchSize = 1.25f;

		IRenderer* pRenderer = gEnv->pRenderer;
		IRenderAuxGeom* pAux = pRenderer->GetIRenderAuxGeom();

		SAuxGeomRenderFlags flags = pAux->GetRenderFlags();
		SAuxGeomRenderFlags renderFlagsRestore = flags;

		flags.SetMode2D3DFlag(e_Mode2D);
		flags.SetDrawInFrontMode(e_DrawInFrontOn);
		flags.SetDepthTestFlag(e_DepthTestOff);
		pAux->SetRenderFlags(flags);
		pAux->SetRenderFlags(renderFlagsRestore);

		const int screenX = 0;
		const int screenY = 0;
		const int screenWidth = pAux->GetCamera().GetViewSurfaceX();
		const int screenHeight = pAux->GetCamera().GetViewSurfaceZ();
		const Vec2 viewportOrigin(static_cast <float> (screenX), static_cast <float> (screenY));
		const Vec2 viewportSize(static_cast <float> (screenWidth), static_cast <float> (screenHeight));

		const ICVar* filterCvar = gEnv->pConsole->GetCVar(s_szFilterByCallerCvarName);
		const bool cvarEnabled = filterCvar && filterCvar->GetString() != nullptr && filterCvar->GetString()[0] != 0 && stricmp(filterCvar->GetString(), s_szFilterByCallerCvarNameDefaultValue) != 0;
		const char* szFilter = cvarEnabled ? filterCvar->GetString() : s_szFilterByCallerCvarNameDefaultValue;

		Vec2 screenPosition = Vec2(10, 10);
		IRenderAuxText::Draw2dLabel(screenPosition.x, screenPosition.y, headerSize, titleColor, false, "Navigation Queries List (filter = %s)", szFilter);

		// Draw queries
		for (auto it = m_queriesDebug.rbegin(); it != m_queriesDebug.rend(); ++it)
		{
			const INavMeshQuery* activeQuery = *it;

			if (!cvarEnabled || CryStringUtils::stristr(activeQuery->GetQueryConfig().szCallerName, szFilter) != nullptr)
			{
				screenPosition.y += spacingQuery;
				const INavMeshQueryDebug& queryDebug = activeQuery->GetQueryDebug();
				const INavMeshQueryDebug::SQueryDebugData& debugData = queryDebug.GetDebugData();

				IRenderAuxText::Draw2dLabel(screenPosition.x, screenPosition.y, textQuerySize, queryColor, false, "ID: %u \tBATCHES: %zu \tTRIANGLES: %zu \tDone: %d \tValid: %d \tTIME(RUNNING): %2.5f ms \tTIME(ALIVE): %2.5f ms \tCALLER: %s", activeQuery->GetId(), static_cast<size_t>(debugData.batchHistory.size()), debugData.trianglesCount, activeQuery->IsDone(), activeQuery->IsValid(), debugData.elapsedTimeRunningInMs, debugData.elapsedTimeTotalInMs, activeQuery->GetQueryConfig().szCallerName);

				// Draw queries batches
				for (const INavMeshQueryDebug::SBatchData& batchData : debugData.batchHistory)
				{
					screenPosition.y += spacingBatch;
					IRenderAuxText::Draw2dLabel(screenPosition.x, screenPosition.y, textBatchSize, batchColor, false, "\t- N: %u \tSIZE: %zu \tTRIANGLES: %zu \tTIME (RUNNING): %2.5f ms", batchData.batchNumber, batchData.batchSize, batchData.triangleDataArray.size(), batchData.elapsedTimeInMs);
				}

				// Draw invalidations
				for (const INavMeshQueryDebug::SInvalidationData& invalidationData : debugData.invalidationsHistory)
				{
					screenPosition.y += spacingBatch;
					stack_string invalidationType;
					if (invalidationData.invalidationType == INavMeshQueryDebug::EInvalidationType::NavMeshRegeneration)
					{
						invalidationType = "NavMesh Regeneration";
					}
					else
					{
						invalidationType = "NavMesh Annotation Changed";
					}
					IRenderAuxText::Draw2dLabel(screenPosition.x, screenPosition.y, textBatchSize, invalidationColor, false, "\t- %s on tile: %u", invalidationType.c_str(), invalidationData.tileId);
				}
			}
		}
	}

#endif // NAV_MESH_QUERY_DEBUG
	void CNavMeshQueryManager::RegisterCVars() const
	{
		ICVar* filterCvar = REGISTER_STRING(s_szFilterByCallerCvarName, nullptr, VF_NULL, 
			"Navigation Queries list with only display those queries that contain the given filter string \n"
			"By default value is set to None (no filter is applied).");
		filterCvar->SetFromString(s_szFilterByCallerCvarNameDefaultValue);
	}

	void CNavMeshQueryManager::UnregisterCVars() const
	{
		IConsole* pConsole = gEnv->pConsole;
		pConsole->UnregisterVariable(s_szFilterByCallerCvarName, true);
	}
}