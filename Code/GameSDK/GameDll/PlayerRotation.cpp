// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "PlayerRotation.h"
#include "Player.h"
#include "GameCVars.h"
#include "Weapon.h"
#include "GameRules.h"
#include "PlayerInput.h"
#include "AutoAimManager.h"
#include "PlayerVisTable.h"
#include "IVehicleSystem.h"
#include "Utility/CryWatch.h"

#if !defined(_RELEASE)
	#define ENABLE_NAN_CHECK
#endif

#ifdef ENABLE_NAN_CHECK
#define PR_CHECKQNAN_FLT(x) \
	assert(((*(unsigned*)&(x))&0xff000000) != 0xff000000u && (*(unsigned*)&(x) != 0x7fc00000))
#else
#define PR_CHECKQNAN_FLT(x) (void*)0
#endif

#define PR_CHECKQNAN_VEC(v) \
	 PR_CHECKQNAN_FLT(v.x); PR_CHECKQNAN_FLT(v.y); PR_CHECKQNAN_FLT(v.z)

/*#define PR_CHECKQNAN_MAT33(v) \
	PR_CHECKQNAN_VEC(v.GetColumn(0)); \
	PR_CHECKQNAN_VEC(v.GetColumn(1)); \
	PR_CHECKQNAN_VEC(v.GetColumn(2))
*/

#define PR_CHECKQNAN_QUAT(q) \
	PR_CHECKQNAN_VEC(q.v); \
	PR_CHECKQNAN_FLT(q.w)

//-----------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------
CPlayerRotation::CPlayerRotation( const CPlayer& player ) : 
	m_player(player),
	m_viewRoll(0.0f),
	m_peekOverAmount(0.0f),
	m_angularImpulseTime(0.0f),
	m_angularImpulseDeceleration(0.0f),
	m_angularImpulseTimeMax(0.0f),
	m_leanAmount(0.0f),
	m_currently_snapping(false),
	m_angularImpulse(ZERO),
	m_angularImpulseDelta(ZERO),
	m_viewAngles(ZERO),
	m_viewQuat(IDENTITY),
	m_viewQuatFinal(IDENTITY),
	m_baseQuat(IDENTITY),
	m_baseQuatLinked(IDENTITY),
	m_viewQuatLinked(IDENTITY),
	m_follow_target_id(0),
	m_snap_target_id(0),
	m_deltaAngles(ZERO),
	m_forceLookVector(ZERO),
	m_externalAngles(ZERO),
	m_bForcedLookAtBlendingEnabled(true),
	m_minAngleTarget(std::numeric_limits<float>::infinity()),
	m_minAngleRate(0.0f),
	m_minAngle(0.0f),
	m_maxAngleTarget(0.0f),
	m_maxAngleRate(0.0f),
	m_maxAngle(0.0f)
{
	m_snap_target_dir.Set( 0,1,0 );
	m_follow_target_dir.Set( 0,1,0 );
	m_frameViewAnglesOffset.Set(0.0f, 0.0f, 0.0f);
}

void CPlayerRotation::FullSerialize( TSerialize ser )
{
	ser.BeginGroup( "PlayerRotation" );
	ser.Value( "viewAngles" , m_viewAngles );
	ser.Value( "leanAmount", m_leanAmount );
	ser.Value( "viewRoll", m_viewRoll );

	//[AlexMcC|19.03.10]: TODO: delete these once we stop reviving players on quick load!
	// When we don't revive, these are overwritten by a calculation that reads from m_viewAngles,
	// which we serialize above.
	ser.Value( "viewQuat", m_viewQuat );
	ser.Value( "viewQuatFinal", m_viewQuatFinal );
	ser.Value( "baseQuat", m_baseQuat );
	ser.EndGroup();
}

IItem* CPlayerRotation::GetCurrentItem(bool includeVehicle)
{
	IItem * pCurrentItem = m_player.GetCurrentItem();
	IVehicle* pVehicle = m_player.GetLinkedVehicle();
	if (pVehicle && includeVehicle)
	{
		if (EntityId weaponId = pVehicle->GetCurrentWeaponId(m_player.GetEntity()->GetId())) 
		{
			if (IItem* pItem = gEnv->pGameFramework->GetIItemSystem()->GetItem(weaponId))    
			{
				if (IWeapon* pWeapon = pItem->GetIWeapon())
				{
					pCurrentItem = pItem;
				}
				else
				{
					return NULL;
				}
			}
		}
	}
	return pCurrentItem;
}


#if TALOS
static void GetTalosInput(CPlayerRotation * pPlayerRotation, const CPlayer& rPlayer, float& x, float& z, const Vec3 playerView[4], float fFrameTime)
{
	//Do I need to reproject the view to actually get the positioning correct? Shouldn't be.
	const Vec3 playerFwd = playerView[1];
	const Vec3 playerRgt = playerView[0];
	const Vec3 playerUp = playerView[2];
	const Vec3 playerViewPos = playerView[3];


	Vec3 playerPos = playerViewPos;

	IPhysicalEntity * pPhysicalEntity = rPlayer.GetEntity()->GetPhysics();
	if(pPhysicalEntity)
	{
		pe_status_dynamics dyn_status;
		pPhysicalEntity->GetStatus(&dyn_status);		
		playerPos = playerViewPos + (dyn_status.v * fFrameTime * 2.0f);
	}

	Vec3 follow_target_dir(ZERO);

	EntityId follow_target_id = 0;
	static EntityId s_follow_target_id = 0;

	CGameRules * pGameRules = g_pGame->GetGameRules();
	int playerTeam = pGameRules->GetTeam(rPlayer.GetEntity()->GetId());
	float cloakedPlayerMultiplier = g_pGameCVars->pl_aim_cloaked_multiplier;
	const bool multipleTeams = pGameRules->GetTeamCount() > 0;

	const TAutoaimTargets& aaTargets = g_pGame->GetAutoAimManager().GetAutoAimTargets();
	const int targetCount = aaTargets.size();
	float fBestTargetDistance = FLT_MAX;

	for (int i = 0; i < targetCount; ++i)
	{
		const SAutoaimTarget& target = aaTargets[i];

		if(multipleTeams &&  (pGameRules->GetTeam(target.entityId) == playerTeam))
		{
			continue;
		}

		Vec3 targetPos = target.secondaryAimPosition;
		
		IEntity * pEntity = gEnv->pEntitySystem->GetEntity(target.entityId);
		if(pEntity)
		{
			IPhysicalEntity * pPhysicalEntity2 = pEntity->GetPhysics();
			if(pPhysicalEntity2)
			{
				pe_status_dynamics dyn_status;
				pPhysicalEntity2->GetStatus(&dyn_status);		
				targetPos = targetPos + (dyn_status.v * fFrameTime);
			}
		}

		Vec3 targetDistVec = (targetPos - playerPos);
		float distance = targetDistVec.GetLength();

		if (distance <= 0.01f)
			continue;

		Vec3 dirToTarget = targetDistVec / distance;

		// fast reject everything behind player, too far away or too near from line of view
		// sort rest by angle to crosshair and distance from player

		const int kAutoaimVisibilityLatency = 1;
		if (!g_pGame->GetPlayerVisTable()->CanLocalPlayerSee(target.entityId, kAutoaimVisibilityLatency))
		{
			// Since both player and target entities are ignored, and the ray still intersected something, there's something in the way.
			// Need to profile this and see if it's faster to do the below checks before doing the linetest. It's fairly expensive but
			// linetests generally are as well... - Richard
			continue;
		}

		const float angleDot = dirToTarget.dot(-playerRgt);
		const float angle = (RAD2DEG(acos_tpl(angleDot)) - 90.f);
		const float absAngle = fabsf(angle);

		const float angleDotV = playerUp.dot(dirToTarget);
		const float angleToTargetV = (RAD2DEG(acos_tpl(angleDotV)) - 90.f);
		const float absAngleV = fabsf(angleToTargetV);

		if ( s_follow_target_id == target.entityId )
		{
			follow_target_id = target.entityId;
			follow_target_dir = dirToTarget;
			break;
		}
		else if(distance < fBestTargetDistance)
		{
			fBestTargetDistance = distance;
			follow_target_id = target.entityId;
			follow_target_dir = dirToTarget;
		}
	}

	if(follow_target_id != 0)
	{
		//Player up is the normal of the plane that we are rotating around - (Correct? Or do we want to project both the player direction and the target direction onto the X/Y plane?)
		Vec3 vProjectedTargetHorz = playerUp.cross(follow_target_dir.cross(playerUp));
		Vec3 vProjectedTargetVert = playerRgt.cross(follow_target_dir.cross(playerRgt));

		float horzDot = vProjectedTargetHorz.GetNormalized().dot(playerFwd);
		float vertDot = vProjectedTargetVert.GetNormalized().dot(playerFwd);

		const float directionDotHorz = follow_target_dir.dot(playerRgt);
		const float directionDotVert = follow_target_dir.dot(playerUp);
		
		const float angle						= acos_tpl(horzDot);
		const float angleToTargetV	= acos_tpl(vertDot);		

		const float fHorzFinalAngle = (float)__fsel(directionDotHorz, -angle, angle);
		const float fVertFinalAngle = (float)__fsel(directionDotVert, angleToTargetV, -angleToTargetV);

		//CryWatch("Angle to target: %.6f", RAD2DEG(angle));
		//CryWatch("Final Angle to target: %.6f", RAD2DEG(fHorzFinalAngle));

		x = x + fVertFinalAngle;
		z = z + fHorzFinalAngle;
	}

	s_follow_target_id  = follow_target_id;

	return;
}
#endif

