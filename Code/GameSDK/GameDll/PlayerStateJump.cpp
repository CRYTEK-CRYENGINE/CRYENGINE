// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description: Implements the Player ledge state

   -------------------------------------------------------------------------
   History:
   - 10.9.10: Created by Stephen M. North

*************************************************************************/
#include "StdAfx.h"
#include "PlayerStateJump.h"
#include "Player.h"
#include "PlayerInput.h"
#include "PlayerStateUtil.h"

#include "ScreenEffects.h"

#include "StatsRecordingMgr.h"

#include "Utility/CryWatch.h"

#include "GameRules.h"
#include "GameCVars.h"
#include "GameCodeCoverage/GameCodeCoverageTracker.h"
#include "Weapon.h"
#include "UI/HUD/HUDEventWrapper.h"
#include "Environment/LedgeManager.h"

#include "NetInputChainDebug.h"
#include <IVehicleSystem.h>

#include "PlayerAnimation.h"

#include "MovementAction.h"
#include "PersistantStats.h"

#include <IPerceptionManager.h>

using namespace crymath;

CPlayerStateJump::CPlayerStateJump()
	: m_jumpState(JState_None)
	, m_jumpLock(0.0f)
	, m_startFallingHeight(FLT_MIN)
	, m_expectedJumpEndHeight(0.0f)
	, m_firstPrePhysicsUpdate(false)
	, m_bSprintJump(false)
	, m_jumpAction(nullptr)
{
}

CPlayerStateJump::~CPlayerStateJump()
{
}

void CPlayerStateJump::OnEnter(CPlayer& player)
{
	m_startFallingHeight = FLT_MIN;
	m_jumpState = JState_None;
	m_jumpLock = 0.0f;
	m_expectedJumpEndHeight = 0.0f;
	m_firstPrePhysicsUpdate = true;
}

void CPlayerStateJump::OnJump(CPlayer& player, const bool isHeavyWeapon, const float fVerticalSpeedModifier)
{
	player.GetMoveRequest().type = eCMT_JumpInstant;
	player.GetMoveRequest().jumping = true;

	StartJump(player, isHeavyWeapon, fVerticalSpeedModifier);

	NETINPUT_TRACE(player.GetEntityId(), (m_jumpState == JState_Jump));
}

void CPlayerStateJump::OnFall(CPlayer& player)
{
	SetJumpState(player, JState_Falling);
}

