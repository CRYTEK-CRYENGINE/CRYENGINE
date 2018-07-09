// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include <CryAISystem/IAISystem.h>

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace StdLib
	{

		//===================================================================================
		//
		// CItemMonitor_NavMeshChangesInAABB
		//
		//===================================================================================

		CItemMonitor_NavMeshChangesInAABB::CItemMonitor_NavMeshChangesInAABB(NavigationAgentTypeID agentTypeID)
			: m_agentTypeID(agentTypeID)
			, m_areaToMonitor(AABB::RESET)
			, m_bCorruptionOccurred(false)
		{
			gEnv->pAISystem->GetNavigationSystem()->AddMeshChangeCallback(m_agentTypeID, functor(*this, &CItemMonitor_NavMeshChangesInAABB::OnNavMeshChanged));
		}

		CItemMonitor_NavMeshChangesInAABB::~CItemMonitor_NavMeshChangesInAABB()
		{
			gEnv->pAISystem->GetNavigationSystem()->RemoveMeshChangeCallback(m_agentTypeID, functor(*this, &CItemMonitor_NavMeshChangesInAABB::OnNavMeshChanged));
		}

		void CItemMonitor_NavMeshChangesInAABB::AddPointToMonitoredArea(const Vec3& pointToCover)
		{
			if (m_areaToMonitor.IsReset())
			{
				m_areaToMonitor.min = m_areaToMonitor.max = pointToCover;
			}
			else
			{
				m_areaToMonitor.Add(pointToCover);
			}
		}

		Client::IItemMonitor::EHealthState CItemMonitor_NavMeshChangesInAABB::UpdateAndCheckForCorruption(Shared::IUqsString& outExplanationInCaseOfCorruption)
		{
			if (m_bCorruptionOccurred)
			{
				outExplanationInCaseOfCorruption.Format("Item corruption occurred due to a nav-mesh change");
				return EHealthState::CorruptionOccurred;
			}
			else
			{
				return EHealthState::StillHealthy;
			}
		}

		void CItemMonitor_NavMeshChangesInAABB::OnNavMeshChanged(NavigationAgentTypeID agentTypeID, NavigationMeshID meshID, uint32 tileID)
		{
			AABB tileAABB;
			gEnv->pAISystem->GetNavigationSystem()->GetTileBoundsForMesh(meshID, tileID, tileAABB);

			// add a small buffer to the tile's AABB to compensate for numerical noise
			tileAABB.Expand(Vec3(0.1f, 0.1f, 0.1f));

			if (tileAABB.IsIntersectBox(m_areaToMonitor))
			{
				m_bCorruptionOccurred = true;
			}
		}
	}
}