void CPlayerRotation::Process(IItem* pCurrentItem, const SActorFrameMovementParams& movement, const SAimAccelerationParams& verticalAcceleration, float frameTime)
{
	CRY_PROFILE_FUNCTION(PROFILE_GAME);

	// reset to the new impulse.
	m_angularImpulseDelta = m_angularImpulse;

	m_deltaAngles = movement.deltaAngles;

	PR_CHECKQNAN_FLT(m_deltaAngles);

	// Store the previous rotation to get the correct rotation for linked actors.
	const Quat previousBaseQuat = m_baseQuat;
	const Quat previousViewQuat = m_viewQuat;

	ProcessForcedLookDirection(m_viewQuatFinal, frameTime);

	ProcessAngularImpulses( frameTime );

	ProcessLeanAndPeek( movement );

	ProcessNormalRoll( frameTime );

	bool shouldProcessTargetAssistance = ShouldProcessTargetAssistance();
	if (shouldProcessTargetAssistance)
	{
		ProcessTargetAssistance( pCurrentItem );
	}

#if TALOS
	if(stricmp(g_pGameCVars->pl_talos->GetString(), m_player.GetEntity()->GetName()) == 0)
	{
		IMovementController* pMovementController = m_player.GetMovementController();
		CRY_ASSERT(pMovementController);
		SMovementState moveState;
		pMovementController->GetMovementState(moveState);

		Vec3 playerView[4] =
		{
			m_viewQuat.GetColumn0(), // Right
			m_viewQuat.GetColumn1(), // Forward
			m_viewQuat.GetColumn2(), // Up
			moveState.eyePosition    // Pos
		};

		GetTalosInput(this, m_player, m_deltaAngles.x, m_deltaAngles.z, playerView, frameTime);
	}
#endif

	float minAngle,maxAngle;
	GetStanceAngleLimits(verticalAcceleration, pCurrentItem, minAngle, maxAngle);
	ClampAngles( minAngle, maxAngle );

	ProcessNormal( frameTime );

	if(shouldProcessTargetAssistance)
	{
		IVehicle* pVehicle = m_player.GetLinkedVehicle();
		if (pVehicle && GetCurrentItem(true))
		{
			if (m_deltaAngles.x!=0.f)
				pVehicle->OnAction(eVAI_RotatePitchAimAssist, eAAM_Always, m_deltaAngles.x, m_player.GetEntity()->GetId());
			if (m_deltaAngles.z!=0.f)
				pVehicle->OnAction(eVAI_RotateYawAimAssist, eAAM_Always, m_deltaAngles.z, m_player.GetEntity()->GetId());
		}
	}

	//update freelook when linked to an entity
	ProcessLinkedState(m_player.m_linkStats, previousBaseQuat, previousViewQuat);

	//Recoil/Zoom sway offset for local player
	ProcessFinalViewEffects( minAngle, maxAngle );

	m_frameViewAnglesOffset.Set(0.0f, 0.0f, 0.0f);
	m_forceLookVector.zero();
	m_externalAngles.Set(0.f, 0.f, 0.f);

	NormalizeQuats();
}

void CPlayerRotation::NormalizeQuats()
{
	m_baseQuat.Normalize();
	m_viewQuat.Normalize();

	PR_CHECKQNAN_QUAT(m_baseQuat);
	PR_CHECKQNAN_QUAT(m_viewQuat);

	m_viewQuatFinal.Normalize();

	//Normalized when used in ProcessLinkedState( )
	//m_baseQuatLinked.Normalize();
	//m_viewQuatLinked.Normalize();
}

void CPlayerRotation::AddAngularImpulse(const Ang3 &angular,float deceleration,float duration)
{
	m_angularImpulse = angular;
	m_angularImpulseDelta = angular;
	m_angularImpulseDeceleration = deceleration;
	m_angularImpulseTimeMax = m_angularImpulseTime = duration;
}

void CPlayerRotation::SetViewRotation( const Quat &rotation )
{
	CRY_ASSERT( rotation.IsValid() );

	const Quat normalizedRotation = rotation.GetNormalized();

	m_viewQuat = normalizedRotation;
	m_viewQuatFinal = normalizedRotation;
	m_viewAngles.SetAnglesXYZ(normalizedRotation);
	m_baseQuat = Quat::CreateRotationZ(m_viewAngles.z).GetNormalized();
}

void CPlayerRotation::SetViewRotationAndKeepBaseOrientation( const Quat &rotation )
{
	CRY_ASSERT( rotation.IsValid() );

	const Quat normalizedRotation = rotation.GetNormalized();

	m_viewQuat = normalizedRotation;
	m_viewQuatFinal = normalizedRotation;
	m_viewAngles.SetAnglesXYZ(normalizedRotation);
}

