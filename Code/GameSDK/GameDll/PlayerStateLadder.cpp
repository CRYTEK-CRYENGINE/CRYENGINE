// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "PlayerStateLadder.h"
#include "Player.h"
#include "PlayerAnimation.h"
#include "PlayerStateUtil.h"
#include "EntityUtility/EntityScriptCalls.h"
#include "Utility/CryWatch.h"
#include "PlayerStateEvents.h"
#include "Weapon.h"
#include "GameCVars.h"

#ifndef _RELEASE
#define LadderLog(...) do { if (g_pGameCVars->pl_ladderControl.ladder_logVerbosity > 0) { CryLog ("[LADDER] " __VA_ARGS__); } } while(0)
#define LadderLogIndent() INDENT_LOG_DURING_SCOPE(g_pGameCVars->pl_ladderControl.ladder_logVerbosity > 0)
static AUTOENUM_BUILDNAMEARRAY(s_onOffAnimTypeNames, ladderAnimTypeList);
#else
#define LadderLog(...) (void)(0)
#define LadderLogIndent(...) (void)(0)
#endif

#define USE_CLIMB_ANIMATIONS 1

static uint32 s_ladderFractionCRC = 0;

class CLadderAction : public TPlayerAction
{
public:
	CLadderAction(CPlayerStateLadder * ladderState, CPlayer & player, FragmentID fragmentID, CPlayerStateLadder::ELadderAnimType animType, const char* cameraAnimFactorAtStart, const char* cameraAnimFactorAtEnd /*float cameraAnimFactorAtStart, float cameraAnimFactorAtEnd*/) :
		TPlayerAction(PP_PlayerActionUrgent, fragmentID),
		m_ladderState(ladderState),
		m_player(player),
		m_animType(animType),
		m_cameraAnimFactorAtStart(0.f),
		m_cameraAnimFactorAtEnd(0.f),
		m_duration(0.f),
		m_interruptable(false)
	{
 		IEntity* pLadder = gEnv->pEntitySystem->GetEntity(ladderState->GetLadderId());

		if ( pLadder )
		{
 			EntityScripts::GetEntityProperty(pLadder, "Camera", cameraAnimFactorAtStart, m_cameraAnimFactorAtStart);
			EntityScripts::GetEntityProperty(pLadder, "Camera", cameraAnimFactorAtEnd, m_cameraAnimFactorAtEnd);
		}
		LadderLog ("Constructing %s instance for %s who's %s a ladder", GetName(), player.GetEntity()->GetEntityTextDescription().c_str(), player.IsOnLadder() ? "on" : "not on");

#ifndef _RELEASE
		ladderState->UpdateNumActions(1);
#endif
	}

#ifndef _RELEASE
	~CLadderAction()
	{
		assert (m_ladderState);

		m_ladderState->UpdateNumActions(-1);
	}
#endif

	void InitialiseWithParams(const char * directionText, const char * footText)
	{
		const CTagDefinition* pFragTagDef = m_context->controllerDef.GetFragmentTagDef(m_fragmentID);

		LadderLog ("Initializing %s instance for %s performing action defined in '%s' direction=%s, foot=%s", GetName(), m_player.GetEntity()->GetEntityTextDescription().c_str(), pFragTagDef->GetFilename(), directionText, footText);
		LadderLogIndent();

		if (directionText)
		{
			pFragTagDef->Set(m_fragTags, pFragTagDef->Find(directionText), true);
		}

		if (footText)
		{
			pFragTagDef->Set(m_fragTags, pFragTagDef->Find(footText), true);
		}
	}

	void OnInitialise()
	{
#ifndef _RELEASE
		CRY_ASSERT_TRACE (0, ("Unexpected anim type %d (%s) for %s action", m_animType, s_onOffAnimTypeNames[m_animType], GetName()));
#endif
	}

	virtual void Enter()
	{
		LadderLog ("Entering %s instance for %s", GetName(), m_player.GetEntity()->GetEntityTextDescription().c_str());
		LadderLogIndent();

		TPlayerAction::Enter();

		IAnimatedCharacter* pAnimChar = m_player.GetAnimatedCharacter();

		if (pAnimChar)
		{
			EMovementControlMethod method = m_interruptable ? eMCM_Entity : eMCM_Animation;
			pAnimChar->SetMovementControlMethods(method, method);
		}

		m_player.BlendPartialCameraAnim(m_cameraAnimFactorAtStart, 0.1f);
		m_player.SetCanTurnBody(false);

		m_ladderState->InformLadderAnimEnter(m_player, this);

		PlayerCameraAnimationSettings cameraAnimationSettings;
		cameraAnimationSettings.positionFactor = 1.0f;
		cameraAnimationSettings.rotationFactor = 0.0f;
		m_player.PartialAnimationControlled(true, cameraAnimationSettings);
	}

	virtual void Exit()
	{
		LadderLog ("Exiting %s instance for %s", GetName(), m_player.GetEntity()->GetEntityTextDescription().c_str());
		LadderLogIndent();

		TPlayerAction::Exit();

		m_ladderState->InformLadderAnimIsDone(m_player, this);

		m_player.PartialAnimationControlled(false, PlayerCameraAnimationSettings());
	}

