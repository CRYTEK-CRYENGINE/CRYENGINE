// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryAISystem/MovementBlock.h>
#include <CryAISystem/ICoverSystem.h>

class CEntityAICoverUserComponent;

class LeaveCoverMovementBlock : public Movement::Block
{
public:
	LeaveCoverMovementBlock(CEntityAICoverUserComponent* pComponentOwner)
		: m_pComponentOwner(pComponentOwner)
	{}

	virtual const char* GetName() const override { return "LeaveCover"; }
	virtual void        Begin(IMovementActor& actor) override;

private:
	CEntityAICoverUserComponent* m_pComponentOwner;
};

class SetupCoverMovementBlock : public Movement::Block
{
public:
	SetupCoverMovementBlock(CEntityAICoverUserComponent* pComponentOwner, CoverID coverId)
		: m_pComponentOwner(pComponentOwner)
		, m_coverId(coverId)
	{}

	virtual const char* GetName() const override { return "SetCoverInfo"; }
	virtual void        Begin(IMovementActor& actor) override;

private:
	CEntityAICoverUserComponent* m_pComponentOwner;
	CoverID                      m_coverId;
};

class EnterCoverMovementBlock : public Movement::Block
{
public:
	EnterCoverMovementBlock(CEntityAICoverUserComponent* pComponentOwner)
		: m_pComponentOwner(pComponentOwner)
	{}

	virtual const char* GetName() const override { return "EnterCover"; }
	virtual void        Begin(IMovementActor& actor) override;
private:
	CEntityAICoverUserComponent* m_pComponentOwner;
};
