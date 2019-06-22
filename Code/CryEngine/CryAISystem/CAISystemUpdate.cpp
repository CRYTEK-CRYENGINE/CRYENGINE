// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/********************************************************************
   -------------------------------------------------------------------------
   File name:   CAISystemUpdate.cpp
   $Id$
   Description: all the update related functionality here

   -------------------------------------------------------------------------
   History:
   - 2007				: Created by Kirill Bulatsev

 *********************************************************************/

#include "StdAfx.h"

#include <stdio.h>

#include <limits>
#include <map>
#include <numeric>
#include <algorithm>

#include <CryPhysics/IPhysics.h>
#include <CryEntitySystem/IEntitySystem.h>
#include <CryScriptSystem/IScriptSystem.h>
#include <Cry3DEngine/I3DEngine.h>
#include <CrySystem/ILog.h>
#include <CrySystem/File/CryFile.h>
#include <CryMath/Cry_Math.h>
#include <CrySystem/ISystem.h>
#include <CrySystem/ITimer.h>
#include <CrySystem/IConsole.h>
#include <CryNetwork/ISerialize.h>
#include <CryAISystem/IAgent.h>
#include <CryAISystem/IAISystemComponent.h>
#include <CryCore/Containers/VectorSet.h>

#include "CAISystem.h"
#include "CryAISystem.h"
#include "AILog.h"
#include "Puppet.h"
#include "AIVehicle.h"
#include "GoalOp.h"
#include "AIPlayer.h"
#include "PipeUser.h"
#include "Leader.h"
#include "AIActions.h"
#include "AICollision.h"
#include "AIRadialOcclusion.h"
#include "CentralInterestManager.h"
#include "StatsManager.h"
#include "AuditionMap/AuditionMap.h"
#include "Group/GroupManager.h"
#include "TargetSelection/TargetTrackManager.h"
#include "BehaviorTree/BehaviorTreeManager.h"
#include "TacticalPointSystem/TacticalPointSystem.h"
#include "Communication/CommunicationManager.h"
#include "Navigation/NavigationSystem/NavigationSystem.h"
#include "CollisionAvoidance/CollisionAvoidanceSystem.h"

#include "DebugDrawContext.h"

 //-----------------------------------------------------------------------------------------------------------
void RemoveNonActors(CAISystem::AIActorSet& actorSet)
{
	for (CAISystem::AIActorSet::iterator it = actorSet.begin(); it != actorSet.end(); )
	{
		IAIObject* pAIObject = it->GetAIObject();
		CRY_ASSERT_TRACE(pAIObject, ("An AI Actors set contains a null entry for object id %d!", it->GetObjectID()));

		// [AlexMcC|29.06.10] We can't trust that this is an AI Actor, because CWeakRef::GetAIObject()
		// doesn't do any type checking. If this weakref becomes invalid, then the id is used by another
		// object (which can happen if we chainload or load a savegame), this might not be an actor anymore.
		const bool bIsActor = pAIObject ? (pAIObject->CastToCAIActor() != NULL) : false;
		CRY_ASSERT_MESSAGE(bIsActor, "A non-actor is in an AI actor set!");

		if (pAIObject && bIsActor)
		{
			++it;
		}
		else
		{
			it = actorSet.erase(it);
		}
	}
}

//-----------------------------------------------------------------------------------------------------------
static bool IsPuppetOnScreen(CPuppet* pPuppet)
{
	IEntity* pEntity = pPuppet->GetEntity();
	if (!pEntity)
		return false;
	IEntityRender* pIEntityRender = pEntity->GetRenderInterface();
	if (!pIEntityRender || !pIEntityRender->GetRenderNode())
		return false;
	int frameDiff = gEnv->nMainFrameID - pIEntityRender->GetRenderNode()->GetDrawFrame();
	if (frameDiff > 2)
		return false;
	return true;
}

//
//-----------------------------------------------------------------------------------------------------------
EPuppetUpdatePriority CAISystem::CalcPuppetUpdatePriority(CPuppet* pPuppet) const
{
	float fMinDistSq = std::numeric_limits<float>::max();
	bool bOnScreen = false;
	const Vec3 pos = pPuppet->GetPos();

	if (gAIEnv.configuration.eCompatibilityMode != ECCM_CRYSIS && gAIEnv.configuration.eCompatibilityMode != ECCM_CRYSIS2)
	{
		// find closest player distance (better than using the camera pos in coop / dedicated server)
		//	and check visibility against all players

		AIObjectOwners::const_iterator ai = gAIEnv.pAIObjectManager->m_Objects.find(AIOBJECT_PLAYER);
		for (; ai != gAIEnv.pAIObjectManager->m_Objects.end() && ai->first == AIOBJECT_PLAYER; ++ai)
		{
			CAIPlayer* pPlayer = CastToCAIPlayerSafe(ai->second.GetAIObject());
			if (pPlayer)
			{
				fMinDistSq = min(fMinDistSq, (pos - pPlayer->GetPos()).GetLengthSquared());

				if (!bOnScreen)
					bOnScreen = (IAIObject::eFOV_Outside != pPlayer->IsPointInFOV(pos, 2.0f));  // double range for this check, real sight range is used below.
			}
		}
	}
	else
	{
		// previous behavior retained for Crysis compatibility
		Vec3 camPos = gEnv->pSystem->GetViewCamera().GetPosition();
		fMinDistSq = Distance::Point_PointSq(camPos, pos);
		bOnScreen = IsPuppetOnScreen(pPuppet);
	}

	// Calculate the update priority of the puppet.
	const float fSightRangeSq = sqr(pPuppet->GetParameters().m_PerceptionParams.sightRange);
	const bool bInSightRange = (fMinDistSq < fSightRangeSq);
	if (bOnScreen)
	{
		return (bInSightRange ? AIPUP_VERY_HIGH : AIPUP_HIGH);
	}
	else
	{
		return (bInSightRange ? AIPUP_MED : AIPUP_LOW);
	}
}

