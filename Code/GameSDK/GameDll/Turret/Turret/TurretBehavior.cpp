// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Utility/CryHash.h"
#include "Turret.h"
#include "TurretBehaviorEvents.h"
#include "TurretBehaviorParams.h"
#include "TurretHelpers.h"

#include <CryAISystem/IAIObject.h>
#include <IItemSystem.h>
#include "WeaponSystem.h"



class CTurretBehavior
	: public CStateHierarchy< CTurret >
{
	DECLARE_STATE_CLASS_BEGIN( CTurret, CTurretBehavior )
	DECLARE_STATE_CLASS_ADD( CTurret, Alive )
	DECLARE_STATE_CLASS_ADD( CTurret, Dead )
	DECLARE_STATE_CLASS_ADD( CTurret, Undeployed )
	DECLARE_STATE_CLASS_ADD( CTurret, PartiallyDeployed )
	DECLARE_STATE_CLASS_ADD( CTurret, Deployed )
	DECLARE_STATE_CLASS_ADD( CTurret, SearchForTarget )
	DECLARE_STATE_CLASS_ADD( CTurret, SearchForTarget_AtLocation )
	DECLARE_STATE_CLASS_ADD( CTurret, SearchForTarget_Sweep )
	DECLARE_STATE_CLASS_ADD( CTurret, SearchForTarget_PauseSweep )
	DECLARE_STATE_CLASS_ADD( CTurret, FollowTarget )
	DECLARE_STATE_CLASS_ADD( CTurret, FollowTarget_Reevaluate )
	DECLARE_STATE_CLASS_ADD( CTurret, FireAtTarget )
	DECLARE_STATE_CLASS_ADD( CTurret, LostTarget )
	DECLARE_STATE_CLASS_END( CTurret )

public:
	TurretBehaviorParams::SBehavior* m_pParams;

	CTimeValue GetTimeNow() const
	{
		return m_pParams->timeNow;
	}


	void UpdateRandomOffset( const TurretBehaviorParams::SRandomOffsetParams& params, TurretBehaviorParams::SRandomOffsetRuntime& dataInOut )
	{
		const CTimeValue timeNow = GetTimeNow();
		if ( dataInOut.nextUpdateTime < timeNow )
		{
			dataInOut.nextUpdateTime = timeNow + CTimeValue( params.updateSeconds );
			dataInOut.value = cry_random( -params.range, params.range );
		}
	}

	void ResetRandomOffsetTime( const TurretBehaviorParams::SRandomOffsetParams& params, TurretBehaviorParams::SRandomOffsetRuntime& dataInOut )
	{
		const CTimeValue timeNow = GetTimeNow();
		dataInOut.nextUpdateTime = timeNow + CTimeValue( params.updateSeconds );
	}
	

	Vec3 m_targetWorldPositionWithoutOffset;
};

//////////////////////////////////////////////////////////////////////////
DEFINE_STATE_CLASS_BEGIN( CTurret, CTurretBehavior, eTurretState_Behavior, Deployed )
	// Constructor member initialization
	m_pParams = nullptr;
	DEFINE_STATE_CLASS_ADD( CTurret, CTurretBehavior, Alive, Root )
		DEFINE_STATE_CLASS_ADD( CTurret, CTurretBehavior, Undeployed, Alive )
		DEFINE_STATE_CLASS_ADD( CTurret, CTurretBehavior, PartiallyDeployed, Alive )
		DEFINE_STATE_CLASS_ADD( CTurret, CTurretBehavior, Deployed, Alive )
			DEFINE_STATE_CLASS_ADD( CTurret, CTurretBehavior, SearchForTarget, Deployed )
				DEFINE_STATE_CLASS_ADD( CTurret, CTurretBehavior, SearchForTarget_AtLocation, SearchForTarget )
				DEFINE_STATE_CLASS_ADD( CTurret, CTurretBehavior, SearchForTarget_Sweep, SearchForTarget )
				DEFINE_STATE_CLASS_ADD( CTurret, CTurretBehavior, SearchForTarget_PauseSweep, SearchForTarget )
			DEFINE_STATE_CLASS_ADD( CTurret, CTurretBehavior, FollowTarget, Deployed )
				DEFINE_STATE_CLASS_ADD( CTurret, CTurretBehavior, FollowTarget_Reevaluate, FollowTarget )
			DEFINE_STATE_CLASS_ADD( CTurret, CTurretBehavior, FireAtTarget, Deployed )
			DEFINE_STATE_CLASS_ADD( CTurret, CTurretBehavior, LostTarget, Deployed )
	DEFINE_STATE_CLASS_ADD( CTurret, CTurretBehavior, Dead, Root )
DEFINE_STATE_CLASS_END( CTurret, CTurretBehavior )