void CPlayerStateJump::StartJump(CPlayer& player, const bool isHeavyWeapon, const float fVerticalSpeedModifier)
{
	const SActorPhysics& actorPhysics = player.GetActorPhysics();
	const SPlayerStats& stats = *player.GetActorStats();
	const float onGroundTime = 0.2f;

	float g = actorPhysics.gravity.len();

	const float jumpHeightScale = 1.0f;
	const float jumpHeight = player.GetActorParams().jumpHeight * jumpHeightScale;

	float playerZ = player.GetEntity()->GetWorldPos().z;
	float expectedJumpEndHeight = playerZ + jumpHeight;

	pe_player_dimensions dimensions;
	IPhysicalEntity* pPhysics = player.GetEntity()->GetPhysics();
	if (pPhysics && pPhysics->GetParams(&dimensions))
	{
		float physicsBottom = dimensions.heightCollider - dimensions.sizeCollider.z;
		if (dimensions.bUseCapsule)
		{
			physicsBottom -= dimensions.sizeCollider.x;
		}
		expectedJumpEndHeight += physicsBottom;
	}

	float jumpSpeed = 0.0f;

	if (g > 0.0f)
	{
		jumpSpeed = sqrt_tpl(2.0f * jumpHeight * (1.0f / g)) * g;

		if (isHeavyWeapon)
		{
			jumpSpeed *= g_pGameCVars->pl_movement.nonCombat_heavy_weapon_speed_scale;
		}
	}

	//this is used to easily find steep ground
	float slopeDelta = (Vec3Constants<float>::fVec3_OneZ - actorPhysics.groundNormal).len();

	SetJumpState(player, JState_Jump);

	Vec3 jumpVec(ZERO);

	bool bNormalJump = true;

	player.PlaySound(CPlayer::ESound_Jump);

	OnSpecialMove(player, IPlayerEventListener::eSM_Jump);

	CCCPOINT_IF(player.IsClient(), PlayerMovement_LocalPlayerNormalJump);
	CCCPOINT_IF(!player.IsClient(), PlayerMovement_NonLocalPlayerNormalJump);

	{
		// This was causing the vertical jumping speed to be much slower.
		float verticalMult = max(1.0f - m_jumpLock, 0.3f);

		const Quat baseQuat = player.GetBaseQuat();
		jumpVec += baseQuat.GetColumn2() * jumpSpeed * verticalMult;
		jumpVec.z += fVerticalSpeedModifier;

#ifdef STATE_DEBUG
		if (g_pGameCVars->pl_debugInterpolation > 1)
		{
			CryWatch("Jumping: vec from player BaseQuat only = (%f, %f, %f)", jumpVec.x, jumpVec.y, jumpVec.z);
		}
#endif

		if (g_pGameCVars->pl_adjustJumpAngleWithFloorNormal && actorPhysics.groundNormal.len2() > 0.0f)
		{
			float vertical = clamp_tpl((actorPhysics.groundNormal.z - 0.25f) / 0.5f, 0.0f, 1.0f);
			Vec3 modifiedJumpDirection = LERP(actorPhysics.groundNormal, Vec3(0, 0, 1), vertical);
			jumpVec = modifiedJumpDirection * jumpVec.len();
		}

#ifdef STATE_DEBUG
		if (g_pGameCVars->pl_debugInterpolation > 1)
		{
			CryWatch("Jumping (%f, %f, %f)", jumpVec.x, jumpVec.y, jumpVec.z);
		}
#endif
	}

	NETINPUT_TRACE(player.GetEntityId(), jumpVec);

	FinalizeVelocity(player, jumpVec);

	if (!player.IsRemote())
	{
		player.HasJumped(player.GetMoveRequest().velocity);
	}

	IPhysicalEntity* pPhysEnt = player.GetEntity()->GetPhysics();
	if (pPhysEnt != NULL)
	{
		SAnimatedCharacterParams params = player.m_pAnimatedCharacter->GetParams();
		pe_player_dynamics pd;
		pd.kAirControl = player.GetAirControl() * g_pGameCVars->pl_jump_control.air_control_scale;
		pd.kAirResistance = player.GetAirResistance() * g_pGameCVars->pl_jump_control.air_resistance_scale;

		params.inertia = player.GetInertia() * g_pGameCVars->pl_jump_control.air_inertia_scale;

		if (player.IsRemote() && (g_pGameCVars->pl_velocityInterpAirControlScale > 0))
		{
			pd.kAirControl = g_pGameCVars->pl_velocityInterpAirControlScale;
		}

		pPhysEnt->SetParams(&pd);

		// Let Animated character handle the inertia
		player.SetAnimatedCharacterParams(params);
	}

#if 0
	if (debugJumping)
	{
		Vec3 entityPos = m_player.GetEntity()->GetWorldPos();
		gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(entityPos, ColorB(255, 255, 255, 255), entityPos, ColorB(255, 255, 0, 255), 2.0f);
		gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(entityPos + Vec3(0, 0, 2), ColorB(255, 255, 255, 255), entityPos + Vec3(0, 0, 2) + desiredVel, ColorB(0, 255, 0, 255), 2.0f);
		gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(entityPos, ColorB(255, 255, 255, 255), entityPos + jumpVec, ColorB(0, 255, 255, 255), 2.0f);
		IRenderAuxText::DrawLabelF(entityPos - entityRight * 1.0f + Vec3(0, 0, 3.0f), 1.5f, "Velo[%2.3f = %2.3f, %2.3f, %2.3f]", m_request.velocity.len(), m_request.velocity.x, m_request.velocity.y, m_request.velocity.z);
	}
#endif

	m_expectedJumpEndHeight = expectedJumpEndHeight;
	m_bSprintJump = player.IsSprinting();
}

