// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Implements the Player ledge state

-------------------------------------------------------------------------
History:
- 9.9.10: Created by Stephen M. North

*************************************************************************/
#include <StdAfx.h>
#include "PlayerStateLedge.h"
#include "GameCVars.h"
#include "Player.h"
#include "Weapon.h"
#include "StatsRecordingMgr.h"
#include "PersistantStats.h"
#include "IAnimatedCharacter.h"
#include "PlayerInput.h"
#include "GameActions.h"

#include "PlayerStateUtil.h"

#include "Environment/LedgeManager.h"

#include "PlayerAnimation.h"
#include "Utility/CryWatch.h"
#include "Melee.h"

static const float s_PlayerMax2DPhysicsVelocity = 9.f;


class CActionLedgeGrab : public TPlayerAction
{
public:

	DEFINE_ACTION("LedgeGrab");

	CActionLedgeGrab(CPlayer &player, const QuatT &ledgeLoc, SLedgeTransitionData::EOnLedgeTransition transition, bool endCrouched, bool comingFromOnGround, bool comingFromSprint)
		: TPlayerAction(PP_PlayerActionUrgent, PlayerMannequin.fragmentIDs.ledgeGrab),
			m_player(player),
			m_targetViewDirTime(0.2f),
			m_transitionType(transition),
			m_endCrouched(endCrouched),
			m_comingFromOnGround(comingFromOnGround),
			m_comingFromSprint(comingFromSprint), 
			m_haveUnHolsteredWeapon(false)
	{
		SetParam("TargetPos", ledgeLoc);

		Ang3 viewAng;
		viewAng.SetAnglesXYZ( ledgeLoc.q );
		viewAng.y = 0.0f;
		m_targetViewDir = Quat::CreateRotationXYZ( viewAng );
	}

	virtual void OnInitialise() override
	{
		const CTagDefinition *fragTagDef = m_context->controllerDef.GetFragmentTagDef(m_fragmentID);
		if (fragTagDef)
		{
			const SMannequinPlayerParams::Fragments::SledgeGrab& ledgeGrabFragment = PlayerMannequin.fragments.ledgeGrab;

			// TODO - remove transition type and change over to just using the ledge's m_ledgeFlagBitfield this 
			// would simplify everything and remove a lot of the dependency on Player_Params.xml

			switch (m_transitionType)
			{
				case SLedgeTransitionData::eOLT_VaultOnto:
				case SLedgeTransitionData::eOLT_HighVaultOnto:
				{
					const TagID vaultID = ledgeGrabFragment.fragmentTagIDs.up;
					fragTagDef->Set(m_fragTags, vaultID, true);						
					break;
				}
				case SLedgeTransitionData::eOLT_VaultOver:
				case SLedgeTransitionData::eOLT_VaultOverIntoFall: // this flag is now deprecated
				case SLedgeTransitionData::eOLT_HighVaultOver:
				case SLedgeTransitionData::eOLT_HighVaultOverIntoFall: // this flag probably shouldn't be needed either in that case
				{
					const TagID vaultID = ledgeGrabFragment.fragmentTagIDs.over;
					fragTagDef->Set(m_fragTags, vaultID, true);
					break;
				}
				case SLedgeTransitionData::eOLT_QuickLedgeGrab:
				{
					const TagID vaultID = ledgeGrabFragment.fragmentTagIDs.quick;
					fragTagDef->Set(m_fragTags, vaultID, true);
					break;
				}
				// perhaps set ledge off of midair and falling here. But ledge should be the default in data
			}

			switch (m_transitionType)
			{
				case SLedgeTransitionData::eOLT_HighVaultOverIntoFall:
				case SLedgeTransitionData::eOLT_HighVaultOnto:
				case SLedgeTransitionData::eOLT_HighVaultOver:
				{
					const TagID highID = ledgeGrabFragment.fragmentTagIDs.high;
					fragTagDef->Set(m_fragTags, highID, true);
					//intentionally no break to fall through
				}

				case SLedgeTransitionData::eOLT_VaultOverIntoFall:
				case SLedgeTransitionData::eOLT_VaultOnto:
				case SLedgeTransitionData::eOLT_VaultOver:
				{
					const TagID vaultID = ledgeGrabFragment.fragmentTagIDs.vault;
					fragTagDef->Set(m_fragTags, vaultID, true);
					SPlayerStats *pPlayerStats = m_player.GetActorStats();
					pPlayerStats->forceSTAP = SPlayerStats::eFS_On;	// force STAP on for vaults
					pPlayerStats->bDisableTranslationPinning = true; // disables translation pinning for vaults (stops 3P legs from cutting through camera)
					
					break;
				}
			}

			if(m_endCrouched)
			{
				const TagID vaultID = ledgeGrabFragment.fragmentTagIDs.endCrouched;
				fragTagDef->Set(m_fragTags, vaultID, true);
			}

			//--- Disabling this for now whilst position adjusted anims with trimmed start-times don't work
			//--- Once this is resolved we'll re-enable.
			//if (m_comingFromOnGround)
			//{
				const TagID vaultID = ledgeGrabFragment.fragmentTagIDs.floor;
				fragTagDef->Set(m_fragTags, vaultID, true);
			//}
			//else
			//{
			//	const TagID vaultID = ledgeGrabFragment.fragmentTagIDs.fall;
			//	fragTagDef->Set(m_fragTags, vaultID, true);
			//}

			//was sprinting
			if (m_comingFromSprint)
			{
				const TagID floorSprintID = ledgeGrabFragment.fragmentTagIDs.floorSprint;
				fragTagDef->Set(m_fragTags, floorSprintID, true);
			}
		}

	}