namespace
{
	Vec3 GetPredictedWorldPosition( CTurret& turret, const Vec3& targetWorldPosition, const Vec3& targetWorldVelocity, const float extraPredictionDelaySeconds )
	{
		const IWeapon* pPrimaryWeapon = turret.GetPrimaryWeapon();
		if ( pPrimaryWeapon == NULL )
		{
			return targetWorldPosition;
		}

		const IFireMode* pFireMode = pPrimaryWeapon->GetFireMode( 0 );
		CRY_ASSERT( pFireMode != NULL );

		IEntityClass* pAmmoType = pFireMode->GetAmmoType();
		CRY_ASSERT( pAmmoType != NULL );

		const CWeaponSystem* pWeaponSystem = g_pGame->GetWeaponSystem();
		const SAmmoParams* pAmmoParams = pWeaponSystem->GetAmmoParams( pAmmoType );
		CRY_ASSERT( pAmmoParams != NULL );

		const float ammoSpeed = pAmmoParams->speed;

		const IEntity* pTurretEntity = turret.GetEntity();
		const Vec3 turretWorldPosition = pTurretEntity->GetWorldPos();
		const Vec3 turretToTarget = targetWorldPosition - turretWorldPosition;

		const float a = targetWorldVelocity.GetLengthSquared() - ( ammoSpeed * ammoSpeed );
		const float b = 2 * turretToTarget.Dot( targetWorldVelocity );
		const float c = turretToTarget.GetLengthSquared();

		const float r = ( b * b ) - ( 4 * a * c );
		if ( r < 0 )
		{
			return targetWorldPosition;
		}

		const float s0 = -b / ( 2 * a );
		const float s1 = sqrt( r ) / ( 2 * a );

		const float t0 = s0 - s1;
		const float t1 = s0 + s1;

		const int solutionCount = ( ( 0 <= t0 ) ? 1 : 0 ) + ( ( 0 <= t1 ) ? 1 : 0 );
		if ( solutionCount == 0 )
		{
			return targetWorldPosition;
		}

		// This can be moved up into turretToTarget... but it behaves reasonably when like this... so let's leave it for the moment.
		const float spinUpSeconds = pFireMode->GetSpinUpTime();
		const float nextShootSeconds = pFireMode->GetNextShotTime();
		const float shootDelaySeconds = ( 0.0f < spinUpSeconds ) ? spinUpSeconds : nextShootSeconds;
		const float clampedShootDelaySeconds = max( 0.0f, shootDelaySeconds );
		const float delaySeconds = extraPredictionDelaySeconds + clampedShootDelaySeconds;
		const float t = ( ( solutionCount == 1 ) ? max( t0, t1 ) : min( t0, t1 ) ) + delaySeconds;

		const Vec3 velocityOffset = targetWorldVelocity * t;
		const Vec3 predictedTargetWorldPosition = targetWorldPosition + velocityOffset;

		return predictedTargetWorldPosition;
	}


	Vec3 GetTargetEntityPredictedWorldPosition( CTurret& turret, IEntity* pTargetEntity, const float extraPredictionDelaySeconds )
	{
		assert( pTargetEntity != NULL );

		const IAIObject* pTargetAiObject = pTargetEntity->GetAI();
		const Vec3 targetWorldPosition = pTargetAiObject ? pTargetAiObject->GetPos() : pTargetEntity->GetWorldPos();

		const IPhysicalEntity* pPhysicalEntity = pTargetEntity->GetPhysics();
		if ( pPhysicalEntity == NULL )
		{
			return targetWorldPosition;
		}

		pe_status_dynamics statusDynamics;
		const int getStatusDynamicsResult = pPhysicalEntity->GetStatus( &statusDynamics );
		const bool getStatusDynamicsSuccess = ( getStatusDynamicsResult != 0 );
		if ( ! getStatusDynamicsSuccess )
		{
			return targetWorldPosition;
		}

		const Vec3& targetWorldVelocity = statusDynamics.v;

		return GetPredictedWorldPosition( turret, targetWorldPosition, targetWorldVelocity, extraPredictionDelaySeconds );
	}


	void SetTurretTargetPredictedPosition( CTurret& turret, IEntity* pTargetEntity, const Vec3 worldOffset, const float extraPredictionDelaySeconds )
	{
		const Vec3 targetPredictedWorldPosition = GetTargetEntityPredictedWorldPosition( turret, pTargetEntity, extraPredictionDelaySeconds );
		turret.SetTargetWorldPosition( targetPredictedWorldPosition + worldOffset );
	}