bool CPlayerStateJump::OnPrePhysicsUpdate(CPlayer& player, const bool isHeavyWeapon, const SActorFrameMovementParams& movement, float frameTime)
{
	if (!m_firstPrePhysicsUpdate)
	{
		// if actor just got linked to a vehicle, abort jump/fall
		if (player.GetLinkedVehicle())
		{
			SetJumpState(player, JState_None);

			if (player.m_stats.fallSpeed)
			{
				player.m_stats.fallSpeed = 0.f;
			}
		}

		switch (m_jumpState)
		{
		case JState_Jump:
			UpdateJumping(player, isHeavyWeapon, movement, frameTime);
			break;
		case JState_Falling:
			UpdateFalling(player, isHeavyWeapon, movement, frameTime);
			break;
		}
	}

	m_firstPrePhysicsUpdate = false;

	NETINPUT_TRACE(player.GetEntityId(), (m_jumpState == JState_Jump));

	return(m_jumpState == JState_None);
}

void CPlayerStateJump::OnExit(CPlayer& player, const bool isHeavyWeapon)
{
	if (m_jumpAction)
	{
		m_jumpAction->TriggerExit();
		m_jumpAction->Release();
		m_jumpAction = NULL;
	}

	IPhysicalEntity* pPhysEnt = player.GetEntity()->GetPhysics();
	if (pPhysEnt != NULL)
	{
		SAnimatedCharacterParams params = player.m_pAnimatedCharacter->GetParams();
		pe_player_dynamics pd;
		pd.kAirControl = player.GetAirControl();
		pd.kAirResistance = player.GetAirResistance();
		params.inertia = player.GetInertia();

		if (player.IsRemote() && (g_pGameCVars->pl_velocityInterpAirControlScale > 0))
		{
			pd.kAirControl = g_pGameCVars->pl_velocityInterpAirControlScale;
		}

		pPhysEnt->SetParams(&pd);

		// Let Animated character handle the inertia
		player.SetAnimatedCharacterParams(params);
	}

	SetJumpState(player, JState_None);
}

void CPlayerStateJump::OnSpecialMove(CPlayer& player, IPlayerEventListener::ESpecialMove specialMove)
{
	if (player.m_playerEventListeners.empty() == false)
	{
		CPlayer::TPlayerEventListeners::const_iterator iter = player.m_playerEventListeners.begin();
		CPlayer::TPlayerEventListeners::const_iterator cur;
		while (iter != player.m_playerEventListeners.end())
		{
			cur = iter;
			++iter;
			(*cur)->OnSpecialMove(&player, specialMove);
		}
	}
}

void CPlayerStateJump::OnFullSerialize(TSerialize ser, CPlayer& player)
{
	ser.BeginGroup("StateJump");
	ser.Value("JumpLock", m_jumpLock);
	if (ser.IsReading())
	{
		EJumpState state = JState_None;
		ser.EnumValue("jumpState", state, JState_None, JState_Total);
		SetJumpState(player, state);
	}
	else
	{
		ser.EnumValue("jumpState", m_jumpState, JState_None, JState_Total);
	}
	ser.EndGroup();
}

void CPlayerStateJump::SetJumpState(CPlayer& player, EJumpState jumpState)
{
	if (jumpState != m_jumpState)
	{
		CRY_ASSERT(m_jumpState >= JState_None && m_jumpState < JState_Total);
		const EJumpState previousJumpState = m_jumpState;

		m_jumpState = jumpState;

		if (IActionController* actionController = player.GetAnimatedCharacter()->GetActionController())
		{
			if (!m_jumpAction && !player.IsAIControlled())
			{
				FragmentID fragID = FRAGMENT_ID_INVALID;
				switch (jumpState)
				{
				case JState_Jump:
					fragID = PlayerMannequin.fragmentIDs.MotionJump;
					break;
				case JState_Falling:
					fragID = PlayerMannequin.fragmentIDs.MotionInAir;
					break;
				}
				if (fragID != FRAGMENT_ID_INVALID)
				{
					m_jumpAction = new CPlayerJump(fragID, PP_PlayerAction);
					m_jumpAction->AddRef();
					actionController->Queue(*m_jumpAction);
				}
			}
		}
	}
}