	virtual void Enter() override
	{
		TPlayerAction::Enter();

		IAnimatedCharacter* pAnimChar = m_player.GetAnimatedCharacter();

		if( pAnimChar )
		{
			pAnimChar->SetMovementControlMethods(eMCM_Animation, eMCM_Animation);
			pAnimChar->RequestPhysicalColliderMode(eColliderMode_Disabled, eColliderModeLayer_Game, "CActionLedgeGrab::Enter()");
		}
		switch (m_transitionType)
		{
		case SLedgeTransitionData::eOLT_VaultOverIntoFall:
		case SLedgeTransitionData::eOLT_VaultOnto:
		case SLedgeTransitionData::eOLT_VaultOver:
		case SLedgeTransitionData::eOLT_HighVaultOverIntoFall:
		case SLedgeTransitionData::eOLT_HighVaultOnto:
		case SLedgeTransitionData::eOLT_HighVaultOver:
			break;
		default:
			{
				PlayerCameraAnimationSettings cameraAnimationSettings;
				cameraAnimationSettings.positionFactor = 1.0f;
				cameraAnimationSettings.rotationFactor = g_pGameCVars->pl_ledgeClamber.cameraBlendWeight;
				m_player.PartialAnimationControlled( true, cameraAnimationSettings );
			}
			break;
		}
	}

	virtual void Exit() override
	{
		TPlayerAction::Exit();

		IAnimatedCharacter* pAnimChar = m_player.GetAnimatedCharacter();

		if( pAnimChar )
		{
			pAnimChar->SetMovementControlMethods(eMCM_Entity, eMCM_Entity);
			pAnimChar->ForceRefreshPhysicalColliderMode();
			pAnimChar->RequestPhysicalColliderMode(eColliderMode_Undefined, eColliderModeLayer_Game, "CActionLedgeGrab::Enter()");
		}

		if (!m_haveUnHolsteredWeapon)
		{
			m_haveUnHolsteredWeapon=true;
			m_player.HolsterItem_NoNetwork(false, true, 0.5f);
		}
		
		m_player.PartialAnimationControlled( false, PlayerCameraAnimationSettings() );

		SPlayerStats *pPlayerStats = m_player.GetActorStats();
		pPlayerStats->forceSTAP = SPlayerStats::eFS_None;
		pPlayerStats->bDisableTranslationPinning=false;

		m_player.StateMachineHandleEventMovement( PLAYER_EVENT_LEDGE_ANIM_FINISHED );
	}

	virtual EStatus Update(float timePassed) override
	{
		TPlayerAction::Update(timePassed);

		//--- Update view direction
		float t = m_activeTime / m_targetViewDirTime;
		t = min(t, 1.0f);
		Quat rotNew;
		rotNew.SetSlerp(m_player.GetViewRotation(), m_targetViewDir, t);
		m_player.SetViewRotation( rotNew );

		if (!m_player.IsThirdPerson())
		{
			if (m_activeTime > 0.5f && !m_haveUnHolsteredWeapon)
			{
				m_haveUnHolsteredWeapon=true;
				m_player.HolsterItem_NoNetwork(false, true, 0.5f);
			}
		}

		return m_eStatus;
	}

private:

	CPlayer& m_player;
	Quat     m_targetViewDir;
	float		 m_targetViewDirTime;
	
	SLedgeTransitionData::EOnLedgeTransition m_transitionType;
	
	bool		 m_endCrouched;
	bool		 m_comingFromOnGround;
	bool		 m_comingFromSprint;
	bool		 m_haveUnHolsteredWeapon;
};

CPlayerStateLedge::SLedgeGrabbingParams CPlayerStateLedge::s_ledgeGrabbingParams;

CPlayerStateLedge::CPlayerStateLedge()
	: m_onLedge(false)
	, m_ledgeSpeedMultiplier(1.0f)
	, m_lastTimeOnLedge(0.0f)
	, m_exitVelocity(ZERO)
	, m_ledgePreviousPosition(ZERO)
	, m_ledgePreviousPositionDiff(ZERO)
	, m_ledgePreviousRotation(ZERO)
	, m_enterFromGround(false)
	, m_enterFromSprint(false)
{
}

CPlayerStateLedge::~CPlayerStateLedge()
{
}

bool CPlayerStateLedge::CanGrabOntoLedge( const CPlayer& player )
{
	if (player.IsClient())
	{
		const float kAirTime[2] = {0.5f, 0.0f};
		const float inAirTime = player.CPlayer::GetActorStats()->inAir;
		const bool canGrabFromAir   = (inAirTime >= kAirTime[player.IsJumping()]);
		const SPlayerStats& playerStats = *player.GetActorStats();

		IItem *pCurrentItem = player.GetCurrentItem();
		if (pCurrentItem)
		{
			IWeapon *pIWeapon = pCurrentItem->GetIWeapon();
			if (pIWeapon)
			{
				CWeapon *pWeapon = static_cast<CWeapon*>(pIWeapon);
				CMelee *pMelee = pWeapon->GetMelee();
				if (pMelee)
				{
					if (pMelee->IsAttacking())
					{
						return false;		// can't ledge grab when we're currently melee-ing
					}
				}
			}
		}

		return canGrabFromAir && (playerStats.pickAndThrowEntity == 0) && (player.IsMovementRestricted() == false);
	}

	return false;
}

