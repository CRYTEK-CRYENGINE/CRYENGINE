// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "FlyHelpers_PathFollower.h"

#include "FlyHelpers_PathFollowerHelpers.h"
#include "FlyHelpers_Debug.h"

namespace FlyHelpers
{

PathFollower::PathFollower()
	: m_targetPosition(0, 0, 0)
	, m_lookAtPosition(0, 0, 0)
	, m_finalPosition(0, 0, 0)
	, m_stopAtFinalLocation(false)
	, m_pCurrentPath(NULL)
{
}

PathFollower::~PathFollower()
{
}

void PathFollower::SetPath(const SShape& path, const bool loopAlongPath)
{
	m_pCurrentPath = &m_path;

	m_path.Clear();
	for (size_t i = 0; i < path.shape.size(); ++i)
	{
		const Vec3& point = path.shape[i];
		m_path.AddPoint(point);
	}

	if (loopAlongPath)
	{
		m_path.MakeLooping();
	}

	m_reversePath.Clear();
	const size_t pathPointCount = m_path.GetPointCount();
	for (size_t i = 1; i <= pathPointCount; ++i)
	{
		const size_t reverseIndex = pathPointCount - i;
		const Vec3& reversePoint = m_path.GetPoint(reverseIndex);
		m_reversePath.AddPoint(reversePoint);
	}

	ResetFinalPathLocation();
}

void PathFollower::SetUseReversePath(const bool useReversePath)
{
	CRY_ASSERT(m_pCurrentPath);

	const bool usingReversePath = (m_pCurrentPath == &m_reversePath);
	const bool shouldSwitchPaths = (usingReversePath != useReversePath);
	if (!shouldSwitchPaths)
	{
		return;
	}

	m_targetLocation = GetReversePathLocation(*m_pCurrentPath, m_targetLocation);
	m_lookAtLocation = GetReversePathLocation(*m_pCurrentPath, m_lookAtLocation);
	m_finalLocation = GetReversePathLocation(*m_pCurrentPath, m_finalLocation);

	m_pCurrentPath = useReversePath ? &m_reversePath : &m_path;

	m_targetPosition = CalculatePathPosition(m_targetLocation);
	m_lookAtPosition = CalculatePathPosition(m_lookAtLocation);
	m_finalPosition = CalculatePathPosition(m_finalLocation);
}

PathLocation PathFollower::GetNonReversedTargetLocation() const
{
	CRY_ASSERT(m_pCurrentPath);

	const bool usingReversePath = (m_pCurrentPath == &m_reversePath);
	if (usingReversePath)
	{
		return GetReversePathLocation(m_reversePath, m_targetLocation);
	}
	else
	{
		return m_targetLocation;
	}
}

void PathFollower::SetFinalPathLocation(const Vec3& position)
{
	m_stopAtFinalLocation = true;

	const PathLocation closestPathLocation = FindClosestPathLocation(*m_pCurrentPath, position);
	m_finalLocation = closestPathLocation;
	m_finalPosition = CalculatePathPosition(m_finalLocation);

	const float distanceToNewFinalPositionSquared = Distance::Point_PointSq(m_finalPosition, m_targetPosition);
	const float pathRadiusSquared = m_params.pathRadius * m_params.pathRadius;
	const bool keepCurrentPathDirection = (distanceToNewFinalPositionSquared <= pathRadiusSquared);
	if (keepCurrentPathDirection)
	{
		if (m_finalLocation < m_targetLocation)
		{
			m_targetLocation = m_finalLocation;
			m_targetPosition = m_finalPosition;
		}
		return;
	}

	bool switchPathDirection = false;
	if (m_params.loopAlongPath)
	{
		const float totalPathDistance = m_pCurrentPath->GetTotalPathDistance();

		const float distanceAlongCurrentPath = m_targetLocation.CalculateDistanceAlongPathInTheCurrentDirectionTo(*m_pCurrentPath, closestPathLocation);

		// TODO: Replace radius with distance from entity to target? Should use projected entity location in path instead?
		const float paddedDistanceAlongCurrentPath = max(0.0f, distanceAlongCurrentPath - m_params.pathRadius);

		const float distanceAlongReversedCurrentPath = totalPathDistance - paddedDistanceAlongCurrentPath;

		switchPathDirection = (distanceAlongReversedCurrentPath < paddedDistanceAlongCurrentPath);
	}
	else
	{
		// TODO: target location might not be the best here, we should use projected entity position instead likely.
		switchPathDirection = (m_finalLocation < m_targetLocation);
	}

	if (switchPathDirection)
	{
		const bool usingReversePath = (m_pCurrentPath == &m_reversePath);
		SetUseReversePath(!usingReversePath);
	}

	m_finalPosition = CalculatePathPosition(m_finalLocation);
}

void PathFollower::ResetFinalPathLocation()
{
	m_stopAtFinalLocation = (!m_params.loopAlongPath);

	SetUseReversePath(false);

	const size_t segmentCount = m_path.GetSegmentCount();
	m_finalLocation.segmentIndex = (segmentCount == 0) ? 0 : segmentCount - 1;
	m_finalLocation.normalizedSegmentPosition = 1.0f;
	m_finalPosition = CalculatePathPosition(m_finalLocation);
}

void PathFollower::SetDesiredSpeed(const float desiredSpeed)
{
	m_params.desiredSpeed = desiredSpeed;
}

void PathFollower::Init(const SShape& path, const PathFollowerParams& params, const PathEntityIn& pathEntity)
{
	m_params = params;
	m_params.lookAheadDistance = max(0.1f, m_params.lookAheadDistance);

	SetPath(path, m_params.loopAlongPath);

	CRY_ASSERT(m_pCurrentPath);

	if (m_params.startPathFromClosestLocation)
	{
		const PathLocation closestPathLocation = FindClosestPathLocation(*m_pCurrentPath, pathEntity.position);
		m_targetLocation = closestPathLocation;
	}
	else
	{
		const PathLocation startPathLocation;
		const PathLocation nearStartPathLocation = TracePathForward(*m_pCurrentPath, startPathLocation, m_params.maxStartDistanceAlongNonLoopingPath);

		// TODO: Don't create a new path copy, have FindClosestPathLocation take ranges?
		const Path startPathSection = CreatePathSubset(*m_pCurrentPath, startPathLocation, nearStartPathLocation);
		if (startPathSection.GetSegmentCount() > 0)
		{
			const PathLocation closestPathLocation = FindClosestPathLocation(startPathSection, pathEntity.position);
			m_targetLocation = closestPathLocation;
		}
		else
		{
			m_targetLocation = PathLocation();
		}
	}

	m_targetPosition = CalculatePathPosition(m_targetLocation);
}

PathEntityOut PathFollower::Update(const PathEntityIn& pathEntity, const float elapsedSeconds)
{
	CRY_ASSERT(m_pCurrentPath);

	const PathLocation targetLocationOld = m_targetLocation;
	const PathLocation lookAtLocationOld = m_lookAtLocation;

	// TODO: Arrived at path end?

	const float distanceToPathSquared = Distance::Point_PointSq(m_targetPosition, pathEntity.position);
	const bool closeToTargetPosition = (distanceToPathSquared < (m_params.pathRadius * m_params.pathRadius));

	if (closeToTargetPosition)
	{
		const float plannedAdvance = m_params.desiredSpeed * elapsedSeconds;
		m_targetLocation = CalculateNextPathLocation(m_targetLocation, plannedAdvance);
		if (m_stopAtFinalLocation)
		{
			const bool passedFinalLocation = (targetLocationOld <= m_finalLocation && m_finalLocation <= m_targetLocation);
			const bool loopedPastFinalLocation = (m_targetLocation <= targetLocationOld && (targetLocationOld <= m_finalLocation || m_finalLocation <= m_targetLocation));
			const bool shouldClampTargetLocation = (passedFinalLocation || loopedPastFinalLocation);
			if (shouldClampTargetLocation)
			{
				m_targetLocation = m_finalLocation;
			}
		}
		m_targetPosition = CalculatePathPosition(m_targetLocation);
	}

	m_lookAtLocation = CalculateNextPathLocation(m_targetLocation, m_params.lookAheadDistance);
	m_lookAtPosition = CalculatePathPosition(m_lookAtLocation);

	PathEntityOut result;
	result.speed = m_params.desiredSpeed;
	result.moveDirection = m_targetPosition - pathEntity.position;

	result.moveDirection.NormalizeSafe(pathEntity.forward);
	result.lookPosition = m_lookAtPosition;
	result.lookDirection = m_lookAtPosition - pathEntity.position;
	result.lookDirection.NormalizeSafe(pathEntity.forward);

	const float distanceToTargetPosition = (m_targetPosition - pathEntity.position).GetLength();
	const float remainingDistanceAlongPathFromTarget = m_targetLocation.CalculateDistanceAlongPathTo(*m_pCurrentPath, m_finalLocation);
	const float remainingDistanceApproximation = distanceToTargetPosition + remainingDistanceAlongPathFromTarget;
	result.distanceToPathEnd = remainingDistanceApproximation;

	if (m_stopAtFinalLocation)
	{
		if (0.0f < m_params.decelerateDistance)
		{
			if (remainingDistanceApproximation < m_params.decelerateDistance)
			{
				result.speed *= remainingDistanceApproximation / m_params.decelerateDistance;
			}
		}
	}

	return result;
}

Vec3 PathFollower::CalculatePathPosition(const PathLocation& pathLocation) const
{
	CRY_ASSERT(m_pCurrentPath);

	if (m_params.loopAlongPath)
	{
		return pathLocation.CalculatePathPositionCatmullRomLooping(*m_pCurrentPath);
	}
	else
	{
		return pathLocation.CalculatePathPositionCatmullRom(*m_pCurrentPath);
	}
}

PathLocation PathFollower::CalculateNextPathLocation(const PathLocation& pathLocation, const float advance) const
{
	CRY_ASSERT(m_pCurrentPath);

	if (m_params.loopAlongPath)
	{
		return TracePathForwardLooping(*m_pCurrentPath, pathLocation, advance);
	}
	else
	{
		return TracePathForward(*m_pCurrentPath, pathLocation, advance);
	}
}

void PathFollower::Draw()
{
	CRY_ASSERT(m_pCurrentPath);

	const bool usingReversedPath = (m_pCurrentPath == &m_reversePath);
	const ColorB pathColor = (usingReversedPath) ? Col_Yellow : Col_White;
	DrawDebugPath(*m_pCurrentPath, pathColor);

	DrawDebugLocation(m_targetPosition, Col_Green);
	DrawDebugLocation(m_lookAtPosition, Col_Red);
	DrawDebugLocation(m_finalPosition, Col_Yellow);
}

}