CPlayerStateJump::EJumpState CPlayerStateJump::GetJumpState() const
{
	return m_jumpState;
}

void CPlayerStateJump::Landed(CPlayer& player, const bool isHeavyWeapon, float fallSpeed)
{
#ifdef STATE_DEBUG
	bool remoteControlled = false;
	IVehicle* pVehicle = player.GetLinkedVehicle();
	if (pVehicle)
	{
		IVehicleSeat* pVehicleSeat = pVehicle->GetSeatForPassenger(player.GetEntityId());
		if (pVehicleSeat && pVehicleSeat->IsRemoteControlled())
		{
			remoteControlled = true;
		}
	}
	CRY_ASSERT_MESSAGE(player.GetLinkedEntity() == NULL || remoteControlled, "Cannot 'land' when you're linked to another entity!");
#endif

	const SPlayerStats& stats = player.m_stats;

	Vec3 playerPosition = player.GetEntity()->GetWorldPos();
	IPhysicalEntity* phys = player.GetEntity()->GetPhysics();
	IMaterialEffects* mfx = gEnv->pGameFramework->GetIMaterialEffects();

	const SActorPhysics& actorPhysics = player.GetActorPhysics();
	int matID = actorPhysics.groundMaterialIdx != -1 ? actorPhysics.groundMaterialIdx : mfx->GetDefaultSurfaceIndex();

	const float fHeightofEntity = playerPosition.z;
	const float worldWaterLevel = player.m_playerStateSwim_WaterTestProxy.GetWaterLevel();

	TMFXEffectId effectId = mfx->GetEffectId("bodyfall", matID);
	if (effectId != InvalidEffectId)
	{
		SMFXRunTimeEffectParams params;
		params.pos = playerPosition;
		float landFallParamVal = (float)__fsel(-(fallSpeed - 7.5f), 0.25f, 0.75f);
		params.AddAudioRtpc("landfall", landFallParamVal);

		const float speedParamVal = min(fabsf((actorPhysics.velocity.z * 0.1f)), 1.0f);
		params.AddAudioRtpc("speed", speedParamVal);

		mfx->ExecuteEffect(effectId, params);
	}

	bool heavyLanded = false;

	IItem* pCurrentItem = player.GetCurrentItem();
	CWeapon* pCurrentWeapon = pCurrentItem ? static_cast<CWeapon*>(pCurrentItem->GetIWeapon()) : NULL;

	if (fallSpeed > 0.0f && player.IsPlayer())
	{
		if (!gEnv->bMultiplayer)
		{
			const float verticalSpeed = fabs(fallSpeed);
			const float speedForHeavyLand = g_pGameCVars->pl_health.fallSpeed_HeavyLand;
			if ((verticalSpeed >= speedForHeavyLand) && (player.GetPickAndThrowEntity() == 0) && !player.IsDead())
			{
				if (!isHeavyWeapon)
				{
					if (pCurrentWeapon)
					{
						pCurrentWeapon->FumbleGrenade();
						pCurrentWeapon->CancelCharge();
					}

					player.StartInteractiveActionByName("HeavyLand", false);
				}
				heavyLanded = true;
			}
		}
	}

	if (player.m_isClient)
	{
		if (fallSpeed > 0.0f)
		{
			const float fallIntensityMultiplier = stats.wasHit ? g_pGameCVars->pl_fall_intensity_hit_multiplier : g_pGameCVars->pl_fall_intensity_multiplier;
			const float fallIntensityMax = g_pGameCVars->pl_fall_intensity_max;
			const float fallTimeMultiplier = g_pGameCVars->pl_fall_time_multiplier;
			const float fallTimeMax = g_pGameCVars->pl_fall_time_max;
			const float zoomMultiplayer = (pCurrentWeapon && pCurrentWeapon->IsZoomed()) ? 0.2f : 1.0f;
			const float direction = (cry_random(0, 1) == 0) ? -1.0f : 1.0f;
			const float intensity = clamp_tpl(fallIntensityMultiplier * fallSpeed * zoomMultiplayer, 0.0f, fallIntensityMax);
			const float shakeTime = clamp_tpl(fallTimeMultiplier * fallSpeed * zoomMultiplayer, 0.0f, fallTimeMax);
			const Vec3 rotation = Vec3(-0.5f, 0.15f * direction, 0.05f * direction);

			if (CScreenEffects* pGameScreenEffects = g_pGame->GetScreenEffects())
			{
				pGameScreenEffects->CamShake(rotation * intensity, Vec3(0, 0, 0), shakeTime, shakeTime, 0.05f, CScreenEffects::eCS_GID_Player);
			}

			IForceFeedbackSystem* pForceFeedback = g_pGame->GetIGameFramework()->GetIForceFeedbackSystem();
			assert(pForceFeedback);

			ForceFeedbackFxId fxId = pForceFeedback->GetEffectIdByName("landFF");
			pForceFeedback->PlayForceFeedbackEffect(fxId, SForceFeedbackRuntimeParams(intensity, 0.0f));

			if (fallSpeed > 7.0f)
			{
				player.PlaySound(CPlayer::ESound_Fall_Drop);
			}

			CPlayer::EPlayerSounds playerSound = heavyLanded ? CPlayer::ESound_Gear_HeavyLand : CPlayer::ESound_Gear_Land;
			player.PlaySound(playerSound, true);
		}
		CCCPOINT(PlayerMovement_LocalPlayerLanded);
	}

	if (IPerceptionManager::GetInstance())
	{
		// Notify AI
		//If silent feet active, ignore here
		const float noiseSupression = 0.0f;
		const float fAISoundRadius = (g_pGameCVars->ai_perception.landed_baseRadius + (g_pGameCVars->ai_perception.landed_speedMultiplier * fallSpeed)) * (1.0f - noiseSupression);
		SAIStimulus stim(AISTIM_SOUND, AISOUND_MOVEMENT_LOUD, player.GetEntityId(), 0,
		                 player.GetEntity()->GetWorldPos() + player.GetEyeOffset(), ZERO, fAISoundRadius);
		IPerceptionManager::GetInstance()->RegisterStimulus(stim);
	}

	// Record 'Land' telemetry stats.

	CStatsRecordingMgr::TryTrackEvent(&player, eGSE_Land, fallSpeed);

	if (fallSpeed > 0.0f)
	{
		player.CreateScriptEvent(heavyLanded ? "heavylanded" : "landed", stats.fallSpeed);
	}
}