void CPlayerStateLedge::OnEnter( CPlayer& player, const SStateEventLedge& ledgeEvent )
{
	
	const SLedgeTransitionData::EOnLedgeTransition ledgeTransition = ledgeEvent.GetLedgeTransition();
	const LedgeId nearestGrabbableLedgeId(ledgeEvent.GetLedgeId());
	const bool isClient = player.IsClient();

	// Record 'LedgeGrab' telemetry and game stats.
	if(ledgeTransition != SLedgeTransitionData::eOLT_None)
	{
		CStatsRecordingMgr::TryTrackEvent(&player, eGSE_LedgeGrab);
		if(isClient)
		{
			g_pGame->GetPersistantStats()->IncrementClientStats(EIPS_LedgeGrabs);
		}
	}

	m_onLedge = true;
	m_postSerializeLedgeTransition = ledgeTransition;

	if( IAnimatedCharacter* pAnimChar = player.GetAnimatedCharacter() )
	{
		//////////////////////////////////////////////////////////////////////////
		// Current weapon and input setup
		IItem* pCurrentItem = player.GetCurrentItem();
		CWeapon* pCurrentWeapon =  (pCurrentItem != NULL) ? static_cast<CWeapon*>(pCurrentItem->GetIWeapon()) : NULL;

		if(pCurrentWeapon != NULL)
		{
			pCurrentWeapon->FumbleGrenade();
			pCurrentWeapon->CancelCharge();
		}

		m_ledgeId = nearestGrabbableLedgeId;
		const SLedgeInfo ledgeInfo = g_pGame->GetLedgeManager()->GetLedgeById(nearestGrabbableLedgeId);
		CRY_ASSERT( ledgeInfo.IsValid() );

		const bool useVault = ledgeInfo.AreAnyFlagsSet( kLedgeFlag_useVault|kLedgeFlag_useHighVault );
		if (!useVault)
		{
			player.HolsterItem_NoNetwork(true);

			IPlayerInput* pPlayerInput = player.GetPlayerInput();
			if ((pPlayerInput != NULL) && (pPlayerInput->GetType() == IPlayerInput::PLAYER_INPUT))
			{
				static_cast<CPlayerInput*>(pPlayerInput)->ForceStopSprinting();
			}

			if (isClient)
			{
				g_pGameActions->FilterLedgeGrab()->Enable(true);
			}

			if(ledgeInfo.AreFlagsSet( kLedgeFlag_endCrouched ))
			{
				player.OnSetStance( STANCE_CROUCH );
			}
		}
		else
		{
			if (pCurrentWeapon != NULL)
			{
				pCurrentWeapon->ClearDelayedFireTimeout();
			}

			if (isClient)
			{
				g_pGameActions->FilterVault()->Enable(true);
			}
		}

		//////////////////////////////////////////////////////////////////////////
		/// Figure out speed based on factors like ledge type
		m_ledgeSpeedMultiplier = GetLedgeGrabSpeed( player );

		SLedgeTransitionData::EOnLedgeTransition transition = ledgeTransition;
		if ((transition > SLedgeTransitionData::eOLT_None) && (transition < SLedgeTransitionData::eOLT_MaxTransitions))
		{
			const SLedgeBlendingParams& ledgeGrabParams = GetLedgeGrabbingParams().m_grabTransitionsParams[transition];

#if 0
			if(transition == SLedgeTransitionData::eOLT_VaultOver || transition == SLedgeTransitionData::eOLT_VaultOnto)
			{
				const float vel2D = player.GetActorPhysics().velocity.GetLength2D();
				m_exitVelocity.Set(0.f, vel2D, ledgeGrabParams.m_vExitVelocity.z); 

				const float animSpeedMult = (float)__fsel( -vel2D, g_pGameCVars->g_vaultMinAnimationSpeed, max(g_pGameCVars->g_vaultMinAnimationSpeed, vel2D / s_PlayerMax2DPhysicsVelocity));

				m_ledgeSpeedMultiplier *= animSpeedMult;
			}
			else
#endif
			{
				m_exitVelocity = ledgeGrabParams.m_vExitVelocity;
			}

			StartLedgeBlending(player,ledgeGrabParams);
		}

		m_ledgePreviousPosition = ledgeInfo.GetPosition();
		m_ledgePreviousRotation.SetRotationVDir( ledgeInfo.GetFacingDirection() );

		//////////////////////////////////////////////////////////////////////////
		/// Notify script side of ledge entity
		IEntity *pLedgeEntity = gEnv->pEntitySystem->GetEntity( ledgeInfo.GetEntityId() );
		if (pLedgeEntity != NULL )
		{
			EntityScripts::CallScriptFunction( pLedgeEntity, pLedgeEntity->GetScriptTable(), "StartUse", ScriptHandle(player.GetEntity()) );
		}

		//////////////////////////////////////////////////////////////////////////
		/// Queue Mannequin action
		const bool endCrouched = ledgeInfo.AreFlagsSet( kLedgeFlag_endCrouched );
		const bool comingFromOnGround = ledgeEvent.GetComingFromOnGround();
		const bool comingFromSprint = ledgeEvent.GetComingFromSprint();

		m_enterFromGround = comingFromOnGround;
		m_enterFromSprint = comingFromSprint;

		CActionLedgeGrab *pLedgeGrabAction = new CActionLedgeGrab( player, m_ledgeBlending.m_qtTargetLocation, transition, endCrouched, comingFromOnGround, comingFromSprint );
		pLedgeGrabAction->SetSpeedBias(m_ledgeSpeedMultiplier);
		player.GetAnimatedCharacter()->GetActionController()->Queue(*pLedgeGrabAction);

		if (!player.IsRemote())
		{
			player.HasClimbedLedge( nearestGrabbableLedgeId, comingFromOnGround, comingFromSprint );
		}
	}
}



