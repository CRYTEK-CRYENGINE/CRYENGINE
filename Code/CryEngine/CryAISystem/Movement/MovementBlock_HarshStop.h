// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#ifndef MovementBlock_HarshStop_h
	#define MovementBlock_HarshStop_h

	#include "MovementPlan.h"

namespace Movement
{
namespace MovementBlocks
{
// Clears the movement variables in the pipe user state and waits
// until the ai proxy says we are no longer moving.
class HarshStop : public Movement::Block
{
public:
	virtual void        Begin(IMovementActor& actor);
	virtual Status      Update(const MovementUpdateContext& context);
	virtual const char* GetName() const { return "HarshStop"; }
};
}
}

#endif // MovementBlock_HarshStop_h