void CPlayerRotation::SetViewRotationOnRevive( const Quat &rotation )
{
	SetViewRotation( rotation );
	m_viewAngles.x = 0.0f;
	m_viewAngles.y = 0.0f;
	m_viewRoll = 0.0f;

	// Reset angle transitions
	m_minAngleTarget = std::numeric_limits<float>::infinity();
}

void CPlayerRotation::ResetLinkedRotation( const Quat& rotation )
{
	CRY_ASSERT(	rotation.IsValid() );

	const Quat normalizedRotation = rotation.GetNormalized();

	m_viewQuatLinked = m_baseQuatLinked = normalizedRotation;
	m_viewQuatFinal  = m_viewQuat = m_baseQuat = normalizedRotation;
}

void CPlayerRotation::GetStanceAngleLimits( const SAimAccelerationParams& verticalAcceleration, IItem* pCurrentPlayerItem, float & minAngle,float & maxAngle)
{
	float fNewMinAngle = verticalAcceleration.angle_min;
	float fNewMaxAngle = verticalAcceleration.angle_max;

	CWeapon* pWeapon = GetCurrentWeapon( pCurrentPlayerItem );
	if (pWeapon)
	{
		float weaponMinAngle = fNewMinAngle;
		float weaponMaxAngle = fNewMaxAngle;
		pWeapon->GetAngleLimits(m_player.GetStance(), weaponMinAngle, weaponMaxAngle);
		fNewMinAngle = max(fNewMinAngle, weaponMinAngle);
		fNewMaxAngle = min(fNewMaxAngle, weaponMaxAngle);
	}

	// Smooth transition for up/down angle limits
	if (m_minAngleTarget == std::numeric_limits<float>::infinity()) 
	{
		m_maxAngle = m_maxAngleTarget = fNewMaxAngle;
		m_minAngle = m_minAngleTarget = fNewMinAngle;
		m_angleTransitionTimer = gEnv->pTimer->GetFrameStartTime();
		m_minAngleRate = 0;
		m_maxAngleRate = 0;
	}
	if (fNewMaxAngle != m_maxAngleTarget)
	{
		m_maxAngle = m_maxAngleTarget;
		m_maxAngleTarget = fNewMaxAngle;
		m_angleTransitionTimer = gEnv->pTimer->GetFrameStartTime();
		m_maxAngleRate = 0;
	}
	if (fNewMinAngle != m_minAngleTarget)
	{
		m_minAngle = m_minAngleTarget;
		m_minAngleTarget = fNewMinAngle;
		m_angleTransitionTimer = gEnv->pTimer->GetFrameStartTime();
		m_minAngleRate = 0;
	}
	const float transitionTime = g_pGameCVars->cl_angleLimitTransitionTime;
	float timer = gEnv->pTimer->GetFrameStartTime().GetDifferenceInSeconds(m_angleTransitionTimer);
	if (timer < transitionTime)
	{
		if (m_minAngle < m_minAngleTarget)
		{
			SmoothCD(m_minAngle, m_minAngleRate, timer, m_minAngleTarget, transitionTime);
			fNewMinAngle = m_minAngle;
		}
		if (m_maxAngle > m_maxAngleTarget)
		{
			SmoothCD(m_maxAngle, m_maxAngleRate, timer, m_maxAngleTarget, transitionTime);
			fNewMaxAngle = m_maxAngle;
		}
	}

	minAngle = DEG2RAD(fNewMinAngle);
	maxAngle = DEG2RAD(fNewMaxAngle);
}


//---------------------------------------------------------------
void CPlayerRotation::ProcessNormalRoll( float frameTime )
{
	//apply lean/roll
	float rollAngleGoal(0);
	const Vec3 velocity = m_player.GetActorPhysics().velocity;
	const float speed2( velocity.len2());

	if ((speed2 > 0.01f) && m_player.m_stats.inAir)
	{
		
		const float maxSpeed = m_player.GetStanceMaxSpeed(STANCE_STAND);
		const float maxSpeedInverse = (float)__fsel(-maxSpeed, 1.0f, __fres(maxSpeed + FLT_EPSILON));
	
		const Vec3 velVec = velocity * maxSpeedInverse;

		const float dotSide(m_viewQuat.GetColumn0() * velVec);

		rollAngleGoal -= DEG2RAD(dotSide * 1.5f);;
	}

	const float tempLean = m_leanAmount;
	const float leanAmountMultiplier = 3.0f;
	const float leanAmount = clamp_tpl(tempLean * leanAmountMultiplier, -1.0f, 1.0f);
	
	rollAngleGoal += DEG2RAD(leanAmount * m_player.m_params.leanAngle);
	Interpolate(m_viewRoll,rollAngleGoal,9.9f,frameTime);

	m_deltaAngles += m_angularImpulseDelta;
}

void CPlayerRotation::ProcessAngularImpulses( float frameTime )
{
	//update angular impulse
	if (m_angularImpulseTime>0.001f)
	{
		m_angularImpulse *= min(m_angularImpulseTime * __fres(m_angularImpulseTimeMax), 1.0f);
		m_angularImpulseTime -= frameTime;
	}
	else if (m_angularImpulseDeceleration>0.001f)
	{
		Interpolate(m_angularImpulse,ZERO,m_angularImpulseDeceleration, frameTime);
	}
	m_angularImpulseDelta -= m_angularImpulse;
}

void CPlayerRotation::ClampAngles( float minAngle, float maxAngle )
{
	//Cap up/down looking
	{
		const float currentViewPitch = GetLocalPitch();
		const float newPitch = clamp_tpl(currentViewPitch + m_deltaAngles.x, minAngle, maxAngle);
		m_deltaAngles.x = newPitch - currentViewPitch;
	}

	//Further limit the view if necessary
	{
		const SViewLimitParams& viewLimits = m_player.m_params.viewLimits;

		const Vec3  limitDir = viewLimits.GetViewLimitDir();

		if (limitDir.len2() < 0.1f)
			return;
		
		const float limitV = viewLimits.GetViewLimitRangeV();
		const float limitH = viewLimits.GetViewLimitRangeH();
		const float limitVUp = viewLimits.GetViewLimitRangeVUp();
		const float limitVDown = viewLimits.GetViewLimitRangeVDown();

		if ((limitH+limitV+fabsf(limitVUp)+fabsf(limitVDown)) > 0.0f)
		{
			//A matrix is built around the view limit, and then the player view angles are checked with it.
			//Later, if necessary the upVector could be made customizable.
			const Vec3 forward(limitDir);
			const Vec3 up(m_baseQuat.GetColumn2());
			const Vec3 right((-(up % forward)).GetNormalized());

			Matrix33 limitMtx;
			limitMtx.SetFromVectors(right,forward,right%forward);
			limitMtx.Invert();

			const Vec3 localDir(limitMtx * m_viewQuat.GetColumn1());

			Ang3 limit;

			if (limitV)
			{
				limit.x = asinf(localDir.z) + m_deltaAngles.x;

				const float deltaX(limitV - fabs(limit.x));

				m_deltaAngles.x = m_deltaAngles.x + (float)__fsel(deltaX, 0.0f, deltaX * (float)__fsel(limit.x, 1.0f, -1.0f));
			}

			if (limitVUp || limitVDown)
			{
				limit.x = asinf(localDir.z) + m_deltaAngles.x;

				const float deltaXUp(limitVUp - limit.x);
				float fNewDeltaX = m_deltaAngles.x;

				const float fDeltaXUpIncrement = (float)__fsel( deltaXUp, 0.0f, deltaXUp);
				fNewDeltaX = fNewDeltaX + (float)__fsel(-fabsf(limitVUp), 0.0f, fDeltaXUpIncrement);

				const float deltaXDown(limitVDown - limit.x);

				const float fDeltaXDownIncrement = (float)__fsel( deltaXDown, deltaXDown, 0.0f);
				fNewDeltaX = fNewDeltaX + (float)__fsel(-fabsf(limitVDown), 0.0f, fDeltaXDownIncrement);

				m_deltaAngles.x = fNewDeltaX;
			}

			if (limitH)
			{
				limit.z = atan2_tpl(-localDir.x,localDir.y) + m_deltaAngles.z;

				const float deltaZ(limitH - fabs(limit.z));

				m_deltaAngles.z = m_deltaAngles.z + (float)__fsel(deltaZ, 0.0f, deltaZ * (float)__fsel(limit.z, 1.0f, -1.0f));
			}
		}
	}
}