	bool GetIsInterruptable()
	{
		return m_interruptable;
	}

	void UpdateCameraAnimFactor()
	{
		if (GetStatus() == IAction::Installed)
		{
			const IScope & rootScope = GetRootScope();
			const float timeLeft = rootScope.CalculateFragmentTimeRemaining();
			const float duration = max(m_duration, timeLeft);
			float fractionComplete = 1.f;
			if (duration > 0.0f)
			{
				fractionComplete = 1.f - (timeLeft / duration);
			}

			m_duration = duration;
			m_player.BlendPartialCameraAnim(LERP (m_cameraAnimFactorAtStart, m_cameraAnimFactorAtEnd, fractionComplete), 0.1f);
		}
	}

protected:
	CPlayer & m_player;
	CPlayerStateLadder * m_ladderState;
	CPlayerStateLadder::ELadderAnimType m_animType;
	float m_cameraAnimFactorAtStart;
	float m_cameraAnimFactorAtEnd;
	float m_duration;
	bool m_interruptable;
};

class CActionLadderGetOn : public CLadderAction
{
public:
	DEFINE_ACTION("LadderGetOn");

	CActionLadderGetOn(CPlayerStateLadder * ladderState, CPlayer &player, CPlayerStateLadder::ELadderAnimType animType) :
		CLadderAction(ladderState, player, PlayerMannequin.fragmentIDs.LadderGetOn, animType, "cameraAnimFraction_getOn", "cameraAnimFraction_onLadder")
	{
	}

	virtual void OnInitialise() override
	{
		switch (m_animType)
		{
		case CPlayerStateLadder::kLadderAnimType_atBottom:           { InitialiseWithParams( "up", NULL);				return; }
		case CPlayerStateLadder::kLadderAnimType_atTopLeftFoot:      { InitialiseWithParams( "down", "left");		return; }
		case CPlayerStateLadder::kLadderAnimType_atTopRightFoot:     { InitialiseWithParams( "down", "right");	return; }
		}
		
		CLadderAction::OnInitialise();
	}

	virtual void Exit() override
	{
		if (IAnimatedCharacter* pAnimChar = m_player.GetAnimatedCharacter())
		{
			Quat animQuat = pAnimChar->GetAnimLocation().q;

			m_player.SetViewRotation(animQuat);
		}

		CLadderAction::Exit();
	}
};

class CActionLadderGetOff : public CLadderAction
{
public:
	DEFINE_ACTION("LadderGetOff");

	CActionLadderGetOff(CPlayerStateLadder * ladderState, CPlayer &player, CPlayerStateLadder::ELadderAnimType animType) :
		CLadderAction(ladderState, player, PlayerMannequin.fragmentIDs.LadderGetOff, animType, "cameraAnimFraction_onLadder", "cameraAnimFraction_getOff")
	{
	}

	virtual void OnInitialise() override
	{
		switch (m_animType)
		{
		case CPlayerStateLadder::kLadderAnimType_atBottom:           { InitialiseWithParams( "down", NULL);		return; }
		case CPlayerStateLadder::kLadderAnimType_atTopLeftFoot:      { InitialiseWithParams( "up", "left");		return; }
		case CPlayerStateLadder::kLadderAnimType_atTopRightFoot:     { InitialiseWithParams( "up", "right");	return; }
		}
		
		CLadderAction::OnInitialise();
	}

	virtual void Exit() override
	{
		if (IAnimatedCharacter* pAnimChar = m_player.GetAnimatedCharacter())
		{
			Quat animQuat = pAnimChar->GetAnimLocation().q;

			m_player.SetViewRotation(animQuat);
		}

		CLadderAction::Exit();
	}
};

class CActionLadderClimbUpDown : public CLadderAction
{
public:
	DEFINE_ACTION("LadderClimbUpDown");

	CActionLadderClimbUpDown(CPlayerStateLadder * ladderState, CPlayer &player) :
		CLadderAction(ladderState, player, PlayerMannequin.fragmentIDs.LadderClimb, CPlayerStateLadder::kLadderAnimType_upLoop, "cameraAnimFraction_onLadder", "cameraAnimFraction_onLadder")
	{
		m_interruptable = true;
	}

	virtual void OnInitialise() override
	{
		InitialiseWithParams("up", "loop");
	}
};

CPlayerStateLadder::CPlayerStateLadder()
	: m_playerIsThirdPerson(false)
	, m_ladderEntityId(0)
	, m_ladderBottom(ZERO)
	, m_offsetFromAnimToRung(0.0f)
	, m_climbInertia(0.0f)
	, m_scaleSettle(0.0f)
	, m_numRungsFromBottomPosition(0)
	, m_topRungNumber(0)
	, m_fractionBetweenRungs(0.0f)
	, m_playGetOffAnim(kLadderAnimType_none)
	, m_playGetOnAnim(kLadderAnimType_none)
	, m_mostRecentlyEnteredAction(nullptr)
#ifndef _RELEASE
	, m_dbgNumActions(0)
#endif
{
	if (s_ladderFractionCRC == 0)
	{
		s_ladderFractionCRC = CCrc32::ComputeLowercase("LadderFraction");
	}
}

