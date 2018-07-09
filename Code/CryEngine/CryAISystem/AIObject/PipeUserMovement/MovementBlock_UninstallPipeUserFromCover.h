// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryAISystem/MovementBlock.h>
#include <CryAISystem/IMovementActor.h>

namespace Movement
{
namespace MovementBlocks
{
class UninstallPipeUserFromCover : public Movement::Block
{
public:
	UninstallPipeUserFromCover(CPipeUser& pipeUser, const MovementStyle::Stance stance)
		: m_pipeUser(pipeUser)
		, m_stance(stance)
	{
	}

	virtual void Begin(IMovementActor& actor)
	{
		m_pipeUser.SetCoverState(ICoverUser::EStateFlags::None);
		actor.GetAdapter().SetStance(m_stance);
	}

	virtual const char* GetName() const { return "UninstallFromCover"; }
private:
	CPipeUser& m_pipeUser;
	MovementStyle::Stance m_stance;
};
}
}