void CPlayerStateLedge::OnPrePhysicsUpdate( CPlayer& player, const SActorFrameMovementParams& movement, float frameTime )
{
	const bool gameIsPaused = (frameTime == 0.0f);

	IF_UNLIKELY ( gameIsPaused )
	{
		return;
	}

	player.GetMoveRequest().type = eCMT_Fly;

	Vec3 posDiff(ZERO);
	const SLedgeInfo ledgeInfo = g_pGame->GetLedgeManager()->GetLedgeById( m_ledgeId );
	if ( ledgeInfo.IsValid() )
	{
		const Vec3 ledgePos = ledgeInfo.GetPosition();
		const Quat ledgeRot = Quat::CreateRotationVDir( ledgeInfo.GetFacingDirection() );
		
		if ((ledgePos != m_ledgePreviousPosition) || (ledgeRot != m_ledgePreviousRotation))
		{
			posDiff = ledgePos - m_ledgePreviousPosition;

			const Quat rotToUse = ledgeRot * Quat::CreateRotationZ(DEG2RAD(180.f));

			player.GetAnimatedCharacter()->ForceMovement(QuatT(posDiff, Quat(IDENTITY)));
			player.GetAnimatedCharacter()->ForceOverrideRotation(rotToUse);

			m_ledgePreviousPosition = ledgePos;
			m_ledgePreviousRotation = ledgeRot;
		}

		m_ledgePreviousPositionDiff = posDiff;

#ifdef STATE_DEBUG
		if (g_pGameCVars->pl_ledgeClamber.debugDraw)
		{
			const float XPOS = 200.0f;
			const float YPOS = 80.0f;
			const float FONT_SIZE = 2.0f;
			const float FONT_COLOUR[4] = {1,1,1,1};
			Vec3 pos = player.GetEntity()->GetPos();
			IRenderAuxText::Draw2dLabel(XPOS, YPOS+20.0f, FONT_SIZE, FONT_COLOUR, false, "Ledge Grab: Cur: (%f %f %f) Tgt: (%f %f %f) T:%f", pos.x, pos.y, pos.z, m_ledgeBlending.m_qtTargetLocation.t.x, m_ledgeBlending.m_qtTargetLocation.t.y, m_ledgeBlending.m_qtTargetLocation.t.z, frameTime);
			gEnv->pRenderer->GetIRenderAuxGeom()->DrawSphere(m_ledgeBlending.m_qtTargetLocation.t, 0.15f, ColorB(0,0,255,255));
			gEnv->pRenderer->GetIRenderAuxGeom()->DrawSphere(pos, 0.1f, ColorB(255,0,0,255));

		}
#endif //_RELEASE

#if 1
		float desiredSpeed=0.f;
		IMovementController *pMovementController = player.GetMovementController();
		if (pMovementController)
		{
			SMovementState curMovementState;
			pMovementController->GetMovementState(curMovementState);

			desiredSpeed = curMovementState.desiredSpeed;
		}

		//CryWatch("ledge: desiredSpeed=%f", desiredSpeed);

		// if we are falling or we are pushing to move on our inputs
		// pass through our moveRequest to keep the motion and mann tags consistent
		if ( (ledgeInfo.AreAnyFlagsSet(kLedgeFlag_useVault|kLedgeFlag_useHighVault) && ledgeInfo.AreFlagsSet(kLedgeFlag_endFalling)) 
			|| desiredSpeed > 0.1f )
		{
			Vec3& moveRequestVelocity = player.GetMoveRequest().velocity;
			moveRequestVelocity = m_ledgeBlending.m_qtTargetLocation.q * m_exitVelocity;

			//if (player.GetAnimatedCharacter()->GetMCMH() != eMCM_Animation)
			//{
			//	CryLog("CPlayerStateLedge::OnPrePhysicsUpdate() setting moveRequestVelocity when not animcontrolled. MCMH=%d; MCMV=%d", player.GetAnimatedCharacter()->GetMCMH(), player.GetAnimatedCharacter()->GetMCMV());
			//}

			if( !posDiff.IsZeroFast() && (g_pGameCVars->g_ledgeGrabMovingledgeExitVelocityMult + frameTime > 0.f) )
			{
				moveRequestVelocity += posDiff*__fres(frameTime)*g_pGameCVars->g_ledgeGrabMovingledgeExitVelocityMult;
			}
		}
#endif
	}
}

void CPlayerStateLedge::OnAnimFinished( CPlayer &player )
{
#if 1
	const SLedgeInfo ledgeInfo = g_pGame->GetLedgeManager()->GetLedgeById( m_ledgeId );
	if ( ledgeInfo.IsValid() )
	{
		if (ledgeInfo.AreAnyFlagsSet(kLedgeFlag_useVault|kLedgeFlag_useHighVault) && ledgeInfo.AreFlagsSet(kLedgeFlag_endFalling))
		{
			player.StateMachineHandleEventMovement( PLAYER_EVENT_FALL );
		}
		else
		{
			player.StateMachineHandleEventMovement( PLAYER_EVENT_GROUND );
		}
	}
#endif

	Vec3& moveRequestVelocity = player.GetMoveRequest().velocity;
	player.GetMoveRequest().jumping=true;
	moveRequestVelocity = m_ledgeBlending.m_qtTargetLocation.q * m_exitVelocity;

	float frameTime = gEnv->pTimer->GetFrameTime();
	if( !m_ledgePreviousPositionDiff.IsZeroFast() && (g_pGameCVars->g_ledgeGrabMovingledgeExitVelocityMult * frameTime > 0.f) )
	{
		moveRequestVelocity += m_ledgePreviousPositionDiff *__fres(frameTime)*g_pGameCVars->g_ledgeGrabMovingledgeExitVelocityMult;
	}
}