const Vec3 CPlayerStateJump::CalculateInAirJumpExtraVelocity(const CPlayer& player, const Vec3& desiredVelocity) const
{
	const SPlayerStats& stats = player.m_stats;
	const float speedUpFactor = 0.175f;

	Vec3 jumpExtraVelocity(0.0f, 0.0f, 0.0f);

	const SActorPhysics& actorPhysics = player.GetActorPhysics();
	if (actorPhysics.velocity.z > 0.0f)
	{
		//Note: Desired velocity is flat (not 'z' component), so jumpHeight should not be altered
		jumpExtraVelocity = desiredVelocity * speedUpFactor;
	}
	else
	{
		//Note: this makes the jump feel less 'floaty', by accelerating the player slightly down
		//      and compensates the extra traveled distance when going up
		const float g = actorPhysics.gravity.len();
		if (g > 0.0f)
		{
			const float jumpHeightScale = 1.0f;
			const float jumpHeight = player.m_params.jumpHeight * jumpHeightScale;

			const float estimatedLandTime = sqrt_tpl(2.0f * jumpHeight * (1.0f / g)) * (1.0f - speedUpFactor);
			assert(estimatedLandTime > 0.0f);
			if (estimatedLandTime > 0.0f)
			{
				const float requiredGravity = (2.0f * jumpHeight) / (estimatedLandTime * estimatedLandTime);
				const float initialAccelerationScale = clamp_tpl((-actorPhysics.velocity.z * 0.6f), 0.0f, 1.0f);
				jumpExtraVelocity = (requiredGravity - g) * actorPhysics.gravity.GetNormalized() * initialAccelerationScale;
			}
		}
	}

	return jumpExtraVelocity;
}

