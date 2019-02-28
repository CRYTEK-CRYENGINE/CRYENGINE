// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
Description: 
  Utility class(es) for movementtransitions system
-------------------------------------------------------------------------
History:
- 7:25:2010	19:08 : Created by Sven Van Soom
*************************************************************************/
#pragma once
#ifndef __MOVEMENT_TRANSITIONS_UTIL_H
#define __MOVEMENT_TRANSITIONS_UTIL_H


struct STransitionFuture
{
	STransitionFuture() 
	: vMoveDirection(ZERO)
	, qOrientation(IDENTITY)
	, speed(-1)
	, vMoveTarget(ZERO)
	, hasMoveTarget(false)
 {}

	Vec3  vMoveDirection; // normalized movement direction at the end of the transition
	Quat  qOrientation;   // orientation at the end of the transition
	float speed;          // movement speed at the end of the transition
	Vec3  vMoveTarget;    // movement target at the time of selecting the transition
	bool  hasMoveTarget;  // vMoveTarget is valid only if this parameter is true
};


struct SMovementTransitionsSample
{
	float time;
	float pseudoSpeed;
	Vec3  moveDirection;
	Vec3  desiredVelocity;
	Vec3  bodyDirection;

	void Reset()
	{
		time = 0.0f;
		pseudoSpeed = 0.0f;
		moveDirection.Set(0,0,0);
		desiredVelocity.Set(0,0,0);
		bodyDirection.Set(0,0,0);
	}
};

#endif // __MOVEMENT_TRANSITIONS_UTIL_H