#define DBG_AUTO_AIM 0

#if DBG_AUTO_AIM

static void DrawDisc(const Vec3& center, Vec3 axis, float innerRadius, float outerRadius, const ColorB& innerColor, const ColorB& outerColor)
{
	axis.NormalizeSafe(Vec3Constants<float>::fVec3_OneZ);
	Vec3 u = ((axis * Vec3Constants<float>::fVec3_OneZ) > 0.5f) ? Vec3Constants<float>::fVec3_OneY : Vec3Constants<float>::fVec3_OneZ;
	Vec3 v = u.cross(axis);
	u = v.cross(axis);

	float sides = ceil_tpl(3.0f * (float)g_PI * outerRadius);
	//sides = 20.0f;
	float step = 1.0f / sides;
	for (float i = 0.0f; i < 0.99f; i += step)
	{
		float a0 = i * 2.0f * (float)g_PI;
		float a1 = (i+step) * 2.0f * (float)g_PI;
		Vec3 i0 = center + innerRadius * (u*cos_tpl(a0) + v*sin_tpl(a0));
		Vec3 i1 = center + innerRadius * (u*cos_tpl(a1) + v*sin_tpl(a1));
		Vec3 o0 = center + outerRadius * (u*cos_tpl(a0) + v*sin_tpl(a0));
		Vec3 o1 = center + outerRadius * (u*cos_tpl(a1) + v*sin_tpl(a1));
		gEnv->pRenderer->GetIRenderAuxGeom()->DrawTriangle(i0, innerColor, i1, innerColor, o1, outerColor);
		gEnv->pRenderer->GetIRenderAuxGeom()->DrawTriangle(i0, innerColor, o1, outerColor, o0, outerColor);
	}
}

#endif

