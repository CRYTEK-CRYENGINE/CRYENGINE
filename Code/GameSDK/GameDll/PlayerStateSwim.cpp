// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "PlayerStateSwim.h"
#include "Player.h"
#include "PlayerStateUtil.h"
#include "PlayerStateEvents.h"
#include "PlayerStateJump.h"
#include "LocalPlayerComponent.h"

#include "Weapon.h"

#include "GameRules.h"
#include "GameCVars.h"

#include "StatsRecordingMgr.h"

#include "MovementAction.h"
#include "PlayerCamera.h"

static const int s_maxWaterVolumesToConsider = 2;

CPlayerStateSwim::CSwimmingParams CPlayerStateSwim::s_swimParams;

void CPlayerStateSwim::CSwimmingParams::SetParamsFromXml(const IItemParamsNode* pParams)
{
	const IItemParamsNode* pSwimParams = pParams->GetChild("SwimmingParams");
	if (pSwimParams)
	{
		const IItemParamsNode* pSprintNode = pSwimParams->GetChild("Sprint");
		if (pSprintNode)
		{
			pSprintNode->GetAttribute("swimSpeedSprintSpeedMul", m_swimSpeedSprintSpeedMul);
			pSprintNode->GetAttribute("swimUpSprintSpeedMul", m_swimUpSprintSpeedMul);
			pSprintNode->GetAttribute("swimSprintSpeedMul", m_swimSprintSpeedMul);
			pSprintNode->GetAttribute("stateSwim_animCameraFactor", m_stateSwim_animCameraFactor);
		}
		const IItemParamsNode* pDolphinNode = pSwimParams->GetChild("Dolphin");
		if (pDolphinNode)
		{
			pDolphinNode->GetAttribute("swimDolphinJumpDepth", m_swimDolphinJumpDepth);
			pDolphinNode->GetAttribute("swimDolphinJumpThresholdSpeed", m_swimDolphinJumpThresholdSpeed);
			pDolphinNode->GetAttribute("swimDolphinJumpSpeedModification", m_swimDolphinJumpSpeedModification);
		}
	}
}

CryAudio::ControlId CPlayerStateSwim::m_submersionDepthParam = CryAudio::InvalidControlId;
float CPlayerStateSwim::m_previousSubmersionDepth = 0.0f;

CPlayerStateSwim::CPlayerStateSwim()
	: m_gravity(ZERO)
	, m_headUnderWaterTimer(-1.0f)
	, m_lastWaterLevel(0.0f)
	, m_lastWaterLevelTime(0.0f)
	, m_verticalVelDueToSurfaceMovement(0.0f)
	, m_onSurface(false)
	, m_bStillDiving(false)
{
	m_submersionDepthParam = CryAudio::StringToId("submersion_depth");
}

