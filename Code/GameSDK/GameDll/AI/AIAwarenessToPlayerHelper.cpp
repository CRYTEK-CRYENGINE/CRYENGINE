// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
 -------------------------------------------------------------------------
  Description: Helper to calculate a value that represents AI awareness toward the player.
*************************************************************************/

#include "StdAfx.h"
#include "AIAwarenessToPlayerHelper.h"
#include <CryAISystem/IAIObject.h>
#include <CryAISystem/IAIActor.h>
#include <CryAISystem/IAIActorProxy.h>
#include "GameCVars.h"
#include "Actor.h"
#include "Turret/Turret/Turret.h"
#include "Turret/Turret/TurretHelpers.h"

#define TIME_BETWEEN_AWARENESS_RECALCULATIONS 0.3f  // to not go thru all actors and awareness entities on every frame

const float CAIAwarenessToPlayerHelper::kAIAwarenessToPlayerAware = (const float)CAIAwarenessToPlayerHelper::AI_Awarness_To_Player_Aware;
const float CAIAwarenessToPlayerHelper::kAIAwarenessToPlayerAlerted = (const float)CAIAwarenessToPlayerHelper::AI_Awarness_To_Player_Alerted;

//////////////////////////////////////////////////////////////////////////
void CAIAwarenessToPlayerHelper::Reset()
{
	m_intCurrentAwareness = 0;
	m_actualAwareness = 0.0f;
	m_animatedAwarenessThatStrivesTowardsActualAwareness = 0.0f;
	m_timeToRecalculateAwareness = 0.f;
	stl::free_container(m_listeners);
	stl::free_container(m_awarenessEntities);
}

//////////////////////////////////////////////////////////////////////////
void CAIAwarenessToPlayerHelper::Serialize( TSerialize ser )
{
	ser.BeginGroup("AIAwarenessToPlayer");
	
	if (ser.IsReading())
	{
		Reset();
	}

	ser.EndGroup();
}

//////////////////////////////////////////////////////////////////////////
void CAIAwarenessToPlayerHelper::Update( float frameTime )
{
	m_timeToRecalculateAwareness -= frameTime;
	if (m_timeToRecalculateAwareness<=0)
	{
		RecalculateAwareness();
		m_timeToRecalculateAwareness = TIME_BETWEEN_AWARENESS_RECALCULATIONS;
	}

	// Animate towards the actual awareness value
	float& animated = m_animatedAwarenessThatStrivesTowardsActualAwareness;
	float& actual = m_actualAwareness;
	const float increaseSpeed = 100.0f;
	const float decreaseSpeed = 10.0f;
	if (actual > animated)
	{
		animated = std::min(animated + increaseSpeed * frameTime, actual);
	}
	else if (actual < animated)
	{
		animated = std::min(75.0f, std::max(animated - decreaseSpeed * frameTime, actual));
	}

	int newIntAwareness = int_ceil(m_animatedAwarenessThatStrivesTowardsActualAwareness);
	if (newIntAwareness != m_intCurrentAwareness)
	{
		m_intCurrentAwareness = newIntAwareness;
		NotifyListeners();
	}
}



//////////////////////////////////////////////////////////////////////////
void CAIAwarenessToPlayerHelper::RecalculateAwareness()
{
	int highestAlertnessInGameRightNow = 0;

	float distanceToTheClosestHostileAgentSq = FLT_MAX;
	Vec3 playerPosition(ZERO);

	const IFactionMap& factionMap = gEnv->pAISystem->GetFactionMap();
	uint8 playerFactionID = factionMap.GetFactionID("Players");

	IAIObject* playerAiObject = NULL;
	CActor* playerActor = static_cast<CActor*>(gEnv->pGameFramework->GetClientActor());

	if (playerActor)
	{
		if (IEntity* playerEntity = playerActor->GetEntity())
		{
			playerPosition = playerEntity->GetWorldPos();
			playerAiObject = playerEntity->GetAI();
		}
	}

	IF_UNLIKELY ((playerActor == NULL) || (playerAiObject == NULL))
		return;

	const bool playerIsCloaked = playerActor ? playerActor->IsCloaked() : true;

	const bool applyProximityToHostileAgentIncrement = m_actualAwareness < kAIAwarenessToPlayerAware && !playerPosition.IsZero();

	// Go through actors
	IActorIteratorPtr actorIt = gEnv->pGameFramework->GetIActorSystem()->CreateActorIterator();
	if (actorIt)
	{
		while (IActor* actor = actorIt->Next())
		{
			const IAIObject* ai = actor->GetEntity()->GetAI();
			const IAIActor* aiActor = ai ? ai->CastToIAIActor() : NULL;
			if (aiActor && aiActor->IsActive())
			{
				const int alertness = GetAlertnessAffectedByVisibility(*aiActor, *playerAiObject, playerIsCloaked);

				highestAlertnessInGameRightNow = std::max(alertness, highestAlertnessInGameRightNow);

				if (applyProximityToHostileAgentIncrement && 
					factionMap.GetReaction(playerFactionID, ai->GetFactionID()) == IFactionMap::Hostile)
				{
					float distanceSq = playerPosition.GetSquaredDistance(ai->GetPos());
					if (distanceToTheClosestHostileAgentSq > distanceSq)
					{
						distanceToTheClosestHostileAgentSq = distanceSq;
					}
				}
			}
		}
	}

	// Go through non Actors
	{
		SAwarenessEntitiesVector::iterator it = m_awarenessEntities.begin();
		SAwarenessEntitiesVector::iterator end = m_awarenessEntities.end();

		for (; it != end; ++it)
		{
			const IAwarenessEntity* pAwarenessEntity = *it;
			const int awareness = pAwarenessEntity->GetAwarenessToActor( playerAiObject, playerActor );
			highestAlertnessInGameRightNow = std::max(awareness, highestAlertnessInGameRightNow);
		}
	}


	///
	assert(highestAlertnessInGameRightNow >= 0 && highestAlertnessInGameRightNow <= 2);

	switch (highestAlertnessInGameRightNow)
	{
	default: 
		{
			const float thresholdDistanceSq = square(g_pGameCVars->ai_ProximityToHostileAlertnessIncrementThresholdDistance);
			if (applyProximityToHostileAgentIncrement && distanceToTheClosestHostileAgentSq < thresholdDistanceSq)
			{
				m_actualAwareness = kAIAwarenessToPlayerAware * (1 - (distanceToTheClosestHostileAgentSq / thresholdDistanceSq));
			}
			else
			{
				m_actualAwareness = 0.0f;
			}
		}
		break;
	case 1:  m_actualAwareness = kAIAwarenessToPlayerAware;  break;
	case 2:  m_actualAwareness = kAIAwarenessToPlayerAlerted; break;
	}
}


