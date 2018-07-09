// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "MovementActor.h"
#include "../PipeUser.h"
#include "MovementPlanner.h"

CAIActor* MovementActor::GetAIActor()
{
	CAIActor* aiActor = NULL;

	if (IEntity* entity = gEnv->pEntitySystem->GetEntity(entityID))
		if (IAIObject* ai = entity->GetAI())
			aiActor = ai->CastToCAIActor();

	return aiActor;
}

const char* MovementActor::GetName() const
{
	const IEntity* pEntity = gEnv->pEntitySystem->GetEntity(entityID);
	return pEntity ? pEntity->GetName() : "(none)";
}

void MovementActor::RequestPathTo(const Vec3& destination, float lengthToTrimFromThePathEnd, const SSnapToNavMeshRulesInfo& snappingRules, const MNMDangersFlags dangersFlags /* = eMNMDangers_None */, const bool considerActorsAsPathObstacles, const MNMCustomPathCostComputerSharedPtr& pCustomPathCostComputer)
{
	const bool cutPathAtSmartObject = false;

	const IEntity* pEntity = gEnv->pEntitySystem->GetEntity(entityID);
	if (!pEntity)
		return;

	if (callbacks.queuePathRequestFunction)
	{
		MNMPathRequest request(pEntity->GetPos(), destination, Vec3(0, 1, 0), -1, 0.0f, lengthToTrimFromThePathEnd, true, NULL, this->pAdapter->GetNavigationAgentTypeID(), dangersFlags);
		request.pCustomPathCostComputer = pCustomPathCostComputer;
		request.snappingRules = snappingRules;
		callbacks.queuePathRequestFunction(request);
	}
}

Movement::PathfinderState MovementActor::GetPathfinderState() const
{
	if (callbacks.checkOnPathfinderStateFunction)
	{
		return callbacks.checkOnPathfinderStateFunction();
	}

	return Movement::CouldNotFindPath;
}

void MovementActor::Log(const char* message)
{
#ifdef AI_COMPILE_WITH_PERSONAL_LOG
	if (GetAIActor())
	{
		GetAIActor()->GetPersonalLog().AddMessage(entityID, message);
	}
#endif
}

IMovementActorAdapter& MovementActor::GetAdapter() const
{
	assert(pAdapter);
	return *pAdapter;
}

bool MovementActor::AddActionAbilityCallbacks(const SMovementActionAbilityCallbacks& ability)
{
	bool bNoCallbackAlreadyPresent = true;
	if (ability.addStartMovementBlocksCallback)
	{
		bNoCallbackAlreadyPresent &= actionAbilities.m_addStartMovementBlocksCallback.AddUnique(ability.addStartMovementBlocksCallback);
	}
	if (ability.addEndMovementBlocksCallback)
	{
		bNoCallbackAlreadyPresent &= actionAbilities.m_addEndMovementBlocksCallback.AddUnique(ability.addEndMovementBlocksCallback);
	}
	if (ability.prePathFollowingUpdateCallback)
	{
		bNoCallbackAlreadyPresent &= actionAbilities.m_prePathFollowingUpdateCallback.AddUnique(ability.prePathFollowingUpdateCallback);
	}
	if (ability.postPathFollowingUpdateCallback)
	{
		bNoCallbackAlreadyPresent &= actionAbilities.m_postPathFollowingUpdateCallback.AddUnique(ability.postPathFollowingUpdateCallback);
	}
	
	CRY_ASSERT_MESSAGE(bNoCallbackAlreadyPresent, "MovementActor::AddActionAbilityCallbacks - Trying to add the same movement action ability twice");
	return true;
}

bool MovementActor::RemoveActionAbilityCallbacks(const SMovementActionAbilityCallbacks& ability)
{
	if (ability.addStartMovementBlocksCallback)
	{
		actionAbilities.m_addStartMovementBlocksCallback.Remove(ability.addStartMovementBlocksCallback);
	}
	if (ability.addEndMovementBlocksCallback)
	{
		actionAbilities.m_addEndMovementBlocksCallback.Remove(ability.addEndMovementBlocksCallback);
	}
	if (ability.prePathFollowingUpdateCallback)
	{
		actionAbilities.m_prePathFollowingUpdateCallback.Remove(ability.prePathFollowingUpdateCallback);
	}
	if (ability.postPathFollowingUpdateCallback)
	{
		actionAbilities.m_postPathFollowingUpdateCallback.Remove(ability.postPathFollowingUpdateCallback);
	}
	return true;
}