bool CPlayerStateSwim::OnPrePhysicsUpdate(CPlayer& player, const SActorFrameMovementParams& movement, float frameTime)
{
	const CPlayerStateSwim_WaterTestProxy& waterProxy = player.m_playerStateSwim_WaterTestProxy;

	CPlayerStateUtil::PhySetFly(player);

	const SPlayerStats& stats = player.m_stats;

#ifdef STATE_DEBUG
	const bool debug = (g_pGameCVars->cl_debugSwimming != 0);
#endif

	const Vec3 entityPos = player.GetEntity()->GetWorldPos();
	const Quat baseQuat = player.GetBaseQuat();
	const Vec3 vRight(baseQuat.GetColumn0());

	Vec3 velocity = player.GetActorPhysics().velocity;

	// Underwater timer, sounds update and surface wave speed.
	if (waterProxy.IsHeadUnderWater())
	{
		m_headUnderWaterTimer += frameTime;
		if (m_headUnderWaterTimer < 0.0f && !m_onSurface)
		{
			player.PlaySound(CPlayer::ESound_DiveIn, true, nullptr, velocity.len());
			m_headUnderWaterTimer = 0.0f;
			m_bStillDiving = true;
		}
	}
	else
	{
		m_headUnderWaterTimer -= frameTime;
		if (m_headUnderWaterTimer > 0.0f && (waterProxy.IsHeadComingOutOfWater() || m_onSurface))
		{
			player.PlaySound(CPlayer::ESound_DiveOut, true, nullptr, velocity.len());
			m_headUnderWaterTimer = 0.0f;
			m_bStillDiving = false;
		}
	}
	m_headUnderWaterTimer = clamp_tpl(m_headUnderWaterTimer, -0.2f, 0.2f);

	// Apply water flow velocity to the player
	Vec3 gravity;
	pe_params_buoyancy buoyancy[s_maxWaterVolumesToConsider];
	if (int count = gEnv->pPhysicalWorld->CheckAreas(entityPos, gravity, &buoyancy[0], s_maxWaterVolumesToConsider))
	{
		for (int i = 0; i < count && i < s_maxWaterVolumesToConsider; ++i)
		{
			// 0 is water
			if (buoyancy[i].iMedium == 0)
			{
				velocity += (buoyancy[i].waterFlow * frameTime);
			}
		}
	}

	// Calculate desired acceleration (user input)
	Vec3 desiredWorldVelocity(ZERO);

	Vec3 acceleration(ZERO);
	{
		Vec3 desiredLocalNormalizedVelocity(ZERO);
		Vec3 desiredLocalVelocity(ZERO);

		const Quat viewQuat = player.GetViewQuat();

		const float backwardMultiplier = (float)__fsel(movement.desiredVelocity.y, 1.0f, g_pGameCVars->pl_swimBackSpeedMul);
		desiredLocalNormalizedVelocity.x = movement.desiredVelocity.x * g_pGameCVars->pl_swimSideSpeedMul;
		desiredLocalNormalizedVelocity.y = movement.desiredVelocity.y * backwardMultiplier;

		float sprintMultiplier = 1.0f;
		if ((player.IsSprinting()) && !player.IsCinematicFlagActive(SPlayerStats::eCinematicFlag_RestrictMovement))
		{
			sprintMultiplier = GetSwimParams().m_swimSprintSpeedMul;

			// Higher speed multiplier when sprinting while looking up
			const float upFraction = clamp_tpl(viewQuat.GetFwdZ(), 0.0f, 1.0f);
			sprintMultiplier *= LERP(1.0f, GetSwimParams().m_swimUpSprintSpeedMul, upFraction);
		}

		const float baseSpeed = player.GetStanceMaxSpeed(STANCE_SWIM);
		desiredLocalVelocity.x = desiredLocalNormalizedVelocity.x * sprintMultiplier * baseSpeed;
		desiredLocalVelocity.y = desiredLocalNormalizedVelocity.y * sprintMultiplier * baseSpeed;
		desiredLocalVelocity.z = desiredLocalNormalizedVelocity.z * g_pGameCVars->pl_swimVertSpeedMul * baseSpeed;

		// The desired movement is applied in view-space, not in entity-space, since entity does not necessarily pitch while swimming.
		desiredWorldVelocity += viewQuat.GetColumn0() * desiredLocalVelocity.x;
		desiredWorldVelocity += viewQuat.GetColumn1() * desiredLocalVelocity.y;

		// though, apply up/down in world space.
		desiredWorldVelocity.z += desiredLocalVelocity.z;

#ifdef STATE_DEBUG
		if (debug)
		{
			IRenderAuxText::DrawLabelF(entityPos - vRight * 1.5f + Vec3(0, 0, 0.8f), 1.5f, "BaseSpeed %1.3f", baseSpeed);
			IRenderAuxText::DrawLabelF(entityPos - vRight * 1.5f + Vec3(0, 0, 1.0f), 1.5f, "SprintMul %1.2f", sprintMultiplier);
			IRenderAuxText::DrawLabelF(entityPos - vRight * 1.5f + Vec3(0, 0, 0.6f), 1.5f, "MoveN[%1.3f, %1.3f, %1.3f]", desiredLocalNormalizedVelocity.x, desiredLocalNormalizedVelocity.y, desiredLocalNormalizedVelocity.z);
			IRenderAuxText::DrawLabelF(entityPos - vRight * 1.5f + Vec3(0, 0, 0.5f), 1.5f, "VeloL[%1.3f, %1.3f, %1.3f]", desiredLocalVelocity.x, desiredLocalVelocity.y, desiredLocalVelocity.z);
			IRenderAuxText::DrawLabelF(entityPos - vRight * 1.5f + Vec3(0, 0, 0.4f), 1.5f, "VeloW[%1.3f, %1.3f, %1.3f]", desiredWorldVelocity.x, desiredWorldVelocity.y, desiredWorldVelocity.z);
		}
#endif

		//Remove a bit of control when entering the water
		const float userControlFraction = (float)__fsel(0.3f - waterProxy.GetSwimmingTimer(), 0.2f, 1.0f);
		acceleration += desiredWorldVelocity * userControlFraction;
	}

	// Apply acceleration (frame-rate independent)
	const float accelerateDelayInv = 3.333f;
	velocity += acceleration * (frameTime * accelerateDelayInv);

#ifdef STATE_DEBUG
	if (debug)
	{
		IRenderAuxText::DrawLabelF(entityPos - vRight * 1.5f - Vec3(0, 0, 0.2f), 1.5f, " Axx[%1.3f, %1.3f, %1.3f]", acceleration.x, acceleration.y, acceleration.z);
	}
#endif

	//--------------------

	const float relativeWaterLevel = waterProxy.GetRelativeWaterLevel() + 0.1f;
	const float surfaceDistanceFraction = clamp_tpl(fabsf(relativeWaterLevel), 0.0f, 1.0f);
	float surfaceProximityInfluence = 1.0f - surfaceDistanceFraction;
	const float verticalVelocityFraction = clamp_tpl((fabsf(desiredWorldVelocity.z) - 0.3f) * 2.5f, 0.0f, 1.0f);
	surfaceProximityInfluence = surfaceProximityInfluence * (1.0f - verticalVelocityFraction);

	// Apply velocity dampening (frame-rate independent)
	Vec3 damping(ZERO);

	const float zSpeedPreDamping = velocity.z;

	{
		damping.x = fabsf(velocity.x);
		damping.y = fabsf(velocity.y);

		// Vertical damping is special, to allow jumping out of water with higher speed,
		// and also not sink too deep when falling down ito the water after jump or such.
		float zDamp = 1.0f + (6.0f * clamp_tpl((-velocity.z - 1.0f) * 0.333f, 0.0f, 1.0f));
		zDamp *= 1.0f - surfaceProximityInfluence;

		damping.z = fabsf(velocity.z) * zDamp;

		const float stopDelayInv = 3.333f;
		damping *= (frameTime * stopDelayInv);
		velocity.x = (float)__fsel((fabsf(velocity.x) - damping.x), (velocity.x - fsgnf(velocity.x) * damping.x), 0.0f);
		velocity.y = (float)__fsel((fabsf(velocity.y) - damping.y), (velocity.y - fsgnf(velocity.y) * damping.y), 0.0f);
		velocity.z = (float)__fsel((fabsf(velocity.z) - damping.z), (velocity.z - fsgnf(velocity.z) * damping.z), 0.0f);

		//Make sure you can not swim above the surface
		if ((relativeWaterLevel >= 0.0f) && (velocity.z > 0.0f))
		{
			velocity.z = 0.0f;
		}
	}

	// Decide if we're on the surface and therefore need to be kept there..
	if (relativeWaterLevel > -0.2f && relativeWaterLevel < 1.0f && fabs_tpl(zSpeedPreDamping) < 0.5f)
	{
		if (!waterProxy.IsHeadUnderWater())
		{
			m_onSurface = true;
		}
	}
	else
	{
		// we only leave the surface if the player moves, otherwise we try and keep the
		// player on the surface, even if they currently arent
		m_onSurface = false;
	}

	// Calculate and apply surface movement to the player.
	float speedDelta = 0.0f;
	if (m_onSurface)
	{
		const float newWaterLevel = waterProxy.GetWaterLevel();
		const float waterLevelDelta = clamp_tpl(newWaterLevel - m_lastWaterLevel, -1.0f, 1.0f);
		const float newCheckedTime = waterProxy.GetWaterLevelTimeUpdated();
		const float timeDelta = newCheckedTime - m_lastWaterLevelTime;

		if (timeDelta > FLT_EPSILON)
		{
			speedDelta = waterLevelDelta / timeDelta;

			velocity.z += speedDelta;
		}

		m_lastWaterLevel = newWaterLevel;
		m_lastWaterLevelTime = newCheckedTime;
	}

	// Set request type and velocity
	player.GetMoveRequest().type = eCMT_Fly;
	player.GetMoveRequest().velocity = velocity;

#ifdef STATE_DEBUG
	// DEBUG VELOCITY
	if (debug)
	{
		IRenderAuxText::DrawLabelF(entityPos - vRight * 1.5f - Vec3(0, 0, 0.0f), 1.5f, "Velo[%1.3f, %1.3f, %1.3f]", velocity.x, velocity.y, velocity.z);
		IRenderAuxText::DrawLabelF(entityPos - vRight * 1.5f - Vec3(0, 0, 0.4f), 1.5f, "Damp[%1.3f, %1.3f, %1.3f]", damping.x, damping.y, damping.z);
		IRenderAuxText::DrawLabelF(entityPos - vRight * 1.5f - Vec3(0, 0, 0.6f), 1.5f, "FrameTime %1.4f", frameTime);
		IRenderAuxText::DrawLabelF(entityPos - vRight * 1.5f + Vec3(0, 0, 0.3f), 1.5f, "DeltaSpeed[%1.3f]", speedDelta);
		//if (bNewSwimJumping)
		//IRenderAuxText::DrawLabel(entityPos - vRight * 0.15f + Vec3(0,0,0.6f), 2.0f, "JUMP");
	}
#endif

	return true;
}

