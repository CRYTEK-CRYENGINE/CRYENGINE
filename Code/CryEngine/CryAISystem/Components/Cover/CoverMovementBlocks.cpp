// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "CoverMovementBlocks.h"

#include "CoverUserComponent.h"

void LeaveCoverMovementBlock::Begin(IMovementActor& actor)
{
	m_pComponentOwner->SetState(ICoverUser::EStateFlags::None);
	m_pComponentOwner->SetCurrentCover(CoverID());
}

void SetupCoverMovementBlock::Begin(IMovementActor& actor)
{
	m_pComponentOwner->ReserveNextCover(CoverID());
	m_pComponentOwner->SetCurrentCover(m_coverId);
	m_pComponentOwner->SetState(ICoverUser::EStateFlags::MovingToCover);
}

void EnterCoverMovementBlock::Begin(IMovementActor& actor)
{
	m_pComponentOwner->SetState(ICoverUser::EStateFlags::InCover);
}