void CPlayerRotation::TargetAimAssistance(CWeapon* pWeapon, float& followH, float& followV, float& scale, float& _fZoomAmount, const Vec3 playerView[4])
{
	CRY_PROFILE_FUNCTION(PROFILE_GAME);

	CRY_ASSERT(m_player.IsClient());

	followH = 0.0f;
	followV = 0.0f;
	scale = 1.0f;
	float bestScale = 1.0f;

	const Vec3 playerFwd = playerView[1];
	const Vec3 playerRgt = playerView[0];
	const Vec3 playerUp = playerView[2];
	const Vec3 playerPos = playerView[3];

	Vec3 follow_target_pos(ZERO);
	float follow_vote_leader = 0.0f;
	float snap_vote_leader = 0.0f;
	Vec3 follow_target_dir(ZERO);
	Vec3 snap_target_dir(ZERO);
	EntityId follow_target_id = 0;
	EntityId snap_target_id = 0;

	CGameRules * pGameRules = g_pGame->GetGameRules();
	float distance_follow_threshold_near	= max(0.0f, g_pGameCVars->aim_assistMinDistance);
	float distance_follow_threshold_far		= max(20.0f, g_pGameCVars->aim_assistMaxDistance);
	int playerTeam = pGameRules->GetTeam(m_player.GetEntity()->GetId());
	float cloakedPlayerMultiplier = g_pGameCVars->pl_aim_cloaked_multiplier;
	const bool multipleTeams = pGameRules->GetTeamCount() > 0;
	const float fFollowFalloffDist = g_pGameCVars->aim_assistFalloffDistance + FLT_EPSILON*g_pGameCVars->aim_assistFalloffDistance;
	const bool playerIsScoped = m_player.GetActorStats()->isScoped;

	float minTurnScale, fAimAssistStrength, fMaxDistMult;

	if(pWeapon)
	{
		const float fZoomAmount = pWeapon->GetZoomTransition();
		_fZoomAmount = fZoomAmount;
		
		const float fStrength						= g_pGameCVars->aim_assistStrength;
		const float fStrengthIronSight              = playerIsScoped ? g_pGameCVars->aim_assistStrength_SniperScope : g_pGameCVars->aim_assistStrength_IronSight;
		const float fDiff								= fStrengthIronSight - fStrength;
		fAimAssistStrength							= fStrength + (fZoomAmount * fDiff);
		
		const float fMinTurn						= g_pGameCVars->aim_assistMinTurnScale;
		const float fMinTurnIronSight               = playerIsScoped ? g_pGameCVars->aim_assistMinTurnScale_SniperScope : g_pGameCVars->aim_assistMinTurnScale_IronSight;
		const float fMinTurnDiff				= fMinTurnIronSight - fMinTurn;
		minTurnScale										= fMinTurn + (fZoomAmount * fMinTurnDiff);

		const float fMaxAssistDist			= g_pGameCVars->aim_assistMaxDistance;
		const float fMaxAssistDist_Iron             = playerIsScoped ? g_pGameCVars->aim_assistMaxDistance_SniperScope : g_pGameCVars->aim_assistMaxDistance_IronSight;
		const float fMaxAssistDistDiff	= (fMaxAssistDist_Iron - fMaxAssistDist) * fZoomAmount;
		fMaxDistMult                                = (fMaxAssistDist + fMaxAssistDistDiff) * __fres(fMaxAssistDist);
	}
	else
	{
		_fZoomAmount = 0.0f;
		fMaxDistMult = 1.0f;
		fAimAssistStrength = g_pGameCVars->aim_assistStrength;
		minTurnScale = g_pGameCVars->aim_assistMinTurnScale;
	}

	const float falloffStartDistance = g_pGameCVars->aim_assistSlowFalloffStartDistance;
	const float falloffPerMeter = 1.0f / (g_pGameCVars->aim_assistSlowDisableDistance - falloffStartDistance);

	const TAutoaimTargets& aaTargets = g_pGame->GetAutoAimManager().GetAutoAimTargets();
	const int targetCount = aaTargets.size();
	float fBestTargetDistance = FLT_MAX;

#if DBG_AUTO_AIM
	SAuxGeomRenderFlags oldFlags = gEnv->pRenderer->GetIRenderAuxGeom()->GetRenderFlags();
	SAuxGeomRenderFlags newFlags = e_Def3DPublicRenderflags;
	newFlags.SetAlphaBlendMode(e_AlphaBlended);
	newFlags.SetDepthTestFlag(e_DepthTestOff);
	newFlags.SetCullMode(e_CullModeNone); 
	gEnv->pRenderer->GetIRenderAuxGeom()->SetRenderFlags(newFlags);
#endif

	for (int i = 0; i < targetCount; ++i)
	{
		const SAutoaimTarget& target = aaTargets[i];

		CRY_ASSERT(target.entityId != m_player.GetEntityId());

		//Skip friendly ai
		if(gEnv->bMultiplayer)
		{
			if(multipleTeams &&  (pGameRules->GetTeam(target.entityId) == playerTeam))
			{
				continue;
			}

		}
		else 
		{		
			if (target.HasFlagSet(eAATF_AIHostile) == false)
				continue;

			distance_follow_threshold_far = fMaxDistMult * (target.HasFlagSet(eAATF_AIRadarTagged) ? g_pGameCVars->aim_assistMaxDistanceTagged : g_pGameCVars->aim_assistMaxDistance);
		}
		
		Vec3 targetPos = target.primaryAimPosition;
		Vec3 targetDistVec = (targetPos - playerPos);
		float distance = targetDistVec.GetLength();
		
		if (distance <= 0.1f)
			continue;

		Vec3 dirToTarget = targetDistVec / distance;

		// fast reject everything behind player, too far away or too near from line of view
		// sort rest by angle to crosshair and distance from player
		float alignment = playerFwd * dirToTarget;
		if (alignment <= 0.0f)
			continue;

		if ((distance < distance_follow_threshold_near) || (distance > distance_follow_threshold_far))
			continue;

		const int kAutoaimVisibilityLatency = 2;
		CPlayerVisTable::SVisibilityParams visParams(target.entityId);
		visParams.queryParams = eVQP_IgnoreGlass;
		if (!g_pGame->GetPlayerVisTable()->CanLocalPlayerSee(visParams, kAutoaimVisibilityLatency))
		{
			// Since both player and target entities are ignored, and the ray still intersected something, there's something in the way.
			// Need to profile this and see if it's faster to do the below checks before doing the linetest. It's fairly expensive but
			// linetests generally are as well... - Richard
			continue;
		}

#if DBG_AUTO_AIM
		const ColorB green(0,255,0,255);
		const ColorB darkgreen(0,155,0,225);
		gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine( playerPos, darkgreen, targetPos, green);
#endif

		const float angleDot = dirToTarget.dot(-playerRgt);
		const float angle = (RAD2DEG(acos_tpl(angleDot)) - 90.f);
		const float absAngle = fabsf(angle);

		const float angleDotV = playerUp.dot(dirToTarget);
		const float angleToTargetV = (RAD2DEG(acos_tpl(angleDotV)) - 90.f);
		const float absAngleV = fabsf(angleToTargetV);

		const float slowModifiedDistance = distance * g_pGameCVars->aim_assistSlowDistanceModifier;
		const float radius_slow_threshold_inner = 0.5f;
		const float radius_slow_threshold_outer = g_pGameCVars->aim_assistSlowThresholdOuter;
		const float angle_slow_threshold_inner = RAD2DEG(atan_tpl(radius_slow_threshold_inner / slowModifiedDistance));
		const float angle_slow_threshold_outer = RAD2DEG(atan_tpl(radius_slow_threshold_outer / slowModifiedDistance));
		float angle_slow_fractionH = clamp_tpl((absAngle - angle_slow_threshold_inner) / (angle_slow_threshold_outer - angle_slow_threshold_inner), 0.0f, 1.0f);
		float angle_slow_fractionV = clamp_tpl((absAngleV - angle_slow_threshold_inner) / (angle_slow_threshold_outer - angle_slow_threshold_inner), 0.0f, 1.0f);

		float angle_slow_fraction = max(angle_slow_fractionH, angle_slow_fractionV);

		const float distance_follow_fraction = clamp_tpl((distance - fFollowFalloffDist) / (distance_follow_threshold_far - fFollowFalloffDist), 0.0f, 1.0f);
		const float radius_follow_threshold_inner = target.innerRadius;
		const float radius_follow_threshold_outer = target.outerRadius;
		const float radius_snap = target.HasFlagSet(eAATF_AIRadarTagged) ?	target.snapRadiusTagged * g_pGameCVars->aim_assistSnapRadiusTaggedScale : 
																			target.snapRadius * g_pGameCVars->aim_assistSnapRadiusScale;
		const float angle_follow_threshold_inner = RAD2DEG(atan_tpl(radius_follow_threshold_inner / distance));
		const float angle_follow_threshold_outer = RAD2DEG(atan_tpl(radius_follow_threshold_outer / distance));
		const float angle_follow_fraction = clamp_tpl((absAngle - angle_follow_threshold_inner) / (angle_follow_threshold_outer - angle_follow_threshold_inner), 0.0f, 1.0f);
		const float angle_follow_fractionV = clamp_tpl((absAngleV - angle_follow_threshold_inner) / (angle_follow_threshold_outer - angle_follow_threshold_inner), 0.0f, 1.0f);
		const float worst_follow_fraction = (float)__fsel(angle_follow_fraction - angle_follow_fractionV, angle_follow_fraction, angle_follow_fractionV);
		float follow_fraction = ((1.0f - worst_follow_fraction) * (1.0f - distance_follow_fraction));
		float follow_vote = follow_fraction;

		//clamp the lower bound of the distance_slow_modifier so it can't be lower than the angle slow fraction
		//  which prevents close but off-centre targets slowing us too much
		const float distance_slow_modifier = clamp_tpl( 1.0f - ((distance - falloffStartDistance) * falloffPerMeter), angle_slow_fraction, 1.0f);


		const float fCombinedModifier = angle_slow_fraction * distance_slow_modifier;
		fBestTargetDistance = (float)__fsel(fCombinedModifier - bestScale, fBestTargetDistance, distance);
		bestScale = min(fCombinedModifier, bestScale);		

		if (follow_vote > follow_vote_leader)
		{
			follow_vote_leader = follow_vote;

			//m_follow_target_id only gets set after the loop -> this won't get hit when a target is selected
			// as a follow target for the first time. This doesn't need to be in the loop.
			if ( m_follow_target_id == target.entityId)
			{
				const Vec3 follow_target_dir_local = m_follow_target_dir;
				Vec3 target_rgt = playerRgt;
				Vec3 target_up = target_rgt.cross(follow_target_dir_local);
				target_rgt = follow_target_dir_local.cross(target_up);
				target_rgt.Normalize();
				target_up.Normalize();
				
				float alignH = dirToTarget * -target_rgt;
				float alignV = dirToTarget.z - follow_target_dir_local.z;
				float angleH = min(fabsf(alignH * fAimAssistStrength), fabsf(angleDot));
				float angleV = min(fabsf(alignV * fAimAssistStrength), fabsf(angleDotV));

				followH = follow_fraction * (float)__fsel(angleDot, angleH, -angleH);
				followV = follow_fraction * (float)__fsel(angleDotV, angleV, -angleV);	
								
				follow_vote_leader += 0.05f; // anti oscillation between different targets
				follow_target_pos = targetPos;
			}

			follow_target_id = target.entityId;
			snap_target_id = target.entityId;
			follow_target_dir = dirToTarget;
			snap_target_dir = PickBestSnapDirection(playerPos, playerFwd, target);

		}
		else if (!follow_target_id && (radius_snap > 0.0f))
		{
			Lineseg lineSegment;
			lineSegment.start = playerPos;
			lineSegment.end = playerPos + (playerFwd * (distance + radius_snap));
			Sphere sphere;
			sphere.center = targetPos;
			sphere.radius = radius_snap;
			Vec3 intersectionPoint;

			if (Intersect::Lineseg_SphereFirst(lineSegment, sphere, intersectionPoint))
			{
				float t = 0.0f;
				const float snap_fraction = 1.0f - (Distance::Point_Lineseg(targetPos, lineSegment, t) * (float)__fres(radius_snap));

				if (snap_fraction > snap_vote_leader)
				{
					snap_vote_leader = snap_fraction;
					snap_target_id = target.entityId;
					snap_target_dir = PickBestSnapDirection(playerPos, playerFwd, target);
				}
			}
		}
	}

#if DBG_AUTO_AIM
	if ((!follow_target_pos.IsZeroFast()) && (g_pGameCVars->pl_targeting_debug != 0))
	{
		float radius_inner = 0.30f;
		float radius_outer = 0.33f;
		ColorB colorInner(255,255,0,0x40);
		ColorB colorOuter(255,255,0,0x40);
		DrawDisc(follow_target_pos, follow_target_dir, radius_inner, radius_outer, colorInner, colorOuter);
	}

	gEnv->pRenderer->GetIRenderAuxGeom()->SetRenderFlags(oldFlags);
#endif

	m_follow_target_id  = follow_target_id;
	m_follow_target_dir = follow_target_dir;

	//IMPORTANT: Apply the minimum-distance scaling of the slowdown _after_ calculating the slowdown for the best target
	//						as we want to help the player aim at the nearest target, and modifying the slowdown multiplier prior to this
	//						could result in a different target being selected

	const float fSlowDownProximityFadeInBand = (g_pGameCVars->aim_assistSlowStopFadeinDistance - g_pGameCVars->aim_assistSlowStartFadeinDistance) + FLT_EPSILON;
	float fSlowDownProximityScale = (fBestTargetDistance - g_pGameCVars->aim_assistSlowStartFadeinDistance) / fSlowDownProximityFadeInBand; 
	Limit(fSlowDownProximityScale, 0.0f, 1.0f);
	float fInvBestScale = (1.0f - bestScale) * fSlowDownProximityScale;
	bestScale = 1.0f - fInvBestScale;

	scale = minTurnScale + ((1.0f - minTurnScale) * bestScale);

	UpdateCurrentSnapTarget(snap_target_id, snap_target_dir);
}