void CPlayerStateLadder::SetClientPlayerOnLadder(IEntity * pLadder, bool onOff)
{
	bool renderLadderLast = false;
	EntityScripts::GetEntityProperty(pLadder, "Camera", "bRenderLadderLast", renderLadderLast);

	const uint32 applyRenderFlags[2] = {0, ENTITY_SLOT_RENDER_NEAREST};
	const uint32 oldFlags = pLadder->GetSlotFlags(0);
	const uint32 newFlags = (oldFlags & ~ENTITY_SLOT_RENDER_NEAREST) | applyRenderFlags[onOff && renderLadderLast];

	pLadder->SetSlotFlags(0, newFlags);
}

void CPlayerStateLadder::OnUseLadder(CPlayer& player, IEntity* pLadder) 
{
	assert (pLadder);

	LadderLog ("%s has started using ladder %s", player.GetEntity()->GetEntityTextDescription().c_str(), pLadder->GetEntityTextDescription().c_str());
	LadderLogIndent();

#ifndef _RELEASE
	CRY_ASSERT_TRACE(m_dbgNumActions == 0, ("%s shouldn't have any leftover ladder actions but has %d", player.GetEntity()->GetEntityTextDescription().c_str(), m_dbgNumActions));
#endif

	assert (player.IsOnLadder());
	player.UpdateVisibility();

	m_playerIsThirdPerson = player.IsThirdPerson();

	if (player.IsClient())
	{
		player.GetPlayerInput()->AddInputCancelHandler(this);
		SetClientPlayerOnLadder(pLadder, true);
	}

	const Vec3 worldPos(pLadder->GetWorldPos());
	const Vec3 direction(pLadder->GetWorldTM().GetColumn1());
	const Vec3 playerEntityPos(player.GetEntity()->GetWorldPos());

	float height = 0.f;
	float horizontalViewLimit = 0.f;
	float stopClimbingDistanceFromBottom = 0.f;
	float stopClimbingDistanceFromTop = 0.f;
	float playerHorizontalOffset = 0.f;
	float distanceBetweenRungs = 0.f;

	float verticalUpViewLimit = 0.f;
	float verticalDownViewLimit = 0.f;
	float getOnDistanceAwayTop = 0.f;
	float getOnDistanceAwayBottom = 0.f;
	bool ladderUseThirdPerson = false;

	EntityScripts::GetEntityProperty(pLadder, "height", height);
	EntityScripts::GetEntityProperty(pLadder, "ViewLimits", "horizontalViewLimit", horizontalViewLimit);
	EntityScripts::GetEntityProperty(pLadder, "Offsets", "stopClimbDistanceFromBottom", stopClimbingDistanceFromBottom);
	EntityScripts::GetEntityProperty(pLadder, "Offsets", "stopClimbDistanceFromTop", stopClimbingDistanceFromTop);
	EntityScripts::GetEntityProperty(pLadder, "Offsets", "playerHorizontalOffset", playerHorizontalOffset);
	EntityScripts::GetEntityProperty(pLadder, "Camera", "distanceBetweenRungs", distanceBetweenRungs);
	EntityScripts::GetEntityProperty(pLadder, "ViewLimits", "verticalUpViewLimit", verticalUpViewLimit);
	EntityScripts::GetEntityProperty(pLadder, "ViewLimits", "verticalDownViewLimit", verticalDownViewLimit);
	EntityScripts::GetEntityProperty(pLadder, "Offsets", "getOnDistanceAwayTop", getOnDistanceAwayTop);
	EntityScripts::GetEntityProperty(pLadder, "Offsets", "getOnDistanceAwayBottom", getOnDistanceAwayBottom);
	EntityScripts::GetEntityProperty(pLadder, "Camera", "bUseThirdPersonCamera", ladderUseThirdPerson);

	const float heightOffsetBottom = stopClimbingDistanceFromBottom;
	const float heightOffsetTop    = stopClimbingDistanceFromTop;

	m_ladderBottom = worldPos + (direction * playerHorizontalOffset);
	m_ladderBottom.z += heightOffsetBottom;
	float ladderClimbableHeight = height - heightOffsetTop - heightOffsetBottom;
	m_playGetOffAnim = kLadderAnimType_none;
	m_playGetOnAnim = kLadderAnimType_none;
	SetMostRecentlyEnteredAction(NULL);
	m_ladderEntityId = pLadder->GetId();
	m_numRungsFromBottomPosition = 0;
	m_fractionBetweenRungs = 0.f;
	m_climbInertia = 0.f;
	m_scaleSettle = 0.f;

	SendLadderFlowgraphEvent(player, pLadder, "PlayerOn");

	const float numberOfRungsFloat = ladderClimbableHeight / distanceBetweenRungs;
	m_topRungNumber = (uint32) max(0.f, numberOfRungsFloat + 0.5f);

	player.SetCanTurnBody(false);
	player.GetActorParams().viewLimits.SetViewLimit(-direction, DEG2RAD(horizontalViewLimit),
		0.f, DEG2RAD(verticalUpViewLimit), 
		DEG2RAD(verticalDownViewLimit), SViewLimitParams::eVLS_Ladder);

	CPlayerStateUtil::PhySetFly( player );
	CPlayerStateUtil::CancelCrouchAndProneInputs( player );

	IAnimatedCharacter* pAnimChar = player.GetAnimatedCharacter();

	if (pAnimChar)
	{
		pAnimChar->RequestPhysicalColliderMode(eColliderMode_Disabled, eColliderModeLayer_Game, "CPlayerStateLadder::OnUseLadder");
		pAnimChar->EnableRigidCollider(0.45f);
	}

	Vec3 snapPlayerToPosition = m_ladderBottom;
	Quat snapPlayerToRotation = pLadder->GetWorldRotation();

	bool playLowerAnimation = true;

	if (playerEntityPos.z > m_ladderBottom.z + ladderClimbableHeight - 0.1f)
	{
		snapPlayerToPosition.z += (height - heightOffsetBottom);
		snapPlayerToPosition -= direction * getOnDistanceAwayTop;
		snapPlayerToRotation *= Quat::CreateRotationZ(gf_PI);
		m_playGetOnAnim = (m_topRungNumber & 1) ? kLadderAnimType_atTopRightFoot : kLadderAnimType_atTopLeftFoot;
		m_numRungsFromBottomPosition = m_topRungNumber;
	}
	else if (playerEntityPos.z < m_ladderBottom.z + 0.1f)
	{
		snapPlayerToPosition.z -= heightOffsetBottom;
		snapPlayerToPosition += direction * getOnDistanceAwayBottom;
		snapPlayerToRotation *= Quat::CreateRotationZ(gf_PI);
		m_playGetOnAnim = kLadderAnimType_atBottom;
	}
	else
	{
		snapPlayerToPosition.z = clamp_tpl(playerEntityPos.z, m_ladderBottom.z, m_ladderBottom.z + ladderClimbableHeight);
		snapPlayerToRotation *= Quat::CreateRotationZ(gf_PI);
		playLowerAnimation = false;
		m_playGetOnAnim = kLadderAnimType_midAirGrab;
	}

	if(player.IsInPickAndThrowMode())
	{
		player.HolsterItem(true);
	}
	else
	{
		CWeapon * pCurrentWeapon = player.GetWeapon(player.GetCurrentItemId());
		if (pCurrentWeapon)
		{
			pCurrentWeapon->SetPlaySelectAction(playLowerAnimation);
		}
	}
	//player.ScheduleItemSwitch(player.FindOrGiveItem(CEntityClassPtr::NoWeapon));

	player.GetEntity()->SetPosRotScale(snapPlayerToPosition, snapPlayerToRotation, Vec3(1.f, 1.f, 1.f));

	if (ladderUseThirdPerson)
	{
		player.SetThirdPerson(true, true);
	}

	float heightFrac = (m_numRungsFromBottomPosition + m_fractionBetweenRungs) / m_topRungNumber;
	player.OnUseLadder(m_ladderEntityId, heightFrac);
}

