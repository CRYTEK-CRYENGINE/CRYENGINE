// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "TurretBehaviorParams.h"

#include "TurretHelpers.h"
#include <CryAISystem/IAIObject.h>


using namespace TurretBehaviorParams;



namespace
{
	void ReadAbsRadiansValueFromDegrees( SmartScriptTable pTable, const char* name, float& absRadiansValueInOut )
	{
		assert( *pTable );
		assert( name != NULL );
		assert( name[ 0 ] != 0 );

		float valueDegrees = 0;
		const bool propertyRead = pTable->GetValue( name, valueDegrees );
		if ( propertyRead )
		{
			absRadiansValueInOut = abs( DEG2RAD( valueDegrees ) );
		}
	}


	void ReadAbsValue( SmartScriptTable pTable, const char* name, float& absValueInOut )
	{
		assert( *pTable );
		assert( name != NULL );
		assert( name[ 0 ] != 0 );

		float value = absValueInOut;
		pTable->GetValue( name, value );
		absValueInOut = abs( value );
	}


	template< typename T >
	void ReadValue( SmartScriptTable pTable, const char* name, T& valueInOut )
	{
		assert( *pTable );
		assert( name != NULL );
		assert( name[ 0 ] != 0 );
		
		pTable->GetValue( name, valueInOut );
	}
}


//////////////////////////////////////////////////////////////////////////
SRandomOffsetParams::SRandomOffsetParams()
: updateSeconds( 0.5f )
, range( 3 )
{

}

void SRandomOffsetParams::Reset( SmartScriptTable pScriptTable )
{
	if ( ! pScriptTable )
	{
		return;
	}

	ReadAbsValue( pScriptTable, "fRandomOffsetSeconds", updateSeconds );
	ReadAbsValue( pScriptTable, "fRandomOffsetRange", range );
}


//////////////////////////////////////////////////////////////////////////
SRandomOffsetRuntime::SRandomOffsetRuntime()
: value( 0 )
{
}


//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
SUndeployedParams::SUndeployedParams()
: isThreat( false )
{
}


void SUndeployedParams::Reset( SmartScriptTable pScriptTable )
{
	SmartScriptTable pUndeployedScripTable = TurretHelpers::GetSubScriptTable( pScriptTable, "Undeployed" );
	if ( ! pUndeployedScripTable )
	{
		return;
	}

	ReadValue( pUndeployedScripTable, "bIsThreat", isThreat );
}


//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
SSearchSweepTowardsLocationRuntime::SSearchSweepTowardsLocationRuntime()
: sweepYawDirection( 1.f )
, targetYawRadians( 0.f )
, arrivedAtSweepEnd( false )
{
}


//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
SSearchSweepParams::SSearchSweepParams()
: yawRadiansHalfLimit( DEG2RAD( 45 ) )
, yawSweepVelocity( DEG2RAD( 90 ) )
, sweepDistance( 10 )
, sweepHeight( 1 )
, retaliationTimeoutSeconds( 6.0f )
{
}


void SSearchSweepParams::Reset( SmartScriptTable pScriptTable )
{
	SmartScriptTable pSearchSweepScriptTable = TurretHelpers::GetSubScriptTable( pScriptTable, "SearchSweep" );
	if ( ! pSearchSweepScriptTable )
	{
		return;
	}

	ReadAbsRadiansValueFromDegrees( pSearchSweepScriptTable, "fYawDegreesHalfLimit", yawRadiansHalfLimit );
	ReadAbsRadiansValueFromDegrees( pSearchSweepScriptTable, "fYawDegreesPerSecond", yawSweepVelocity );
	ReadAbsValue( pSearchSweepScriptTable, "fDistance", sweepDistance );
	ReadValue( pSearchSweepScriptTable, "fHeight", sweepHeight );
	ReadAbsValue( pSearchSweepScriptTable, "fRetaliationTimeoutSeconds", retaliationTimeoutSeconds );

	randomOffset.Reset( pSearchSweepScriptTable );
}


