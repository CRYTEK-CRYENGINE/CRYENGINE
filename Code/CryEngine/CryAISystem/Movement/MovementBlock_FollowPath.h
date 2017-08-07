// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#ifndef MovementBlock_FollowPath_h
#define MovementBlock_FollowPath_h

#include "MovementPlan.h"
#include <CryAISystem/MovementStyle.h>
#include "MovementHelpers.h"

namespace Movement
{
namespace MovementBlocks
{
class FollowPath : public Movement::Block
{
public:
	FollowPath(const CNavPath& path, const float endDistance, const MovementStyle& style, const bool endsInCover);
	virtual bool                    InterruptibleNow() const { return true; }
	virtual void                    Begin(IMovementActor& actor);
	virtual void                    End(IMovementActor& actor);
	virtual Movement::Block::Status Update(const MovementUpdateContext& context);

	void                            UpdateLooking(const MovementUpdateContext& context, const bool targetReachable, const float pathDistanceToEnd, const Vec3& followTargetPosition);
	virtual const char*             GetName() const { return "FollowPath"; }

private:
	void UpdateCoverLocations(CPipeUser& pipeUser);

private:
	CNavPath                         m_path;
	MovementStyle                    m_style;
	Movement::Helpers::StuckDetector m_stuckDetector;
	std::shared_ptr<Vec3>            m_lookTarget;
	float                            m_finishBlockEndDistance;
	float                            m_accumulatedPathFollowerFailureTime;
	bool                             m_endsInCover;
};
}
}

#endif // MovementBlock_FollowPath_h