// Called when the player finishes his exit animation (or from OnExit if it triggers no exit animation)
static void LadderExitIsComplete(CPlayer & player)
{
	LadderLog ("Ladder exit is complete");
	LadderLogIndent();

	assert (! player.IsOnLadder());
	player.BlendPartialCameraAnim(0.0f, 0.1f);
	player.SetCanTurnBody(true);
	//player.SelectLastValidItem();

	if (!player.IsCinematicFlagActive(SPlayerStats::eCinematicFlag_HolsterWeapon))
	{
		player.HolsterItem(false);
	}

	IAnimatedCharacter* pAnimChar = player.GetAnimatedCharacter();

	if( pAnimChar )
	{
		pAnimChar->SetMovementControlMethods(eMCM_Entity, eMCM_Entity);
		pAnimChar->ForceRefreshPhysicalColliderMode();
		pAnimChar->RequestPhysicalColliderMode(eColliderMode_Undefined, eColliderModeLayer_Game, "CPlayerStateLadder::OnExit()");
		pAnimChar->DisableRigidCollider();
	}
}

void CPlayerStateLadder::OnExit( CPlayer& player )
{
	LadderLog ("%s has stopped using ladder (%s)", player.GetEntity()->GetEntityTextDescription().c_str(), s_onOffAnimTypeNames[m_playGetOffAnim]);
	LadderLogIndent();

	assert (! player.IsOnLadder());
	player.UpdateVisibility();

	IPlayerInput * pPlayerInput = player.GetPlayerInput();

	if (pPlayerInput)
	{
		pPlayerInput->RemoveInputCancelHandler(this);
	}

	IEntity * pLadder = gEnv->pEntitySystem->GetEntity(m_ladderEntityId);

	if (pLadder)
	{
		if (player.IsClient())
		{
			SetClientPlayerOnLadder(pLadder, false);
		}
		SendLadderFlowgraphEvent(player, pLadder, "PlayerOff");
	}

	m_ladderBottom.zero();
	m_ladderEntityId = 0;

	player.GetActorParams().viewLimits.ClearViewLimit(SViewLimitParams::eVLS_Ladder);

	pe_player_dynamics simPar;

	IPhysicalEntity* piPhysics = player.GetEntity()->GetPhysics();
	if (!piPhysics || piPhysics->GetParams(&simPar) == 0)
	{
		return;
	}

	IAnimatedCharacter* pAnimChar = player.GetAnimatedCharacter();

	//player.GetActorStats()->gravity = simPar.gravity;

	CPlayerStateUtil::PhySetNoFly( player, simPar.gravity );
	CPlayerStateUtil::CancelCrouchAndProneInputs( player );

	InterruptCurrentAnimation();
	SetMostRecentlyEnteredAction(NULL);

	if (m_playGetOffAnim)
	{
		QueueLadderAction (player, new CActionLadderGetOff(this, player, m_playGetOffAnim));
	}
	else
	{
		// Finishing the above 'get off' animation will retrieve the player's weapon... if we're not playing one then we unholster it now
		QueueLadderAction (player, NULL);
		LadderExitIsComplete(player);
	}

	bool ladderUseThirdPerson = false;
	EntityScripts::GetEntityProperty(pLadder, "Camera", "bUseThirdPersonCamera", ladderUseThirdPerson);

	if (ladderUseThirdPerson && !m_playerIsThirdPerson)
	{
		player.SetThirdPerson(false);
	}

	ELadderLeaveLocation loc = eLLL_Drop;
	if (m_playGetOffAnim == kLadderAnimType_atBottom)
	{
		loc = eLLL_Bottom;
	}
	else if (m_playGetOffAnim == kLadderAnimType_atTopLeftFoot || m_playGetOffAnim == kLadderAnimType_atTopRightFoot)
	{
		loc = eLLL_Top;
	}

	player.OnLeaveLadder(loc);
}