#ifdef CRYAISYSTEM_DEBUG
void CAISystem::TryUpdateDebugFakeDamageIndicators()
{
	#if CRY_PLATFORM_WINDOWS

	CRY_PROFILE_FUNCTION(PROFILE_AI);
	// Delete the debug lines if the debug draw is not on.
	if ((gAIEnv.CVars.DebugDraw == 0))
	{
		m_vecDebugLines.clear();
		m_vecDebugBoxes.clear();
	}

	// Update fake damage indicators
	if (gAIEnv.CVars.legacyDebugDraw.DrawFakeDamageInd > 0)
	{
		for (unsigned i = 0; i < m_DEBUG_fakeDamageInd.size(); )
		{
			m_DEBUG_fakeDamageInd[i].t -= m_frameDeltaTime;
			if (m_DEBUG_fakeDamageInd[i].t < 0)
			{
				m_DEBUG_fakeDamageInd[i] = m_DEBUG_fakeDamageInd.back();
				m_DEBUG_fakeDamageInd.pop_back();
			}
			else
				++i;
		}
		m_DEBUG_screenFlash = max(0.0f, m_DEBUG_screenFlash - m_frameDeltaTime);
	}
	else
	{
		m_DEBUG_fakeDamageInd.clear();
		m_DEBUG_screenFlash = 0.0f;
	}

	#endif // #if CRY_PLATFORM_WINDOWS
}
#endif //CRYAISYSTEM_DEBUG

bool CAISystem::GetUpdateAllAlways() const
{
	bool updateAllAlways = gAIEnv.CVars.UpdateAllAlways != 0;
	return updateAllAlways;
}

struct SSortedPuppetB
{
	SSortedPuppetB(CPuppet* o, float dot, float d) : obj(o), weight(0.f), dot(dot), dist(d) {}
	bool operator<(const SSortedPuppetB& rhs) const { return weight < rhs.weight; }

	float    weight, dot, dist;
	CPuppet* obj;
};

 //===================================================================
 // Subsystems Updates
 //===================================================================

void CAISystem::SubsystemUpdateActionManager()
{
	CRY_PROFILE_FUNCTION(PROFILE_AI)
	CRY_ASSERT(m_pAIActionManager);

	if (m_pAIActionManager)
		m_pAIActionManager->Update();
}

void CAISystem::SubsystemUpdateRadialOcclusionRaycast()
{
	CRY_PROFILE_FUNCTION(PROFILE_AI)
	CAIRadialOcclusionRaycast::UpdateActiveCount();
}

void CAISystem::SubsystemUpdateLightManager()
{
	CRY_PROFILE_FUNCTION(PROFILE_AI)
	m_lightManager.Update(false);
}

void CAISystem::SubsystemUpdateNavigation()
{
	CRY_PROFILE_FUNCTION(PROFILE_AI)
	CRY_ASSERT(m_pNavigation);

	if (m_pNavigation)
		m_pNavigation->Update(m_frameStartTime, m_frameDeltaTime);
}

void CAISystem::SubsystemUpdateBannedSOs()
{
	CRY_PROFILE_FUNCTION(PROFILE_AI)
	CRY_ASSERT(m_pSmartObjectManager);
	PREFAST_ASSUME(m_pSmartObjectManager);
	m_pSmartObjectManager->UpdateBannedSOs(m_frameDeltaTime);
}

void CAISystem::SubsystemUpdateSystemComponents()
{
	CRY_PROFILE_FUNCTION(PROFILE_AI)	
	for (auto& systemComponent : m_setSystemComponents)
	{
		systemComponent->Update(m_frameDeltaTime);
	}
}

void CAISystem::SubsystemUpdateCommunicationManager()
{
	CRY_PROFILE_FUNCTION(PROFILE_AI)
	if (gAIEnv.CVars.legacyCommunicationSystem.CommunicationSystem)
	{
		CRY_ASSERT(gAIEnv.pCommunicationManager);
		gAIEnv.pCommunicationManager->Update(m_frameDeltaTime);
	}
}