void CPlayerStateLedge::OnExit( CPlayer& player )
{
	IAnimatedCharacter* pAnimatedCharacter = player.GetAnimatedCharacter();

	if( (pAnimatedCharacter != NULL) && (m_ledgeId.IsValid()) )
	{
		const SLedgeInfo ledgeInfo = g_pGame->GetLedgeManager()->GetLedgeById( m_ledgeId );
		CRY_ASSERT( ledgeInfo.IsValid() );

		const bool useVault = ledgeInfo.AreAnyFlagsSet( kLedgeFlag_useVault|kLedgeFlag_useHighVault );
		const bool isClient = player.IsClient();
		if (isClient)
		{
			if (useVault)
			{
				g_pGameActions->FilterVault()->Enable(false);
			}
			else
			{
				g_pGameActions->FilterLedgeGrab()->Enable(false);
			}
		}

		player.m_params.viewLimits.ClearViewLimit(SViewLimitParams::eVLS_Ledge);

		// Added to remove the curious lean the player has after performing a ledge grab.
		Quat viewRotation = player.GetViewRotation();
		Ang3 viewAngles;
		viewAngles.SetAnglesXYZ( viewRotation );
		viewAngles.y = 0.0f;
		viewRotation = Quat::CreateRotationXYZ( viewAngles );
		player.SetViewRotation( viewRotation );

		pAnimatedCharacter->ForceOverrideRotation( player.GetEntity()->GetWorldRotation() );

		if(ledgeInfo.AreFlagsSet( kLedgeFlag_endCrouched ))
		{
			if(isClient)
			{
				IPlayerInput* pInput = player.GetPlayerInput();
				if ((pInput != NULL) && (pInput->GetType() == IPlayerInput::PLAYER_INPUT))
				{
					static_cast<CPlayerInput*>(pInput)->AddCrouchAction();
				}

				if(player.IsThirdPerson() == false)
				{
					player.m_torsoAimIK.Enable( true ); //Force snap for smooth transition
				}
			}
		}
	}

	m_ledgeId = LedgeId();
}

void CPlayerStateLedge::UpdateNearestGrabbableLedge( const CPlayer& player, SLedgeTransitionData* pLedgeState, const bool ignoreCharacterMovement )
{
	pLedgeState->m_ledgeTransition = SLedgeTransitionData::eOLT_None;
	pLedgeState->m_nearestGrabbableLedgeId = LedgeId::invalid_id;

	// get the position and movement direction for this entity
	// prepare as parameters for the ledge manager

	const Matrix34& mtxWorldTm = player.GetEntity()->GetWorldTM();
	Vec3 vPosition = mtxWorldTm.GetTranslation();

	vPosition += Vec3(0.0f, 0.0f, player.GetStanceInfo(player.GetStance())->viewOffset.z);

	Vec3 vScanDirection = mtxWorldTm.TransformVector(GetLedgeGrabbingParams().m_ledgeNearbyParams.m_vSearchDir);
	vScanDirection.NormalizeFast();

	// retrieve the ledge manager and run a query on whether a ledge is within the parameters
	const CPlayerStateLedge::SLedgeNearbyParams& nearbyParams = GetLedgeGrabbingParams().m_ledgeNearbyParams;
	const LedgeId ledgeId = g_pGame->GetLedgeManager()->FindNearestLedge(vPosition, vScanDirection, GetLedgeGrabbingParams().m_ledgeNearbyParams.m_fMaxDistance, DEG2RAD(nearbyParams.m_fMaxAngleDeviationFromSearchDirInDegrees), DEG2RAD(nearbyParams.m_fMaxExtendedAngleDeviationFromSearchDirInDegrees));
	if (ledgeId.IsValid())
	{
		CWeapon* pCurrentWeapon = player.GetWeapon(player.GetCurrentItemId());
		const bool canLedgeGrabWithWeapon = (pCurrentWeapon == NULL) || (pCurrentWeapon->CanLedgeGrab());
		if (canLedgeGrabWithWeapon)
		{
			const bool characterMovesTowardLedge = IsCharacterMovingTowardsLedge(player, ledgeId);
			const bool ledgePointsTowardCharacter = IsLedgePointingTowardCharacter(player, ledgeId);
			const bool isPressingJump = player.m_jumpButtonIsPressed;
			if ((characterMovesTowardLedge || ignoreCharacterMovement) && ledgePointsTowardCharacter)
			{
				pLedgeState->m_nearestGrabbableLedgeId = ledgeId;
				pLedgeState->m_ledgeTransition = GetBestTransitionToLedge(player, player.GetEntity()->GetWorldPos(), ledgeId, pLedgeState);
			}	
		}
	}
}

