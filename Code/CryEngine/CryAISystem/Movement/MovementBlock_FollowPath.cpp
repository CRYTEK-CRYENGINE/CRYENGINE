// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "MovementBlock_FollowPath.h"
#include <CryAISystem/MovementUpdateContext.h>
#include "MovementActor.h"
#include "PipeUser.h"
#include "Cover/CoverSystem.h"
#include "Navigation/NavigationSystem/NavigationSystem.h"
#include <CryAISystem/NavigationSystem/INavMeshQueryManager.h>

namespace Movement
{
namespace MovementBlocks
{

FollowPath::FollowPath(
  IPlan& plan,
  NavigationAgentTypeID navigationAgentTypeID,
  const CNavPath& path,
  const float endDistance,
  const MovementStyle& style,
  const bool bLastFollowBlock)
	: m_bRegisteredAsPlanMonitor(false)
	, m_plan(plan)
	, m_navigationAgentTypeID(navigationAgentTypeID)
	, m_path(path)
	, m_finishBlockEndDistance(endDistance)
	, m_accumulatedPathFollowerFailureTime(0.0f)
	, m_style(style)
	, m_bLastFollowBlock(bLastFollowBlock)
	, m_executionState(EExecutionState::NotYetExecuting)
{
	RegisterAsPlanMonitor();
}

FollowPath::~FollowPath()
{
	UnregisterAsPlanMonitor();
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

	m_executionState = EExecutionState::CurrentlyExecuting;
}

void FollowPath::End(IMovementActor& actor)
{
	m_lookTarget.reset();
	m_executionState = EExecutionState::FinishedExecuting;
	UnregisterAsPlanMonitor();
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

bool FollowPath::CheckForReplanning(const MovementUpdateContext& context)
{
	bool bNeedsReplanning = false;

	if (!m_queuedNavMeshChanges.empty())
	{
		switch (m_executionState)
		{
		case EExecutionState::NotYetExecuting:
			bNeedsReplanning = IsPathInterferingWithAnyQueuedNavMeshChanges(context.pathFollower.GetParams().pQueryFilter);
			break;

		case EExecutionState::CurrentlyExecuting:
			bNeedsReplanning = IsPathFollowerInterferingWithAnyQueuedNavMeshChanges(context.pathFollower);
			break;

		case EExecutionState::FinishedExecuting:
			bNeedsReplanning = false;
			break;

		default:
			static_assert((int)EExecutionState::Count == 3, "EExecutionState enum literals mismatch");
		}

		m_queuedNavMeshChanges.clear();
	}

	return bNeedsReplanning;
}

void FollowPath::RegisterAsPlanMonitor()
{
	if (!m_bRegisteredAsPlanMonitor)
	{
		m_plan.InstallPlanMonitor(this);
		gAIEnv.pNavigationSystem->AddMeshChangeCallback(m_navigationAgentTypeID, functor(*this, &FollowPath::OnNavigationMeshChanged));
		gAIEnv.pNavigationSystem->AddMeshAnnotationChangeCallback(m_navigationAgentTypeID, functor(*this, &FollowPath::OnNavigationAnnotationChanged));
		m_bRegisteredAsPlanMonitor = true;
	}
}

void FollowPath::UnregisterAsPlanMonitor()
{
	if (m_bRegisteredAsPlanMonitor)
	{
		m_plan.UninstallPlanMonitor(this);
		gAIEnv.pNavigationSystem->RemoveMeshChangeCallback(m_navigationAgentTypeID, functor(*this, &FollowPath::OnNavigationMeshChanged));
		gAIEnv.pNavigationSystem->RemoveMeshAnnotationChangeCallback(m_navigationAgentTypeID, functor(*this, &FollowPath::OnNavigationAnnotationChanged));
		m_bRegisteredAsPlanMonitor = false;
	}
}

void FollowPath::OnNavigationMeshChanged(NavigationAgentTypeID navigationAgentTypeID, NavigationMeshID meshID, MNM::TileID tileID)
{
	QueueNavigationChange(navigationAgentTypeID, meshID, tileID, SMeshTileChange::EChangeType::AfterGeneration);
}

void FollowPath::OnNavigationAnnotationChanged(NavigationAgentTypeID navigationAgentTypeID, NavigationMeshID meshID, MNM::TileID tileID)
{
	QueueNavigationChange(navigationAgentTypeID, meshID, tileID, SMeshTileChange::EChangeType::Annotation);
}

void FollowPath::QueueNavigationChange(NavigationAgentTypeID navigationAgentTypeID, NavigationMeshID meshID, MNM::TileID tileID, const SMeshTileChange::ChangeFlags& changeFlag)
{
	if (gAIEnv.CVars.movement.MovementSystemPathReplanningEnabled)
	{
		if (m_path.GetMeshID() == meshID)
		{
			const SMeshTileChange meshTileChange(meshID, tileID, changeFlag);
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

bool FollowPath::IsPathOverlappingWithAnyAffectedNavMeshTileBounds() const
{
	//
	// build an AABB that encompasses the full path
	//

	const TPathPoints& pathPoints = m_path.GetPath();

	if (pathPoints.empty())
	{
		return false;
	}

	AABB pathAABB;
	pathAABB.min = pathAABB.max = pathPoints.front().vPos;

	for (auto it = ++pathPoints.cbegin(); it != pathPoints.cend(); ++it)
	{
		pathAABB.Add(it->vPos);
	}

	// extend the path's AABB a bit as the path is usually very close to the ground and might therefore slightly slip under the tile's AABB
	pathAABB.Expand(Vec3(0.0f, 0.0f, 0.5f));

	//
	// check if any tile of the queued changes overlaps with the path's AABB
	//

	for (const SMeshTileChange& change : m_queuedNavMeshChanges)
	{
		assert(m_path.GetMeshID() == change.meshID);

		AABB tileAABB;
		gAIEnv.pNavigationSystem->GetTileBoundsForMesh(change.meshID, change.tileID, tileAABB);
		if (pathAABB.IsIntersectBox(tileAABB))
		{
			return true;
		}
	}

	return false;
}

bool FollowPath::IsPathTraversableOnNavMesh(const INavMeshQueryFilter* pFilter) const
{
	if (const NavigationMeshID meshIDUsedByPath = m_path.GetMeshID())
	{
		const MNM::CNavMesh& navMeshUsedByPath = gAIEnv.pNavigationSystem->GetMesh(meshIDUsedByPath).navMesh;
		const TPathPoints& pathPoints = m_path.GetPath();

		if (!pathPoints.empty())
		{
			auto it1 = pathPoints.cbegin();
			auto it2 = it1;
			++it2;

			// - perform raycasts along all segments of the remaining path
			// - as soon as a raycast fails we know that the segment can no longer be used to move along
			const MNM::real_t verticalRange(2.0f);
			for (; it2 != pathPoints.cend(); it1 = it2++)
			{
				const Vec3& segmentPos1 = it1->vPos;
				const Vec3& segmentPos2 = it2->vPos;

				const MNM::vector3_t mnmStartLoc = navMeshUsedByPath.ToMeshSpace(segmentPos1);
				const MNM::vector3_t mnmEndLoc = navMeshUsedByPath.ToMeshSpace(segmentPos2);
				const MNM::TriangleID triStart = navMeshUsedByPath.QueryTriangleAt(mnmStartLoc, verticalRange, verticalRange, MNM::ENavMeshQueryOverlappingMode::BoundingBox_Partial, pFilter);
				const MNM::TriangleID triEnd = navMeshUsedByPath.QueryTriangleAt(mnmEndLoc, verticalRange, verticalRange, MNM::ENavMeshQueryOverlappingMode::BoundingBox_Partial, pFilter);

				if (!triStart || !triEnd)
				{
					return false;
				}

				if (triStart)
				{
					MNM::CNavMesh::RayCastRequest<512> raycastRequest;
					const MNM::ERayCastResult raycastResult = navMeshUsedByPath.RayCast(mnmStartLoc, triStart, mnmEndLoc, triEnd, raycastRequest, pFilter);

					if (raycastResult != MNM::ERayCastResult::NoHit)
					{
						return false;
					}
				}
			}
		}
	}

	return true;
}

bool FollowPath::IsPathInterferingWithAnyQueuedNavMeshChanges(const INavMeshQueryFilter* pFilter) const
{
	if (IsPathOverlappingWithAnyAffectedNavMeshTileBounds())
	{
		SMeshTileChange::ChangeFlags overallChangeFlags;

		for (const SMeshTileChange& change : m_queuedNavMeshChanges)
		{
			overallChangeFlags |= change.changeFlags;
		}

		if (overallChangeFlags.Check(SMeshTileChange::EChangeType::Annotation))
		{
			if (!m_path.CanPassFilter(0, pFilter))
			{
				return true;
			}
		}

		if (overallChangeFlags.Check(SMeshTileChange::EChangeType::AfterGeneration))
		{
			if (!IsPathTraversableOnNavMesh(pFilter))
			{
				return true;
			}
		}
	}

	return false;
}

bool FollowPath::IsPathFollowerInterferingWithAnyQueuedNavMeshChanges(const IPathFollower& pathFollower) const
{
	for (const SMeshTileChange& change : m_queuedNavMeshChanges)
	{
		const bool bAnnotationChange = change.changeFlags.Check(SMeshTileChange::EChangeType::Annotation);
		const bool bDataChange = change.changeFlags.Check(SMeshTileChange::EChangeType::AfterGeneration);

		if (pathFollower.IsRemainingPathAffectedByNavMeshChange(change.meshID, change.tileID, bAnnotationChange, bDataChange))
		{
			return true;
		}
	}
	return false;
}

}
}