void CAISystem::TrySubsystemUpdateVisionMap(const CTimeValue frameStartTime, const float frameDeltaTime, const bool isAutomaticUpdate)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI)
	CRY_ASSERT(gAIEnv.pVisionMap);
	if (!ShouldUpdateSubsystem(IAISystem::ESubsystemUpdateFlag::VisionMap, isAutomaticUpdate))
		return;

	gAIEnv.pVisionMap->Update(frameStartTime, frameDeltaTime);
}

void CAISystem::TrySubsystemUpdateAuditionMap(const CTimeValue frameStartTime, const float frameDeltaTime, const bool isAutomaticUpdate)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI)
	CRY_ASSERT(gAIEnv.pAuditionMap);
	if (!ShouldUpdateSubsystem(IAISystem::ESubsystemUpdateFlag::AuditionMap, isAutomaticUpdate))
		return;
	
	gAIEnv.pAuditionMap->Update(frameStartTime, frameDeltaTime);
}

void CAISystem::SubsystemUpdateGroupManager()
{
	CRY_PROFILE_FUNCTION(PROFILE_AI)
	if (gAIEnv.CVars.legacyGroupSystem.GroupSystem)
	{
		CRY_ASSERT(gAIEnv.pGroupManager);
		gAIEnv.pGroupManager->Update(m_frameDeltaTime);
	}
}

void CAISystem::TrySubsystemUpdateCoverSystem(const CTimeValue frameStartTime, const float frameDeltaTime, const bool isAutomaticUpdate)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI)
	if (gAIEnv.CVars.legacyCoverSystem.CoverSystem)
	{
		if (!ShouldUpdateSubsystem(IAISystem::ESubsystemUpdateFlag::CoverSystem, isAutomaticUpdate))
			return;

		gAIEnv.pCoverSystem->Update(frameStartTime, frameDeltaTime);
	}
}

void CAISystem::TrySubsystemUpdateNavigationSystem(const CTimeValue frameStartTime, const float frameDeltaTime, const bool isAutomaticUpdate)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI)
	CRY_ASSERT(gAIEnv.pNavigationSystem);
	if (!ShouldUpdateSubsystem(IAISystem::ESubsystemUpdateFlag::NavigationSystem, isAutomaticUpdate))
		return;
	
	gAIEnv.pNavigationSystem->Update(frameStartTime, frameDeltaTime, false);
}

void CAISystem::SubsystemUpdatePlayers()
{
	CRY_PROFILE_FUNCTION(PROFILE_AI)
	CRY_ASSERT(gAIEnv.pAIObjectManager);
	AIObjectOwners::const_iterator ai = gAIEnv.pAIObjectManager->m_Objects.find(AIOBJECT_PLAYER);

	for (; ai != gAIEnv.pAIObjectManager->m_Objects.end() && ai->first == AIOBJECT_PLAYER; ++ai)
	{
		CAIPlayer* pPlayer = CastToCAIPlayerSafe(ai->second.GetAIObject());
		if (pPlayer)
			pPlayer->Update(IAIObject::EUpdateType::Full);
	}
}

void CAISystem::SubsystemUpdateGroups()
{
	CRY_PROFILE_FUNCTION(PROFILE_AI)
	const int64 dt = (m_frameStartTime - m_lastGroupUpdateTime).GetMilliSecondsAsInt64();
	if (dt > gAIEnv.CVars.AIUpdateInterval)
	{
		for (AIGroupMap::iterator it = m_mapAIGroups.begin(); it != m_mapAIGroups.end(); ++it)
			it->second->Update();

		m_lastGroupUpdateTime = m_frameStartTime;
	}
}

void CAISystem::TrySubsystemUpdateMovementSystem(const CTimeValue frameStartTime, const float frameDeltaTime, const bool isAutomaticUpdate)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI)
	CRY_ASSERT(gAIEnv.pMovementSystem);
	if (!ShouldUpdateSubsystem(IAISystem::ESubsystemUpdateFlag::MovementSystem, isAutomaticUpdate))
		return;

	gAIEnv.pMovementSystem->Update(frameStartTime, frameDeltaTime);
}


void CAISystem::SubsystemUpdateLeaders()
{
	CRY_PROFILE_FUNCTION(PROFILE_AI)
	const static float leaderUpdateRate(.2f);
	static float leaderNoUpdatedTime(.0f);
	leaderNoUpdatedTime += m_frameDeltaTime;
	if (leaderNoUpdatedTime > leaderUpdateRate)
	{
		leaderNoUpdatedTime = 0.0f;
		AIObjectOwners::iterator aio = gAIEnv.pAIObjectManager->m_Objects.find(AIOBJECT_LEADER);

		for (; aio != gAIEnv.pAIObjectManager->m_Objects.end(); ++aio)
		{
			if (aio->first != AIOBJECT_LEADER)
				break;

			CLeader* pLeader = aio->second.GetAIObject()->CastToCLeader();
			if (pLeader)
				pLeader->Update(IAIObject::EUpdateType::Full);
		}
	}
}