	Vec3 CalculateAndUpdateSweepSearchPosition( CTurret& turret, const float frameDeltaSeconds, const TurretBehaviorParams::SSearchSweepParams& params, TurretBehaviorParams::SSearchSweepRuntime& dataInOut )
	{
		IEntity* pEntity = turret.GetEntity();
		const Matrix34& worldTM = pEntity->GetWorldTM();
		const Vec3 forward( 0, 1, 0 );

		dataInOut.arrivedAtSweepEnd = false;

		const float yawRadians = turret.CalculateYawRadians();

		const float absYawLimitRadians = params.yawRadiansHalfLimit;
		const float minYawLimitRadians = -absYawLimitRadians;
		const float maxYawLimitRadians = absYawLimitRadians;

		const float yawSweepVelocity = params.yawSweepVelocity;

		float clampedYawRadians = 0;
		if ( yawRadians < minYawLimitRadians )
		{
			clampedYawRadians = yawRadians + ( frameDeltaSeconds * yawSweepVelocity );
		}
		else if ( maxYawLimitRadians < yawRadians )
		{
			clampedYawRadians = yawRadians - ( frameDeltaSeconds * yawSweepVelocity );
		}
		else
		{
			const float sweepDirection = dataInOut.sweepDirection;
			const float newYawRadians = yawRadians + frameDeltaSeconds * yawSweepVelocity * sweepDirection;
			if ( newYawRadians <= minYawLimitRadians || maxYawLimitRadians <= newYawRadians )
			{
				dataInOut.sweepDirection = -sweepDirection;
				dataInOut.arrivedAtSweepEnd = true;
			}
			clampedYawRadians = clamp_tpl( newYawRadians, minYawLimitRadians, maxYawLimitRadians );
		}

		const Vec2 targetForwardLocalPositionXY = TurretHelpers::CalculateTargetLocalPositionXY( clampedYawRadians, params.sweepDistance );
		const Vec3 targetLocalPosition( targetForwardLocalPositionXY.x, targetForwardLocalPositionXY.y, 0 );

		const Vec3 targetWorldLocation = worldTM.TransformPoint( targetLocalPosition );
		return targetWorldLocation;
	}


	Vec3 CalculateAndUpdateSweepToLocationPosition( CTurret& turret, const float frameDeltaSeconds, const float yawSweepVelocity, TurretBehaviorParams::SSearchSweepTowardsLocationRuntime& dataInOut )
	{
		IEntity* pEntity = turret.GetEntity();
		const Matrix34& worldTM = pEntity->GetWorldTM();
		const Vec3 forward( 0, 1, 0 );

		dataInOut.arrivedAtSweepEnd = false;

		const float yawRadians = turret.CalculateYawRadians();

		const float deltaRadians = ( frameDeltaSeconds * yawSweepVelocity * dataInOut.sweepYawDirection );
		const float desiredYawRadians = yawRadians + deltaRadians;

		const float deltaYawToTarget = TurretHelpers::Wrap( dataInOut.targetYawRadians - desiredYawRadians, 0.f, 2.f * gf_PI );
		const float signDeltaYaw = ( deltaYawToTarget < gf_PI ) ? 1.0f : -1.0f;

		float clampedYawRadians = desiredYawRadians;
		if ( signDeltaYaw != dataInOut.sweepYawDirection )
		{
			dataInOut.arrivedAtSweepEnd = true;
			clampedYawRadians = dataInOut.targetYawRadians;
		}

		const float sweepDistance = 100.0f;
		const Vec2 targetForwardLocalPositionXY = TurretHelpers::CalculateTargetLocalPositionXY( clampedYawRadians, sweepDistance );
		const Vec3 targetLocalPosition( targetForwardLocalPositionXY.x, targetForwardLocalPositionXY.y, 0 );

		const Vec3 targetWorldLocation = worldTM.TransformPoint( targetLocalPosition );
		return targetWorldLocation;
	}


	void EnsureTargetRemainsInRange( CTurret& turret, IEntity* pTargetEntity, const float timeoutSeconds )
	{
		assert( pTargetEntity );

		const Vec3 targetPosition = pTargetEntity->GetWorldPos();
		const float additionalDistance = 20.0f;
		const float eyeVisionRangeScale = turret.GetEyeVisionRangeScaleToSeePosition( targetPosition, additionalDistance );
		const float clampedEyeVisionRangeScale = max( 1.0f, eyeVisionRangeScale );

		turret.SetEyeVisionRangeScale( clampedEyeVisionRangeScale, timeoutSeconds );
	}


	bool RetaliateAgainstShooter( CTurret& turret, const HitInfo* pHit, const float timeoutSeconds )
	{
		assert( pHit != NULL );

		IEntity* pTargetEntity = turret.GetValidVisibleTarget();
		if ( pTargetEntity != NULL )
		{
			return false;
		}

		const EntityId shooterEntityId = pHit->shooterId;
		IEntity* pShooterEntity = gEnv->pEntitySystem->GetEntity( shooterEntityId );
		if ( pShooterEntity == NULL )
		{
			return false;
		}

		const bool isShooterHostile = turret.IsEntityHostileAndThreatening( pShooterEntity );
		if ( ! isShooterHostile )
		{
			return false;
		}

		EnsureTargetRemainsInRange( turret, pShooterEntity, timeoutSeconds );

		turret.SetTargetEntity( pShooterEntity );

		return true;
	}

	
}


//////////////////////////////////////////////////////////////////////////
const CTurretBehavior::TStateIndex CTurretBehavior::Root( CTurret& turret, const SStateEvent& event )
{
	const ETurretBehaviorEvent eventId = static_cast< ETurretBehaviorEvent >( event.GetEventId() );

	switch ( eventId )
	{
	case STATE_EVENT_INIT:
		m_pParams = &turret.GetBehaviorParams();
		break;

	case STATE_EVENT_TURRET_FORCE_STATE:
		{
			const SStateEventForceState& stateEventForceState = static_cast< const SStateEventForceState& >( event );
			const ETurretBehaviorState forcedStateId = stateEventForceState.GetForcedState();
			if ( forcedStateId == eTurretBehaviorState_Undeployed )
			{
				return State_Undeployed;
			}
			else if ( forcedStateId == eTurretBehaviorState_Deployed )
			{
				return State_Deployed;
			}
			else if ( forcedStateId == eTurretBehaviorState_PartiallyDeployed )
			{
				return State_PartiallyDeployed;
			}
		}
		break;
	}

	return State_Done;
}