void CPlayerStateSwim::OnEnter(CPlayer& player)
{
	m_headUnderWaterTimer = -1.0f;
	player.m_playerStateSwim_WaterTestProxy.OnEnterWater(player);

	IPhysicalEntity* pPhysEnt = player.GetEntity()->GetPhysics();
	if (pPhysEnt != nullptr)
	{
		// get current gravity before setting to zero.
		pe_player_dynamics simPar;
		if (pPhysEnt->GetParams(&simPar) != 0)
		{
			m_gravity = simPar.gravity;
		}
		CPlayerStateUtil::PhySetFly(player);
	}

	m_lastWaterLevel = player.m_playerStateSwim_WaterTestProxy.GetWaterLevel();
	m_lastWaterLevelTime = player.m_playerStateSwim_WaterTestProxy.GetWaterLevelTimeUpdated();

	player.m_stats.inAir = 0.0f;

	if (player.IsClient())
	{
		ICameraMode::AnimationSettings animationSettings;
		animationSettings.positionFactor = 1.0f;
		animationSettings.rotationFactor = GetSwimParams().m_stateSwim_animCameraFactor;

		player.GetPlayerCamera()->SetCameraModeWithAnimationBlendFactors(eCameraMode_PartialAnimationControlled, animationSettings, "Entering swim state");

		if (!player.IsCinematicFlagActive(SPlayerStats::eCinematicFlag_HolsterWeapon))
			player.HolsterItem(true);

		if (gEnv->bMultiplayer) // any left hand holding in SP?
		{
			player.HideLeftHandObject(true);
		}

		UpdateAudio(player);
		player.PlaySound(CPlayer::ESound_WaterEnter);
	}

	// Record 'Swim' telemetry stats.
	CStatsRecordingMgr::TryTrackEvent(&player, eGSE_Swim, true);
}

