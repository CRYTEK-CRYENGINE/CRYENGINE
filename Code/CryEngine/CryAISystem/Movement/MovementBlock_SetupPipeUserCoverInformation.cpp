// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#include "StdAfx.h"
#include "MovementBlock_SetupPipeUserCoverInformation.h"
#include "MovementActor.h"
#include <CryAISystem/ICoverSystem.h>
#include "Cover/CoverSystem.h"
#include "PipeUser.h"

namespace Movement
{
namespace MovementBlocks
{
void SetupActorCoverInformation::Begin(IMovementActor& actor)
{
	actor.GetAdapter().SetupCoverInformation();
}
}
}
