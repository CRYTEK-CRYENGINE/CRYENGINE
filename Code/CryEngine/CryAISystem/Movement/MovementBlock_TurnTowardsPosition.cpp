// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "MovementBlock_TurnTowardsPosition.h"
#include <CryAISystem/MovementUpdateContext.h>
#include "MovementActor.h"
#include "PipeUser.h"

namespace Movement
{
namespace MovementBlocks
{
TurnTowardsPosition::TurnTowardsPosition(const Vec3& position)
	: m_positionToTurnTowards(position)
	, m_timeSpentAligning(0.0f)
	, m_correctBodyDirTime(0.0f)
{
}

Movement::Block::Status TurnTowardsPosition::Update(const MovementUpdateContext& context)
{
	// Align body towards the move target
	const Vec3 actorPysicalPosition = context.actor.GetAdapter().GetPhysicsPosition();
	Vec3 dirToMoveTarget = m_positionToTurnTowards - actorPysicalPosition;
	dirToMoveTarget.z = 0.0f;
	dirToMoveTarget.Normalize();
	context.actor.GetAdapter().SetBodyTargetDirection(dirToMoveTarget);

	// If animation body direction is within the angle threshold,
	// wait for some time and then mark the agent as ready to move
	const Vec3 actualBodyDir = context.actor.GetAdapter().GetAnimationBodyDirection();
	const bool lookingTowardsMoveTarget = (actualBodyDir.Dot(dirToMoveTarget) > cosf(DEG2RAD(17.0f)));
	if (lookingTowardsMoveTarget)
		m_correctBodyDirTime += context.updateTime;
	else
		m_correctBodyDirTime = 0.0f;

	const float timeSpentAligning = m_timeSpentAligning + context.updateTime;
	m_timeSpentAligning = timeSpentAligning;

	if (m_correctBodyDirTime > 0.2f)
		return Movement::Block::Finished;

	const float timeout = 8.0f;
	if (timeSpentAligning > timeout)
	{
		gEnv->pLog->LogWarning("Agent '%s' at %f %f %f failed to turn towards %f %f %f within %f seconds. Proceeding anyway.",
		                       context.actor.GetName(), actorPysicalPosition.x, actorPysicalPosition.y, actorPysicalPosition.z,
		                       m_positionToTurnTowards.x, m_positionToTurnTowards.y, m_positionToTurnTowards.z, timeout);
		return Movement::Block::Finished;
	}

	return Movement::Block::Running;
}

void TurnTowardsPosition::End(IMovementActor& actor)
{
	actor.GetAdapter().ResetBodyTarget();
}
}
}
