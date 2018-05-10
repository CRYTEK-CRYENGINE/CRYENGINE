// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "MovementSystemCreator.h"
#include "MovementSystem.h"

IMovementSystem* MovementSystemCreator::CreateMovementSystem() const
{
	return new MovementSystem();
}
