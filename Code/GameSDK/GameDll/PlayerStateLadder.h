// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __PLAYERSTATE_LADDER_H__
#define __PLAYERSTATE_LADDER_H__

#include "AutoEnum.h"
#include "Player.h"
#include "IPlayerInput.h"

#define ladderAnimTypeList(func)             \
	func(kLadderAnimType_none)                 \
	func(kLadderAnimType_atTopLeftFoot)        \
	func(kLadderAnimType_atTopRightFoot)       \
	func(kLadderAnimType_atBottom)             \
	func(kLadderAnimType_upLeftFoot)           \
	func(kLadderAnimType_upLoop)               \
	func(kLadderAnimType_upRightFoot)          \
	func(kLadderAnimType_downLeftFoot)         \
	func(kLadderAnimType_downRightFoot)        \
	func(kLadderAnimType_midAirGrab)           \

class CPlayer;
struct SActorFrameMovementParams;
class SmartScriptTable;
class CLadderAction;

class CPlayerStateLadder : IPlayerInputCancelHandler
{
public:
	CPlayerStateLadder();

	bool OnPrePhysicsUpdate( CPlayer& player, const SActorFrameMovementParams &movement, float frameTime );
	void OnExit( CPlayer& player );

	void OnUseLadder(CPlayer& player, IEntity* pLadder);
	void LeaveLadder(CPlayer& player, ELadderLeaveLocation leaveLocation);
	void SetHeightFrac(CPlayer& player, float heightFrac);
	void InformLadderAnimEnter(CPlayer & player, CLadderAction * thisAction);
	void InformLadderAnimIsDone(CPlayer & player, CLadderAction * thisAction);
	EntityId GetLadderId(){ return m_ladderEntityId; };

	static bool IsUsableLadder(CPlayer& player, IEntity* pLadder, const SmartScriptTable& ladderProperties);

	AUTOENUM_BUILDENUMWITHTYPE_WITHNUM(ELadderAnimType, ladderAnimTypeList, kLadderAnimType_num);

#ifndef _RELEASE
	void UpdateNumActions(int change);
#endif

private:
	bool m_playerIsThirdPerson;
	EntityId m_ladderEntityId;
	Vec3 m_ladderBottom;
	float m_offsetFromAnimToRung;
	float m_climbInertia;
	float m_scaleSettle;
	uint32 m_numRungsFromBottomPosition;
	uint32 m_topRungNumber;
	float m_fractionBetweenRungs;
	ELadderAnimType m_playGetOffAnim;
	ELadderAnimType m_playGetOnAnim;
	CLadderAction * m_mostRecentlyEnteredAction;

	void SetClientPlayerOnLadder(IEntity * pLadder, bool onOff);
	void QueueLadderAction(CPlayer& player, CLadderAction * action);
	void SetMostRecentlyEnteredAction(CLadderAction * thisAction);
	void InterruptCurrentAnimation();

	static void SendLadderFlowgraphEvent(CPlayer & player, IEntity * pLadderEntity, const char * eventName);

	// IPlayerInputCancelHandler
	DEFINE_CANCEL_HANDLER_NAME("CPlayerStateLadder")
		virtual bool HandleCancelInput(CPlayer & player, TCancelButtonBitfield cancelButtonPressed);
	// ~IPlayerInputCancelHandler

#ifndef _RELEASE
	int m_dbgNumActions;
#endif
};

#endif // __PLAYERSTATE_LADDER_H__