void CPlayerStateJump::UpdateJumping(CPlayer& player, const bool isHeavyWeapon, const SActorFrameMovementParams& movement, float frameTime)
{
	Vec3 desiredVel(ZERO);
	if (UpdateCommon(player, isHeavyWeapon, movement, frameTime, &desiredVel))
	{
		Vec3 jumpExtraForce(0, 0, 0);
		switch (GetJumpState())
		{
		case JState_Jump:
			jumpExtraForce = CalculateInAirJumpExtraVelocity(player, desiredVel);
			break;
		}
		desiredVel += jumpExtraForce;

		FinalizeVelocity(player, desiredVel);

		if (CPlayerStateUtil::IsOnGround(player))
		{
			Land(player, isHeavyWeapon, frameTime);
		}
	}
}

void CPlayerStateJump::UpdateFalling(CPlayer& player, const bool isHeavyWeapon, const SActorFrameMovementParams& movement, float frameTime)
{
	Vec3 desiredVel(ZERO);

	if (!CPlayerStateUtil::IsOnGround(player))
	{
		if (UpdateCommon(player, isHeavyWeapon, movement, frameTime, &desiredVel))
		{
			FinalizeVelocity(player, desiredVel);
		}
	}
	else
	{
		// We were falling, now we're on ground - we landed!
		Land(player, isHeavyWeapon, frameTime);
	}
}

bool CPlayerStateJump::UpdateCommon(CPlayer& player, const bool isHeavyWeapon, const SActorFrameMovementParams& movement, float frameTime, Vec3* pDesiredVel)
{
	Vec3 move(ZERO);
	const bool bigWeaponRestrict = isHeavyWeapon;
	CPlayerStateUtil::CalculateGroundOrJumpMovement(player, movement, bigWeaponRestrict, move);

	return UpdateCommon(player, isHeavyWeapon, move, frameTime, pDesiredVel);
}

bool CPlayerStateJump::UpdateCommon(CPlayer& player, const bool isHeavyWeapon, const Vec3& move, float frameTime, Vec3* pDesiredVel)
{
	GetDesiredVelocity(move, player, pDesiredVel);

	const SActorPhysics& actorPhysics = player.GetActorPhysics();

	// generate stats.
	if (actorPhysics.velocity * actorPhysics.gravity > 0.0f)
	{
		const float fHeightofEntity = player.GetEntity()->GetWorldTM().GetTranslation().z;
		m_startFallingHeight = (float)__fsel(-player.m_stats.fallSpeed, fHeightofEntity, max(m_startFallingHeight, fHeightofEntity));
		player.m_stats.fallSpeed = -actorPhysics.velocity.z;
	}

	if (!gEnv->bMultiplayer && player.IsInPickAndThrowMode() && (player.m_stats.fallSpeed > 10.f))
		player.ExitPickAndThrow();

	// inAir is set to 0.0f if we're swimming later - before refactoring this test happened *after* that, hence this test is here.
	m_jumpLock = (float)__fsel(-fabsf(player.m_stats.inAir), max(0.0f, m_jumpLock - frameTime), m_jumpLock);

	return true;
}

void CPlayerStateJump::FinalizeVelocity(CPlayer& player, const Vec3& newVelocity)
{
	const float fNewSpeed = newVelocity.len();

	const float fVelocityMultiplier = (float)__fsel(fNewSpeed - 22.0f, __fres(fNewSpeed + FLT_EPSILON) * 22.0f, 1.0f);

	// TODO: Maybe we should tell physics about this new velocity ? Or maybe SPlayerStats::velocity ? (stephenn).
	player.GetMoveRequest().velocity = newVelocity * fVelocityMultiplier;
}

