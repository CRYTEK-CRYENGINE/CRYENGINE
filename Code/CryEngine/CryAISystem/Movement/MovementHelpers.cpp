// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include <CryAISystem/MovementUpdateContext.h>
#include "MovementActor.h"
#include "PipeUser.h"
#include "MovementHelpers.h"

namespace Movement
{
namespace Helpers
{

// ----------------------------------------------------------------
// Stuck Detector
// ----------------------------------------------------------------

StuckDetector::StuckDetector()
	: m_accumulatedTimeAgentIsStuck(0.0f)
	, m_agentDistanceToTheEndInPreviousUpdate(std::numeric_limits<float>::max())
{
}

void StuckDetector::Update(const MovementUpdateContext& context)
{
	const EntityId actorEntityId = context.actor.GetEntityId();
	IEntity* pEntity = gEnv->pEntitySystem->GetEntity(actorEntityId);
	const Vec3& pipeUserPosition = pEntity->GetPos();

	const float distToEnd = context.pathFollower.GetDistToEnd(&pipeUserPosition);
	const float treshold = 0.05f;
	if (distToEnd + treshold < m_agentDistanceToTheEndInPreviousUpdate)
	{
		m_agentDistanceToTheEndInPreviousUpdate = distToEnd;
		m_accumulatedTimeAgentIsStuck = 0.0f;
	}
	else
	{
		m_accumulatedTimeAgentIsStuck += context.updateTime;
	}
}

bool StuckDetector::IsAgentStuck() const
{
	const float maxAllowedTimeAgentCanBeStuck = 3.0f;
	return m_accumulatedTimeAgentIsStuck > maxAllowedTimeAgentCanBeStuck;
}

void StuckDetector::Reset()
{
	m_accumulatedTimeAgentIsStuck = 0.0f;
	m_agentDistanceToTheEndInPreviousUpdate = std::numeric_limits<float>::max();
}

void BeginPathFollowing(IMovementActor& actor, const MovementStyle& style, const CNavPath& navPath)
{
	actor.GetAdapter().SetActorPath(style, navPath);
	actor.GetAdapter().SetActorStyle(style, navPath);
}

// Returns false when target is unreachable
bool UpdatePathFollowing(PathFollowResult& result, const MovementUpdateContext& context, const MovementStyle& style)
{
	context.actor.GetAdapter().ConfigurePathfollower(style);

	IPathFollower& pathFollower = context.pathFollower;
	const Vec3 position = context.actor.GetAdapter().GetPhysicsPosition();
	const Vec3 velocity = context.actor.GetAdapter().GetVelocity();

	const bool targetReachable = pathFollower.Update(result, position, velocity, context.updateTime);

	if (targetReachable)
	{
		context.actor.GetAdapter().SetMovementOutputValue(result);
	}
	else
	{
		context.actor.GetAdapter().ClearMovementState();
	}

	return targetReachable;
}
}
}