void CPlayerStateLadder::InterruptCurrentAnimation()
{
	if (m_mostRecentlyEnteredAction)
	{
		m_mostRecentlyEnteredAction->ForceFinish();
	}
}

void CPlayerStateLadder::QueueLadderAction(CPlayer& player, CLadderAction * action)
{
	LadderLog ("Queuing %s ladder anim '%s'", player.GetEntity()->GetEntityTextDescription().c_str(), action ? action->GetName() : "NULL");
	LadderLogIndent();

	if (action)
	{
		player.GetAnimatedCharacter()->GetActionController()->Queue(*action);
	}
}

void CPlayerStateLadder::SendLadderFlowgraphEvent(CPlayer & player, IEntity * pLadderEntity, const char * eventName)
{
	SEntityEvent event( ENTITY_EVENT_SCRIPT_EVENT );
	event.nParam[0] = (INT_PTR)eventName;
	event.nParam[1] = IEntityClass::EVT_ENTITY;
	EntityId entityId = player.GetEntityId();
	event.nParam[2] = (INT_PTR)&entityId;
	pLadderEntity->SendEvent(event);
}

bool CPlayerStateLadder::OnPrePhysicsUpdate( CPlayer& player, const SActorFrameMovementParams &movement, float frameTime )
{
	Vec3 requiredMovement(ZERO);

	IEntity * pLadder = gEnv->pEntitySystem->GetEntity(m_ladderEntityId);

	assert (player.IsOnLadder());

#ifndef _RELEASE
	if (g_pGameCVars->pl_ladderControl.ladder_logVerbosity)
	{
		CryWatch ("[LADDER] RUNG=$3%u/%u$o FRAC=$3%.2f$o: %s is %.2fm up a ladder, move=%.2f - %s, %s, %s, camAnim=%.2f, $7INERTIA=%.2f SETTLE=%.2f", m_numRungsFromBottomPosition, m_topRungNumber, m_fractionBetweenRungs, player.GetEntity()->GetEntityTextDescription().c_str(), player.GetEntity()->GetWorldPos().z - m_ladderBottom.z, requiredMovement.z, player.CanTurnBody() ? "$4can turn body$o" : "$3cannot turn body$o", (player.GetEntity()->GetSlotFlags(0) & ENTITY_SLOT_RENDER_NEAREST) ? "render nearest" : "render normal", player.IsOnLadder() ? "$3on a ladder$o" : "$4not on a ladder$o", player.GetActorStats()->partialCameraAnimFactor, m_climbInertia, m_scaleSettle);

		if (m_mostRecentlyEnteredAction && m_mostRecentlyEnteredAction->GetStatus() == IAction::Installed)
		{
			const IScope & animScope = m_mostRecentlyEnteredAction->GetRootScope();
			const float timeRemaining = animScope.CalculateFragmentTimeRemaining();
			CryWatch ("[LADDER] Animation: '%s' (timeActive=%.2f timeRemaining=%.2f speed=%.2f)", m_mostRecentlyEnteredAction->GetName(), m_mostRecentlyEnteredAction->GetActiveTime(), timeRemaining, m_mostRecentlyEnteredAction->GetSpeedBias());
		}
		else
		{
			CryWatch ("[LADDER] Animation: %s", m_mostRecentlyEnteredAction ? "NOT PLAYING" : "NONE");
		}
	}
#endif

	IScriptTable * pEntityScript = pLadder ? pLadder->GetScriptTable() : NULL;
	SmartScriptTable propertiesTable;

	if (pEntityScript)
	{
		pEntityScript->GetValue("Properties", propertiesTable);
	}

	int bUsable = 0;
	if (pLadder == NULL || (propertiesTable->GetValue("bUsable", bUsable) && bUsable == 0))
	{
		player.StateMachineHandleEventMovement(SStateEventLeaveLadder(eLLL_Drop));
	}
	else if (m_playGetOnAnim != kLadderAnimType_none)
	{
		if (!player.IsCinematicFlagActive(SPlayerStats::eCinematicFlag_HolsterWeapon))
		{
			player.HolsterItem(true);
		}

		IItem * pItem = player.GetCurrentItem();
		bool canPlayGetOnAnim = (pItem == NULL /*|| pItem->GetEntity()->GetClass() == CEntityClassPtr::NoWeapon*/);

		if (! canPlayGetOnAnim)
		{
			// Waiting for player to switch to 'NoWeapon' item - check this is still happening! (If not, warn and play the get on animation anyway - let's not get stuck here!)
			EntityId switchingToItemID = player.GetActorStats()->exchangeItemStats.switchingToItemID;
			IEntity * pEntity = gEnv->pEntitySystem->GetEntity(switchingToItemID);

			if (pEntity == NULL /*|| pEntity->GetClass() != CEntityClassPtr::NoWeapon*/)
			{
				GameWarning ("!%s should be switching to 'NoWeapon' but is using %s and switching to %s", player.GetEntity()->GetEntityTextDescription().c_str(), pItem->GetEntity()->GetClass()->GetName(), pEntity ? pEntity->GetClass()->GetName() : "<NULL>");
				//player.SelectItemByClass(CEntityClassPtr::NoWeapon);
				canPlayGetOnAnim = true;
			}
		}

		if (canPlayGetOnAnim)
		{
			// Currently we don't have a mid-air grab animation, so let's just go straight to the climbing anim if that's why we're here
			if (m_playGetOnAnim != kLadderAnimType_midAirGrab)
			{
				QueueLadderAction (player, new CActionLadderGetOn(this, player, m_playGetOnAnim));
			}
			QueueLadderAction (player, new CActionLadderClimbUpDown(this, player));
			m_playGetOnAnim = kLadderAnimType_none;
		}
	}
	else if (m_mostRecentlyEnteredAction && !m_mostRecentlyEnteredAction->GetIsInterruptable())
	{
		m_mostRecentlyEnteredAction->UpdateCameraAnimFactor();
	}
	else if (player.GetActorStats())
	{
			float pushUpDown = movement.desiredVelocity.y;
			const float deflection = fabsf(pushUpDown);

			float movementInertiaDecayRate = 0.f;
			float movementAcceleration = 0.f;
			float movementSettleSpeed = 0.f;
			float movementSpeedUpwards = 0.f;
			float movementSpeedDownwards = 0.f;

			EntityScripts::GetEntityProperty(pLadder, "Movement", "movementInertiaDecayRate", movementInertiaDecayRate);
			EntityScripts::GetEntityProperty(pLadder, "Movement", "movementAcceleration", movementAcceleration);
			EntityScripts::GetEntityProperty(pLadder, "Movement", "movementSettleSpeed", movementSettleSpeed);
			EntityScripts::GetEntityProperty(pLadder, "Movement", "movementSpeedUpwards", movementSpeedUpwards);
			EntityScripts::GetEntityProperty(pLadder, "Movement", "movementSpeedDownwards", movementSpeedDownwards);

			const float inertiaDecayAmount = frameTime * movementInertiaDecayRate * (1.f - deflection);

			if (m_climbInertia >= 0.f)
			{
				if (m_fractionBetweenRungs > 0.5f || m_climbInertia < 0.0001f)
				{
					m_climbInertia = max(0.f, m_climbInertia - inertiaDecayAmount);
				}
				else
				{
					m_climbInertia = min(1.f, m_climbInertia + inertiaDecayAmount * 0.1f);
				}
			}
			else
			{
				if (m_fractionBetweenRungs <= 0.5f || m_climbInertia > -0.0001f)
				{
					m_climbInertia = min(0.f, m_climbInertia + inertiaDecayAmount);
				}
				else
				{
					m_climbInertia = max(-1.f, m_climbInertia - inertiaDecayAmount * 0.1f);
				}
			}

			m_climbInertia = clamp_tpl(m_climbInertia + pushUpDown * movementAcceleration * frameTime, -1.f, 1.f);
			m_scaleSettle = min (m_scaleSettle + inertiaDecayAmount, 1.f - fabsf(m_climbInertia));

			const float maxAutoSettleMovement = frameTime * m_scaleSettle * movementSettleSpeed;
			float settleToHere = (m_fractionBetweenRungs > 0.5f) ? 1.f : 0.f;
			const float settleOffset = (settleToHere - m_fractionBetweenRungs);

			m_fractionBetweenRungs += frameTime * m_climbInertia * (float)__fsel (m_climbInertia, movementSpeedUpwards, movementSpeedDownwards) + clamp_tpl(settleOffset, -maxAutoSettleMovement, maxAutoSettleMovement);

			if (m_fractionBetweenRungs < 0.f)
			{
				if (m_numRungsFromBottomPosition > 0)
				{
					-- m_numRungsFromBottomPosition;
					m_fractionBetweenRungs += 1.f;
				}
				else
				{
					m_fractionBetweenRungs = 0.f;
					if (pushUpDown < -0.5f)
					{
						m_playGetOffAnim = kLadderAnimType_atBottom;
					}
				}
			}
			else
			{
				if (m_fractionBetweenRungs >= 1.f)
				{
					++ m_numRungsFromBottomPosition;
					m_fractionBetweenRungs -= 1.f;
				}

				if (m_topRungNumber == m_numRungsFromBottomPosition)
				{
					m_fractionBetweenRungs = 0.f;
					if (pushUpDown > 0.5f)
					{
						int topBlockedValue = 0;
						const bool bTopIsBlocked = propertiesTable->GetValue("bTopBlocked", topBlockedValue) && topBlockedValue;

						if (bTopIsBlocked == false)
						{
							m_playGetOffAnim = (m_topRungNumber & 1) ? kLadderAnimType_atTopRightFoot : kLadderAnimType_atTopLeftFoot;
						}
#ifndef _RELEASE
						else if (g_pGameCVars->pl_ladderControl.ladder_logVerbosity)
						{
							CryWatch ("[LADDER] %s can't climb up and off %s - top is blocked", player.GetEntity()->GetName(), pLadder->GetEntityTextDescription().c_str());
						}
#endif
					}
				}

			float heightFrac = (m_numRungsFromBottomPosition + m_fractionBetweenRungs) / m_topRungNumber;
			player.OnLadderPositionUpdated(heightFrac);
		}

		float distanceBetweenRungs = 0.f;
		EntityScripts::GetEntityProperty(pLadder, "Camera", "distanceBetweenRungs", distanceBetweenRungs);

		const Vec3 stopAtPosBottom = m_ladderBottom;
		const float distanceUpLadder = (m_numRungsFromBottomPosition + m_fractionBetweenRungs) * /*g_pGameCVars->pl_ladderControl.ladder_*/distanceBetweenRungs;
		const Vec3 setThisPosition(stopAtPosBottom.x, stopAtPosBottom.y, stopAtPosBottom.z + distanceUpLadder);

		player.GetEntity()->SetPos(setThisPosition);

		if (m_mostRecentlyEnteredAction)
		{
			const float animFraction = m_fractionBetweenRungs * 0.5f + ((m_numRungsFromBottomPosition & 1) ? 0.5f : 0.0f);

#ifndef _RELEASE
			if (g_pGameCVars->pl_ladderControl.ladder_logVerbosity)
			{
				CryWatch ("[LADDER] $7Setting anim fraction to %.4f", animFraction);
			}
#endif

			m_mostRecentlyEnteredAction->SetParam(s_ladderFractionCRC, animFraction);
		}
	}

	return m_playGetOffAnim == kLadderAnimType_none;
}