//////////////////////////////////////////////////////////////////////////
const CTurretBehavior::TStateIndex CTurretBehavior::Alive( CTurret& turret, const SStateEvent& event )
{
	const ETurretBehaviorEvent eventId = static_cast< ETurretBehaviorEvent >( event.GetEventId() );
	switch ( eventId )
	{
	case STATE_EVENT_ENTER:
		{
			turret.AddToTacticalManager();
		}
		break;

	case STATE_EVENT_TURRET_HIT:
		{
			const SStateEventHit& stateEventHit = static_cast< const SStateEventHit& >( event );
			const HitInfo* pHit = stateEventHit.GetHit();
			turret.HandleHit( pHit );
		}
		break;

	case STATE_EVENT_TURRET_HACK_FAIL:
		{
			const SStateEventHackFail& stateEventHackFail = static_cast< const SStateEventHackFail& >( event );
			const EntityId hackerEntityId = stateEventHackFail.GetHackerEntityId();
			const IEntity* pHackerEntity = gEnv->pEntitySystem->GetEntity( hackerEntityId );
			turret.NotifyGroupTargetSpotted( pHackerEntity );
		}
		break;
	}

	const bool isDead = turret.IsDead();
	if ( isDead )
	{
		return State_Dead;
	}

	return State_Continue;
}



//////////////////////////////////////////////////////////////////////////
const CTurretBehavior::TStateIndex CTurretBehavior::Undeployed( CTurret& turret, const SStateEvent& event )
{
	const ETurretBehaviorEvent eventId = static_cast< ETurretBehaviorEvent >( event.GetEventId() );
	switch ( eventId )
	{
	case STATE_EVENT_ENTER:
		turret.NotifyBehaviorStateEnter( eTurretBehaviorState_Undeployed );
		turret.StartFragmentByName( "Undeployed" );
		turret.SetThreateningForHostileFactions( m_pParams->undeployedParams.isThreat );
		turret.SetTargetTrackClassThreat( 0.0f );
		return State_Done;

	case STATE_EVENT_TURRET_PRE_PHYSICS_UPDATE:
		turret.SetThreateningForHostileFactions( m_pParams->undeployedParams.isThreat );
		return State_Continue;

	case STATE_EVENT_EXIT:
		turret.SetThreateningForHostileFactions( true );
		return State_Done;
	}

	return State_Continue;
}


//////////////////////////////////////////////////////////////////////////
const CTurretBehavior::TStateIndex CTurretBehavior::PartiallyDeployed( CTurret& turret, const SStateEvent& event )
{
	const ETurretBehaviorEvent eventId = static_cast< ETurretBehaviorEvent >( event.GetEventId() );
	switch ( eventId )
	{
	case STATE_EVENT_ENTER:
		turret.NotifyBehaviorStateEnter( eTurretBehaviorState_PartiallyDeployed );
		turret.StartFragmentByName( "PartiallyDeployed" );
		turret.SetThreateningForHostileFactions( false );
		turret.SetTargetTrackClassThreat( 0.0f );
		return State_Done;
	}

	return State_Continue;
}


//////////////////////////////////////////////////////////////////////////
const CTurretBehavior::TStateIndex CTurretBehavior::Deployed( CTurret& turret, const SStateEvent& event )
{
	const ETurretBehaviorEvent eventId = static_cast< ETurretBehaviorEvent >( event.GetEventId() );
	switch ( eventId )
	{
	case STATE_EVENT_ENTER:
		turret.NotifyBehaviorStateEnter( eTurretBehaviorState_Deployed );
		turret.StartFragmentByName( "Idle" );
		turret.SetThreateningForHostileFactions( true );
		turret.SetTargetTrackClassThreat( 1.2f );
		return State_Done;

	case STATE_EVENT_TURRET_PRE_PHYSICS_UPDATE:
		return State_SearchForTarget;
	}

	return State_Continue;
}


