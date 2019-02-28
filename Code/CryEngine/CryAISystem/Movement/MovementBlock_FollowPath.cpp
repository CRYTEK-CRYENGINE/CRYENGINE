// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "MovementBlock_FollowPath.h"
#include <CryAISystem/MovementUpdateContext.h>
#include "MovementActor.h"
#include "PipeUser.h"
#include "Cover/CoverSystem.h"

namespace Movement
{
namespace MovementBlocks
{

FollowPath::FollowPath(
  const CNavPath& path,
  const float endDistance,
  const MovementStyle& style,
  const bool bLastFollowBlock)
	: m_path(path)
	, m_finishBlockEndDistance(endDistance)
	, m_accumulatedPathFollowerFailureTime(0.0f)
	, m_style(style)
	, m_bLastFollowBlock(bLastFollowBlock)
{
}

void FollowPath::Begin(IMovementActor& actor)
{
	m_accumulatedPathFollowerFailureTime = 0.0f;
	m_stuckDetector.Reset();
	Movement::Helpers::BeginPathFollowing(actor, m_style, m_path);

	if (m_style.ShouldGlanceInMovementDirection())
	{
		// Offset the look timer so that we don't start running the
		// looking immediately. It effectively means that we delay
		// the looking code from kicking in for X seconds.
		// It's good because the agent normally needs some time to start
		// movement transitions etc.
		static float lookTimeOffset = -1.5f;
		actor.GetAdapter().SetLookTimeOffset(lookTimeOffset);
		m_lookTarget = actor.GetAdapter().CreateLookTarget();
	}
}

void FollowPath::End(IMovementActor& actor)
{
	m_lookTarget.reset();
}

Movement::Block::Status FollowPath::Update(const MovementUpdateContext& context)
{
	context.actor.GetAdapter().ResetMovementContext();
	context.actor.GetActionAbilities().PrePathFollowUpdateCall(context, m_bLastFollowBlock);

	PathFollowResult result;
	const bool targetReachable = Movement::Helpers::UpdatePathFollowing(result, context, m_style);

	const Vec3 physicsPosition = context.actor.GetAdapter().GetPhysicsPosition();
	const float pathDistanceToEnd = context.pathFollower.GetDistToEnd(&physicsPosition);

	context.actor.GetAdapter().UpdateLooking(context.updateTime, m_lookTarget, targetReachable, pathDistanceToEnd, result.followTargetPos, m_style);
	context.actor.GetActionAbilities().PostPathFollowUpdateCall(context, m_bLastFollowBlock);

	m_stuckDetector.Update(context);

	if (m_stuckDetector.IsAgentStuck())
	{
		return Movement::Block::CantBeFinished;
	}

	if (!targetReachable)
	{
		// Track how much time we spent in this FollowPath block
		// without being able to reach the movement target.
		m_accumulatedPathFollowerFailureTime += context.updateTime;

		// If it keeps on failing we don't expect to recover.
		// We report back that we can't be finished and the planner
		// will attempt to re-plan for the current situation.
		if (m_accumulatedPathFollowerFailureTime > 3.0f)
		{
			return Movement::Block::CantBeFinished;
		}
	}

	if (m_finishBlockEndDistance > 0.0f)
	{
		if (pathDistanceToEnd < m_finishBlockEndDistance)
		{
			return Movement::Block::Finished;
		}
	}

	IMovementActor& actor = context.actor;

	if (result.reachedEnd)
	{

		const Vec3 velocity = actor.GetAdapter().GetVelocity();
		const Vec3 groundVelocity(velocity.x, velocity.y, 0.0f);
		const Vec3 moveDir = actor.GetAdapter().GetMoveDirection();
		const float speed = groundVelocity.Dot(moveDir);
		const bool stopped = speed < 0.01f;

		if (stopped)
		{
			return Movement::Block::Finished;
		}
	}

	return Movement::Block::Running;
}
}
}