//////////////////////////////////////////////////////////////////////////
SSearchSweepRuntime::SSearchSweepRuntime()
: sweepDirection( 1 )
, arrivedAtSweepEnd( false )
{
}


//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
SSearchSweepPauseParams::SSearchSweepPauseParams()
: limitPauseSeconds( 1 )
{
}


void SSearchSweepPauseParams::Reset( SmartScriptTable pScriptTable )
{
	SmartScriptTable pSearchSweepPauseScriptTable = TurretHelpers::GetSubScriptTable( pScriptTable, "SearchSweepPause" );
	if ( ! pSearchSweepPauseScriptTable )
	{
		return;
	}

	ReadAbsValue( pSearchSweepPauseScriptTable, "fLimitPauseSeconds", limitPauseSeconds );

	randomOffset.Reset( pSearchSweepPauseScriptTable );
}


//////////////////////////////////////////////////////////////////////////
SSearchSweepPauseRuntime::SSearchSweepPauseRuntime()
{
}


//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
SFollowTargetParams::SFollowTargetParams()
: reevaluateTargetSeconds( 0.5f )
, minFireDelaySeconds( 0.1f )
, maxFireDelaySeconds( 0.15f )
, aimVerticalOffset( -0.3f )
, predictionDelaySeconds( 0.1f )
, keepViewRangeTimeoutSeconds( 3.0f )
{
}


void SFollowTargetParams::Reset( SmartScriptTable pScriptTable )
{
	SmartScriptTable pFollowTargetScriptTable = TurretHelpers::GetSubScriptTable( pScriptTable, "FollowTarget" );
	if ( ! pFollowTargetScriptTable )
	{
		return;
	}

	ReadAbsValue( pFollowTargetScriptTable, "fReevaluateTargetSeconds", reevaluateTargetSeconds );
	ReadValue( pFollowTargetScriptTable, "fAimVerticalOffset", aimVerticalOffset );
	ReadValue( pFollowTargetScriptTable, "fPredictionDelaySeconds", predictionDelaySeconds );
	ReadAbsValue( pFollowTargetScriptTable, "fKeepViewRangeTimeoutSeconds", keepViewRangeTimeoutSeconds );
	ReadAbsValue( pFollowTargetScriptTable, "fMinStartFireDelaySeconds", minFireDelaySeconds );
	ReadAbsValue( pFollowTargetScriptTable, "fMaxStartFireDelaySeconds", maxFireDelaySeconds );
	maxFireDelaySeconds = max( minFireDelaySeconds, maxFireDelaySeconds );

	randomOffset.Reset( pFollowTargetScriptTable );
}


float SFollowTargetParams::GetFireDelaySeconds() const
{
	const float fireDelaySeconds = cry_random( minFireDelaySeconds, maxFireDelaySeconds );
	return fireDelaySeconds;
}


//////////////////////////////////////////////////////////////////////////
SFollowTargetRuntime::SFollowTargetRuntime()
{
}


//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
SFireTargetParams::SFireTargetParams()
: minStopFireSeconds( 0.5f )
, maxStopFireSeconds( 0.75f )
{
}


void SFireTargetParams::Reset( SmartScriptTable pScriptTable )
{
	SmartScriptTable pFireTargetScriptTable = TurretHelpers::GetSubScriptTable( pScriptTable, "FireTarget" );
	if ( ! pFireTargetScriptTable )
	{
		return;
	}

	ReadAbsValue( pFireTargetScriptTable, "fMinStopFireSeconds", minStopFireSeconds );
	ReadAbsValue( pFireTargetScriptTable, "fMaxStopFireSeconds", maxStopFireSeconds );
	maxStopFireSeconds = max( minStopFireSeconds, maxStopFireSeconds );

	randomOffset.Reset( pFireTargetScriptTable );
}


float SFireTargetParams::GetStopFireSeconds() const
{
	const float stopFireSeconds = cry_random( minStopFireSeconds, maxStopFireSeconds );
	return stopFireSeconds;
}