SLedgeTransitionData::EOnLedgeTransition CPlayerStateLedge::GetBestTransitionToLedge(const CPlayer& player, const Vec3& refPosition, const LedgeId& ledgeId, SLedgeTransitionData* pLedgeState)
{
	CRY_ASSERT_MESSAGE(ledgeId.IsValid(), "Ledge index should be valid");

	const CLedgeManager* pLedgeManager = g_pGame->GetLedgeManager();
	const SLedgeInfo detectedLedge = pLedgeManager->GetLedgeById(ledgeId);

	CRY_ASSERT( detectedLedge.IsValid() );

	const bool useVault = detectedLedge.AreFlagsSet( kLedgeFlag_useVault );
	const bool useHighVault = detectedLedge.AreFlagsSet( kLedgeFlag_useHighVault );
	const bool isThinLedge = detectedLedge.AreFlagsSet( kLedgeFlag_isThin );
	const bool endFalling = detectedLedge.AreFlagsSet( kLedgeFlag_endFalling );
	const bool usableByMarines = detectedLedge.AreFlagsSet( kledgeFlag_usableByMarines );
	const Vec3 ledgePos = detectedLedge.GetPosition();

	if (CanReachPlatform(player, ledgePos, refPosition))
	{
		return SLedgeTransitionData::eOLT_None;
	}

	QuatT targetLocation;

	const Quat pRot = Quat::CreateRotationVDir( detectedLedge.GetFacingDirection() );

	const Vec3 vToVec = pLedgeManager->FindVectorToClosestPointOnLedge(refPosition, detectedLedge);
	targetLocation.q = pRot * Quat::CreateRotationZ(DEG2RAD(180)); 

	float bestDistanceSqr = 100.0f;
	SLedgeTransitionData::EOnLedgeTransition bestTransition = SLedgeTransitionData::eOLT_None;
	 
	// if required could add handling to reject ledges based on marines within CLedgeManager::FindNearestLedge()
	if (!usableByMarines)
	{
		return SLedgeTransitionData::eOLT_None;
	}

	//Get nearest entry point, from available transitions
	for (int i = 0; i < SLedgeTransitionData::eOLT_MaxTransitions; ++i)
	{
		const SLedgeBlendingParams& ledgeBlendParams = GetLedgeGrabbingParams().m_grabTransitionsParams[i];
		targetLocation.t = refPosition + vToVec - (Vec3(0, 0, ledgeBlendParams.m_vPositionOffset.z)) - ((targetLocation.q * FORWARD_DIRECTION) * ledgeBlendParams.m_vPositionOffset.y);

		if ((ledgeBlendParams.m_ledgeType == eLT_Thin) && !isThinLedge)
		{
			continue;
		}
		else if ((ledgeBlendParams.m_ledgeType == eLT_Wide) && isThinLedge)
		{
			continue;
		}
		else if (ledgeBlendParams.m_bIsVault != useVault)
		{
			continue;
		}
		else if (ledgeBlendParams.m_bIsHighVault != useHighVault)
		{
			continue;
		}
		else if (ledgeBlendParams.m_bEndFalling != endFalling)
		{
			continue;
		}

		float distanceSqr = (targetLocation.t - refPosition).len2();
		if (distanceSqr < bestDistanceSqr)
		{
			bestTransition = (SLedgeTransitionData::EOnLedgeTransition)(i);
			bestDistanceSqr = distanceSqr;
		}
	}

	return bestTransition;
}

bool CPlayerStateLedge::CanReachPlatform(const CPlayer& player, const Vec3& ledgePosition, const Vec3& refPosition)
{
	const SPlayerStats& stats = *player.GetActorStats();
	const SActorPhysics& actorPhysics = player.GetActorPhysics();

	const Vec3 distanceVector = ledgePosition - refPosition;

	//Some rough prediction for vertical movement...
	const Vec2 distanceVector2D = Vec2(distanceVector.x, distanceVector.y);
	const float distance2DtoWall = distanceVector2D.GetLength();
	const float estimatedTimeToReachWall = stats.speedFlat ? distance2DtoWall / stats.speedFlat : distance2DtoWall;

	const float predictedHeightWhenReachingWall = refPosition.z + (actorPhysics.velocity.z * estimatedTimeToReachWall) + (0.5f * actorPhysics.gravity.z * (estimatedTimeToReachWall*estimatedTimeToReachWall));
	const float predictedHeightWhenReachingWallDiff = (ledgePosition.z - predictedHeightWhenReachingWall);

	//If falling already ...
	if (actorPhysics.velocity.z < 0.0f)
	{
		return (predictedHeightWhenReachingWallDiff < 0.1f);
	}
	else if ((fabs(actorPhysics.gravity.z) > 0.0f) && !gEnv->bMultiplayer)		// Causes problems with network in multiplayer
	{
		//Going up ...
		const float estimatedTimeToReachZeroVerticalSpeed = -actorPhysics.velocity.z / actorPhysics.gravity.z;
		const float predictedHeightWhenReachingZeroSpeed = refPosition.z + (actorPhysics.velocity.z * estimatedTimeToReachZeroVerticalSpeed) + (0.5f * actorPhysics.gravity.z * (estimatedTimeToReachZeroVerticalSpeed*estimatedTimeToReachZeroVerticalSpeed));
		const float predictedHeightWhenReachingZeroSpeedDiff = (ledgePosition.z - predictedHeightWhenReachingZeroSpeed);

		return ((predictedHeightWhenReachingZeroSpeedDiff < 0.2f) && !player.m_jumpButtonIsPressed) ;
	}

	return false;
}

//--------------------------------------------------------------------------------
QuatT CPlayerStateLedge::CalculateLedgeOffsetLocation( const Matrix34& worldMat, const Vec3& vPositionOffset, const bool keepOrientation ) const
{
	QuatT targetLocation;
	const Vec3& vWorldPos = worldMat.GetTranslation();
	const bool doOrientationBlending = !keepOrientation;

	const CLedgeManager* pLedgeManager = g_pGame->GetLedgeManager();
	assert(m_ledgeId.IsValid());

	const SLedgeInfo& ledgeInfo = pLedgeManager->GetLedgeById( m_ledgeId );
	assert(ledgeInfo.IsValid());

	const Quat pRot = doOrientationBlending ? Quat::CreateRotationVDir( ledgeInfo.GetFacingDirection() ) : Quat(worldMat);

	const Vec3 vToVec =	pLedgeManager->FindVectorToClosestPointOnLedge(vWorldPos, ledgeInfo);

	targetLocation.q = doOrientationBlending ? pRot * Quat::CreateRotationZ(DEG2RAD(180)) : pRot; 
	targetLocation.t = vWorldPos + vToVec;

	return targetLocation;
}

