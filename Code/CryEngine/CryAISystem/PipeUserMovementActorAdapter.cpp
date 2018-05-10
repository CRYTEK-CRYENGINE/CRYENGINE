// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "PipeUserMovementActorAdapter.h"
#include "PipeUser.h"

void PipeUserMovementActorAdapter::OnMovementPlanProduced()
{
	m_attachedPipeUser.SetSignal(AISIGNAL_DEFAULT, "MovementPlanProduced");
}

bool PipeUserMovementActorAdapter::GetDesignedPath(SShape& pathShape) const
{
	return gAIEnv.pNavigation->GetDesignerPath(m_attachedPipeUser.GetPathToFollow(), pathShape);
}

NavigationAgentTypeID PipeUserMovementActorAdapter::GetNavigationAgentTypeID() const
{
	return m_attachedPipeUser.GetNavigationTypeID();
}

Vec3 PipeUserMovementActorAdapter::GetPhysicsPosition() const
{
	return m_attachedPipeUser.GetPhysicsPos();
}

void PipeUserMovementActorAdapter::SetActorPath(const MovementStyle& style, const INavPath& navPath)
{
	m_attachedPipeUser.m_Path.SetPathPoints(navPath.GetPath());
}

MovementStyle::Speed GetPipeUserAdjustedSpeedForPathLength(const MovementStyle& style, const float pathLength)
{
	MovementStyle::Speed requestedSpeed = style.GetSpeed();

	static float minDistForSprint = 5.0f;
	static float minDistForRun = 2.5f;

	if ((requestedSpeed == MovementStyle::Sprint) && (pathLength < minDistForSprint))
		requestedSpeed = MovementStyle::Run;

	if ((requestedSpeed == MovementStyle::Run) && (pathLength < minDistForRun))
		requestedSpeed = MovementStyle::Walk;

	return requestedSpeed;
}

void SetPipeUserSpeedValueFromMovementSpeed(MovementStyle::Speed speedLiteral, CPipeUser& pipeUser)
{
	switch (speedLiteral)
	{
	case MovementStyle::Walk:
		pipeUser.SetSpeed(AISPEED_WALK);
		break;

	case MovementStyle::Run:
		pipeUser.SetSpeed(AISPEED_RUN);
		break;

	case MovementStyle::Sprint:
		pipeUser.SetSpeed(AISPEED_SPRINT);
		break;

	default:
		assert(false);
		break;
	}
}

void PipeUserMovementActorAdapter::SetActorStyle(const MovementStyle& style, const INavPath& navPath)
{
	const bool isMoving = m_attachedPipeUser.GetBodyInfo().isMoving;

	const bool continueMovingAtPathEnd = navPath.GetParams().continueMovingAtEnd;

	const bool adjustSpeedForPathLength = !isMoving && !continueMovingAtPathEnd;
	if (adjustSpeedForPathLength)
	{
		const MovementStyle::Speed adjustedSpeed = GetPipeUserAdjustedSpeedForPathLength(style, navPath.GetPathLength(false));
		SetPipeUserSpeedValueFromMovementSpeed(adjustedSpeed, m_attachedPipeUser);
	}
	else
	{
		if (style.HasSpeedLiteral())
		{
			m_attachedPipeUser.SetSpeed(style.GetSpeedLiteral());
		}
		else
		{
			SetPipeUserSpeedValueFromMovementSpeed(style.GetSpeed(), m_attachedPipeUser);
		}
	}

	SetStance(style.GetStance());

	if (style.ShouldStrafe())
		m_attachedPipeUser.SetAllowedStrafeDistances(999.0f, 999.0f, true);
	else
		m_attachedPipeUser.SetAllowedStrafeDistances(0.0f, 0.0f, false);

	m_attachedPipeUser.GetState().bodyOrientationMode = style.GetBodyOrientationMode();
}