//////////////////////////////////////////////////////////////////////////
const CTurretBehavior::TStateIndex CTurretBehavior::SearchForTarget( CTurret& turret, const SStateEvent& event )
{
	const ETurretBehaviorEvent eventId = static_cast< ETurretBehaviorEvent >( event.GetEventId() );
	switch ( eventId )
	{
	case STATE_EVENT_TURRET_PRE_PHYSICS_UPDATE:
		return State_SearchForTarget_Sweep;

	case STATE_EVENT_TURRET_HACK_FAIL:
		{
			const SStateEventHackFail& stateEventHackFail = static_cast< const SStateEventHackFail& >( event );
			const EntityId hackerEntityId = stateEventHackFail.GetHackerEntityId();
			const IEntity* pHackerEntity = gEnv->pEntitySystem->GetEntity( hackerEntityId );
			if ( pHackerEntity != NULL )
			{
				m_pParams->sweepTowardsLocationRuntime = TurretBehaviorParams::SSearchSweepTowardsLocationRuntime();

				const Vec3 targetWorldPosition = pHackerEntity->GetWorldPos();

				IEntity* pTurretEntity = turret.GetEntity();
				assert( pTurretEntity != NULL );

				const Matrix34& turretWorldTM = pTurretEntity->GetWorldTM();
				const Matrix34 invertedTurretWorldTM = turretWorldTM.GetInverted();

				const Vec3 desiredLookLocalPosition = invertedTurretWorldTM * targetWorldPosition;

				float yawDistance = 0.f;
				float targetYawRadians = 0.f;
				TurretHelpers::CalculateTargetYaw( desiredLookLocalPosition, targetYawRadians, yawDistance );

				m_pParams->sweepTowardsLocationRuntime.targetYawRadians = targetYawRadians;

				const float currentYawRadians = turret.CalculateYawRadians();

				const float deltaYaw = TurretHelpers::Wrap( targetYawRadians - currentYawRadians, 0.f, 2.f * gf_PI );
				const float signDeltaYaw = ( deltaYaw < gf_PI ) ? 1.0f : -1.0f;
				m_pParams->sweepTowardsLocationRuntime.sweepYawDirection = signDeltaYaw;

				return State_SearchForTarget_AtLocation;
			}
			else
			{
				return State_Done;
			}
		}
	}

	return State_Continue;
}


//////////////////////////////////////////////////////////////////////////
const CTurretBehavior::TStateIndex CTurretBehavior::SearchForTarget_AtLocation( CTurret& turret, const SStateEvent& event )
{
	const ETurretBehaviorEvent eventId = static_cast< ETurretBehaviorEvent >( event.GetEventId() );
	switch ( eventId )
	{
	case STATE_EVENT_TURRET_PRE_PHYSICS_UPDATE:
		{
			const SStateEventPrePhysicsUpdate& eventPrePhysicsUpdate = static_cast< const SStateEventPrePhysicsUpdate& >( event );
			const float frameTimeSeconds = eventPrePhysicsUpdate.GetFrameTimeSeconds();

			m_targetWorldPositionWithoutOffset = CalculateAndUpdateSweepToLocationPosition( turret, frameTimeSeconds, m_pParams->searchSweepParams.yawSweepVelocity, m_pParams->sweepTowardsLocationRuntime );

			UpdateRandomOffset( m_pParams->searchSweepParams.randomOffset, m_pParams->searchSweepRuntime.randomOffset );
			const Vec3 randomOffsetLocal = Vec3( 0, 0, m_pParams->searchSweepRuntime.randomOffset.value + m_pParams->searchSweepParams.sweepHeight );
			IEntity* pEntity = turret.GetEntity();
			const Matrix34& worldTM = pEntity->GetWorldTM();
			const Vec3 randomOffsetWorld = worldTM.TransformVector( randomOffsetLocal );
			const Vec3 targetWorldPosition = m_targetWorldPositionWithoutOffset + randomOffsetWorld;

			turret.SetTargetWorldPosition( targetWorldPosition );

			const bool arrivedAtSweepEnd = m_pParams->sweepTowardsLocationRuntime.arrivedAtSweepEnd;
			if ( arrivedAtSweepEnd )
			{
				return State_SearchForTarget_PauseSweep;
			}

			IEntity* pTargetEntity = turret.GetValidVisibleTarget();
			turret.SetTargetEntity( pTargetEntity );

			if ( pTargetEntity == NULL )
			{
				return State_Done;
			}
			else
			{
				return State_FollowTarget;
			}
		}
	}

	return State_Continue;
}