void CPlayerStateLadder::LeaveLadder(CPlayer& player, ELadderLeaveLocation leaveLocation)
{
	switch (leaveLocation)
	{
	case eLLL_Drop:
		player.GetMoveRequest().velocity = Vec3(0.f, 0.f, -1.f);
		player.GetMoveRequest().type = eCMT_Impulse;
		break;
	case eLLL_Top:
		m_playGetOffAnim = (m_topRungNumber & 1) ? kLadderAnimType_atTopRightFoot : kLadderAnimType_atTopLeftFoot;
		break;
	case eLLL_Bottom:
		m_playGetOffAnim = kLadderAnimType_atBottom;
		break;
	}
}

void CPlayerStateLadder::SetHeightFrac(CPlayer& player, float heightFrac)
{
	heightFrac *= m_topRungNumber;
	m_numRungsFromBottomPosition = (int)heightFrac;
	m_fractionBetweenRungs = heightFrac - m_numRungsFromBottomPosition;
}

bool CPlayerStateLadder::IsUsableLadder(CPlayer& player, IEntity* pLadder, const SmartScriptTable& ladderProperties)
{
	bool retVal = false;

	if(pLadder && !player.IsOnLadder() && (player.CanTurnBody() || player.IsThirdPerson()))
	{
		float height = 0.f;
		ladderProperties->GetValue("height", height);

		if(height > 0.f)
		{
			const Matrix34& ladderTM = pLadder->GetWorldTM();
			Vec3 ladderPos = ladderTM.GetTranslation();
			Vec3 playerPos = player.GetEntity()->GetWorldPos();

			const bool bIsOnTop = (playerPos.z + 0.1f) > (ladderPos.z + height);
			const char* szAngleVariable = bIsOnTop ? "approachAngleTop" : "approachAngle";

			float angleRange = 0.f;
			ladderProperties->GetValue(szAngleVariable, angleRange);

			retVal = true;

			if(angleRange != 0.f)
			{
				Vec2 ladderToPlayer((playerPos - ladderPos));
				Vec2 ladderDirection(ladderTM.GetColumn1());

				ladderToPlayer.Normalize();
				ladderDirection.Normalize();

				if(angleRange < 0.f)
				{
					//negative angle means you want the middle of the available range to be from the opposite direction
					angleRange = -angleRange;
					ladderToPlayer = -ladderToPlayer;
				}

				if (bIsOnTop)
				{
					if (-ladderToPlayer.Dot(ladderDirection) < cos(DEG2RAD(angleRange)))
					{
						retVal = false;
					}
				}
				else
				{
					if (ladderToPlayer.Dot(ladderDirection) < cos(DEG2RAD(angleRange)))
					{
						retVal = false;
					}
				}
			}
		}
	}

#ifndef _RELEASE
	if (g_pGameCVars->pl_ladderControl.ladder_logVerbosity)
	{
		if (pLadder)
		{
			float distanceBetweenRungs = 0.f;
			float stopClimbingDistanceFromBottom = 0.f;

			EntityScripts::GetEntityProperty(pLadder, "Camera", "distanceBetweenRungs", distanceBetweenRungs);
			EntityScripts::GetEntityProperty(pLadder, "Offsets", "stopClimbDistanceFromBottom", stopClimbingDistanceFromBottom);

			ColorB ladderColour(150, 150, 255, 150);
			IRenderAuxGeom * pGeom = gEnv->pRenderer->GetIRenderAuxGeom();
			const Vec3 ladderBasePos = pLadder->GetWorldPos();
			const Matrix34& ladderTM = pLadder->GetWorldTM();
			//const float distanceBetweenRungs = /*g_pGameCVars->pl_ladderControl.ladder_*/_distanceBetweenRungs;
			float height = 0.f;
			AABB entityBounds;
			pLadder->GetLocalBounds(entityBounds);
			const Vec3 rungEndSideways = ladderTM.GetColumn0() * entityBounds.GetSize().x * 0.5f;
			EntityScripts::GetEntityProperty(pLadder, "height", height);
			const Vec3 offsetToTop = height * ladderTM.GetColumn2();

			for (float rungHeight = stopClimbingDistanceFromBottom; rungHeight < height; rungHeight += distanceBetweenRungs)
			{
				const Vec3 rungCentre(ladderBasePos.x, ladderBasePos.y, ladderBasePos.z + rungHeight);
				pGeom->DrawLine(rungCentre - rungEndSideways, ladderColour, rungCentre + rungEndSideways, ladderColour, 20.f);
			}

			pGeom->DrawLine(ladderBasePos - rungEndSideways, ladderColour, ladderBasePos - rungEndSideways + offsetToTop, ladderColour, 20.f);
			pGeom->DrawLine(ladderBasePos + rungEndSideways, ladderColour, ladderBasePos + rungEndSideways + offsetToTop, ladderColour, 20.f);
		}

		CryWatch ("[LADDER] Is %s usable by %s? %s", pLadder ? pLadder->GetEntityTextDescription().c_str() : "<NULL ladder entity>", player.GetEntity()->GetEntityTextDescription().c_str(), retVal ? "$3YES$o" : "$4NO$o");
	}
#endif

	return retVal;
}

