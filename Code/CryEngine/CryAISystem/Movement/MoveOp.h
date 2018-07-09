// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#ifndef MoveOp_h
	#define MoveOp_h

	#include "EnterLeaveUpdateGoalOp.h"
	#include <CryAISystem/MovementRequest.h>
	#include <CryAISystem/MovementStyle.h>

struct MovementRequestResult;

class MoveOp : public EnterLeaveUpdateGoalOp
{
public:
	MoveOp();
	MoveOp(const XmlNodeRef& node);

	virtual void Enter(CPipeUser& pipeUser);
	virtual void Leave(CPipeUser& pipeUser);
	virtual void Update(CPipeUser& pipeUser);

	void         SetMovementStyle(MovementStyle movementStyle);
	void         SetStopWithinDistance(float distance);
	void         SetRequestStopWhenLeaving(bool requestStop);

	enum DestinationType
	{
		Target,
		Cover,
		ReferencePoint,
		FollowPath,
		Formation,
	};

private:
	Vec3  DestinationPositionFor(CPipeUser& pipeUser);
	Vec3  GetCoverRegisterLocation(const CPipeUser& pipeUser) const;
	void  RequestMovementTo(const Vec3& position, CPipeUser& pipeUser);
	void  RequestFollowExistingPath(const char* pathName, CPipeUser& pipeUser);
	void  ReleaseCurrentMovementRequest();
	void  MovementRequestCallback(const MovementRequestResult& result);
	void  ChaseTarget(CPipeUser& pipeUser);
	float GetSquaredDistanceToDestination(CPipeUser& pipeUser);
	void  RequestStop(CPipeUser& pipeUser);
	void  GetClosestDesignedPath(CPipeUser& pipeUser, stack_string& closestPathName);
	void  SetupDangersFlagsForDestination(bool shouldAvoidDangers);
	Vec3              m_destinationAtTimeOfMovementRequest;
	float             m_stopWithinDistanceSq;
	MovementStyle     m_movementStyle;
	MovementRequestID m_movementRequestID;
	DestinationType   m_destination;
	stack_string      m_pathName;
	MNMDangersFlags   m_dangersFlags;
	bool              m_requestStopWhenLeaving;

	// When following according to the formation, we impose 2 radii around our formation slot:
	// If we're *outside* the outer radius and behind, we catch up until we are inside the *inner* radius. From then on
	// we simply match our speed with that of the formation owner. If we fall behind outside the outer radius,
	// we catch up again. If we're outside the outer radius but *ahead*, then we slow down.
	struct FormationInfo
	{
		enum State
		{
			State_MatchingLeaderSpeed, // we're inside the inner radius
			State_CatchingUp,          // we're outside the outer radius and behind our formation slot
			State_SlowingDown,         // we're outside the outer radius and ahead of our formation slot
		};

		State state;
		Vec3  positionInFormation;
		bool  positionInFormationIsReachable;

		FormationInfo()
			: state(State_MatchingLeaderSpeed)
			, positionInFormation(ZERO)
			, positionInFormationIsReachable(true)
		{}
	};

	FormationInfo m_formationInfo;    // only used if m_destination == Formation, ignored for all other destination types
};

struct MoveOpDictionaryCollection
{
	MoveOpDictionaryCollection();

	CXMLAttrReader<MoveOp::DestinationType> destinationTypes;
};

#endif // MoveOp_h
