// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "MoveOp.h"

#include <CryAISystem/IMovementSystem.h>
#include <CryAISystem/MovementRequest.h>
#include "Cover/CoverSystem.h"
#include "PipeUser.h"
#include <CrySystem/XML/XMLAttrReader.h>
#include "AIBubblesSystem/AIBubblesSystem.h"
#include "Navigation/NavigationSystem/NavigationSystem.h"

MoveOpDictionaryCollection::MoveOpDictionaryCollection()
{
	destinationTypes.Reserve(5);
	destinationTypes.Add("Target", MoveOp::Target);
	destinationTypes.Add("Cover", MoveOp::Cover);
	destinationTypes.Add("RefPoint", MoveOp::ReferencePoint);
	destinationTypes.Add("FollowPath", MoveOp::FollowPath);
	destinationTypes.Add("Formation", MoveOp::Formation);
}

MoveOpDictionaryCollection g_moveOpDictionaryCollection;

MoveOp::MoveOp()
	: m_movementRequestID(0)
	, m_stopWithinDistanceSq(0.0f)
	, m_destinationAtTimeOfMovementRequest(ZERO)
	, m_destination(ReferencePoint)
	, m_dangersFlags(eMNMDangers_None)
	, m_requestStopWhenLeaving(false)
{
	m_movementStyle.SetMovingToCover(false);
}

MoveOp::MoveOp(const XmlNodeRef& node)
	: m_movementRequestID(0)
	, m_stopWithinDistanceSq(0.0f)
	, m_destinationAtTimeOfMovementRequest(ZERO)
	, m_destination(ReferencePoint)
	, m_dangersFlags(eMNMDangers_None)
	, m_requestStopWhenLeaving(false)
{
	// Speed? Stance?
	m_movementStyle.ReadFromXml(node);

	// Destination? Target/Cover/ReferencePoint
	g_moveOpDictionaryCollection.destinationTypes.Get(node, "to", m_destination, true);
	m_movementStyle.SetMovingToCover(m_destination == Cover);
	m_movementStyle.SetMovingAlongDesignedPath(m_destination == FollowPath);

	if (m_destination == FollowPath)
	{
		m_pathName = node->getAttr("pathName");
	}

	float stopWithinDistance(0.0f);
	node->getAttr("stopWithinDistance", stopWithinDistance);
	SetStopWithinDistance(stopWithinDistance);

	bool shouldAvoidDangers = true;
	node->getAttr("shouldAvoidDangers", shouldAvoidDangers);

	SetupDangersFlagsForDestination(shouldAvoidDangers);
}

void MoveOp::SetupDangersFlagsForDestination(bool shouldAvoidDangers)
{
	if (!shouldAvoidDangers)
	{
		m_dangersFlags = eMNMDangers_None;
		return;
	}

	switch (m_destination)
	{
	case Target:
	case Formation:
		m_dangersFlags = eMNMDangers_Explosive;
		break;
	case Cover:
	case ReferencePoint:
		m_dangersFlags = eMNMDangers_AttentionTarget | eMNMDangers_Explosive;
		break;
	case FollowPath:
		m_dangersFlags = eMNMDangers_None;
		break;
	default:
		assert(0);
		m_dangersFlags = eMNMDangers_None;
		break;
	}
}

void MoveOp::Enter(CPipeUser& pipeUser)
{
	if (m_destination != FollowPath)
	{
		RequestMovementTo(DestinationPositionFor(pipeUser), pipeUser);
	}
	else
	{
		if (m_pathName.empty())
		{
			GetClosestDesignedPath(pipeUser, m_pathName);
		}
		RequestFollowExistingPath(m_pathName.c_str(), pipeUser);
	}
}

void MoveOp::Leave(CPipeUser& pipeUser)
{
	const bool requestWasStillRunning = m_movementRequestID != 0;

	ReleaseCurrentMovementRequest();

	const bool shouldRequestStop = (m_requestStopWhenLeaving && requestWasStillRunning) || GetStatus() == eGOR_FAILED;
	if (shouldRequestStop)
	{
		RequestStop(pipeUser);
	}
}

void MoveOp::Update(CPipeUser& pipeUser)
{
	if (m_destination == Target || m_destination == Formation)
	{
		ChaseTarget(pipeUser);
	}

	const bool stopMovementWhenWithinCertainDistance = m_stopWithinDistanceSq > 0.0f;

	if (stopMovementWhenWithinCertainDistance)
	{
		if (GetSquaredDistanceToDestination(pipeUser) < m_stopWithinDistanceSq)
		{
			ReleaseCurrentMovementRequest();
			RequestStop(pipeUser);
			GoalOpSucceeded();
		}
	}
}

