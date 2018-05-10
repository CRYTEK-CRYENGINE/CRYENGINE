// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//! \cond INTERNAL

#pragma once

struct IMovementActor;
struct IMovementSystem;
class CPipeUser;
class IPathFollower;
struct SOBJECTSTATE;
struct MovementRequest;

namespace Movement
{
struct IPlanner;
}

//! Contains information needed while updating the movement system.
//! Constructed every frame in the movement system update.
struct MovementUpdateContext
{
	MovementUpdateContext(
	  IMovementActor& _actor,
	  IMovementSystem& _movementSystem,
	  IPathFollower& _pathFollower,
	  Movement::IPlanner& _planner,
	  const float _updateTime
	  )
		: actor(_actor)
		, movementSystem(_movementSystem)
		, pathFollower(_pathFollower)
		, planner(_planner)
		, updateTime(_updateTime)
	{
	}

	IMovementActor&     actor;
	IMovementSystem&    movementSystem;
	IPathFollower&      pathFollower;
	Movement::IPlanner& planner;
	const float         updateTime;
};

//! \endcond