//------------------------------------------------------------------------
void CPlayerStateSwim::OnExit(CPlayer& player)
{
	m_headUnderWaterTimer = -1.0f;
	player.m_playerStateSwim_WaterTestProxy.OnExitWater(player);
	player.m_actorPhysics.groundNormal = Vec3(0, 0, 1);

	if (player.IsClient())
	{
		//check if the camera still exists
		if (player.m_playerCamera != nullptr)
		{
			player.GetPlayerCamera()->SetCameraMode(eCameraMode_Default, "Leaving swim state");
		}

		//Unselect underwater weapon here
		if (!player.IsCinematicFlagActive(SPlayerStats::eCinematicFlag_HolsterWeapon))
		{
			player.HolsterItem(false);
		}

		if (gEnv->bMultiplayer) // any left hand holding in SP?
		{
			player.HideLeftHandObject(false);
		}

		UpdateAudio(player);

		if (m_bStillDiving)
		{
			Vec3 const& velocity = player.GetActorPhysics().velocity;
			player.PlaySound(CPlayer::ESound_DiveOut, true, nullptr, velocity.len());
			m_bStillDiving = false;
		}

		player.PlaySound(CPlayer::ESound_WaterExit);
	}

	IPhysicalEntity* pPhysEnt = player.GetEntity()->GetPhysics();
	if (pPhysEnt != nullptr)
	{
		CPlayerStateUtil::PhySetNoFly(player, m_gravity);
	}

	// Record 'Swim' telemetry stats.

	CStatsRecordingMgr::TryTrackEvent(&player, eGSE_Swim, false);
}