void CPlayerRotation::ProcessTargetAssistance( IItem* pCurrentPlayerItem )
{
	// aim assistance
	float targetAimAssistAngleFollowH;
	float targetAimAssistAngleFollowV;
	float targetAimAssistAngleScale;

	IPlayerInput * pIPlayerInput = m_player.GetPlayerInput();
	float absInput = 0.0f;

	float aimAssistPowerFactor = 1.0f;
	bool isUsingDedicatedInput = gEnv->IsDedicated();

#if (USE_DEDICATED_INPUT)
	isUsingDedicatedInput |= g_pGameCVars->g_playerUsesDedicatedInput ? true : false;
#endif

	if(pIPlayerInput && !isUsingDedicatedInput)
	{
		CRY_ASSERT(pIPlayerInput->GetType() == IPlayerInput::PLAYER_INPUT);
		CPlayerInput * pPlayerInput = static_cast<CPlayerInput*>(pIPlayerInput);

		const Ang3 rawDeltas = pPlayerInput->GetRawControllerInput();

		absInput = min((fabsf(rawDeltas.x) + fabsf(rawDeltas.z)) * 1.4f, 1.0f);

		aimAssistPowerFactor = 2.0f - absInput;
	}

	//Gather weapon info before processing target aim assistance
	bool playerWeaponIsRequestingSnap = false;
	CWeapon* pWeapon = GetCurrentWeapon( pCurrentPlayerItem );
	if (pWeapon)
	{
		playerWeaponIsRequestingSnap = pWeapon->ShouldSnapToTarget();
	}

	IMovementController* pMovementController = m_player.GetMovementController();
	CRY_ASSERT(pMovementController);
	SMovementState moveState;
	pMovementController->GetMovementState(moveState);

	Vec3 playerView[4] =
	{
		m_viewQuat.GetColumn0(), // Right
		m_viewQuat.GetColumn1(), // Forward
		m_viewQuat.GetColumn2(), // Up
		moveState.eyePosition    // Pos
	};

	IVehicle* pVehicle = m_player.GetLinkedVehicle();
	if (pVehicle)
	{
		Vec3 up(0.f,0.f,1.f);
		playerView[1] = moveState.eyeDirection;	// Forward
		playerView[0] = (playerView[1].cross(up)).GetNormalizedSafe(); // Right
		playerView[2] = playerView[0].cross(playerView[1]); // Up
	}

	float fZoomAmount = 0.0f;
	TargetAimAssistance(pWeapon, targetAimAssistAngleFollowH, targetAimAssistAngleFollowV, targetAimAssistAngleScale, fZoomAmount, playerView);

	targetAimAssistAngleScale = powf(targetAimAssistAngleScale, aimAssistPowerFactor);

	//TODO: Fix so it's not using auto aim unless selected

	float fFollowAssistScale = (absInput * (1.0f - fZoomAmount)) + min(absInput * fZoomAmount * __fres(g_pGameCVars->aim_assistInputForFullFollow_Ironsight), 1.0f);

	m_deltaAngles.z = (m_deltaAngles.z * targetAimAssistAngleScale) + (absInput * targetAimAssistAngleFollowH);
	m_deltaAngles.x = (m_deltaAngles.x * targetAimAssistAngleScale) + (absInput * targetAimAssistAngleFollowV);

	const bool snapToTarget = (!gEnv->bMultiplayer && playerWeaponIsRequestingSnap && !m_snap_target_dir.IsZero());
	const EntityId closeCombatSnapTargetId = g_pGame->GetAutoAimManager().GetCloseCombatSnapTarget();

	if (closeCombatSnapTargetId)
	{
		Vec3 snapDirection = GetCloseCombatSnapTargetDirection(closeCombatSnapTargetId);
		if (!snapDirection.IsZero())
		{
			const float blendSpeed = clamp_tpl( g_pGameCVars->pl_melee.melee_snap_blend_speed , 0.0f, 1.0f);
			m_deltaAngles.z = -asin_tpl(snapDirection * m_viewQuat.GetColumn0()) * blendSpeed;
			m_deltaAngles.x = asin_tpl(snapDirection * m_viewQuat.GetColumn2()) * blendSpeed;
		}
	}
	else if (snapToTarget)
	{
		const float zoomInTime = pWeapon->GetZoomInTime();
		const float blendFactor = clamp_tpl(pow_tpl((1.0f-zoomInTime),2.0f), 0.0f, 1.0f);
		m_deltaAngles.z = -asin_tpl(m_snap_target_dir * m_viewQuat.GetColumn0()) * blendFactor;
		m_deltaAngles.x = asin_tpl(m_snap_target_dir * m_viewQuat.GetColumn2()) * blendFactor;	
	}
	m_currently_snapping = playerWeaponIsRequestingSnap;

#if !defined(_RELEASE)
	if(g_pGameCVars->ctrlr_OUTPUTDEBUGINFO)
	{
		float white[] = {1,1,1,1};
		IRenderAuxText::Draw2dLabel( 20, 100, 1.4f, white, false, "Aim Acceleration & Assist\n  absInput: %.6f", absInput );
	}
#endif
}