void CAISystem::SubsystemUpdateSmartObjectManager()
{
	CRY_PROFILE_FUNCTION(PROFILE_AI)
	if (m_bUpdateSmartObjects)
	{
		CRY_ASSERT(m_pSmartObjectManager);
		m_pSmartObjectManager->Update();
	}
}

void CAISystem::TrySubsystemUpdateGlobalRayCaster(const CTimeValue frameStartTime, const float frameDeltaTime, const bool isAutomaticUpdate)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI)
	CRY_ASSERT(gAIEnv.pRayCaster);
	if (!gAIEnv.pRayCaster || !ShouldUpdateSubsystem(IAISystem::ESubsystemUpdateFlag::GlobalRaycaster, isAutomaticUpdate))
		return;

	gAIEnv.pRayCaster->SetQuota(gAIEnv.CVars.RayCasterQuota);
	gAIEnv.pRayCaster->Update(frameDeltaTime);
}

void CAISystem::TrySubsystemUpdateGlobalIntersectionTester(const CTimeValue frameStartTime, const float frameDeltaTime, const bool isAutomaticUpdate)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI)
	CRY_ASSERT(gAIEnv.pIntersectionTester);
	if (!gAIEnv.pIntersectionTester || !ShouldUpdateSubsystem(IAISystem::ESubsystemUpdateFlag::GlobalIntersectionTester, isAutomaticUpdate))
		return;

	gAIEnv.pIntersectionTester->SetQuota(gAIEnv.CVars.IntersectionTesterQuota);
	gAIEnv.pIntersectionTester->Update(frameDeltaTime);
}

void CAISystem::TrySubsystemUpdateClusterDetector(const CTimeValue frameStartTime, const float frameDeltaTime, const bool isAutomaticUpdate)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI)
	CRY_ASSERT(gAIEnv.pClusterDetector);
	if (!gAIEnv.pClusterDetector || !ShouldUpdateSubsystem(IAISystem::ESubsystemUpdateFlag::ClusterDetector, isAutomaticUpdate))
		return;

	gAIEnv.pClusterDetector->Update(frameStartTime, frameDeltaTime);
}

void CAISystem::SubsystemUpdateInterestManager()
{
	CRY_PROFILE_FUNCTION(PROFILE_AI)
	ICentralInterestManager* pInterestManager = CCentralInterestManager::GetInstance();
	CRY_ASSERT(pInterestManager);
	if (pInterestManager->Enable(gAIEnv.CVars.legacyInterestSystem.InterestSystem != 0))
		pInterestManager->Update(m_frameDeltaTime);
}

void CAISystem::TrySubsystemUpdateBehaviorTreeManager(const CTimeValue frameStartTime, const float frameDeltaTime, const bool isAutomaticUpdate)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI)
	CRY_ASSERT(gAIEnv.pBehaviorTreeManager);
	if (!gAIEnv.pBehaviorTreeManager || !ShouldUpdateSubsystem(IAISystem::ESubsystemUpdateFlag::BehaviorTreeManager, isAutomaticUpdate))
		return;

	gAIEnv.pBehaviorTreeManager->Update(frameStartTime, frameDeltaTime);
}

// Update asynchronous TPS processing
void CAISystem::SubsystemUpdateTacticalPointSystem()
{
	CRY_PROFILE_FUNCTION(PROFILE_AI)
	if (gAIEnv.CVars.legacyTacticalPointSystem.TacticalPointSystem)
	{
		CRY_ASSERT(gAIEnv.pTacticalPointSystem);
		gAIEnv.pTacticalPointSystem->Update(gAIEnv.CVars.legacyTacticalPointSystem.TacticalPointUpdateTime);
	}
}

