// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "MovementPlanner.h"
#include "MovementActor.h"
#include <CryAISystem/MovementUpdateContext.h>
#include "MovementSystem.h"

#include "MovementBlock_FollowPath.h"
#include "MovementBlock_HarshStop.h"
#include "MovementBlock_TurnTowardsPosition.h"
#include "MovementBlock_UseSmartObject.h"
#include "MovementBlock_UseExactPositioning.h"

#include "../Navigation/NavigationSystem/NavigationSystem.h"

namespace Movement
{
GenericPlanner::GenericPlanner(NavigationAgentTypeID navigationAgentTypeID)
	: m_navigationAgentTypeID(navigationAgentTypeID)
	, m_pendingPathReplanning()
	, m_pathfinderRequestQueued(false)
{
	gAIEnv.pNavigationSystem->AddMeshChangeCallback(m_navigationAgentTypeID, functor(*this, &GenericPlanner::OnNavigationMeshChanged));
	gAIEnv.pNavigationSystem->AddMeshAnnotationChangeCallback(m_navigationAgentTypeID, functor(*this, &GenericPlanner::OnNavigationAnnotationChanged));
}

GenericPlanner::~GenericPlanner()
{
	gAIEnv.pNavigationSystem->RemoveMeshChangeCallback(m_navigationAgentTypeID, functor(*this, &GenericPlanner::OnNavigationMeshChanged));
	gAIEnv.pNavigationSystem->RemoveMeshAnnotationChangeCallback(m_navigationAgentTypeID, functor(*this, &GenericPlanner::OnNavigationAnnotationChanged));
}

bool GenericPlanner::IsUpdateNeeded() const
{
	return m_plan.HasBlocks() || m_pathfinderRequestQueued;
}

void GenericPlanner::StartWorkingOnRequest(const MovementRequestID& requestId, const MovementRequest& request, const MovementUpdateContext& context)
{
	m_replanningAfterFailCount = 0;
	m_pendingPathReplanning.Clear();
	StartWorkingOnRequest_Internal(requestId, request, context);
}

void GenericPlanner::StartWorkingOnRequest_Internal(const MovementRequestID& requestId, const MovementRequest& request, const MovementUpdateContext& context)
{
	assert(IsReadyForNewRequest());

	m_requestId = requestId;
	m_request = request;

	if (request.type == MovementRequest::MoveTo)
	{
		if (!m_request.style.IsMovingAlongDesignedPath())
		{
			// Future: We could path find from a bit further along the current plan.

			context.actor.RequestPathTo(request.destination, request.lengthToTrimFromThePathEnd, request.snappingRules, request.dangersFlags, request.considerActorsAsPathObstacles, request.pCustomPathCostComputer);
			m_pathfinderRequestQueued = true;

			//
			// Problem
			//  What if the controller starts planning the path from the current
			//  position while following a path and then keeps executing the plan
			//  and starts using a smart object and then a path comes back?
			//  This is not cool because we can't replace the current plan with
			//  the new one while traversing the smart object.
			//
			// Solution
			//  The controller can only start planning when not traversing a
			//  smart object and it must cut off all upcoming smart object blocks.
			//  This guarantees that when we get the path we'll be running a block
			//  that is interruptible, or no block. Good, this is what we want.
			//

			m_plan.CutOffAfterCurrentBlock();
		}
		else
		{
			// If we are following a path we don't need to request a path.
			// We can directly produce a plan.
			ProducePlan(context);
		}
	}
	else if (request.type == MovementRequest::Stop)
	{
		ProducePlan(context);
	}
	else
	{
		assert(false);   // Unsupported request type
	}
}

void GenericPlanner::CancelCurrentRequest(MovementActor& actor)
{
	//
	// The request has been canceled but the plan remains intact.
	// This means that if the actor is running along a path he will
	// keep running along that path.
	//
	// The idea is that an actor should only stop if a stop is
	// explicitly requested.
	//
	// Let's see how it works out in practice :)
	//

	m_requestId = MovementRequestID();
}

IPlanner::Status GenericPlanner::Update(const MovementUpdateContext& context)
{
	IPlanner::Status status;

	if (m_pathfinderRequestQueued)
	{
		CheckOnPathfinder(context, OUT status);

		if (status.HasPathfinderFailed())
		{
			return status;
		}
	}

	if (!m_pendingPathReplanning.bNavMeshChanged)
	{
		CheckForNeedToPathReplanningDueToNavMeshChanges(context);
	}

	if (m_pendingPathReplanning.IsPending())
	{
		if (IsReadyForNewRequest())
		{
			context.actor.Log("Movement plan couldn't be finished either due to path-invalidation from NavMesh change or due to sudden non-interruptible block, re-planning now.");
			StartWorkingOnRequest(m_requestId, m_request, context);
		}
	}

	ExecuteCurrentPlan(context, OUT status);

	return status;
}

void GenericPlanner::OnNavigationMeshChanged(NavigationAgentTypeID navigationAgentTypeID, NavigationMeshID meshID, uint32 tileID)
{
	QueueNavigationChange(navigationAgentTypeID, meshID, tileID, SMeshTileChange::EChangeType::AfterGeneration);
}

void GenericPlanner::OnNavigationAnnotationChanged(NavigationAgentTypeID navigationAgentTypeID, NavigationMeshID meshID, uint32 tileID)
{
	QueueNavigationChange(navigationAgentTypeID, meshID, tileID, SMeshTileChange::EChangeType::Annotation);
}

void GenericPlanner::QueueNavigationChange(NavigationAgentTypeID navigationAgentTypeID, NavigationMeshID meshID, uint32 tileID, const SMeshTileChange::ChangeFlags& changeFlag)
{
	if (gAIEnv.CVars.MovementSystemPathReplanningEnabled)
	{
		//
		// We're interested in the NavMesh-change only when we have a plan that we're executing or are still path-finding.
		//
		// Notice: The case where we're still waiting for the MNMPathfinder to find us a path is already handled by the MNMPathfinder itself.
		//         The NavigationSystem::Update() method is carefully designed such that the order of [MNMPathfinder gets notified of NavMesh-changes],
		//         [re-plans], [notifies of his found path] and the [time until GenericPlanner builds the plan by a call from MovementSystem::Update()] is even handling
		//         the case where a freshly found path would immediately become invalid on the next update loop and get detected by GenericPlanner as he already has
		//         a plan by that time.
		//         In other words: an extra OR-check for "m_pathfinderRequestQueued == true" is not necessary here.
		//
		if (m_plan.HasBlocks())
		{
			//
			// Ignore NavMesh changes when moving along a designer-placed path. It's the responsibility of the
			// level designer to place paths such that they will not interfere with (his scripted) NavMesh changes.
			//
			if (!m_request.style.IsMovingAlongDesignedPath())
			{
				// extra check for whether we're already aware of some previous NavMesh-change having invalidated our path (we're just waiting for the SmartObject-traversal to finish before re-planning)
				if (!m_pendingPathReplanning.bNavMeshChanged)
				{
					SMeshTileChange meshTileChange(meshID, tileID, changeFlag);
					auto findIt = std::find(m_queuedNavMeshChanges.begin(), m_queuedNavMeshChanges.end(), meshTileChange);
					if (findIt != m_queuedNavMeshChanges.end())
					{
						findIt->changeFlags |= meshTileChange.changeFlags;
					}
					else
					{
						m_queuedNavMeshChanges.push_back(meshTileChange);
					}
				}
			}
		}
	}
}

void GenericPlanner::CheckForNeedToPathReplanningDueToNavMeshChanges(const MovementUpdateContext& context)
{
	AIAssert(!m_pendingPathReplanning.bNavMeshChanged);

	//
	// React to NavMesh changes:
	// If we're no longer path-finding, we must have a plan that we might be executing already, so check with the path-follower if any NavMesh change will affect the path that
	// is being traversed by the plan.
	//

	if (!m_pathfinderRequestQueued && m_plan.HasBlocks())
	{
		if (!m_pendingPathReplanning.bNavMeshChanged)
		{
			//
			// Check with the path-follower for whether any of the NavMesh changes affects the path we're traversing.
			//
			for (const SMeshTileChange& meshTileChange : m_queuedNavMeshChanges)
			{
				const bool bAnnotationChange = meshTileChange.changeFlags.Check(SMeshTileChange::EChangeType::Annotation);
				const bool bDataChange = meshTileChange.changeFlags.Check(SMeshTileChange::EChangeType::AfterGeneration);
				if (context.pathFollower.IsRemainingPathAffectedByNavMeshChange(meshTileChange.meshID, meshTileChange.tileID, bAnnotationChange, bDataChange))
				{
					m_pendingPathReplanning.bNavMeshChanged = true;
					break;
				}
			}
		}

		//
		// Now, we don't need the collected NavMesh changes anymore.
		//
		stl::free_container(m_queuedNavMeshChanges);
	}
}

bool GenericPlanner::IsReadyForNewRequest() const
{
	if (m_pathfinderRequestQueued)
		return false;

	return m_plan.InterruptibleNow();
}

void GenericPlanner::GetStatus(MovementRequestStatus& status) const
{
	if (m_pathfinderRequestQueued)
	{
		status.id = MovementRequestStatus::FindingPath;
	}
	else
	{
		status.id = MovementRequestStatus::ExecutingPlan;
	}

	if (m_plan.GetBlockCount() > 0)
	{
		status.planStatus.abandoned = m_requestId.IsInvalid();
		status.planStatus.currentBlockIndex = m_plan.GetCurrentBlockIndex();

		switch (m_plan.GetLastStatus())
		{
		case Plan::Status::Running:
			status.planStatus.status = MovementRequestStatus::PlanStatus::Running;
			break;
		case Plan::Status::CantBeFinished:
			status.planStatus.status = MovementRequestStatus::PlanStatus::CantFinish;
			break;
		case Plan::Status::Finished:
			status.planStatus.status = MovementRequestStatus::PlanStatus::Finished;
			break;
		default:
			status.planStatus.status = MovementRequestStatus::PlanStatus::None;
			break;
		}

		for (uint32 i = 0, n = m_plan.GetBlockCount(); i < n; ++i)
		{
			const char* blockName = m_plan.GetBlock(i)->GetName();
			status.planStatus.blockInfos.push_back(blockName);
		}
	}
	else
	{
		status.planStatus.status = MovementRequestStatus::PlanStatus::None;
		status.planStatus.abandoned = false;
		status.planStatus.currentBlockIndex = Plan::NoBlockIndex;
	}
}

void GenericPlanner::CheckOnPathfinder(const MovementUpdateContext& context, OUT IPlanner::Status& status)
{
	const PathfinderState state = context.actor.GetPathfinderState();

	const bool pathfinderFinished = (state != StillFinding);
	if (pathfinderFinished)
	{
		m_pathfinderRequestQueued = false;

		if (state == FoundPath)
		{
			// Careful: of course, we started pathfinding at a time when only interruptible blocks were running, but:
			//          At the time we received the resulting path, a UseSmartObject block might have just transitioned from its internal "Prepare" state to "Traverse" which
			//          means that this block suddenly is no longer interruptible (!) So, just don't replace the ongoing plan, but set a pending-flag to let Update() do a full re-pathfinding
			//          at an appropriate time.

			if (m_plan.InterruptibleNow())
			{
				// fine, the pathfind request started and finished within an interruptible block
				ProducePlan(context);
			}
			else
			{
				// oh oh... we started in an interruptible block (fine), but received the path when that particular block was suddenly no longer interruptible!
				m_pendingPathReplanning.bSuddenNonInterruptibleBlock = true;
			}
		}
		else /*state == CouldNotFindPath || state == Canceled*/
		{
			status.SetPathfinderFailed(m_requestId);
		}
	}
}

void GenericPlanner::ExecuteCurrentPlan(const MovementUpdateContext& context, OUT IPlanner::Status& status)
{
	if (m_plan.HasBlocks())
	{
		const Plan::Status s = m_plan.Execute(context);

		if (s == Plan::Finished)
		{
			status.SetRequestSatisfied(m_plan.GetRequestId());
			m_plan.Clear(context.actor);
		}
		else if (s == Plan::CantBeFinished)
		{
			// The current plan can't be finished. We'll re-plan as soon as
			// we can to see if we can satisfy the request.
			// Note: it could become necessary to add a "retry count".
			if (m_request.style.IsMovingAlongDesignedPath())
			{
				status.SetMovingAlongPathFailed(m_plan.GetRequestId());
			}
			else if (IsReadyForNewRequest())
			{
				if (CanReplan(m_request))
				{
					++m_replanningAfterFailCount;

					context.actor.Log("Movement plan couldn't be finished, re-planning.");
					const MovementRequest replanRequest = m_request;
					StartWorkingOnRequest_Internal(m_requestId, replanRequest, context);
				}
				else
				{
					status.SetReachedMaxAllowedReplans(m_plan.GetRequestId());
				}
			}
		}
		else
		{
			assert(s == Plan::Running);
		}
	}
}

void GenericPlanner::ProducePlan(const MovementUpdateContext& context)
{
	// Future: It would be good to respect the current plan and patch it
	// up with the newly found path. This would also require that we
	// kick off the path finding process with a predicted future position.

	// We should have shaved off non-interruptible movement blocks in
	// StartWorkingOnRequest before we started path finding.
	assert(m_plan.InterruptibleNow());

	m_plan.Clear(context.actor);
	m_plan.SetRequestId(m_requestId);

	switch (m_request.type)
	{
	case MovementRequest::MoveTo:
		ProduceMoveToPlan(context);
		break;

	case MovementRequest::Stop:
		ProduceStopPlan(context);
		break;

	default:
		assert(false);
	}

	context.actor.GetAdapter().OnMovementPlanProduced();
}

void GenericPlanner::ProduceMoveToPlan(const MovementUpdateContext& context)
{
	using namespace Movement::MovementBlocks;
	if (m_request.style.IsMovingAlongDesignedPath())
	{
		// If the agent is forced to follow an AIPath then only the
		// FollowPath block is needed
		SShape designedPath;
		if (!context.actor.GetAdapter().GetDesignedPath(designedPath))
			return;

		if (designedPath.shape.empty())
			return;

		TPathPoints fullPath(designedPath.shape.begin(), designedPath.shape.end());
		CNavPath navPath;
		navPath.SetPathPoints(fullPath);
		m_plan.AddBlock(BlockPtr(new FollowPath(navPath, .0f, m_request.style, false)));
	}
	else
	{
		const INavPath* pNavPath = context.actor.GetCallbacks().getPathFunction();
		const TPathPoints fullPath = pNavPath->GetPath();

		if (m_request.style.ShouldTurnTowardsMovementDirectionBeforeMoving())
		{
			if (fullPath.size() >= 2)
			{
				// Stop moving before we turn. The turn expects to be standing still.
				m_plan.AddBlock<HarshStop>();

				// Using the second point of the path. Ideally, we should turn
				// towards a position once we've processed the path with the
				// smart path follower to be sure we're really turning towards
				// the direction we will later move towards. But the path has
				// been somewhat straightened out already so this should be fine.
				TPathPoints::const_iterator it = fullPath.begin();
				++it;
				const Vec3 positionToTurnTowards = it->vPos;

				// Adding a dead-zone here because small inaccuracies
				// in the positioning will otherwise make the object turn in a
				// seemingly 'random' direction each time when it is ordered to
				// move to the spot where it already is at.
				static const float distanceThreshold = 0.2f;
				static const float distanceThresholdSq = distanceThreshold * distanceThreshold;
				const float distanceToTurnPointSq = Distance::Point_PointSq(positionToTurnTowards, context.actor.GetAdapter().GetPhysicsPosition());
				if (distanceToTurnPointSq >= distanceThresholdSq)
				{
					m_plan.AddBlock(BlockPtr(new TurnTowardsPosition(positionToTurnTowards)));
				}
			}
		}

		context.actor.GetActionAbilities().CreateStartBlocksForPlanner(m_request, [this](BlockPtr block)
		{
			m_plan.AddBlock(block);
		});

		// Go through the full path from start to end and split it up into
		// FollowPath & UseSmartObject blocks.
		TPathPoints::const_iterator first = fullPath.begin();
		TPathPoints::const_iterator curr = fullPath.begin();
		TPathPoints::const_iterator end = fullPath.end();

		std::shared_ptr<UseSmartObject> lastAddedSmartObjectBlock;

		assert(curr != end);

		while (curr != end)
		{
			TPathPoints::const_iterator next = curr;
			++next;

			const PathPointDescriptor& point = *curr;

			const bool isSmartObject = point.navType == IAISystem::NAV_SMARTOBJECT;
			const bool isCustomObject = point.navType == IAISystem::NAV_CUSTOM_NAVIGATION;
			const bool isLastNode = next == end;

			CNavPath path;

			if (isCustomObject || isSmartObject || isLastNode)
			{
				// Extract the path between the first point and
				// the smart object/last node we just found.
				TPathPoints points;
				points.assign(first, next);
				path.SetPathPoints(points);

				const bool blockAfterThisIsUseExactPositioning = isLastNode && (m_request.style.GetExactPositioningRequest() != 0);
				const bool blockAfterThisUsesSomeFormOfExactPositioning = isSmartObject || isCustomObject || blockAfterThisIsUseExactPositioning;
				const float endDistance = blockAfterThisUsesSomeFormOfExactPositioning ? 2.5f : 0.0f;   // The value 2.5 meters was used prior to Crysis 3
				m_plan.AddBlock(BlockPtr(new FollowPath(path, endDistance, m_request.style, isLastNode)));

				if (lastAddedSmartObjectBlock)
				{
					lastAddedSmartObjectBlock->SetUpcomingPath(path);
					lastAddedSmartObjectBlock->SetUpcomingStyle(m_request.style);
				}

				if (blockAfterThisIsUseExactPositioning)
				{
					assert(!isSmartObject);
					assert(!path.Empty());
					m_plan.AddBlock(BlockPtr(new UseExactPositioning(path, m_request.style)));
				}
			}

			if (isSmartObject || isCustomObject)
			{
				assert(!path.Empty());
				if (isSmartObject)
				{
					std::shared_ptr<UseSmartObject> useSmartObjectBlock(new UseSmartObject(path, point.offMeshLinkData, m_request.style));
					lastAddedSmartObjectBlock = useSmartObjectBlock;
					m_plan.AddBlock(useSmartObjectBlock);
				}
				else
				{
					MovementSystem aiMovementSystem = static_cast<MovementSystem&>(context.movementSystem);
					m_plan.AddBlock(aiMovementSystem.CreateCustomBlock(path, point.offMeshLinkData, m_request.style));
				}

				++curr;
				first = curr;
			}
			else
			{
				++curr;
			}
		}

		context.actor.GetActionAbilities().CreateEndBlocksForPlanner(m_request, [this](BlockPtr block)
		{
			m_plan.AddBlock(block);
		});
	}
}

void GenericPlanner::ProduceStopPlan(const MovementUpdateContext& context)
{
	using namespace Movement::MovementBlocks;

	m_plan.AddBlock(BlockPtr(new HarshStop()));
}

bool GenericPlanner::CanReplan(const MovementRequest& request) const
{
	return m_replanningAfterFailCount < s_maxAllowedReplanning;
}
}