bool CPlayerRotation::ShouldProcessTargetAssistance() const
{
	if (gEnv->bMultiplayer)
	{
		return false;
	}

	if (!m_player.IsClient())
		return false;

	if (!g_pGameCVars->pl_aim_assistance_enabled)
		return false;

	if ((gEnv->bMultiplayer == false) && (g_pGameCVars->g_difficultyLevel >= g_pGameCVars->pl_aim_assistance_disabled_atDifficultyLevel))
		return false;

	IPlayerInput* pIPlayerInput = m_player.GetPlayerInput();
	if (pIPlayerInput && (pIPlayerInput->GetType() == IPlayerInput::PLAYER_INPUT))
	{
		return !(static_cast<CPlayerInput*>(pIPlayerInput)->IsAimingWithMouse());
	}

	return true;
}


void CPlayerRotation::ProcessNormal( float frameTime )
{
#ifdef ENABLE_NAN_CHECK
	//create a matrix perpendicular to the ground
	Vec3 up(0,0,1);
	//..or perpendicular to the linked object Z
	SLinkStats *pLinkStats = &m_player.m_linkStats;
	if (pLinkStats->linkID && pLinkStats->flags & SLinkStats::LINKED_FREELOOK)
	{
		IEntity *pLinked = pLinkStats->GetLinked();
		if (pLinked)
			up = pLinked->GetRotation().GetColumn2();
	}
	
	const Vec3 right(m_baseQuat.GetColumn0());
	const Vec3 forward((up % right).GetNormalized());

	PR_CHECKQNAN_VEC(up);
	PR_CHECKQNAN_VEC(right);
#endif //ENABLE_NAN_CHECK

	const Ang3 vNewDeltaAngles = m_deltaAngles * m_player.m_stats.flashBangStunMult;

#ifdef PLAYER_MOVEMENT_DEBUG_ENABLED
	m_player.DebugGraph_AddValue("AimDeltaH", vNewDeltaAngles.z);
	m_player.DebugGraph_AddValue("AimDeltaV", vNewDeltaAngles.x);
#endif

	Ang3 newViewAngles;
	newViewAngles.Set(m_viewAngles.x + vNewDeltaAngles.x, m_viewAngles.y, m_viewAngles.z + vNewDeltaAngles.z);
	newViewAngles += m_externalAngles;

	//These values need to be used because the player rotation is a quat and quaternions wrap on 720 degrees
	newViewAngles.z = (float)__fsel(  newViewAngles.z - (2.0f * gf_PI2),  newViewAngles.z - (4.0f * gf_PI2), newViewAngles.z);
	newViewAngles.z = (float)__fsel(-(newViewAngles.z + (2.0f * gf_PI2)), newViewAngles.z + (4.0f * gf_PI2), newViewAngles.z);
	
	m_viewAngles = newViewAngles;
		
	if (m_player.CanTurnBody())
	{
		m_baseQuat = Quat::CreateRotationZ(newViewAngles.z);
	}
	
	newViewAngles.y += m_viewRoll;
	m_viewQuat.SetRotationXYZ(newViewAngles);
	
	m_deltaAngles = vNewDeltaAngles;

	if(!m_player.GetLinkedVehicle())
	{
		CHANGED_NETWORK_STATE_REF(m_player, CPlayer::ASPECT_INPUT_CLIENT);
	}
}

void CPlayerRotation::ProcessLeanAndPeek( const SActorFrameMovementParams& movement )
{
	const float leanAmt = (float)__fsel(0.01f - fabsf(movement.desiredLean), 0.0f, movement.desiredLean);

	m_leanAmount = leanAmt;

	//check if its possible
	if ((leanAmt*leanAmt) < 0.01f)
	{
		m_leanAndPeekInfo.Reset();	//Clear any previous result
	}
	else
	{
		const EStance stance = m_player.GetStance();
		const float noLean(0.0f);
		const Vec3 playerPosition = m_player.GetEntity()->GetWorldPos();
		const Vec3 headPos(playerPosition + m_baseQuat * m_player.GetStanceViewOffset(stance,&noLean));
		const Vec3 newPos(playerPosition + m_baseQuat * m_player.GetStanceViewOffset(stance,&m_leanAmount));

		/*gEnv->pRenderer->GetIRenderAuxGeom()->DrawSphere(headPos, 0.05f, ColorB(0,0,255,255) );
		gEnv->pRenderer->GetIRenderAuxGeom()->DrawSphere(newPos, 0.05f, ColorB(0,0,255,255) );
		gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(headPos, ColorB(0,0,255,255), newPos, ColorB(0,0,255,255));*/

		const int rayFlags(rwi_stop_at_pierceable|rwi_colltype_any);
		IPhysicalEntity *pSkip(m_player.GetEntity()->GetPhysics());

		const float distMult(3.0f);
		const float distMultInv(1.0f/distMult);

		const Vec3& limitPoint = m_leanAndPeekInfo.GetLeanLimit(headPos + m_viewQuat.GetColumn1() * 0.25f, (newPos - headPos)*distMult, ent_terrain|ent_static|ent_rigid|ent_sleeping_rigid, rayFlags, &pSkip, pSkip ? 1 : 0);

		const bool validHit = (!limitPoint.IsZero());
		if (validHit)
		{
			const float dist((headPos - newPos).len2() * distMult);
			const float invDist = dist>0.f ? __fres(dist) : 0.f;
			m_leanAmount *= ((limitPoint - headPos).len2() * invDist * distMultInv);

			//gEnv->pRenderer->GetIRenderAuxGeom()->DrawSphere(hit.pt, 0.05f, ColorB(0,255,0,255) );
		}
	}

	// TODO(Márcio): Maybe do some checks here!
	m_peekOverAmount = movement.desiredPeekOver;
}

Vec3 CPlayerRotation::GetCloseCombatSnapTargetDirection(EntityId snapTargetId) const
{
	const SAutoaimTarget* pAATarget = g_pGame->GetAutoAimManager().GetTargetInfo(snapTargetId);
	if(pAATarget)
	{
		Vec3 aimPos = m_player.GetEntity()->GetWorldPos() + m_player.GetEyeOffset();

		return GetCloseCombatSnapTargetDirection(aimPos, m_player.GetViewMatrix().GetColumn1(), *pAATarget);
	}

	return ZERO;
}


Vec3 CPlayerRotation::PickBestSnapDirection( const Vec3& aimPos, const Vec3& aimDirection, const SAutoaimTarget& aaTarget ) const
{
	//If not already on target (or started snapping
	bool shouldTryToSnap = (m_player.GetCurrentTargetEntityId() != aaTarget.entityId) || m_currently_snapping;
	if (shouldTryToSnap)
	{
		return GetCloseCombatSnapTargetDirection(aimPos, aimDirection, aaTarget);
	}

	return ZERO;
}

