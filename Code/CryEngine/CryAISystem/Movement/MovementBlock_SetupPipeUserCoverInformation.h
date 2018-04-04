// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#pragma once

#ifndef MovementBlock_SetupActorCoverInformation_h
	#define MovementBlock_SetupActorCoverInformation_h

	#include "MovementPlan.h"

namespace Movement
{
namespace MovementBlocks
{
class SetupActorCoverInformation : public Movement::Block
{
public:
	virtual void        Begin(IMovementActor& actor);
	virtual const char* GetName() const { return "SetCoverInfo"; }
};
}
}

#endif // MovementBlock_SetupActorCoverInformation_h