void CPlayerStateJump::Land(CPlayer& player, const bool isHeavyWeapon, float frameTime)
{
	if (gEnv->bMultiplayer && IsJumping())
	{
		m_jumpLock = min(g_pGameCVars->pl_jump_baseTimeAddedPerJump + (m_jumpLock * g_pGameCVars->pl_jump_currentTimeMultiplierOnJump), g_pGameCVars->pl_jump_maxTimerValue);
	}

	const float fHeightofEntity = player.GetEntity()->GetWorldPos().z;

	if (player.IsClient())
	{
		CPlayerStateUtil::ApplyFallDamage(player, m_startFallingHeight, fHeightofEntity);
	}

	// TODO: Physics sync.
	const float fallSpeed = player.m_stats.fallSpeed;
	Landed(player, isHeavyWeapon, fabsf(player.GetActorPhysics().velocityDelta.z)); // fallspeed might be incorrect on a dedicated server (pos is synced from client, but also smoothed).

	player.m_stats.wasHit = false;

	SetJumpState(player, JState_None);

	if (player.m_stats.fallSpeed)
	{
		player.m_stats.fallSpeed = 0.0f;

		const float worldWaterLevel = player.m_playerStateSwim_WaterTestProxy.GetWaterLevel();
		if (fHeightofEntity < worldWaterLevel)
		{
			player.CreateScriptEvent("jump_splash", worldWaterLevel - fHeightofEntity);
		}

	}
}

void CPlayerStateJump::GetDesiredVelocity(const Vec3& move, const CPlayer& player, Vec3* pDesiredVel) const
{
	// Generate jump velocity.
	const float fMaxMove = 1.0f;

	float fGroundNormalZ = fMaxMove;

	if (move.len2() > 0.01f)
	{
		const Matrix34A baseMtx = Matrix34A(player.GetBaseQuat());
		Matrix34A baseMtxZ(baseMtx * Matrix33::CreateScale(Vec3(0, 0, 1)));
		baseMtxZ.SetTranslation(Vec3(0,0,0));

		Vec3 vDesiredVel(ZERO);
		if (player.IsRemote())
		{
			vDesiredVel = move;
		}
		else
		{
			Vec3 vVelocity = player.GetActorPhysics().velocity;

			const float fDiffMultiplier = 0.3f;
			const float fMaxDiff = 0.1f;
			const float fMinMove = 0.5f;

			Vec3 vMoveFlat = move - baseMtxZ * move;
			Vec3 vCurrVelFlat = vVelocity - baseMtxZ * vVelocity;

			float fCurrVelSizeSq = vCurrVelFlat.GetLengthSquared();

			Vec3 vMoveFlatNormalized = vMoveFlat.GetNormalized();
			Vec3 vCurDirFlatTemp = vCurrVelFlat.GetNormalized();

			Vec3 vCurVelFlatNormalized = fCurrVelSizeSq <= 0.0f ? Vec3(ZERO) : vCurDirFlatTemp;

			float fDot = vMoveFlatNormalized | vCurVelFlatNormalized;

			Vec3 vScaledMoveFlat = vMoveFlat * clamp(fDot, fMinMove, fMaxMove);
			float fMoveMult = max(abs(fDot) * fDiffMultiplier, fMaxDiff);

			Vec3 vReducedMove = (vMoveFlat - vCurrVelFlat) * fMoveMult;

			vDesiredVel = fDot < 0.0f ? vReducedMove : vScaledMoveFlat;

			float fDesiredVelSizeSq = vDesiredVel.GetLengthSquared();

			Vec3 vDesiredVelNorm = vDesiredVel.GetNormalized();

			Vec3 vClampedVel = vDesiredVelNorm * max(1.5f, sqrt(fCurrVelSizeSq));

			vDesiredVel = fDesiredVelSizeSq < fCurrVelSizeSq ? vDesiredVel : vClampedVel;
		}

		*pDesiredVel = vDesiredVel;
	}
}