std::shared_ptr<Vec3> PipeUserMovementActorAdapter::CreateLookTarget()
{
	return m_attachedPipeUser.CreateLookTarget();
}

void PipeUserMovementActorAdapter::ResetMovementContext()
{
	m_attachedPipeUser.ResetMovementContext();
}

void PipeUserMovementActorAdapter::SetLookTimeOffset(float lookTimeOffset)
{
	m_lookTimeOffset = lookTimeOffset;
}

void PipeUserMovementActorAdapter::UpdateLooking(float updateTime, std::shared_ptr<Vec3> lookTarget, const bool targetReachable, const float pathDistanceToEnd, const Vec3& followTargetPosition, const MovementStyle& style)
{
	if (lookTarget.get() && style.ShouldGlanceInMovementDirection())
	{
		// Once every X seconds the agent will look in the direction of
		// the follow target. This conveys the fact that the agent
		// is planning to see where he's going and not just blindly
		// walking backwards somewhere.
		// The agent will only glance in the XY-plane.

		static float lookDuration = 0.7f;
		static float timeBetweenLooks = 4.0f;
		static float blendFactor = 0.5f;            // 1.0 = look straight at follow target, 0.0 = look straight at aim target
		static float distanceToEndThreshold = 3.0f; // If we're closer to the end than this we won't glance

		m_lookTimeOffset += updateTime;
		if (m_lookTimeOffset > timeBetweenLooks)
			m_lookTimeOffset = 0.0f;

		const bool lookAtFollowTarget = m_lookTimeOffset >= 0.0f && m_lookTimeOffset<lookDuration&& targetReachable&& pathDistanceToEnd> distanceToEndThreshold;
		if (lookAtFollowTarget)
		{
			const Vec3 eyePosition = m_attachedPipeUser.GetPos();
			const Vec3 aimTargetPosition = m_attachedPipeUser.GetState().vAimTargetPos;

			const Vec3 eyeToFollowTargetDirection = (followTargetPosition - eyePosition).GetNormalized();
			const Vec3 eyeToAimTargetDirection = (aimTargetPosition - eyePosition).GetNormalized();

			Vec3 blendedAndSummedLookDirection = eyeToFollowTargetDirection * blendFactor + eyeToAimTargetDirection * (1.0f - blendFactor);
			blendedAndSummedLookDirection.z = 0.0f;
			const Vec3 finalLookDirection = blendedAndSummedLookDirection.GetNormalizedSafe(eyeToAimTargetDirection);
			*lookTarget = eyePosition + finalLookDirection * 30.0f;
		}
		else
		{
			// Setting the look target to zero means we're not providing
			// a look target at this moment, so another look target in the
			// look target stack may be used until we start providing again.
			*lookTarget = Vec3(ZERO);
		}
	}
}

Vec3 PipeUserMovementActorAdapter::GetVelocity() const
{
	return m_attachedPipeUser.GetVelocity();
}

Vec3 PipeUserMovementActorAdapter::GetMoveDirection() const
{
	return m_attachedPipeUser.GetState().vMoveDir;
}

void PipeUserMovementActorAdapter::ConfigurePathfollower(const MovementStyle& style)
{
	SOBJECTSTATE& pipeUserState = m_attachedPipeUser.GetState();
	float normalSpeed = 1.0f;
	float minSpeed = 1.0f;
	float maxSpeed = 1.0f;

	m_attachedPipeUser.GetMovementSpeedRange(
	  pipeUserState.fMovementUrgency,
	  pipeUserState.allowStrafing,
	  normalSpeed, minSpeed, maxSpeed);

	INavPath* navPath = m_attachedPipeUser.GetINavPath();
	const bool continueMovingAtEnd = navPath ? navPath->GetParams().continueMovingAtEnd : false;

	PathFollowerParams& pathFollowerParams = m_attachedPipeUser.GetPathFollower()->GetParams();
	pathFollowerParams.minSpeed = minSpeed;
	pathFollowerParams.maxSpeed = maxSpeed;
	pathFollowerParams.normalSpeed = clamp_tpl(normalSpeed, minSpeed, maxSpeed);
	pathFollowerParams.endDistance = 0.0f;
	pathFollowerParams.maxAccel = m_attachedPipeUser.m_movementAbility.maxAccel;
	pathFollowerParams.maxDecel = m_attachedPipeUser.m_movementAbility.maxDecel;
	pathFollowerParams.stopAtEnd = !continueMovingAtEnd;
	pathFollowerParams.isAllowedToShortcut = !style.IsMovingAlongDesignedPath();
}