//--------------------------------------------------------------------------------
void CPlayerStateLedge::StartLedgeBlending( CPlayer& player, const SLedgeBlendingParams &blendingParams)
{
	CRY_TODO(15, 4, 2010, "Remove this CVAR once the ledge grab is sorted, is a useful debug tool for now...");

	assert(m_ledgeSpeedMultiplier > 0);

	m_ledgeBlending.m_qtTargetLocation = CalculateLedgeOffsetLocation( player.GetEntity()->GetWorldTM(), blendingParams.m_vPositionOffset, blendingParams.m_bKeepOrientation);

	Vec3 forceLookVector = m_ledgeBlending.m_qtTargetLocation.GetColumn1();
	forceLookVector.z = 0.0f;
	forceLookVector.Normalize();
	m_ledgeBlending.m_forwardDir = forceLookVector;

}

bool CPlayerStateLedge::IsCharacterMovingTowardsLedge( const CPlayer& player, const LedgeId& ledgeId)
{
	CRY_ASSERT_MESSAGE(ledgeId.IsValid(), "Ledge index should be valid");

	// if stick is pressed forwards, always take this ledge
	// 0.2f --> big enough to allow some room for stick deadzone
	const Vec3 inputMovement = player.GetBaseQuat().GetInverted() * player.GetLastRequestedVelocity();
	if (inputMovement.y >= 0.2f) 
	{
		const IEntity* pCharacterEntity = player.GetEntity();
		const Vec3 vCharacterPos = pCharacterEntity->GetWorldPos();

		const CLedgeManager* pLedgeManager = g_pGame->GetLedgeManager();
		const SLedgeInfo detectedLedge = pLedgeManager->GetLedgeById(ledgeId);
		if ( detectedLedge.IsValid() )
		{
			Vec3 vCharactertoLedge = pLedgeManager->FindVectorToClosestPointOnLedge(vCharacterPos, detectedLedge);

			vCharactertoLedge.z = 0.0f;

			Vec3 vVelocity = player.GetActorPhysics().velocity;
			vVelocity.z = 0.0f;

			return (vCharactertoLedge * vVelocity > 0.0f);
		}
	}

	return false;
}

//----------------------------------------------------------
bool CPlayerStateLedge::IsLedgePointingTowardCharacter(const CPlayer& player, const LedgeId& ledgeId)
{
	CRY_ASSERT_MESSAGE(ledgeId.IsValid(), "Ledge index should be valid");

	const CLedgeManager* pLedgeManager = g_pGame->GetLedgeManager();
	const SLedgeInfo detectedLedge = pLedgeManager->GetLedgeById(ledgeId);

	if (detectedLedge.IsValid())
	{
		const IEntity* pCharacterEntity = player.GetEntity();
		const Vec3 vLedgePos = detectedLedge.GetPosition();
		const Vec3 vCharacterPos = pCharacterEntity->GetWorldPos();
		const Vec3 vLedgeToCharacter = vCharacterPos - vLedgePos;

		return ((detectedLedge.GetFacingDirection() * vLedgeToCharacter) >= 0.0f);
	}

	return false;
}

void CPlayerStateLedge::SLedgeGrabbingParams::SetParamsFromXml(const IItemParamsNode* pParams)
{
	const char* gameModeParams = "LedgeGrabbingParams";
	
	const IItemParamsNode* pLedgeGrabParams = pParams ? pParams->GetChild( gameModeParams ) : NULL;

	if (pLedgeGrabParams)
	{
		pLedgeGrabParams->GetAttribute("normalSpeedUp", m_fNormalSpeedUp);
		pLedgeGrabParams->GetAttribute("mobilitySpeedUp", m_fMobilitySpeedUp);
		pLedgeGrabParams->GetAttribute("mobilitySpeedUpMaximum", m_fMobilitySpeedUpMaximum);

		m_ledgeNearbyParams.SetParamsFromXml(pLedgeGrabParams->GetChild("LedgeNearByParams"));

		m_grabTransitionsParams[SLedgeTransitionData::eOLT_MidAir].SetParamsFromXml(pLedgeGrabParams->GetChild("PullUpMidAir"));
		m_grabTransitionsParams[SLedgeTransitionData::eOLT_Falling].SetParamsFromXml(pLedgeGrabParams->GetChild("PullUpFalling"));
		m_grabTransitionsParams[SLedgeTransitionData::eOLT_VaultOver].SetParamsFromXml(pLedgeGrabParams->GetChild("VaultOver"));
		m_grabTransitionsParams[SLedgeTransitionData::eOLT_VaultOverIntoFall].SetParamsFromXml(pLedgeGrabParams->GetChild("VaultOverIntoFall"));
		m_grabTransitionsParams[SLedgeTransitionData::eOLT_VaultOnto].SetParamsFromXml(pLedgeGrabParams->GetChild("VaultOnto"));
		m_grabTransitionsParams[SLedgeTransitionData::eOLT_HighVaultOver].SetParamsFromXml(pLedgeGrabParams->GetChild("HighVaultOver"));
		m_grabTransitionsParams[SLedgeTransitionData::eOLT_HighVaultOverIntoFall].SetParamsFromXml(pLedgeGrabParams->GetChild("HighVaultOverIntoFall"));
		m_grabTransitionsParams[SLedgeTransitionData::eOLT_HighVaultOnto].SetParamsFromXml(pLedgeGrabParams->GetChild("HighVaultOnto"));
		m_grabTransitionsParams[SLedgeTransitionData::eOLT_QuickLedgeGrab].SetParamsFromXml(pLedgeGrabParams->GetChild("PullUpQuick"));
	}
}