void CAISystem::SubsystemUpdateAmbientFire()
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	if (gAIEnv.CVars.legacyFiring.AmbientFireEnable == 0)
		return;

	int64 dt((GetFrameStartTime() - m_lastAmbientFireUpdateTime).GetMilliSecondsAsInt64());
	if (dt < (int)(gAIEnv.CVars.legacyFiring.AmbientFireUpdateInterval * 1000.0f))
		return;

	// Marcio: Update ambient fire towards all players.
	for (AIObjectOwners::const_iterator ai = gAIEnv.pAIObjectManager->m_Objects.find(AIOBJECT_ACTOR), end = gAIEnv.pAIObjectManager->m_Objects.end(); ai != end && ai->first == AIOBJECT_ACTOR; ++ai)
	{
		CAIObject* obj = ai->second.GetAIObject();
		if (!obj || !obj->IsEnabled())
			continue;

		CPuppet* pPuppet = obj->CastToCPuppet();
		if (!pPuppet)
			continue;

		// By default make every AI in ambient fire, only the ones that are allowed to shoot right are set explicitly.
		pPuppet->SetAllowedToHitTarget(false);
	}

	for (AIObjectOwners::const_iterator ai = gAIEnv.pAIObjectManager->m_Objects.find(AIOBJECT_VEHICLE), end = gAIEnv.pAIObjectManager->m_Objects.end(); ai != end && ai->first == AIOBJECT_VEHICLE; ++ai)
	{
		CAIVehicle* obj = CastToCAIVehicleSafe(ai->second.GetAIObject());
		if (!obj || !obj->IsDriverInside())
			continue;
		CPuppet* pPuppet = obj->CastToCPuppet();
		if (!pPuppet)
			continue;

		// By default make every AI in ambient fire, only the ones that are allowed to shoot right are set explicitly.
		pPuppet->SetAllowedToHitTarget(false);
	}

	AIObjectOwners::const_iterator plit = gAIEnv.pAIObjectManager->m_Objects.find(AIOBJECT_PLAYER);
	for (; plit != gAIEnv.pAIObjectManager->m_Objects.end() && plit->first == AIOBJECT_PLAYER; ++plit)
	{
		CAIPlayer* pPlayer = CastToCAIPlayerSafe(plit->second.GetAIObject());
		if (!pPlayer)
			return;

		m_lastAmbientFireUpdateTime = GetFrameStartTime();

		const Vec3& playerPos = pPlayer->GetPos();
		const Vec3& playerDir = pPlayer->GetMoveDir();

		typedef std::vector<SSortedPuppetB> TShooters;
		TShooters shooters;
		shooters.reserve(32);                     // sizeof(SSortedPuppetB) = 16, 512 / 16 = 32, will go in bucket allocator

		float maxDist = 0.0f;

		// Update
		for (AIObjectOwners::const_iterator ai = gAIEnv.pAIObjectManager->m_Objects.find(AIOBJECT_ACTOR), end = gAIEnv.pAIObjectManager->m_Objects.end(); ai != end && ai->first == AIOBJECT_ACTOR; ++ai)
		{
			CAIObject* obj = ai->second.GetAIObject();
			if (!obj->IsEnabled())
				continue;
			CPuppet* pPuppet = obj->CastToCPuppet();
			if (!pPuppet)
				continue;

			CAIObject* pTarget = (CAIObject*)pPuppet->GetAttentionTarget();

			if ((pTarget != NULL) && pTarget->IsAgent())
			{
				if (pTarget == pPlayer)
				{
					Vec3 dirPlayerToPuppet = pPuppet->GetPos() - playerPos;
					float dist = dirPlayerToPuppet.NormalizeSafe();
					if (dist > 0.01f && dist < pPuppet->GetParameters().m_fAttackRange)
					{
						maxDist = max(maxDist, dist);
						float dot = playerDir.Dot(dirPlayerToPuppet);
						shooters.push_back(SSortedPuppetB(pPuppet, dot, dist));
					}
					continue;
				}
			}

			// Shooting something else than player, allow to hit.
			pPuppet->SetAllowedToHitTarget(true);
		}

		// Update
		for (AIObjectOwners::const_iterator ai = gAIEnv.pAIObjectManager->m_Objects.find(AIOBJECT_VEHICLE), end = gAIEnv.pAIObjectManager->m_Objects.end(); ai != end && ai->first == AIOBJECT_VEHICLE; ++ai)
		{
			CAIVehicle* obj = ai->second.GetAIObject()->CastToCAIVehicle();
			if (!obj->IsDriverInside())
				continue;
			CPuppet* pPuppet = obj->CastToCPuppet();
			if (!pPuppet)
				continue;

			CAIObject* pTarget = (CAIObject*)pPuppet->GetAttentionTarget();

			if ((pTarget != NULL) && pTarget->IsAgent())
			{
				if (pTarget == pPlayer)
				{
					Vec3 dirPlayerToPuppet = pPuppet->GetPos() - playerPos;
					float dist = dirPlayerToPuppet.NormalizeSafe();

					if ((dist > 0.01f) && (dist < pPuppet->GetParameters().m_fAttackRange) && pPuppet->AllowedToFire())
					{
						maxDist = max(maxDist, dist);

						float dot = playerDir.Dot(dirPlayerToPuppet);
						shooters.push_back(SSortedPuppetB(pPuppet, dot, dist));
					}

					continue;
				}
			}
			// Shooting something else than player, allow to hit.
			pPuppet->SetAllowedToHitTarget(true);
		}

		if (!shooters.empty() && maxDist > 0.01f)
		{
			// Find nearest shooter
			TShooters::iterator nearestIt = shooters.begin();
			float nearestWeight = sqr((1.0f - nearestIt->dot) / 2) * (0.3f + 0.7f * nearestIt->dist / maxDist);
			;
			for (TShooters::iterator it = shooters.begin() + 1; it != shooters.end(); ++it)
			{
				float weight = sqr((1.0f - it->dot) / 2) * (0.3f + 0.7f * it->dist / maxDist);
				if (weight < nearestWeight)
				{
					nearestWeight = weight;
					nearestIt = it;
				}
			}

			Vec3 dirToNearest = nearestIt->obj->GetPos() - playerPos;
			dirToNearest.NormalizeSafe();

			for (TShooters::iterator it = shooters.begin(); it != shooters.end(); ++it)
			{
				Vec3 dirPlayerToPuppet = it->obj->GetPos() - playerPos;
				float dist = dirPlayerToPuppet.NormalizeSafe();
				float dot = dirToNearest.Dot(dirPlayerToPuppet);
				it->weight = sqr((1.0f - dot) / 2) * (dist / maxDist);
			}

			std::sort(shooters.begin(), shooters.end());

			uint32 i = 0;
			uint32 quota = gAIEnv.CVars.legacyFiring.AmbientFireQuota;

			for (TShooters::iterator it = shooters.begin(); it != shooters.end(); ++it)
			{
				it->obj->SetAllowedToHitTarget(true);
				if ((++i >= quota) && (it->dist > 7.5f)) // Always allow to hit if in 2.5 meter radius
					break;
			}
		}
	}
}