void PipeUserMovementActorAdapter::SetMovementOutputValue(const PathFollowResult& result)
{
	Vec3 desiredMoveDir = result.velocityOut;
	const float desiredSpeed = desiredMoveDir.NormalizeSafe();
	Vec3 physicalPosition = m_attachedPipeUser.GetPhysicsPos();

	SOBJECTSTATE& pipeUserState = m_attachedPipeUser.GetState();
	IPathFollower* pPathFollower = m_attachedPipeUser.GetPathFollower();

	pipeUserState.fDesiredSpeed = desiredSpeed;
	pipeUserState.fTargetSpeed = desiredSpeed;
	pipeUserState.fDistanceToPathEnd = pPathFollower ? pPathFollower->GetDistToEnd(&physicalPosition) : .0f;
	pipeUserState.vMoveDir = desiredMoveDir;
	pipeUserState.vMoveTarget = result.followTargetPos;
	pipeUserState.vInflectionPoint = result.inflectionPoint;
}

void PipeUserMovementActorAdapter::ClearMovementState()
{
	SOBJECTSTATE& pipeUserState = m_attachedPipeUser.GetState();
	pipeUserState.fDesiredSpeed = 0.0f;
	pipeUserState.vMoveTarget.zero();
	pipeUserState.vMoveDir.zero();
	pipeUserState.vInflectionPoint.zero();
}

void PipeUserMovementActorAdapter::SetStance(const MovementStyle::Stance stance)
{
	switch (stance)
	{
	case MovementStyle::Relaxed:
		m_attachedPipeUser.GetState().bodystate = STANCE_RELAXED;
		break;

	case MovementStyle::Alerted:
		m_attachedPipeUser.GetState().bodystate = STANCE_ALERTED;
		break;

	case MovementStyle::Stand:
		m_attachedPipeUser.GetState().bodystate = STANCE_STAND;
		break;

	case MovementStyle::Crouch:
		m_attachedPipeUser.GetState().bodystate = STANCE_CROUCH;
		break;

	default:
		assert(false);
		break;
	}
}

bool PipeUserMovementActorAdapter::IsMoving() const
{
	const SAIBodyInfo& bodyInfo = m_attachedPipeUser.QueryBodyInfo();
	return bodyInfo.isMoving;
}

void PipeUserMovementActorAdapter::ResetBodyTarget()
{
	m_attachedPipeUser.ResetBodyTargetDir();
}

void PipeUserMovementActorAdapter::SetBodyTargetDirection(const Vec3& direction)
{
	m_attachedPipeUser.SetBodyTargetDir(direction);
}

Vec3 PipeUserMovementActorAdapter::GetAnimationBodyDirection() const
{
	return m_attachedPipeUser.GetBodyInfo().vAnimBodyDir;
}

void PipeUserMovementActorAdapter::RequestExactPosition(const SAIActorTargetRequest* request, const bool lowerPrecision)
{
	assert(request != NULL);

	SAIActorTargetRequest& pipeUserActorTargetRequest = m_attachedPipeUser.GetState().actorTargetReq;

	pipeUserActorTargetRequest = *request;
	pipeUserActorTargetRequest.id = ++m_attachedPipeUser.m_actorTargetReqId;
	pipeUserActorTargetRequest.lowerPrecision = lowerPrecision;
}

