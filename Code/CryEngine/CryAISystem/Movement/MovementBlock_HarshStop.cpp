// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "MovementBlock_HarshStop.h"
#include <CryAISystem/MovementUpdateContext.h>
#include "MovementActor.h"
#include "PipeUser.h"

namespace Movement
{
namespace MovementBlocks
{
void HarshStop::Begin(IMovementActor& actor)
{
	actor.GetAdapter().ClearMovementState();
}

Movement::Block::Status HarshStop::Update(const MovementUpdateContext& context)
{
	const bool stopped = !context.actor.GetAdapter().IsMoving();

	return stopped ? Finished : Running;
}
}
}