inline bool PuppetFloatSorter(const std::pair<CPuppet*, float>& lhs, const std::pair<CPuppet*, float>& rhs)
{
	return lhs.second < rhs.second;
}

void CAISystem::SubsystemUpdateExpensiveAccessoryQuota()
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);

	for (unsigned i = 0; i < m_delayedExpAccessoryUpdates.size(); )
	{
		SAIDelayedExpAccessoryUpdate& update = m_delayedExpAccessoryUpdates[i];
		update.timeMs -= (int)(m_frameDeltaTime * 1000.0f);

		if (update.timeMs < 0)
		{
			update.pPuppet->SetAllowedToUseExpensiveAccessory(update.state);
			m_delayedExpAccessoryUpdates[i] = m_delayedExpAccessoryUpdates.back();
			m_delayedExpAccessoryUpdates.pop_back();
		}
		else
			++i;
	}

	const int UpdateTimeMs = 3000;
	const float fUpdateTimeMs = (float)UpdateTimeMs;

	int64 dt((GetFrameStartTime() - m_lastExpensiveAccessoryUpdateTime).GetMilliSecondsAsInt64());
	if (dt < UpdateTimeMs)
		return;

	m_lastExpensiveAccessoryUpdateTime = GetFrameStartTime();

	m_delayedExpAccessoryUpdates.clear();

	Vec3 interestPos = gEnv->pSystem->GetViewCamera().GetPosition();

	std::vector<std::pair<CPuppet*, float>> puppets;
	VectorSet<CPuppet*> stateRemoved;

	// Choose the best of each group, then best of the best.
	for (AIGroupMap::iterator it = m_mapAIGroups.begin(), itend = m_mapAIGroups.end(); it != itend; ++it)
	{
		CAIGroup* pGroup = it->second;

		CPuppet* pBestUnit = 0;
		float bestVal = FLT_MAX;

		for (TUnitList::iterator itu = pGroup->GetUnits().begin(), ituend = pGroup->GetUnits().end(); itu != ituend; ++itu)
		{
			CPuppet* pPuppet = CastToCPuppetSafe(itu->m_refUnit.GetAIObject());
			if (!pPuppet)
				continue;
			if (!pPuppet->IsEnabled())
				continue;

			if (pPuppet->IsAllowedToUseExpensiveAccessory())
				stateRemoved.insert(pPuppet);

			const int accessories = pPuppet->GetParameters().m_weaponAccessories;
			if ((accessories & (AIWEPA_COMBAT_LIGHT | AIWEPA_PATROL_LIGHT)) == 0)
				continue;

			if (pPuppet->GetProxy())
			{
				SAIWeaponInfo wi;
				pPuppet->GetProxy()->QueryWeaponInfo(wi);
				if (!wi.hasLightAccessory)
					continue;
			}

			float val = Distance::Point_Point(interestPos, pPuppet->GetPos());

			if (pPuppet->GetAttentionTargetThreat() == AITHREAT_AGGRESSIVE)
				val *= 0.5f;
			else if (pPuppet->GetAttentionTargetThreat() >= AITHREAT_INTERESTING)
				val *= 0.8f;

			if (val < bestVal)
			{
				bestVal = val;
				pBestUnit = pPuppet;
			}
		}

		if (pBestUnit)
		{
			CCCPOINT(UpdateExpensiveAccessoryQuota);
			puppets.push_back(std::make_pair(pBestUnit, bestVal));
		}
	}

	std::sort(puppets.begin(), puppets.end(), PuppetFloatSorter);

	unsigned maxExpensiveAccessories = 3;
	for (unsigned i = 0, ni = puppets.size(); i < ni && i < maxExpensiveAccessories; ++i)
	{
		stateRemoved.erase(puppets[i].first);

		if (!puppets[i].first->IsAllowedToUseExpensiveAccessory())
		{
			//		puppets[i].first->SetAllowedToUseExpensiveAccessory(true);

			int timeMs = (int)(fUpdateTimeMs * 0.5f + cry_random(0.0f, fUpdateTimeMs) * 0.4f);
			m_delayedExpAccessoryUpdates.push_back(SAIDelayedExpAccessoryUpdate(puppets[i].first, timeMs, true));
		}
	}

	for (unsigned i = 0, ni = stateRemoved.size(); i < ni; ++i)
	{
		int timeMs = (int)(cry_random(0.0f, fUpdateTimeMs) * 0.4f);
		m_delayedExpAccessoryUpdates.push_back(SAIDelayedExpAccessoryUpdate(stateRemoved[i], timeMs, false));
	}
}