EActorTargetPhase PipeUserMovementActorAdapter::GetActorPhase() const
{
	return m_attachedPipeUser.GetState().curActorTargetPhase;
}

const CAIActor* FindClosestActorTryingToUseSmartObject(const OffMeshLink_SmartObject& SOLink, const CPipeUser& defaultClosestPipeUser)
{
	CSmartObject* pSmartObject = SOLink.m_pSmartObject;
	EntityId smartObjectId = pSmartObject->GetEntityId();

	const Vec3 smartObjectStart = pSmartObject->GetHelperPos(SOLink.m_pFromHelper);
	const CAIActor* closestActor = defaultClosestPipeUser.CastToCAIActor();

	// Find the closest of all actors trying to use this SmartObject
	{
		ActorLookUp& lookUp = *gAIEnv.pActorLookUp;
		size_t activeCount = lookUp.GetActiveCount();

		float distanceClosestSq = Distance::Point_PointSq(closestActor->GetPos(), smartObjectStart);

		for (size_t actorIndex = 0; actorIndex < activeCount; ++actorIndex)
		{
			const Vec3 position = lookUp.GetPosition(actorIndex);
			const float distanceSq = Distance::Point_PointSq(position, smartObjectStart);

			if (distanceSq < distanceClosestSq)
			{
				if (CPipeUser* closerActor = lookUp.GetActor<CPipeUser>(actorIndex))
				{
					if (closerActor->GetPendingSmartObjectID() == smartObjectId)
					{
						distanceClosestSq = distanceSq;

						closestActor = closerActor;
					}
				}
			}
		}
	}

	assert(closestActor);

	return closestActor;
}

bool AreWeClosestActorTryingToUseSmartObject(const OffMeshLink_SmartObject& SOLink, const CPipeUser& pipeUser)
{
	const CAIActor* closestActor = FindClosestActorTryingToUseSmartObject(SOLink, pipeUser);
	return (closestActor && (closestActor == pipeUser.CastToCAIActor()));
}

bool PipeUserMovementActorAdapter::IsClosestToUseTheSmartObject(const OffMeshLink_SmartObject& smartObjectLink) const
{
	return AreWeClosestActorTryingToUseSmartObject(smartObjectLink, m_attachedPipeUser);
}

bool PipeUserMovementActorAdapter::PrepareNavigateSmartObject(CSmartObject* pSmartObject, OffMeshLink_SmartObject* pSmartObjectLink)
{
	CSmartObjectManager* pSmartObjectManager = gAIEnv.pSmartObjectManager;
	if (pSmartObjectManager->PrepareNavigateSmartObject(&m_attachedPipeUser, pSmartObject, pSmartObjectLink->m_pSmartObjectClass, pSmartObjectLink->m_pFromHelper, pSmartObjectLink->m_pToHelper) && m_attachedPipeUser.m_eNavSOMethod != nSOmNone)
	{
		SAIActorTargetRequest& pipeUserActorTargetRequest = m_attachedPipeUser.GetState().actorTargetReq;
		pipeUserActorTargetRequest.id = ++m_attachedPipeUser.m_actorTargetReqId;
		pipeUserActorTargetRequest.lowerPrecision = true;
		return true;
	}

	return false;
}

void PipeUserMovementActorAdapter::InvalidateSmartObjectLink(CSmartObject* pSmartObject, OffMeshLink_SmartObject* pSmartObjectLink)
{
}

void PipeUserMovementActorAdapter::ResetActorTargetRequest()
{
	SAIActorTargetRequest& pipeUserActorTargetRequest = m_attachedPipeUser.GetState().actorTargetReq;
	pipeUserActorTargetRequest.Reset();
}

void PipeUserMovementActorAdapter::CancelRequestedPath()
{
	const bool isActorRemoved = false;
	m_attachedPipeUser.CancelRequestedPath(isActorRemoved);
}