//////////////////////////////////////////////////////////////////////////
const CTurretBehavior::TStateIndex CTurretBehavior::SearchForTarget_Sweep( CTurret& turret, const SStateEvent& event )
{
	const ETurretBehaviorEvent eventId = static_cast< ETurretBehaviorEvent >( event.GetEventId() );
	switch ( eventId )
	{

	case STATE_EVENT_TURRET_PRE_PHYSICS_UPDATE:
		{
			const SStateEventPrePhysicsUpdate& eventPrePhysicsUpdate = static_cast< const SStateEventPrePhysicsUpdate& >( event );
			const float frameTimeSeconds = eventPrePhysicsUpdate.GetFrameTimeSeconds();

			m_targetWorldPositionWithoutOffset = CalculateAndUpdateSweepSearchPosition( turret, frameTimeSeconds, m_pParams->searchSweepParams, m_pParams->searchSweepRuntime );

			UpdateRandomOffset( m_pParams->searchSweepParams.randomOffset, m_pParams->searchSweepRuntime.randomOffset );
			const Vec3 randomOffsetLocal = Vec3( 0, 0, m_pParams->searchSweepRuntime.randomOffset.value + m_pParams->searchSweepParams.sweepHeight );
			IEntity* pEntity = turret.GetEntity();
			const Matrix34& worldTM = pEntity->GetWorldTM();
			const Vec3 randomOffsetWorld = worldTM.TransformVector( randomOffsetLocal );
			const Vec3 targetWorldPosition = m_targetWorldPositionWithoutOffset + randomOffsetWorld;

			turret.SetTargetWorldPosition( targetWorldPosition );

			const bool arrivedAtSweepEnd = m_pParams->searchSweepRuntime.arrivedAtSweepEnd;
			if ( arrivedAtSweepEnd )
			{
				return State_SearchForTarget_PauseSweep;
			}

			IEntity* pTargetEntity = turret.GetValidVisibleTarget();
			turret.SetTargetEntity( pTargetEntity );

			if ( pTargetEntity == NULL )
			{
				return State_Done;
			}
			else
			{
				return State_FollowTarget;
			}
		}
		break;

	case STATE_EVENT_TURRET_HIT:
		{
			const SStateEventHit& stateEventHit = static_cast< const SStateEventHit& >( event );
			const HitInfo* pHit = stateEventHit.GetHit();
			turret.HandleHit( pHit );

			const bool isDead = turret.IsDead();
			if ( isDead )
			{
				return State_Dead;
			}

			const bool foundShooter = RetaliateAgainstShooter( turret, pHit, m_pParams->searchSweepParams.retaliationTimeoutSeconds );
			if ( foundShooter )
			{
				return State_FollowTarget;
			}
		}
		break;
	}

	return State_Continue;
}


//////////////////////////////////////////////////////////////////////////
const CTurretBehavior::TStateIndex CTurretBehavior::SearchForTarget_PauseSweep( CTurret& turret, const SStateEvent& event )
{
	const ETurretBehaviorEvent eventId = static_cast< ETurretBehaviorEvent >( event.GetEventId() );
	switch ( eventId )
	{
	case STATE_EVENT_ENTER:
		ResetRandomOffsetTime( m_pParams->searchSweepPauseParams.randomOffset, m_pParams->searchSweepRuntime.randomOffset );
		m_pParams->searchSweepPauseRuntime.exitTime = GetTimeNow() + CTimeValue( m_pParams->searchSweepPauseParams.limitPauseSeconds );
		return State_Done;

	case STATE_EVENT_TURRET_PRE_PHYSICS_UPDATE:
		{
			const CTimeValue timeNow = GetTimeNow();
			const CTimeValue& exitTime = m_pParams->searchSweepPauseRuntime.exitTime;
			if ( exitTime < timeNow )
			{
				return State_SearchForTarget_Sweep;
			}

			UpdateRandomOffset( m_pParams->searchSweepPauseParams.randomOffset, m_pParams->searchSweepRuntime.randomOffset );

			const Vec3 randomOffsetLocal = Vec3( 0, 0, m_pParams->searchSweepRuntime.randomOffset.value + m_pParams->searchSweepParams.sweepHeight );
			IEntity* pEntity = turret.GetEntity();
			const Matrix34& worldTM = pEntity->GetWorldTM();
			const Vec3 randomOffsetWorld = worldTM.TransformVector( randomOffsetLocal );
			const Vec3 targetWorldPosition = m_targetWorldPositionWithoutOffset + randomOffsetWorld;

			turret.SetTargetWorldPosition( targetWorldPosition );

			IEntity* pTargetEntity = turret.GetValidVisibleTarget();
			turret.SetTargetEntity( pTargetEntity );

			if ( pTargetEntity == NULL )
			{
				return State_Done;
			}
			else
			{
				return State_FollowTarget;
			}
		}
		break;

	case STATE_EVENT_TURRET_HIT:
		{
			const SStateEventHit& stateEventHit = static_cast< const SStateEventHit& >( event );
			const HitInfo* pHit = stateEventHit.GetHit();
			turret.HandleHit( pHit );

			const bool isDead = turret.IsDead();
			if ( isDead )
			{
				return State_Dead;
			}

			const bool foundShooter = RetaliateAgainstShooter( turret, pHit, m_pParams->searchSweepParams.retaliationTimeoutSeconds );
			if ( foundShooter )
			{
				return State_FollowTarget;
			}

			return State_Done;
		}
		break;
	}

	return State_Continue;
}



