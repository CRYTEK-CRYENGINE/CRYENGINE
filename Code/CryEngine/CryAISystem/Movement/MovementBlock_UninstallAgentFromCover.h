// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved. 

#pragma once

#ifndef MovementBlock_UninstallAgentFromCover_h
	#define MovementBlock_UninstallAgentFromCover_h

	#include "MovementPlan.h"

namespace Movement
{
namespace MovementBlocks
{
class UninstallAgentFromCover : public Movement::Block
{
public:
	UninstallAgentFromCover(const MovementStyle::Stance stance)
		: m_stance(stance)
	{
	}

	virtual void Begin(IMovementActor& actor)
	{
		actor.GetAdapter().SetInCover(false);
		actor.GetAdapter().SetStance(m_stance);
	}

	virtual const char* GetName() const { return "UninstallFromCover"; }
private:
	MovementStyle::Stance m_stance;
};
}
}

#endif // MovementBlock_UninstallAgentFromCover_h