Vec3 CPlayerRotation::GetCloseCombatSnapTargetDirection( const Vec3& aimPos, const Vec3& aimDirection, const SAutoaimTarget& aaTarget ) const
{
	Lineseg targetLine;
	targetLine.start = aaTarget.secondaryAimPosition;
	targetLine.end = aaTarget.primaryAimPosition;

	Lineseg aimLine;
	aimLine.start = aimPos;
	aimLine.end = aimPos + (aimDirection * g_pGameCVars->aim_assistMaxDistance);

	float t0 = -1.0f, t1 = -1.0f;
	Distance::Lineseg_LinesegSq<float>(targetLine, aimLine, &t0, &t1);

	if (t0 >= 0.0f)
	{

		const Vec3 snapTarget = ((targetLine.start) + ((targetLine.end - targetLine.start) * t0));

#if DBG_AUTO_AIM
		SAuxGeomRenderFlags oldFlags = gEnv->pRenderer->GetIRenderAuxGeom()->GetRenderFlags();
		SAuxGeomRenderFlags newFlags = e_Def3DPublicRenderflags;
		newFlags.SetAlphaBlendMode(e_AlphaBlended);
		newFlags.SetDepthTestFlag(e_DepthTestOff);
		newFlags.SetCullMode(e_CullModeNone); 
		gEnv->pRenderer->GetIRenderAuxGeom()->SetRenderFlags(newFlags);

		gEnv->pRenderer->GetIRenderAuxGeom()->DrawLine(targetLine.start, ColorB(196, 196, 0), targetLine.end, ColorB(196, 196, 0), 4.0f);
		gEnv->pRenderer->GetIRenderAuxGeom()->DrawSphere(snapTarget, 0.125f, ColorB(0, 196, 0));

		gEnv->pRenderer->GetIRenderAuxGeom()->SetRenderFlags(oldFlags);
#endif

		return (snapTarget - aimPos).GetNormalizedSafe(Vec3Constants<float>::fVec3_OneY);
	}

	return ZERO;
}

void CPlayerRotation::UpdateCurrentSnapTarget( const EntityId snapTargetId, const Vec3& snapDirection )
{
	//If started to snap, we have to keep the initial target
	if (m_currently_snapping)
	{
		if (m_snap_target_id == snapTargetId)
		{
			m_snap_target_id = snapTargetId;
			m_snap_target_dir = snapDirection;
		}
		else 
		{
			m_snap_target_id = 0;
			m_snap_target_dir.zero();
		}
	}
	else
	{
		m_snap_target_id = snapDirection.IsZero() ? 0 : snapTargetId;
		m_snap_target_dir = snapDirection;
	}
}

CWeapon* CPlayerRotation::GetCurrentWeapon( IItem* pCurrentPlayerItem ) const
{
	return pCurrentPlayerItem ? static_cast<CWeapon*>(pCurrentPlayerItem->GetIWeapon()) : NULL;
}

void CPlayerRotation::ProcessForcedLookDirection( const Quat& lastViewQuat, float frameTime )
{
	const float forceLookLenSqr(m_forceLookVector.len2());
	
	if (forceLookLenSqr < 0.001f)
		return;
	
	const float forceLookLen(sqrt_tpl(forceLookLenSqr));
	Vec3 forceLook(m_forceLookVector);
	forceLook *= (float)__fres(forceLookLen);
	forceLook = lastViewQuat.GetInverted() * forceLook;

	const float smoothSpeed(6.6f * forceLookLen);

	float blendAmount = min(1.0f,frameTime*smoothSpeed);
	if(!m_bForcedLookAtBlendingEnabled)
	{
		blendAmount = 1.0f; 
	}

	m_deltaAngles.x += asinf(forceLook.z) * blendAmount;
	m_deltaAngles.z += atan2_tpl(-forceLook.x,forceLook.y) * blendAmount;

	PR_CHECKQNAN_VEC(m_deltaAngles);
}

void CPlayerRotation::ProcessLinkedState( SLinkStats& linkStats, const Quat& lastBaseQuat, const Quat& lastViewQuat )
{
	if (!linkStats.linkID)
		return;
	
	IEntity *pLinked = linkStats.GetLinked();
	if (pLinked)
	{
		//at this point m_baseQuat and m_viewQuat contain the previous frame rotation, I'm using them to calculate the delta rotation.
		m_baseQuatLinked *= lastBaseQuat.GetInverted() * m_baseQuat;
		m_viewQuatLinked *= lastViewQuat.GetInverted() * m_viewQuat;

		m_baseQuatLinked.Normalize();
		m_viewQuatLinked.Normalize();

		const Quat qLinkedRotation = pLinked->GetRotation();

		m_baseQuat = qLinkedRotation * m_baseQuatLinked;
		m_viewQuat = qLinkedRotation * m_viewQuatLinked;
	}
}

void CPlayerRotation::ProcessFinalViewEffects( float minAngle, float maxAngle )
{
	if (!m_player.IsClient())
	{
		m_viewQuatFinal = m_viewQuat;
	}
	else
	{
		Ang3 viewAngleOffset = m_frameViewAnglesOffset;

		float currentViewPitch = GetLocalPitch();
		float newPitch = clamp_tpl(currentViewPitch + viewAngleOffset.x, minAngle, maxAngle);
		viewAngleOffset.x = newPitch - currentViewPitch;

		m_viewQuatFinal = m_viewQuat * Quat::CreateRotationXYZ(viewAngleOffset);
	}

}

void CPlayerRotation::AddViewAngles( const Ang3 &angles )
{
	m_externalAngles += angles;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void CPlayerRotation::SLeanAndPeekInfo::Reset()
{
	CancelPendingRay();

	m_lastLimit.Set(0.0f, 0.0f, 0.0f);
}

const Vec3& CPlayerRotation::SLeanAndPeekInfo::GetLeanLimit( const Vec3 &startPos, const Vec3 &dir, int objTypes, int flags, IPhysicalEntity **pSkipEnts /*= NULL*/, int nSkipEnts /*= 0*/ )
{
	//Only queue ray cast if not waiting for another one
	if (m_queuedRayID == 0)
	{
		m_queuedRayID = g_pGame->GetRayCaster().Queue(
			RayCastRequest::MediumPriority,
			RayCastRequest(startPos, dir,
			objTypes,
			flags,
			pSkipEnts,
			nSkipEnts),
			functor(*this, &SLeanAndPeekInfo::OnRayCastDataReceived));
	}

	return m_lastLimit;
}

void CPlayerRotation::SLeanAndPeekInfo::OnRayCastDataReceived( const QueuedRayID& rayID, const RayCastResult& result )
{
	CRY_ASSERT(rayID == m_queuedRayID);

	m_queuedRayID = 0;

	if (result.hitCount > 0)
	{
		m_lastLimit = result.hits[0].pt;
	}
	else
	{
		m_lastLimit.Set(0.0f, 0.0f, 0.0f);
	}
}

void CPlayerRotation::SLeanAndPeekInfo::CancelPendingRay()
{
	if (m_queuedRayID != 0)
	{
		g_pGame->GetRayCaster().Cancel(m_queuedRayID);
	}
	m_queuedRayID = 0;
}