//////////////////////////////////////////////////////////////////////////
const CTurretBehavior::TStateIndex CTurretBehavior::FollowTarget( CTurret& turret, const SStateEvent& event )
{
	const ETurretBehaviorEvent eventId = static_cast< ETurretBehaviorEvent >( event.GetEventId() );
	switch ( eventId )
	{
	case STATE_EVENT_ENTER:
		{
			m_pParams->followTargetRuntime.startFireTime = GetTimeNow() + CTimeValue( m_pParams->followTargetParams.GetFireDelaySeconds() );
			m_pParams->followTargetRuntime.reevaluateTime = GetTimeNow() + CTimeValue( m_pParams->followTargetParams.reevaluateTargetSeconds );
			turret.NotifyPreparingToFire( m_pParams->followTargetRuntime.startFireTime );

			const IEntity* pTargetEntity = turret.GetTargetEntity();
			turret.NotifyGroupTargetSpotted( pTargetEntity );
		}
		return State_Done;

	case STATE_EVENT_EXIT:
		turret.NotifyCancelPreparingToFire();
		return State_Done;

	case STATE_EVENT_TURRET_PRE_PHYSICS_UPDATE:
		{
			const SStateEventPrePhysicsUpdate& eventPrePhysicsUpdate = static_cast< const SStateEventPrePhysicsUpdate& >( event );
			const float frameTimeSeconds = eventPrePhysicsUpdate.GetFrameTimeSeconds();

			const CTimeValue timeNow = GetTimeNow();
			const CTimeValue reevaluateTime = m_pParams->followTargetRuntime.reevaluateTime;
			if ( reevaluateTime < timeNow )
			{
				return State_FollowTarget_Reevaluate;
			}

			IEntity* pTargetEntity = turret.GetTargetEntity();
			if ( pTargetEntity == NULL )
			{
				return State_LostTarget;
			}

			m_pParams->lostTargetRuntime.UpdateFromEntity( pTargetEntity, m_pParams->lostTargetParams.lastVelocityValueWeight );

			const bool isTargetCloaked = turret.IsEntityCloaked( pTargetEntity );
			if ( isTargetCloaked )
			{
				return State_LostTarget;
			}

			const bool isFiringAllowed = turret.GetAllowFire();
			if ( isFiringAllowed )
			{
				const CTimeValue startFireTime = m_pParams->followTargetRuntime.startFireTime;
				if ( startFireTime < timeNow )
				{
					const Vec3 targetPosition = GetTargetEntityPredictedWorldPosition( turret, pTargetEntity, m_pParams->followTargetParams.predictionDelaySeconds );
					const bool isTargetInWeaponRange = turret.IsInPrimaryWeaponRange( targetPosition );
					if ( isTargetInWeaponRange )
					{
						return State_FireAtTarget;
					}
				}
			}

			SetTurretTargetPredictedPosition( turret, pTargetEntity, Vec3( 0, 0, m_pParams->followTargetParams.aimVerticalOffset ), m_pParams->followTargetParams.predictionDelaySeconds );

			const float defaultEyeVisionRange = turret.GetCachedEyeVisionRangeParamValue();
			const bool isEntityInEyeRange = turret.IsEntityInRange( pTargetEntity, defaultEyeVisionRange );
			if ( ! isEntityInEyeRange )
			{
				EnsureTargetRemainsInRange( turret, pTargetEntity, m_pParams->followTargetParams.keepViewRangeTimeoutSeconds );
			}

			return State_Done;
		}
	}

	return State_Continue;
}



//////////////////////////////////////////////////////////////////////////
const CTurretBehavior::TStateIndex CTurretBehavior::FollowTarget_Reevaluate( CTurret& turret, const SStateEvent& event )
{
	const ETurretBehaviorEvent eventId = static_cast< ETurretBehaviorEvent >( event.GetEventId() );
	switch ( eventId )
	{
	case STATE_EVENT_TURRET_PRE_PHYSICS_UPDATE:
		{
			m_pParams->followTargetRuntime.reevaluateTime = GetTimeNow() + CTimeValue( m_pParams->followTargetParams.reevaluateTargetSeconds );

			IEntity* pTargetEntity = turret.GetValidVisibleTarget();
			turret.SetTargetEntity( pTargetEntity );

			if ( pTargetEntity == NULL )
			{
				return State_LostTarget;
			}
			else
			{
				return State_FollowTarget;
			}
		}
	}

	return State_Continue;
}



//////////////////////////////////////////////////////////////////////////
const CTurretBehavior::TStateIndex CTurretBehavior::FireAtTarget( CTurret& turret, const SStateEvent& event )
{
	const ETurretBehaviorEvent eventId = static_cast< ETurretBehaviorEvent >( event.GetEventId() );
	switch ( eventId )
	{
	case STATE_EVENT_ENTER:
		m_pParams->fireTargetRuntime.stopFireTime = GetTimeNow() + CTimeValue( m_pParams->fireTargetParams.GetStopFireSeconds() );
		turret.StartFirePrimaryWeapon();
		return State_Done;

	case STATE_EVENT_EXIT:
		turret.StopFirePrimaryWeapon();
		return State_Done;

	case STATE_EVENT_TURRET_PRE_PHYSICS_UPDATE:
		{
			const CTimeValue timeNow = GetTimeNow();
			const CTimeValue stopFireTime = m_pParams->fireTargetRuntime.stopFireTime;
			if ( stopFireTime < timeNow )
			{
				return State_FollowTarget;
			}

			IEntity* pTargetEntity = turret.GetValidVisibleTarget();
			turret.SetTargetEntity( pTargetEntity );
			if ( pTargetEntity == NULL )
			{
				return State_LostTarget;
			}

			m_pParams->lostTargetRuntime.UpdateFromEntity( pTargetEntity, m_pParams->lostTargetParams.lastVelocityValueWeight );

			const bool isTargetCloaked = turret.IsEntityCloaked( pTargetEntity );
			if ( isTargetCloaked )
			{
				return State_LostTarget;
			}

			SetTurretTargetPredictedPosition( turret, pTargetEntity, Vec3( 0, 0, m_pParams->followTargetParams.aimVerticalOffset ), m_pParams->followTargetParams.predictionDelaySeconds );
		}
		return State_Done;

	}

	return State_Continue;
}



