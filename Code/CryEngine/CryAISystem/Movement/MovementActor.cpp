// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#include "StdAfx.h"
#include "MovementActor.h"
#include "../PipeUser.h"
#include "MovementPlanner.h"

void MovementActor::SetLowCoverStance()
{
	GetAIActor()->GetState().bodystate = STANCE_LOW_COVER;
}

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

void MovementActor::RequestPathTo(const Vec3& destination, float lengthToTrimFromThePathEnd, const MNMDangersFlags dangersFlags /* = eMNMDangers_None */, const bool considerActorsAsPathObstacles)
{
	const bool cutPathAtSmartObject = false;

	const IEntity* pEntity = gEnv->pEntitySystem->GetEntity(entityID);
	if (!pEntity)
		return;

	if (callbacks.queuePathRequestFunction)
	{
		MNMPathRequest request(pEntity->GetPos(), destination, Vec3(0, 1, 0), -1, 0.0f, lengthToTrimFromThePathEnd, true, NULL, this->pAdapter->GetNavigationAgentTypeID(), dangersFlags);
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
