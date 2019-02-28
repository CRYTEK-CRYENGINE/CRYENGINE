// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryAISystem/MovementBlock.h>

namespace Movement
{
namespace MovementBlocks
{
class SetupPipeUserCoverInformation : public Movement::Block
{
public:
	SetupPipeUserCoverInformation(CPipeUser& pipeUser)
		: m_pipeUser(pipeUser)
	{}
	virtual void        Begin(IMovementActor& actor);
	virtual const char* GetName() const { return "SetCoverInfo"; }
private:
	CPipeUser& m_pipeUser;
};
}
}