void CAISystem::SubsystemUpdateActorsAndTargetTrackAndORCA()
{
	CRY_PROFILE_FUNCTION(PROFILE_AI)
		AIAssert(!m_iteratingActorSet);
	m_iteratingActorSet = true;

	RemoveNonActors(m_enabledAIActorsSet);

	AIActorVector& fullUpdates = m_tmpFullUpdates;
	fullUpdates.resize(0);

	AIActorVector& dryUpdates = m_tmpDryUpdates;
	dryUpdates.resize(0);

	AIActorVector& allUpdates = m_tmpAllUpdates;
	allUpdates.resize(0);

	uint32 activeAIActorCount = m_enabledAIActorsSet.size();
	gAIEnv.pStatsManager->SetStat(eStat_ActiveActors, static_cast<float>(activeAIActorCount));

	uint32 fullUpdateCount = 0;
	if (activeAIActorCount > 0)
	{
		const float updateInterval = max(gAIEnv.CVars.AIUpdateInterval, 0.0001f);
		const float updatesPerSecond = (activeAIActorCount / updateInterval) + m_enabledActorsUpdateError;
		unsigned actorUpdateCount = (unsigned)floorf(updatesPerSecond * m_frameDeltaTime);
		if (m_frameDeltaTime > 0.0f)
			m_enabledActorsUpdateError = updatesPerSecond - actorUpdateCount / m_frameDeltaTime;

		uint32 skipped = 0;
		m_enabledActorsUpdateHead %= activeAIActorCount;
		uint32 idx = m_enabledActorsUpdateHead;

		for (uint32 i = 0; i < activeAIActorCount; ++i)
		{
			// [AlexMcC|29.06.10] We can't trust that this is an AI Actor, because CWeakRef::GetAIObject()
			// doesn't do any type checking. If this weakref becomes invalid, then the id is used by another
			// object (which can happen if we chainload or load a savegame), this might not be an actor anymore.

			IAIObject* object = m_enabledAIActorsSet[idx++ % activeAIActorCount].GetAIObject();
			CAIActor* actor = object->CastToCAIActor();
			AIAssert(actor);

			if (actor)
			{
				if (object->GetAIType() != AIOBJECT_PLAYER)
				{
					if (fullUpdates.size() < actorUpdateCount)
						fullUpdates.push_back(actor);
					else
						dryUpdates.push_back(actor);
				}
				else if (fullUpdates.size() < actorUpdateCount)
				{
					++skipped;
				}

				allUpdates.push_back(actor);
			}
		}

		{
			CRY_PROFILE_SECTION(PROFILE_AI, "UpdateActors - Full Updates");

			AIActorVector::iterator it = fullUpdates.begin();
			AIActorVector::iterator end = fullUpdates.end();

			for (; it != end; ++it)
			{
				CAIActor* pAIActor = *it;
				if (CPuppet* pPuppet = pAIActor->CastToCPuppet())
					pPuppet->SetUpdatePriority(CalcPuppetUpdatePriority(pPuppet));

				// Full update
				pAIActor->Update(IAIObject::EUpdateType::Full);

				if (!pAIActor->m_bUpdatedOnce && m_bUpdateSmartObjects && pAIActor->IsEnabled())
				{
					if (CPipeUser* pPipeUser = pAIActor->CastToCPipeUser())
					{
						if (!pPipeUser->GetCurrentGoalPipe() || !pPipeUser->GetCurrentGoalPipe()->GetSubpipe())
						{
							pAIActor->m_bUpdatedOnce = true;
						}
					}
					else
					{
						pAIActor->m_bUpdatedOnce = true;
					}
				}

				m_totalActorsUpdateCount++;

				if (m_totalActorsUpdateCount >= (int)activeAIActorCount)
				{
					// full update cycle finished on all ai objects
					// now allow updating smart objects
					m_bUpdateSmartObjects = true;
					m_totalActorsUpdateCount = 0;
				}

				fullUpdateCount++;
			}

			// CE-1629: special case if there is only a CAIPlayer (and no other CAIActor) to ensure that smart-objects will get updated
			if (fullUpdates.empty() && actorUpdateCount > 0)
			{
				m_bUpdateSmartObjects = true;
			}

			// Advance update head.
			m_enabledActorsUpdateHead += fullUpdateCount;
			m_enabledActorsUpdateHead += skipped;
		}

		{
			CRY_PROFILE_SECTION(PROFILE_AI, "UpdateActors - Dry Updates");

			AIActorVector::iterator it = dryUpdates.begin();
			AIActorVector::iterator end = dryUpdates.end();

			for (; it != end; ++it)
				SingleDryUpdate(*it);
		}

		{
			CRY_PROFILE_SECTION(PROFILE_AI, "UpdateActors - Subsystems Updates");

			for (IAISystemComponent* systemComponent : m_setSystemComponents)
			{
				if (systemComponent->WantActorUpdates(IAIObject::EUpdateType::Full))
				{
					for (CAIActor* pAIActor : fullUpdates)
					{
						systemComponent->ActorUpdate(pAIActor, IAIObject::EUpdateType::Full, m_frameDeltaTime);
					}
				}
				if (systemComponent->WantActorUpdates(IAIObject::EUpdateType::Dry))
				{
					for (CAIActor* pAIActor : dryUpdates)
					{
						systemComponent->ActorUpdate(pAIActor, IAIObject::EUpdateType::Dry, m_frameDeltaTime);
					}
				}
			}
		}

		SubsystemUpdateTargetTrackManager();
	}

	SubsystemUpdateCollisionAvoidanceSystem();

	if (activeAIActorCount > 0)
	{
		CRY_PROFILE_SECTION(PROFILE_AI, "UpdateActors - Proxy Updates");
		{
			{
				AIActorVector::iterator fit = fullUpdates.begin();
				AIActorVector::iterator fend = fullUpdates.end();

				for (; fit != fend; ++fit)
					(*fit)->UpdateProxy(IAIObject::EUpdateType::Full);
			}

			{
				AIActorVector::iterator dit = dryUpdates.begin();
				AIActorVector::iterator dend = dryUpdates.end();

				for (; dit != dend; ++dit)
					(*dit)->UpdateProxy(IAIObject::EUpdateType::Dry);
			}
		}

		gAIEnv.pStatsManager->SetStat(eStat_FullUpdates, static_cast<float>(fullUpdateCount));

		RemoveNonActors(m_disabledAIActorsSet);
	}
	else
	{
		// No active puppets, allow updating smart objects
		m_bUpdateSmartObjects = true;
	}

	RemoveNonActors(m_disabledAIActorsSet);

	if (!m_disabledAIActorsSet.empty())
	{
		uint32 inactiveAIActorCount = m_disabledAIActorsSet.size();
		const float updateInterval = 0.3f;
		const float updatesPerSecond = (inactiveAIActorCount / updateInterval) + m_disabledActorsUpdateError;
		unsigned aiActorDisabledUpdateCount = (unsigned)floorf(updatesPerSecond * m_frameDeltaTime);
		if (m_frameDeltaTime > 0.0f)
			m_disabledActorsUpdateError = updatesPerSecond - aiActorDisabledUpdateCount / m_frameDeltaTime;

		m_disabledActorsHead %= inactiveAIActorCount;
		uint32 idx = m_disabledActorsHead;

		for (unsigned i = 0; (i < aiActorDisabledUpdateCount) && inactiveAIActorCount; ++i)
		{
			// [AlexMcC|29.06.10] We can't trust that this is an AI Actor, because CWeakRef::GetAIObject()
			// doesn't do any type checking. If this weakref becomes invalid, then the id is used by another
			// object (which can happen if we chainload or load a savegame), this might not be an actor anymore.
			IAIObject* object = m_disabledAIActorsSet[idx++ % inactiveAIActorCount].GetAIObject();
			CAIActor* actor = object ? object->CastToCAIActor() : NULL;
			AIAssert(actor);

			actor->UpdateDisabled(IAIObject::EUpdateType::Full);

			// [AlexMcC|28.09.09] UpdateDisabled might remove the puppet from the disabled set, so the size might change
			inactiveAIActorCount = m_disabledAIActorsSet.size();
		}

		// Advance update head.
		m_disabledActorsHead += aiActorDisabledUpdateCount;
	}

	AIAssert(m_iteratingActorSet);
	m_iteratingActorSet = false;
}

void CAISystem::SubsystemUpdateTargetTrackManager()
{
	CRY_PROFILE_FUNCTION(PROFILE_AI)
	CRY_ASSERT(gAIEnv.pTargetTrackManager);
	gAIEnv.pTargetTrackManager->ShareFreshestTargetData();
}

void CAISystem::SubsystemUpdateCollisionAvoidanceSystem()
{
	CRY_PROFILE_FUNCTION(PROFILE_AI)
	if (gAIEnv.CVars.collisionAvoidance.EnableORCA)
		gAIEnv.pCollisionAvoidanceSystem->Update(m_frameDeltaTime);
}

void CAISystem::SingleDryUpdate(CAIActor* pAIActor)
{
	CRY_PROFILE_FUNCTION(PROFILE_AI);
	if (pAIActor->IsEnabled())
		pAIActor->Update(IAIObject::EUpdateType::Dry);
	else
		pAIActor->UpdateDisabled(IAIObject::EUpdateType::Dry);
}