//////////////////////////////////////////////////////////////////////////
SFireTargetRuntime::SFireTargetRuntime()
{
}


//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

SLostTargetRuntime::SLostTargetRuntime()
: lastTargetKnownPosition( 0, 0, 0 )
, lastTargetKnownVelocity( 0, 0, 0 )
, predictedPosition( 0, 0, 0 )
, predictedVelocity( 0, 0, 0 )
, lastEntityId( 0 )
, firing( false )
{

}


void SLostTargetRuntime::UpdateFromEntity( IEntity* pEntity, const float lastValueWeight )
{
	if ( pEntity == NULL )
	{
		lastEntityId = 0;
		return;
	}

	const IAIObject* pAiObject = pEntity->GetAI();
	lastTargetKnownPosition = pAiObject ? pAiObject->GetPos() : pEntity->GetWorldPos();
	lastTargetKnownVelocity.Set( 0, 0, 0 ); 

	const IPhysicalEntity* pPhysicalEntity = pEntity->GetPhysics();
	if ( pPhysicalEntity == NULL )
	{
		return;
	}

	pe_status_dynamics statusDynamics;
	const int getStatusDynamicsResult = pPhysicalEntity->GetStatus( &statusDynamics );
	const bool getStatusDynamicsSuccess = ( getStatusDynamicsResult != 0 );
	if ( ! getStatusDynamicsSuccess )
	{
		return;
	}

	lastTargetKnownVelocity = statusDynamics.v;

	const EntityId newEntityId = pEntity->GetId();
	if ( newEntityId == lastEntityId )
	{
		predictedPosition = lastTargetKnownPosition;
		predictedVelocity.SetLerp( lastTargetKnownVelocity, predictedVelocity, lastValueWeight );
	}
	else
	{
		lastEntityId = newEntityId;
		predictedPosition = lastTargetKnownPosition;
		predictedVelocity = lastTargetKnownVelocity;
	}
}


//////////////////////////////////////////////////////////////////////////
SLostTargetParams::SLostTargetParams()
: backToSearchSeconds( 1.0f )
, dampenVelocityTimeSeconds( 1.0f )
, lastVelocityValueWeight( 0.9f )
{

}

void SLostTargetParams::Reset( SmartScriptTable pScriptTable )
{
	SmartScriptTable pLostTargetScriptTable = TurretHelpers::GetSubScriptTable( pScriptTable, "LostTarget" );
	if ( ! pLostTargetScriptTable )
	{
		return;
	}

	ReadAbsValue( pLostTargetScriptTable, "fBackToSearchSeconds", backToSearchSeconds );
	ReadAbsValue( pLostTargetScriptTable, "fDampenVelocityTimeSeconds", dampenVelocityTimeSeconds );
	ReadValue( pLostTargetScriptTable, "fLastVelocityValueWeight", lastVelocityValueWeight );
	lastVelocityValueWeight = clamp_tpl( lastVelocityValueWeight, 0.0f, 1.0f );
}



//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void SBehavior::Reset( SmartScriptTable pScriptTable )
{
	searchSweepRuntime = SSearchSweepRuntime();
	searchSweepPauseRuntime = SSearchSweepPauseRuntime();
	followTargetRuntime = SFollowTargetRuntime();
	fireTargetRuntime = SFireTargetRuntime();
	lostTargetRuntime = SLostTargetRuntime();
	sweepTowardsLocationRuntime = SSearchSweepTowardsLocationRuntime();

	searchSweepParams.Reset( pScriptTable );
	searchSweepPauseParams.Reset( pScriptTable );
	followTargetParams.Reset( pScriptTable );
	fireTargetParams.Reset( pScriptTable );
	lostTargetParams.Reset( pScriptTable );
	undeployedParams.Reset( pScriptTable );
}

void SBehavior::UpdateTimeNow()
{
	timeNow = TurretHelpers::GetTimeNow();
}