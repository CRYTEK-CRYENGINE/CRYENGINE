// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "MovementActorAdapter.h"

#include "NavigationComponent.h"

Vec3 CMovementActorAdapter::GetVelocity() const
{
	return m_pOwningNavigationComponent->GetVelocity();
}

Vec3 CMovementActorAdapter::GetPhysicsPosition() const
{
	return m_pOwningNavigationComponent->GetPosition();
}

bool CMovementActorAdapter::IsMoving() const
{
	return GetVelocity().GetLengthSquared() > 0.01f;
}

void CMovementActorAdapter::SetMovementOutputValue(const PathFollowResult& result)
{
	m_pOwningNavigationComponent->NewStateComputed(result, this);
}

void CMovementActorAdapter::ClearMovementState()
{
	PathFollowResult result;
	result.velocityOut = ZERO;
	
	m_pOwningNavigationComponent->NewStateComputed(result, this);
}

void CMovementActorAdapter::ConfigurePathfollower(const MovementStyle& style)
{
	PathFollowerParams& pathFollowerParams = m_pPathfollower->GetParams();

	pathFollowerParams.isAllowedToShortcut = !style.IsMovingAlongDesignedPath();

	m_pOwningNavigationComponent->FillPathFollowerParams(pathFollowerParams);
}