void CPlayerStateSwim::OnUpdate(CPlayer& player, float frameTime)
{
	// DT 12908: swim animation not playing after cutscene.
	//           due to "NoWeapon" not being equipped as it hasn't been loaded into the inventory yet.
	//           (Part 2).
	// this function is called until "NoWeapon" is equipped.
	if (player.IsClient())
	{
		const float breathingInterval = 5.0f;
		player.m_pPlayerTypeComponent->UpdateSwimming(frameTime, breathingInterval);
	}
}

bool CPlayerStateSwim::DetectJump(CPlayer& player, const SActorFrameMovementParams& movement, float frameTime, float* pVerticalSpeedModifier) const
{
	const float minInWaterTime = 0.35f;
	const CPlayerStateSwim_WaterTestProxy& waterProxy = player.m_playerStateSwim_WaterTestProxy;
	const bool allowJump = waterProxy.GetSwimmingTimer() > minInWaterTime;
	if (allowJump)
	{
		// we broke the surface at a velocity enough to dolphin jump.
		if (!m_onSurface && (waterProxy.GetRelativeWaterLevel() > -GetSwimParams().m_swimDolphinJumpDepth))
		{
			const float velZ = player.GetActorPhysics().velocity.z;

			if ((velZ > GetSwimParams().m_swimDolphinJumpThresholdSpeed))
			{
				if (pVerticalSpeedModifier)
				{
					*pVerticalSpeedModifier = GetSwimParams().m_swimDolphinJumpSpeedModification;
				}

				return true;
			}
		}
	}

	return false;
}

void CPlayerStateSwim::UpdateAudio(CPlayer const& player)
{
	if (m_submersionDepthParam != CryAudio::InvalidControlId)
	{
		float submersionDepth = 0.0f;

		if (player.m_playerStateSwim_WaterTestProxy.IsInWater())
		{
			submersionDepth = fabs_tpl(player.m_playerStateSwim_WaterTestProxy.GetRelativeWaterLevel());
		}

		if (fabs_tpl(submersionDepth - m_previousSubmersionDepth) > 0.1f || submersionDepth == 0.0f)
		{
			m_previousSubmersionDepth = submersionDepth;
			gEnv->pAudioSystem->SetParameter(m_submersionDepthParam, submersionDepth);
		}
	}
}