//////////////////////////////////////////////////////////////////////////
const CTurretBehavior::TStateIndex CTurretBehavior::LostTarget( CTurret& turret, const SStateEvent& event )
{
	const ETurretBehaviorEvent eventId = static_cast< ETurretBehaviorEvent >( event.GetEventId() );
	switch ( eventId )
	{
	case STATE_EVENT_ENTER:
		m_pParams->lostTargetRuntime.backToSearchTime = GetTimeNow() + CTimeValue( m_pParams->lostTargetParams.backToSearchSeconds );
		m_pParams->lostTargetRuntime.targetLostTimeStart = GetTimeNow();
		m_pParams->lostTargetRuntime.firing = true;
		m_pParams->lostTargetRuntime.nextSwitchFireTime = GetTimeNow() + CTimeValue( m_pParams->fireTargetParams.GetStopFireSeconds() );
		turret.StartFirePrimaryWeapon();
		return State_Done;

	case STATE_EVENT_EXIT:
		turret.StopFirePrimaryWeapon();
		return State_Done;

	case STATE_EVENT_TURRET_PRE_PHYSICS_UPDATE:
		{
			const CTimeValue& timeNow = GetTimeNow();
			if ( m_pParams->lostTargetRuntime.nextSwitchFireTime <= timeNow )
			{
				m_pParams->lostTargetRuntime.firing = ! m_pParams->lostTargetRuntime.firing;
				if ( m_pParams->lostTargetRuntime.firing )
				{
					m_pParams->lostTargetRuntime.nextSwitchFireTime = timeNow + CTimeValue( m_pParams->fireTargetParams.GetStopFireSeconds() );
				}
				else
				{
					m_pParams->lostTargetRuntime.nextSwitchFireTime = timeNow + CTimeValue( m_pParams->followTargetParams.GetFireDelaySeconds() );
					turret.StopFirePrimaryWeapon();
				}
			}

			if ( m_pParams->lostTargetRuntime.firing )
			{
				turret.StartFirePrimaryWeapon(); // TODO: Remove this and make sure start/stop fire are reliable when called on the same frame. Without this here, sometimes it will not start firing!
			}

			const SStateEventPrePhysicsUpdate& eventPrePhysicsUpdate = static_cast< const SStateEventPrePhysicsUpdate& >( event );
			const float frameTimeSeconds = eventPrePhysicsUpdate.GetFrameTimeSeconds();

			IEntity* pTargetEntity = turret.GetValidVisibleTarget();
			turret.SetTargetEntity( pTargetEntity );
			if ( pTargetEntity != NULL )
			{
				const bool isTargetCloaked = turret.IsEntityCloaked( pTargetEntity );
				if ( ! isTargetCloaked )
				{
					return State_FollowTarget;
				}
			}

			
			if ( m_pParams->lostTargetRuntime.backToSearchTime <= timeNow )
			{
				return State_SearchForTarget;
			}

			const float dampenVelocityTimeSeconds = m_pParams->lostTargetParams.dampenVelocityTimeSeconds;
			if ( dampenVelocityTimeSeconds <= 0.0f )
			{
				turret.SetTargetWorldPosition( m_pParams->lostTargetRuntime.predictedPosition );
			}
			else
			{
				CTimeValue lostTargetTime = timeNow - m_pParams->lostTargetRuntime.targetLostTimeStart;

				const Vec3 targetPredictedWorldPosition = m_pParams->lostTargetRuntime.predictedPosition + m_pParams->lostTargetRuntime.predictedVelocity * frameTimeSeconds;
				turret.SetTargetWorldPosition( targetPredictedWorldPosition );

				const float dampenVelocity = max( 0.0f, dampenVelocityTimeSeconds - lostTargetTime.GetSeconds() ) / dampenVelocityTimeSeconds;

				m_pParams->lostTargetRuntime.predictedVelocity = m_pParams->lostTargetRuntime.lastTargetKnownVelocity * dampenVelocity;
				m_pParams->lostTargetRuntime.predictedPosition = targetPredictedWorldPosition;
			}
		}
		return State_Done;
	}

	return State_Continue;
}



//////////////////////////////////////////////////////////////////////////
const CTurretBehavior::TStateIndex CTurretBehavior::Dead( CTurret& turret, const SStateEvent& event )
{
	const ETurretBehaviorEvent eventId = static_cast< ETurretBehaviorEvent >( event.GetEventId() );
	switch ( eventId )
	{
	case STATE_EVENT_ENTER:
		turret.StartFragmentByName( "Dead" );
		turret.RemoveFromTacticalManager();
		turret.NotifyBehaviorStateEnter( eTurretBehaviorState_Dead );
		return State_Done;

	case STATE_EVENT_TURRET_FORCE_STATE:
		return State_Done;
	}

	return State_Continue;
}