void MoveOp::SetMovementStyle(MovementStyle movementStyle)
{
	m_movementStyle = movementStyle;
}

void MoveOp::SetStopWithinDistance(float distance)
{
	m_stopWithinDistanceSq = square(distance);
}

void MoveOp::SetRequestStopWhenLeaving(bool requestStop)
{
	m_requestStopWhenLeaving = requestStop;
}

static CFormation* getFormation(CPipeUser& pipeUserInFormation, float range);

Vec3               MoveOp::DestinationPositionFor(CPipeUser& pipeUser)
{
	switch (m_destination)
	{
	case Target:
		{
			if (IAIObject* target = pipeUser.GetAttentionTarget())
			{
				CPipeUser* pPipeUser = target->CastToCPipeUser();
				const Vec3 targetPosition = pPipeUser ? pPipeUser->GetPhysicsPos() : target->GetPosInNavigationMesh(pipeUser.GetNavigationTypeID());
				Vec3 targetVelocity = target->GetVelocity();
				targetVelocity.z = .0f;
				return targetPosition + targetVelocity * 0.5f;
			}
			else
			{
				AIQueueBubbleMessage("MoveOp Destination Position",
				                     pipeUser.GetEntityID(),
				                     "I don't have a target and MoveOp is set to chase the target.",
				                     eBNS_LogWarning | eBNS_Balloon);
				return Vec3(ZERO);
			}
		}

	case Cover:
		{
			return GetCoverRegisterLocation(pipeUser);
		}

	case ReferencePoint:
		{
			return pipeUser.GetRefPoint()->GetPos();
		}

	case Formation:
		{
			if (IAIObject* pIAIObject = pipeUser.GetSpecialAIObject("formation", 100000.0f)) // 2013-11-08 [christianw]: the formation leader can now be up to 100 km (!) away before the followers give up (was implicitly 20 meters before, c. f. "range" parameter for CPipeUser::GetSpecialAIObject())
			{
				m_formationInfo.positionInFormation = pIAIObject->GetPos();

				if (CFormation* formation = getFormation(pipeUser, 100000.0f))
				{
					if (const CPathMarker* pm = formation->GetPathMarker())
					{
						int pointIndex = formation->GetPointIndex(GetWeakRef(&pipeUser));
						if (pointIndex != -1)
						{
							// get offset in formation
							// get direction in path marker at offset.y (the follow distance)
							Vec3 offset;
							formation->GetPointOffset(pointIndex, offset);
							const Vec3 dir = pm->GetDirectionAtDistanceFromNewestPoint(offset.y);
							const float lookAheadDistance = 6.0f; // experimental value
							const Vec3 positionToMoveTo = m_formationInfo.positionInFormation + dir * lookAheadDistance;

							if (gAIEnv.pNavigationSystem->IsPointReachableFromPosition(pipeUser.GetNavigationTypeID(), pipeUser.GetPathAgentEntity(), pipeUser.GetEntity()->GetPos(), positionToMoveTo, nullptr))
							{
								m_formationInfo.positionInFormationIsReachable = true;
								return positionToMoveTo;
							}
							else
							{
								m_formationInfo.positionInFormationIsReachable = false;

								// follow the leader slowly until our formation slot is reachable again
								if (pm->GetPointCount() > 0)
								{
									const float distanceBehindLeader = 1.0f;  // experimental value
									return pm->GetPointAtDistanceFromNewestPoint(distanceBehindLeader);
								}
							}
						}
					}
				}
				// fallback
				const Vec3 targetVelocity = pipeUser.GetFormationVelocity();
				const float lookAheadDistanceRelativeToTargetVelocity = 0.5f;
				return m_formationInfo.positionInFormation + targetVelocity * lookAheadDistanceRelativeToTargetVelocity;
			}
			else
			{
				AIQueueBubbleMessage("MoveOp seeking formation failed",
				                     pipeUser.GetEntityID(),
				                     "I don't have a formation and MoveOp is set to chase it.",
				                     eBNS_LogWarning | eBNS_Balloon);
				return Vec3(ZERO);
			}
		}

	default:
		{
			assert(0);
			return Vec3Constants<float>::fVec3_Zero;
		}
	}
}

Vec3 MoveOp::GetCoverRegisterLocation(const CPipeUser& pipeUser) const
{
	if (CoverID coverID = pipeUser.GetCoverRegister())
	{
		const float distanceToCover = pipeUser.GetParameters().distanceToCover;
		return gAIEnv.pCoverSystem->GetCoverLocation(coverID, distanceToCover);
	}
	else
	{
		assert(0);
		AIQueueBubbleMessage("MoveOp:CoverLocation", pipeUser.GetEntityID(), "MoveOp failed to get the cover location due to an invalid Cover ID in the cover register.", eBNS_LogWarning | eBNS_Balloon | eBNS_BlockingPopup);
		return Vec3Constants<float>::fVec3_Zero;
	}
}