void CPlayerStateLedge::SLedgeNearbyParams::SetParamsFromXml(const IItemParamsNode* pParams)
{
	if (pParams)
	{
		pParams->GetAttribute("searchDir", m_vSearchDir);
		pParams->GetAttribute("maxDistance", m_fMaxDistance);
		pParams->GetAttribute("maxAngleDeviationFromSearchDirInDegrees", m_fMaxAngleDeviationFromSearchDirInDegrees);
		pParams->GetAttribute("maxExtendedAngleDeviationFromSearchDirInDegrees", m_fMaxExtendedAngleDeviationFromSearchDirInDegrees);
	}
}

void CPlayerStateLedge::SLedgeBlendingParams::SetParamsFromXml(const IItemParamsNode* pParams)
{
	if (pParams)
	{
		pParams->GetAttribute("positionOffset", m_vPositionOffset);
		pParams->GetAttribute("moveDuration", m_fMoveDuration);
		pParams->GetAttribute("correctionDuration", m_fCorrectionDuration);

		const char *pLedgeType = pParams->GetAttribute("ledgeType");
		if (pLedgeType)
		{
			if (!stricmp(pLedgeType, "Wide"))
			{
				m_ledgeType = eLT_Wide;
			}
			else if (!stricmp(pLedgeType, "Thin"))
			{
				m_ledgeType = eLT_Thin;
			}
		}

		int isVault = 0;
		pParams->GetAttribute("isVault", isVault);
		m_bIsVault = (isVault != 0);

		int isHighVault = 0;
		pParams->GetAttribute("isHighVault", isHighVault);
		m_bIsHighVault = (isHighVault != 0);

		pParams->GetAttribute("exitVelocityY", m_vExitVelocity.y);
		pParams->GetAttribute("exitVelocityZ", m_vExitVelocity.z);
		int keepOrientation = 0;
		pParams->GetAttribute("keepOrientation", keepOrientation);
		m_bKeepOrientation = (keepOrientation != 0);
		int endFalling = 0;
		pParams->GetAttribute("endFalling", endFalling);
		m_bEndFalling = (endFalling != 0);
	}
}

CPlayerStateLedge::SLedgeGrabbingParams& CPlayerStateLedge::GetLedgeGrabbingParams()
{
	return s_ledgeGrabbingParams;
}


bool CPlayerStateLedge::TryLedgeGrab( CPlayer& player, const float expectedJumpEndHeight, const float startJumpHeight, const bool bSprintJump, SLedgeTransitionData* pLedgeState, const bool ignoreCharacterMovementWhenFindingLedge )
{
	CRY_ASSERT( pLedgeState );

	if( !CanGrabOntoLedge( player ) )
	{
		return false;
	}

	UpdateNearestGrabbableLedge( player, pLedgeState, ignoreCharacterMovementWhenFindingLedge );

	if( pLedgeState->m_ledgeTransition != SLedgeTransitionData::eOLT_None )
	{
		bool bDoLedgeGrab = true;

		const bool isFalling = player.GetActorPhysics().velocity*player.GetActorPhysics().gravity>0.0f;
		if (!isFalling)
		{
			const SLedgeInfo ledgeInfo = g_pGame->GetLedgeManager()->GetLedgeById( LedgeId(pLedgeState->m_nearestGrabbableLedgeId) );
			CRY_ASSERT( ledgeInfo.IsValid() );

			const bool isWindow = ledgeInfo.AreFlagsSet( kLedgeFlag_isWindow );
			const bool useVault = ledgeInfo.AreAnyFlagsSet( kLedgeFlag_useVault|kLedgeFlag_useHighVault );

			if (!isWindow)		// Windows always use ledge grabs
			{
				float ledgeHeight = ledgeInfo.GetPosition().z;
				if (expectedJumpEndHeight > (ledgeHeight + g_pGameCVars->g_ledgeGrabClearHeight))
				{
					if (useVault)
					{
						// TODO - this code doesn't seem right.. needs looking at
						float playerZ = player.GetEntity()->GetWorldPos().z;
						if (playerZ + g_pGameCVars->g_vaultMinHeightDiff > ledgeHeight)
						{
							bDoLedgeGrab = false;
						}
						else
						{
							bDoLedgeGrab = bSprintJump;
						}
					}
					else
					{
						bDoLedgeGrab = false;
					}
				}
			}
		}

		if (bDoLedgeGrab)
		{
			if( player.IsClient() )
			{
				const float fHeightofEntity = player.GetEntity()->GetWorldPos().z;
				CPlayerStateUtil::ApplyFallDamage( player, startJumpHeight, fHeightofEntity );
			}

		}
		return bDoLedgeGrab;
	}

	return false;
}

float CPlayerStateLedge::GetLedgeGrabSpeed( const CPlayer& player ) const
{
	const float kSpeedMultiplier[3] = { GetLedgeGrabbingParams().m_fNormalSpeedUp, GetLedgeGrabbingParams().m_fMobilitySpeedUp, GetLedgeGrabbingParams().m_fMobilitySpeedUpMaximum };

	uint32 speedSelection = 0;
	return kSpeedMultiplier[speedSelection];
}

void CPlayerStateLedge::Serialize( TSerialize serializer )
{
	uint16 ledgeId = m_ledgeId;

	serializer.Value( "LedgeId", ledgeId );
	serializer.Value( "LedgeTransition", m_postSerializeLedgeTransition );

	if(serializer.IsReading())
	{
		m_ledgeId = ledgeId;
	}
}

void CPlayerStateLedge::PostSerialize( CPlayer& player )
{
	// Re-start the ledge grab
	SLedgeTransitionData ledgeData( m_ledgeId );
	ledgeData.m_ledgeTransition = (SLedgeTransitionData::EOnLedgeTransition)m_postSerializeLedgeTransition;
	
	OnEnter( player, SStateEventLedge( ledgeData ) );
}
