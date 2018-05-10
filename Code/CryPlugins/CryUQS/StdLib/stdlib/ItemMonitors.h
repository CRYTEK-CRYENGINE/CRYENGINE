// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryAISystem/Serialization/NavigationSerialize.h>

// *INDENT-OFF* - <hard to read code and declarations due to inconsistent indentation>

namespace UQS
{
	namespace StdLib
	{

		//===================================================================================
		//
		// CItemMonitor_NavMeshChangesInAABB
		//
		// - monitors for changes in the navigation mesh that affect a given AABB
		// - changes within that AABB count as corruption of the reasoning space
		//
		//===================================================================================

		class CItemMonitor_NavMeshChangesInAABB : public UQS::Client::IItemMonitor
		{
		public:
			explicit               CItemMonitor_NavMeshChangesInAABB(NavigationAgentTypeID agentTypeID);
			                       ~CItemMonitor_NavMeshChangesInAABB();
			void                   AddPointToMonitoredArea(const Vec3& pointToCover);

			// IItemMonitor
			virtual EHealthState   UpdateAndCheckForCorruption(Shared::IUqsString& outExplanationInCaseOfCorruption) override;
			// ~IItemMonitor

		private:
			void                   OnNavMeshChanged(NavigationAgentTypeID agentTypeID, NavigationMeshID meshID, uint32 tileID);

		private:
			NavigationAgentTypeID  m_agentTypeID;
			AABB                   m_areaToMonitor;
			bool                   m_bCorruptionOccurred;
		};

	}
}