void MoveOp::RequestMovementTo(const Vec3& position, CPipeUser& pipeUser)
{
	assert(!m_movementRequestID);

	MovementRequest movementRequest;
	movementRequest.entityID = pipeUser.GetEntityID();
	movementRequest.destination = position;
	movementRequest.callback = functor(*this, &MoveOp::MovementRequestCallback);
	movementRequest.style = m_movementStyle;
	movementRequest.dangersFlags = m_dangersFlags;

	m_movementRequestID = gEnv->pAISystem->GetMovementSystem()->QueueRequest(movementRequest);

	m_destinationAtTimeOfMovementRequest = position;
}

void MoveOp::RequestFollowExistingPath(const char* pathName, CPipeUser& pipeUser)
{
	assert(pathName);

	if (!pathName)
		return;

	CAIActor* pCAIActor = pipeUser.CastToCAIActor();
	assert(pCAIActor);
	if (!pCAIActor)
		return;

	pCAIActor->SetPathToFollow(pathName);

	MovementRequest movementRequest;
	movementRequest.entityID = pipeUser.GetEntityID();
	movementRequest.callback = functor(*this, &MoveOp::MovementRequestCallback);
	movementRequest.style = m_movementStyle;

	m_movementRequestID = gEnv->pAISystem->GetMovementSystem()->QueueRequest(movementRequest);

}

void MoveOp::ReleaseCurrentMovementRequest()
{
	if (m_movementRequestID)
	{
		gEnv->pAISystem->GetMovementSystem()->CancelRequest(m_movementRequestID);
		m_movementRequestID = 0;
	}
}

void MoveOp::MovementRequestCallback(const MovementRequestResult& result)
{
	assert(m_movementRequestID == result.requestID);

	m_movementRequestID = 0;

	if (result == MovementRequestResult::ReachedDestination)
	{
		GoalOpSucceeded();
	}
	else
	{
		GoalOpFailed();

		if (m_destination == Cover)
		{
			// Todo: Blacklist cover
		}
	}
}

static CFormation* getFormation(CPipeUser& pipeUserInFormation, float range)
{
	CAISystem* pAISystem = GetAISystem();
	CFormation* pFormation = NULL;  // return value

	if (CLeader* pLeader = pAISystem->GetLeader(pipeUserInFormation.GetGroupId()))
	{
		if (CAIObject* pFormationOwner = pLeader->GetFormationOwner().GetAIObject())
		{
			pFormation = pFormationOwner->m_pFormation;
		}
	}

	if (!pFormation)
	{
		IAIObject* pBoss = pAISystem->GetNearestObjectOfTypeInRange(&pipeUserInFormation, AIOBJECT_ACTOR, 0, range, AIFAF_HAS_FORMATION | AIFAF_INCLUDE_DEVALUED | AIFAF_SAME_GROUP_ID);
		if (!pBoss)
			pBoss = pAISystem->GetNearestObjectOfTypeInRange(&pipeUserInFormation, AIOBJECT_VEHICLE, 0, range, AIFAF_HAS_FORMATION | AIFAF_INCLUDE_DEVALUED | AIFAF_SAME_GROUP_ID);

		if (CAIObject* pTheBoss = static_cast<CAIObject*>(pBoss))
		{
			pFormation = pTheBoss->m_pFormation;
		}
	}

	return pFormation;
}

