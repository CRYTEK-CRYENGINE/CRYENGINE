// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/**********************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: The player on ground state.

-------------------------------------------------------------------------
History:
- 6.10.10: Created by Stephen M. North

*************************************************************************/
#ifndef __PlayerStateGround_h__
#define __PlayerStateGround_h__

class CPlayer;
class CPlayerMovementAction;
struct SActorFrameMovementParams;
class CPlayerStateGround
{
public:
	CPlayerStateGround();

	void OnEnter( CPlayer& player );
	void OnPrePhysicsUpdate( CPlayer& player, const SActorFrameMovementParams &movement, float frameTime, const bool isHeavyWeapon, const bool isPlayer );
	void OnUpdate( CPlayer& player, float frameTime );
	void OnExit( CPlayer& player );

private:

	bool m_inertiaIsZero;

	void ProcessAlignToTarget(const CAutoAimManager& autoAimManager, CPlayer& player, const IActor* pTarget);
	bool CheckForVaultTrigger(CPlayer & player, float frameTime);
};

#endif // __PlayerStateGround_h__