void CPlayerStateLadder::SetMostRecentlyEnteredAction(CLadderAction * thisAction)
{
	if (thisAction)
	{
		thisAction->AddRef();
	}

	if (m_mostRecentlyEnteredAction)
	{
		m_mostRecentlyEnteredAction->Release();
	}

	m_mostRecentlyEnteredAction = thisAction;
}

void CPlayerStateLadder::InformLadderAnimEnter(CPlayer & player, CLadderAction * thisAction)
{
	assert (thisAction);

	LadderLog ("Action '%s' has been entered", thisAction->GetName());
	LadderLogIndent();

	SetMostRecentlyEnteredAction(thisAction);
}

void CPlayerStateLadder::InformLadderAnimIsDone(CPlayer & player, CLadderAction * thisAction)
{
	assert (thisAction);

	LadderLog ("Action '%s' is done", thisAction->GetName());
	LadderLogIndent();

	if (player.IsClient())
	{
		if (player.IsOnLadder())
		{
			if (IEntity * pLadder = gEnv->pEntitySystem->GetEntity(m_ladderEntityId))
			{
				SetClientPlayerOnLadder(pLadder, true);
			}
		}
	}

	if (thisAction == m_mostRecentlyEnteredAction)
	{
		//SetMostRecentlyEnteredAction(NULL);

		if (player.IsOnLadder() == false && thisAction->GetIsInterruptable() == false)
		{
			LadderExitIsComplete(player);
		}
	}
}

bool CPlayerStateLadder::HandleCancelInput(CPlayer & player, TCancelButtonBitfield cancelButtonPressed)
{
	assert (player.IsOnLadder());

	if ((cancelButtonPressed & (kCancelPressFlag_switchItem)) == 0)
	{
		if (m_mostRecentlyEnteredAction && m_mostRecentlyEnteredAction->GetIsInterruptable())
		{
			player.StateMachineHandleEventMovement(SStateEventLeaveLadder(eLLL_Drop));
		}
		return true;
	}

	return false;
}

#ifndef _RELEASE
void CPlayerStateLadder::UpdateNumActions(int change)
{
	assert (change == 1 || change == -1);

	assert (m_dbgNumActions >= 0);
	m_dbgNumActions += change;

	assert (m_dbgNumActions >= 0);
}
#endif