void MoveOp::ChaseTarget(CPipeUser& pipeUser)
{
	const Vec3 targetPosition = DestinationPositionFor(pipeUser);

	const float targetDeviation = targetPosition.GetSquaredDistance(m_destinationAtTimeOfMovementRequest);
	const float deviationThreshold = square(0.5f);

	if (targetDeviation > deviationThreshold)
	{
		ReleaseCurrentMovementRequest();
		RequestMovementTo(targetPosition, pipeUser);
	}

	if (m_destination == Formation)
	{
		if (CFormation* formation = getFormation(pipeUser, 100000.0f))
		{
			if (CAIObject* formationOwner = formation->GetOwner())
			{
				if (CAIActor* formationOwnerAsAIActor = formationOwner->CastToCAIActor())
				{
					if (formation->GetPathMarker())
					{
						float newSpeed = formationOwnerAsAIActor->GetState().fMovementUrgency;

						if (m_formationInfo.positionInFormationIsReachable)
						{
							// check for whether we have to catch up with the formation owner, slow down or whether it's ok to move along with the same speed as he does

							static const float innerRadiusSqr = square(2.0f); // experimental value
							static const float outerRadiusSqr = square(4.0f); // experimental value

							const float distanceToFormationSlotSqr = (pipeUser.GetPos() - m_formationInfo.positionInFormation).GetLengthSquared();

							const Vec3 futureFormationSlot = formation->GetPathMarker()->GetPointAtDistance(m_formationInfo.positionInFormation, 2.0f);
							const Vec3 dirSelfToFormationSlot = (m_formationInfo.positionInFormation - pipeUser.GetPos()).GetNormalized();
							const Vec3 dirFormationSlotToFutureFormationSlot = (futureFormationSlot - m_formationInfo.positionInFormation).GetNormalized();
							const float dot = dirSelfToFormationSlot * dirFormationSlotToFutureFormationSlot;

							// check for state transition
							switch (m_formationInfo.state)
							{
							case FormationInfo::State_MatchingLeaderSpeed:
								if (distanceToFormationSlotSqr > outerRadiusSqr)
								{
									if (dot > 0.0f)
										m_formationInfo.state = FormationInfo::State_CatchingUp;
									else
										m_formationInfo.state = FormationInfo::State_SlowingDown;
								}
								else if (distanceToFormationSlotSqr > innerRadiusSqr)
								{
									if (dot < 0.0f)  // we're ahead -> prevent slowly outrunning our nav-mesh path
										m_formationInfo.state = FormationInfo::State_SlowingDown;
								}
								break;

							case FormationInfo::State_CatchingUp:
								if (distanceToFormationSlotSqr < innerRadiusSqr)
								{
									m_formationInfo.state = FormationInfo::State_MatchingLeaderSpeed;
								}
								else if (dot < 0.0f) // sudden change in direction occurred
								{
									m_formationInfo.state = FormationInfo::State_MatchingLeaderSpeed;
								}
								break;

							case FormationInfo::State_SlowingDown:
								if (distanceToFormationSlotSqr < innerRadiusSqr)
								{
									if (dot > 0.0f)  // we're behind
										m_formationInfo.state = FormationInfo::State_MatchingLeaderSpeed;
								}
								else if (dot > 0.0f) // sudden change in direction occurred
								{
									m_formationInfo.state = FormationInfo::State_MatchingLeaderSpeed;
								}
								break;
							}

							switch (m_formationInfo.state)
							{
							case FormationInfo::State_MatchingLeaderSpeed:
								// nothing, just adhere to the leader's speed
								break;

							case FormationInfo::State_CatchingUp:
								newSpeed = std::max(1.0f, newSpeed * 1.5f); // experimental values
								break;

							case FormationInfo::State_SlowingDown:
								newSpeed = std::min(0.3f, newSpeed * 0.5f); // experimental values
								break;
							}
						}
						else
						{
							// our formation slot is not reachable, and we're moving behind the leader already, so keep a slightly slower speed than the leader
							const float fractionOfLeaderSpeed = 0.8f; // experimental value
							newSpeed *= fractionOfLeaderSpeed;
						}

						pipeUser.SetSpeed(newSpeed);
						m_movementStyle.SetSpeedLiteral(newSpeed);    // also use the changed speed when re-pathing
					}
				}
			}
		}
	}
}

float MoveOp::GetSquaredDistanceToDestination(CPipeUser& pipeUser)
{
	const Vec3 destinationPosition = DestinationPositionFor(pipeUser);
	return destinationPosition.GetSquaredDistance(pipeUser.GetPos());
}

void MoveOp::RequestStop(CPipeUser& pipeUser)
{
	MovementRequest movementRequest;
	movementRequest.entityID = pipeUser.GetEntityID();
	movementRequest.type = MovementRequest::Stop;
	gEnv->pAISystem->GetMovementSystem()->QueueRequest(movementRequest);
}

void MoveOp::GetClosestDesignedPath(CPipeUser& pipeUser, stack_string& closestPathName)
{
	const Vec3& userPosition = pipeUser.GetPos();
	const ShapeMap& shapesMap = gAIEnv.pNavigation->GetDesignerPaths();
	ShapeMap::const_iterator currentShapeIt = shapesMap.begin();
	ShapeMap::const_iterator endShapesMap = shapesMap.end();
	float minDistance = FLT_MAX;
	for (; currentShapeIt != endShapesMap; ++currentShapeIt)
	{
		float distanceToNearestPointOnPath;
		Vec3 closestPoint;
		currentShapeIt->second.NearestPointOnPath(userPosition, false, distanceToNearestPointOnPath, closestPoint);
		if (distanceToNearestPointOnPath < minDistance)
		{
			minDistance = distanceToNearestPointOnPath;
			closestPathName = currentShapeIt->first;
		}
	}
}
