// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Implements the Player ledge state

-------------------------------------------------------------------------
History:
- 14.9.10: Created by Stephen M. North

*************************************************************************/
#ifndef __PlayerStateDead_h__
#define __PlayerStateDead_h__

#pragma once

class CPlayer;
class CPlayerStateMachine;
struct SActorFrameMovementParams;
class CPlayerStateDead
{
	enum EAICorpseUpdateStatus
	{
		eCorpseStatus_WaitingForSwap = 0,
		eCorpseStatus_SwapDone
	};

public:

	struct UpdateCtx
	{
		float frameTime;
	};

	 CPlayerStateDead();
	~CPlayerStateDead();

	void OnEnter( CPlayer& player);
	void OnLeave( CPlayer& player );
	void OnPrePhysicsUpdate( CPlayer& player, const SActorFrameMovementParams& movement, float frameTime );
	void OnUpdate( CPlayer& player, const CPlayerStateDead::UpdateCtx& updateCtx  );
	void Serialize( TSerialize& serializer );

protected:
	void UpdateAICorpseStatus( CPlayer& player, const CPlayerStateDead::UpdateCtx& updateCtx );

private:
	float m_swapToCorpseTimeout;
	EAICorpseUpdateStatus m_corpseUpdateStatus;
};

#endif // __PlayerStateDead_h__