//////////////////////////////////////////////////////////////////////////
void CAIAwarenessToPlayerHelper::AddListener(CAIAwarenessToPlayerHelper::IListener *pListener)
{
	stl::push_back_unique( m_listeners, pListener );
}

//////////////////////////////////////////////////////////////////////////
void CAIAwarenessToPlayerHelper::RemoveListener(CAIAwarenessToPlayerHelper::IListener *pListener)
{
	stl::find_and_erase( m_listeners, pListener );
}

//////////////////////////////////////////////////////////////////////////
CAIAwarenessToPlayerHelper::VisorIconColor CAIAwarenessToPlayerHelper::GetMarkerColorForAgent(const EntityId entityId) const
{
	CAIAwarenessToPlayerHelper::VisorIconColor defaultColor = Green;
	IEntity* entity = gEnv->pEntitySystem->GetEntity(entityId);
	const IAIObject* ai = entity ? entity->GetAI() : NULL;
	const IAIActor* aiActor = ai ? ai->CastToIAIActor() : NULL;
	IAIObject* playerAiObject = NULL;
	CActor* playerActor = static_cast<CActor*>(gEnv->pGameFramework->GetClientActor());
	if (playerActor)
	{
		if (IEntity* playerEntity = playerActor->GetEntity())
		{
			playerAiObject = playerEntity->GetAI();
		}
	}
	if (!playerActor || !playerAiObject)
		return defaultColor;

	const bool playerIsCloaked = playerActor->IsCloaked();

	if (aiActor)
	{
		const int alertness = GetAlertnessAffectedByVisibility(*aiActor, *playerAiObject, playerIsCloaked);

		if (alertness == 0)
			return Green;
		else if (alertness == 1)
			return Orange;
		else
			return Red;
	}
	else
	{
		// Turrets are not AI actors so they are treated a bit differently
		// TODO: extend this to generic IAwarenessEntity. for now in C3 is fine as we dont want Towers to be show here.
		if (CTurret* turret = TurretHelpers::FindTurret(entityId))
		{
			if (IEntity* turretEntity = turret->GetEntity())
			{
				if (IAIObject* turretAI = turretEntity->GetAI())
				{
					const bool turretIsDeployed = (turret->GetStateId() == eTurretBehaviorState_Deployed);
					if (!playerIsCloaked && turretIsDeployed && turretAI->IsHostile(playerAiObject) && turret->IsVisionIdInVisionRange(playerAiObject->GetVisionID()))
						return Red;
					else
						return Green;
				}
			}
		}
	}

	return defaultColor;
}

//////////////////////////////////////////////////////////////////////////
void CAIAwarenessToPlayerHelper::RegisterAwarenessEntity( CAIAwarenessToPlayerHelper::IAwarenessEntity* pAwarenessEntity )
{
	stl::push_back_unique( m_awarenessEntities, pAwarenessEntity );
}

//////////////////////////////////////////////////////////////////////////
void CAIAwarenessToPlayerHelper::UnregisterAwarenessEntity( CAIAwarenessToPlayerHelper::IAwarenessEntity* pAwarenessEntity )
{
	stl::find_and_erase( m_awarenessEntities, pAwarenessEntity );
}

//////////////////////////////////////////////////////////////////////////
void CAIAwarenessToPlayerHelper::NotifyListeners()
{
	if (!m_listeners.empty())
	{
		TListenersVector::iterator iter = m_listeners.begin();
		while (iter != m_listeners.end())
		{
			IListener* pListener = *iter;
			pListener->AwarenessChanged( m_intCurrentAwareness );
			++iter;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
int CAIAwarenessToPlayerHelper::GetAlertnessAffectedByVisibility(const IAIActor& aiActor, const IAIObject& playerAiObject, const bool playerIsCloaked) const
{
	int alertness = aiActor.GetProxy()->GetAlertnessState();

	// Clamp the alertness to orange (1) if the player is cloaked or
	// if he's not currently seen by this ai actor.
	if (playerIsCloaked || !aiActor.CanSee(playerAiObject.GetVisionID()))
	{
		alertness = std::min(alertness, 1);
	}

	return alertness;
}

