// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

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

	virtual const char*             GetName() const { return "FollowPath"; }

private:
	CNavPath                         m_path;
	MovementStyle                    m_style;
	Movement::Helpers::StuckDetector m_stuckDetector;
	std::shared_ptr<Vec3>            m_lookTarget;
	float                            m_finishBlockEndDistance;
	float                            m_accumulatedPathFollowerFailureTime;
	bool                             m_bLastFollowBlock;
};
}
}
