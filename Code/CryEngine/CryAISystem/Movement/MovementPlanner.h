// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#ifndef MovementPlanner_h
	#define MovementPlanner_h

	#include "MovementPlan.h"
	#include <CryAISystem/MovementRequest.h>
	#include <CryAISystem/IMovementPlanner.h>

struct MovementActor;
struct MovementUpdateContext;

namespace Movement
{
// This generic movement planner will be able to these satisfy requests:
// - Move to position in open space
// - Move to cover
// - Move along path
// It will take care of leaving the cover before moving to a position.
class GenericPlanner : public IPlanner
{
public:
	explicit GenericPlanner(NavigationAgentTypeID navigationAgentTypeID);
	virtual bool   IsUpdateNeeded() const override;
	virtual void   StartWorkingOnRequest(const MovementRequestID& requestId, const MovementRequest& request, const MovementUpdateContext& context) override;
	virtual void   CancelCurrentRequest(IMovementActor& actor) override;
	virtual Status Update(const MovementUpdateContext& context) override;
	virtual bool   IsReadyForNewRequest() const override;
	virtual void   GetStatus(MovementRequestStatus& status) const override;

private:
	void CheckOnPathfinder(const MovementUpdateContext& context, OUT Status& status);
	void ExecuteCurrentPlan(const MovementUpdateContext& context, OUT Status& status);
	void ProducePlan(const MovementUpdateContext& context);
	void ProduceMoveToPlan(const MovementUpdateContext& context);
	void ProduceStopPlan(const MovementUpdateContext& context);
	bool CanReplan(const MovementRequest& request) const;
	void StartWorkingOnRequest_Internal(const MovementRequestID& requestId, const MovementRequest& request, const MovementUpdateContext& context);

private:
	// Reasons for potential path-replanning
	struct SPendingPathReplanning
	{
		bool bSomeBlockNeedsReplanning;    // some of the movement blocks in the plan recognized potentially problematic environment changes (e.g. NavMesh regeneration), that affects them and therefore request to re-plan
		bool bSuddenNonInterruptibleBlock; // the pathfinder returned a path after a previously interruptible block suddenly became non-interruptible (can happen when the UseSmartObject transitions from its internal "Prepare" state to "Traverse")

		SPendingPathReplanning()
		{
			Clear();
		}

		void Clear()
		{
			bSomeBlockNeedsReplanning = bSuddenNonInterruptibleBlock = false;
		}

		bool IsPending() const
		{
			return bSomeBlockNeedsReplanning || bSuddenNonInterruptibleBlock;
		}
	};

	const NavigationAgentTypeID  m_navigationAgentTypeID;
	SPendingPathReplanning       m_pendingPathReplanning;   // dirty-flag to automatically re-path as soon as the possibly existing plan allows for it again
	Plan                         m_plan;
	MovementRequestID            m_requestId;
	MovementRequest              m_request;
	uint8                        m_replanningAfterFailCount;
	bool                         m_pathfinderRequestQueued;
	const static uint8           s_maxAllowedReplanning = 3;
};
}

#endif // MovementPlanner_h
