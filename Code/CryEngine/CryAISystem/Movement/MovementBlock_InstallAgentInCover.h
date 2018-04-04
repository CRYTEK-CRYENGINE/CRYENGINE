// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#pragma once

#ifndef MovementBlock_InstallAgentInCover_h
	#define MovementBlock_InstallAgentInCover_h

	#include "MovementPlan.h"

namespace Movement
{
namespace MovementBlocks
{
class InstallAgentInCover : public Movement::Block
{
public:
	virtual void Begin(IMovementActor& actor)
	{
		actor.GetAdapter().InstallInLowCover(true);
	}

	virtual const char* GetName() const { return "InstallInCover"; }
};
}
}

#endif // MovementBlock_InstallAgentInCover_h
