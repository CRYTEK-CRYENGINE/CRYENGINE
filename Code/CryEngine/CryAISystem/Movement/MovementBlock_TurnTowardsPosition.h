// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#ifndef MovementBlock_TurnTowardsPosition_h
	#define MovementBlock_TurnTowardsPosition_h

	#include "MovementPlan.h"
	#include <CryAISystem/MovementStyle.h>

namespace Movement
{
namespace MovementBlocks
{
class TurnTowardsPosition : public Movement::Block
{
public:
	TurnTowardsPosition(const Vec3& position);
	virtual void                    End(IMovementActor& actor);
	virtual Movement::Block::Status Update(const MovementUpdateContext& context);
	virtual bool                    InterruptibleNow() const { return true; }
	virtual const char*             GetName() const          { return "TurnTowardsPosition"; }

private:
	Vec3  m_positionToTurnTowards;
	float m_timeSpentAligning;
	float m_correctBodyDirTime;
};
}
}

#endif // MovementBlock_TurnTowardsPosition_h
