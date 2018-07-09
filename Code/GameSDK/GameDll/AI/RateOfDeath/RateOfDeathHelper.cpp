// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "RateOfDeathHelper.h"

#include <CryAISystem/IAIObject.h>
#include <CryAISystem/IAIActor.h>
#include <CryAISystem/IAIActorProxy.h>

#include "AutoEnum.h"

namespace RateOfDeath
{

#define ROD_CVARS_LIST( x ) \
	x( ai_RODAliveTime ) \
	x( ai_RODMoveInc ) \
	x( ai_RODStanceInc ) \
	x( ai_RODDirInc ) \
	x( ai_RODKillZoneInc ) \
	x( ai_RODKillRangeMod ) \
	x( ai_RODCombatRangeMod ) \
	x( ai_RODReactionDistInc ) \
	x( ai_RODReactionDirInc )

	AUTOENUM_BUILDENUMWITHTYPE_WITHNUM( ERateOfDeathCVars, ROD_CVARS_LIST, eRateOfDeathCVarsCount );

	namespace Impl
	{

		AUTOENUM_BUILDNAMEARRAY( g_rateOfDeathCVarsNames, ROD_CVARS_LIST );

		const ICVar* g_rateOfDeathCVars[ eRateOfDeathCVarsCount ] = { 0 };

		float GetCVarValue( const ERateOfDeathCVars cvar, const float defaultValue )
		{
			CRY_ASSERT( cvar < eRateOfDeathCVarsCount );

			const ICVar* pVar = g_rateOfDeathCVars[ cvar ];
			if ( pVar )
			{
				const float value = pVar->GetFVal();
				return value;
			}
			else
			{
				const char* const cvarName = g_rateOfDeathCVarsNames[ cvar ];
				CRY_ASSERT( cvarName );

				pVar = gEnv->pConsole->GetCVar( cvarName );
				CRY_ASSERT( pVar );

				g_rateOfDeathCVars[ cvar ] = pVar;

				const float value = pVar ? pVar->GetFVal() : defaultValue;
				return value;
			}
		}
	}

	float GetDefaultStayAliveTime() { return Impl::GetCVarValue( ai_RODAliveTime, 3.0f); }
	float GetMoveInc() { return Impl::GetCVarValue( ai_RODMoveInc, 3.0f ); }
	float GetStanceInc() { return Impl::GetCVarValue( ai_RODStanceInc, 2.0f ); }
	float GetDirectionInc() { return Impl::GetCVarValue( ai_RODDirInc, 0.0f ); }
	float GetKillZoneInc() { return Impl::GetCVarValue( ai_RODKillZoneInc, -4.0f ); }
	float GetKillRangeMod() { return Impl::GetCVarValue( ai_RODKillRangeMod, 0.15f ); }
	float GetCombatRangeMod() { return Impl::GetCVarValue( ai_RODCombatRangeMod, 0.55f ); }
	float GetReactionDistInc() { return Impl::GetCVarValue( ai_RODReactionDistInc, 0.1f ); }
	float GetReactionDirInc() { return Impl::GetCVarValue( ai_RODReactionDirInc, 2.0f ); }
}



//////////////////////////////////////////////////////////////////////////
#ifdef DEBUGDRAW_RATE_OF_DEATH_HELPER
namespace
{
	const char* const GetZoneName( const EAITargetZone zone )
	{
		switch( zone )
		{
		case AIZONE_IGNORE: return "Ignore";
		case AIZONE_KILL: return "Kill";
		case AIZONE_COMBAT_NEAR: return "Combat Near";
		case AIZONE_COMBAT_FAR: return "Combat Far";
		case AIZONE_WARN: return "Warn";
		case AIZONE_OUT: return "Out";
		default: return "Unknown";
		}
	}
}
#endif


//////////////////////////////////////////////////////////////////////////
CRateOfDeathHelper_HealthThreshold::CRateOfDeathHelper_HealthThreshold()
: m_stayAliveTime( 0 )
, m_lastMaxHealth( -1 )
, m_normalizedHealth( 0 )
, m_normalizedHealthThreshold( 0 )
{
	
}


//////////////////////////////////////////////////////////////////////////
CRateOfDeathHelper_HealthThreshold::~CRateOfDeathHelper_HealthThreshold()
{

}


//////////////////////////////////////////////////////////////////////////
void CRateOfDeathHelper_HealthThreshold::Reset()
{
	m_stayAliveTime = 0;
	m_lastMaxHealth = -1;
	m_normalizedHealth = 0;
	m_normalizedHealthThreshold = 0;
}


//////////////////////////////////////////////////////////////////////////
#ifdef DEBUGDRAW_RATE_OF_DEATH_HELPER
void CRateOfDeathHelper_HealthThreshold::DebugDraw( float& drawPosY )
{
	IRenderer* pRenderer = gEnv->pRenderer;
	const float color[ 4 ] = { 1, 1, 1, 1 };

	pRenderer->Draw2dLabel( 20, drawPosY, 1.2f, color, false, "Stay Alive Time: %f", m_stayAliveTime );
	drawPosY += 15;

	pRenderer->Draw2dLabel( 20, drawPosY, 1.2f, color, false, "Normalized Health: %f", m_normalizedHealth );
	drawPosY += 15;

	pRenderer->Draw2dLabel( 20, drawPosY, 1.2f, color, false, "Normalized Health Threshold: %f", m_normalizedHealthThreshold );
	drawPosY += 15;
}
#endif



//////////////////////////////////////////////////////////////////////////
void CRateOfDeathHelper_HealthThreshold::SetStayAliveTime( const float stayAliveTime )
{
	m_stayAliveTime = max( 0.0f, stayAliveTime );
}


//////////////////////////////////////////////////////////////////////////
float CRateOfDeathHelper_HealthThreshold::GetStayAliveTime() const
{
	return m_stayAliveTime;
}


//////////////////////////////////////////////////////////////////////////
void CRateOfDeathHelper_HealthThreshold::SetHealth( const float health, const float maxHealth )
{
	CRY_ASSERT( 0.0f < maxHealth );
	m_normalizedHealth = health / maxHealth;

	if ( m_lastMaxHealth < maxHealth )
	{
		m_normalizedHealthThreshold = m_normalizedHealth;
	}
	m_lastMaxHealth = maxHealth;
}


//////////////////////////////////////////////////////////////////////////
void CRateOfDeathHelper_HealthThreshold::Update( const float elapsedSeconds )
{
	if ( m_stayAliveTime <= FLT_EPSILON )
	{
		m_normalizedHealthThreshold = 0;
	}
	else
	{
		const float invStayAliveTime = 1.0f / m_stayAliveTime;
		const float damageThresholdDelta = invStayAliveTime * elapsedSeconds;

		m_normalizedHealthThreshold = max( 0.0f, m_normalizedHealthThreshold - damageThresholdDelta );
	}
}


//////////////////////////////////////////////////////////////////////////
bool CRateOfDeathHelper_HealthThreshold::IsDamageAllowed() const
{
	const bool isDamageAllowed = ( m_normalizedHealthThreshold <= m_normalizedHealth );
	return isDamageAllowed;
}


//////////////////////////////////////////////////////////////////////////
float CRateOfDeathHelper_HealthThreshold::GetNormalizedHealth() const
{
	return m_normalizedHealth;
}


//////////////////////////////////////////////////////////////////////////
float CRateOfDeathHelper_HealthThreshold::GetNormalizedHealthThreshold() const
{
	return m_normalizedHealthThreshold;
}






//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
CRateOfDeathHelper_AttackerInfo::CRateOfDeathHelper_AttackerInfo()
: m_position( 0, 0, 0 )
, m_attackRange( 100 )
, m_usingMelee( false )
{

}


//////////////////////////////////////////////////////////////////////////
CRateOfDeathHelper_AttackerInfo::~CRateOfDeathHelper_AttackerInfo()
{
}


//////////////////////////////////////////////////////////////////////////
void CRateOfDeathHelper_AttackerInfo::Reset()
{
	m_position = Vec3( 0, 0, 0 );
	m_attackRange = 100;
	m_usingMelee = false;
}


//////////////////////////////////////////////////////////////////////////
void CRateOfDeathHelper_AttackerInfo::SetPosition( const Vec3& position )
{
	m_position = position;
}


//////////////////////////////////////////////////////////////////////////
const Vec3& CRateOfDeathHelper_AttackerInfo::GetPosition() const
{
	return m_position;
}


//////////////////////////////////////////////////////////////////////////
void CRateOfDeathHelper_AttackerInfo::SetAttackRange( const float attackRange )
{
	m_attackRange = attackRange;
}


//////////////////////////////////////////////////////////////////////////
float CRateOfDeathHelper_AttackerInfo::GetAttackRange() const
{
	return m_attackRange;
}


//////////////////////////////////////////////////////////////////////////
void CRateOfDeathHelper_AttackerInfo::SetUsingMelee( const bool usingMelee )
{
	m_usingMelee = usingMelee;
}


//////////////////////////////////////////////////////////////////////////
bool CRateOfDeathHelper_AttackerInfo::GetUsingMelee() const
{
	return m_usingMelee;
}


//////////////////////////////////////////////////////////////////////////
EAITargetZone CRateOfDeathHelper_AttackerInfo::CalculateZone( const Vec3 position ) const
{
	const float killRangeMod = RateOfDeath::GetKillRangeMod();
	const float combatRangeMod = RateOfDeath::GetCombatRangeMod();

	const float squaredDistance = Distance::Point_PointSq( m_position, position );

	const float squaredKillRange = sqr( m_attackRange * killRangeMod );
	const float squaredCombatRangeNear = sqr( m_attackRange * ( killRangeMod + combatRangeMod ) * 0.5f );
	const float squaredCombatRangeFar = sqr( m_attackRange * ( killRangeMod + combatRangeMod ) );
	const float squaredWarnRange = sqr( m_attackRange );

	if ( squaredDistance < squaredKillRange )
	{
		return AIZONE_KILL;
	}
	else if ( squaredDistance < squaredCombatRangeNear )
	{
		return AIZONE_COMBAT_NEAR;
	}
	else if ( squaredDistance < squaredCombatRangeFar )
	{
		return AIZONE_COMBAT_FAR;
	}
	else if ( squaredDistance < squaredWarnRange )
	{
		return AIZONE_WARN;
	}
	else
	{
		return AIZONE_OUT;
	}
}




//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
CRateOfDeathHelper_Target::CRateOfDeathHelper_Target()
: m_pTarget( NULL )
, m_pTargetActor( NULL )
, m_stance( STANCE_NULL )
, m_zone( AIZONE_OUT )
, m_isInVehicle( false )
, m_stayAliveTimeFactor( 1.0f )
{
}


//////////////////////////////////////////////////////////////////////////
CRateOfDeathHelper_Target::~CRateOfDeathHelper_Target()
{
}


//////////////////////////////////////////////////////////////////////////
void CRateOfDeathHelper_Target::Reset()
{
	m_pTarget = NULL;
	m_pTargetActor = NULL;
	m_stance = STANCE_NULL;
	m_zone = AIZONE_OUT;
	m_isInVehicle = false;
	m_stayAliveTimeFactor = 1.0f;

	m_healthThresholdHelper.Reset();
}


//////////////////////////////////////////////////////////////////////////
void CRateOfDeathHelper_Target::SetTarget( const IAIObject* pTarget )
{
	const IAIActor* const pTargetActor = ( pTarget == NULL ) ? NULL : pTarget->CastToIAIActor();
	if ( pTargetActor && pTargetActor->IsActive() )
	{
		if ( m_pTarget != pTarget )
		{
			Reset();
		}

		m_pTarget = pTarget;
		m_pTargetActor = pTargetActor;
	}
	else
	{
		Reset();
	}
}


//////////////////////////////////////////////////////////////////////////
#ifdef DEBUGDRAW_RATE_OF_DEATH_HELPER
void CRateOfDeathHelper_Target::DebugDraw( float& drawPosY )
{
	IRenderer* pRenderer = gEnv->pRenderer;
	const float color[ 4 ] = { 1, 1, 1, 1 };

	if ( m_pTarget )
	{
		pRenderer->Draw2dLabel( 10, drawPosY, 1.2f, color, false, "Target '%s'", m_pTarget->GetName() );
	}
	else
	{
		pRenderer->Draw2dLabel( 10, drawPosY, 1.2f, color, false, "No Target" );
	}
	drawPosY += 15;

	pRenderer->Draw2dLabel( 15, drawPosY, 1.2f, color, false, "Stance: %s", GetStanceName( m_stance ) );
	drawPosY += 15;

	pRenderer->Draw2dLabel( 15, drawPosY, 1.2f, color, false, "Zone: %s", GetZoneName( m_zone ) );
	drawPosY += 15;

	pRenderer->Draw2dLabel( 15, drawPosY, 1.2f, color, false, "In vehicle: %s", m_isInVehicle ? "yes" : "no" );
	drawPosY += 15;

	pRenderer->Draw2dLabel( 15, drawPosY, 1.2f, color, false, "Stay alive time factor: %f", m_stayAliveTimeFactor );
	drawPosY += 15;

	m_healthThresholdHelper.DebugDraw( drawPosY );
	drawPosY += 5;
}
#endif


//////////////////////////////////////////////////////////////////////////
void CRateOfDeathHelper_Target::Update( const float elapsedTimeSeconds, const CRateOfDeathHelper_AttackerInfo& attacker )
{
	if ( m_pTargetActor == NULL )
	{
		return;
	}

	UpdateStance();
	UpdateIsInVehicle();
	UpdateZone( attacker );
	UpdateStayAliveTime( attacker );
	UpdateHealth( elapsedTimeSeconds );
}


//////////////////////////////////////////////////////////////////////////
void CRateOfDeathHelper_Target::UpdateStance()
{
	CRY_ASSERT( m_pTargetActor );

	IAIActorProxy* const pAiActorProxy = m_pTargetActor->GetProxy();
	CRY_ASSERT( pAiActorProxy );

	const SOBJECTSTATE& objectState = m_pTargetActor->GetState();
	m_stance = static_cast< EStance >( objectState.bodystate );
}


//////////////////////////////////////////////////////////////////////////
void CRateOfDeathHelper_Target::UpdateIsInVehicle()
{
	CRY_ASSERT( m_pTargetActor );

	IAIActorProxy* const pAiActorProxy = m_pTargetActor->GetProxy();
	CRY_ASSERT( pAiActorProxy );

	m_isInVehicle = ( pAiActorProxy->GetLinkedVehicleEntityId() != 0 );
}


//////////////////////////////////////////////////////////////////////////
void CRateOfDeathHelper_Target::UpdateZone( const CRateOfDeathHelper_AttackerInfo& attacker )
{
	CRY_ASSERT( m_pTarget );

	const Vec3& position = m_pTarget->GetPos();
	m_zone = attacker.CalculateZone( position );
}


//////////////////////////////////////////////////////////////////////////
void CRateOfDeathHelper_Target::UpdateStayAliveTime( const CRateOfDeathHelper_AttackerInfo& attacker )
{
	const float stayAliveTime = CalculateStayAliveTime( attacker );
	m_healthThresholdHelper.SetStayAliveTime( stayAliveTime );
}


//////////////////////////////////////////////////////////////////////////
void CRateOfDeathHelper_Target::UpdateHealth( const float elapsedTimeSeconds )
{
	CRY_ASSERT( m_pTargetActor );

	IAIActorProxy* pAiActorProxy = m_pTargetActor->GetProxy();
	CRY_ASSERT( pAiActorProxy );

	const float actorHealth = pAiActorProxy->GetActorHealth();

	const float actorMaxHealth = pAiActorProxy->GetActorMaxHealth();
	const float actorMaxArmor = static_cast< float >( pAiActorProxy->GetActorMaxArmor() );
	const float combinedMaxHealth = actorMaxHealth + actorMaxArmor;

	m_healthThresholdHelper.SetHealth( actorHealth, combinedMaxHealth );
	m_healthThresholdHelper.Update( elapsedTimeSeconds );
}


//////////////////////////////////////////////////////////////////////////
bool CRateOfDeathHelper_Target::IsDamageAllowedByHealthThreshold() const
{
	return m_healthThresholdHelper.IsDamageAllowed();
}


//////////////////////////////////////////////////////////////////////////
bool CRateOfDeathHelper_Target::IsDamageAllowedByMercyTime() const
{
	if ( ! m_pTargetActor )
	{
		return true;
	}

	const bool isInMercyTime = m_pTargetActor->IsLowHealthPauseActive();
	return ( ! isInMercyTime );
}


//////////////////////////////////////////////////////////////////////////
void CRateOfDeathHelper_Target::SetStayAliveTimeFactor( const float stayAliveTimeFactor )
{
	m_stayAliveTimeFactor = stayAliveTimeFactor;
}


//////////////////////////////////////////////////////////////////////////
float CRateOfDeathHelper_Target::GetStayAliveTimeFactor() const
{
	return m_stayAliveTimeFactor;
}


float CRateOfDeathHelper_Target::GetStayAliveTime() const
{
	return m_healthThresholdHelper.GetStayAliveTime() * m_stayAliveTimeFactor;
}


//////////////////////////////////////////////////////////////////////////
const IAIObject* CRateOfDeathHelper_Target::GetAIObject() const
{
	return m_pTarget;
}


//////////////////////////////////////////////////////////////////////////
const IAIActor* CRateOfDeathHelper_Target::GetAIActor() const
{
	return m_pTargetActor;
}


//////////////////////////////////////////////////////////////////////////
bool CRateOfDeathHelper_Target::IsActorTarget() const
{
	return ( m_pTargetActor != NULL );
}


//////////////////////////////////////////////////////////////////////////
EStance CRateOfDeathHelper_Target::GetStance() const
{
	return m_stance;
}


//////////////////////////////////////////////////////////////////////////
EAITargetZone CRateOfDeathHelper_Target::GetZone() const
{
	return m_zone;
}


//////////////////////////////////////////////////////////////////////////
bool CRateOfDeathHelper_Target::GetIsInVehicle() const
{
	return m_isInVehicle;
}


//////////////////////////////////////////////////////////////////////////
float CRateOfDeathHelper_Target::CalculateStayAliveTime( const CRateOfDeathHelper_AttackerInfo& attacker ) const
{
	const float defaultTargetStayAliveTime = RateOfDeath::GetDefaultStayAliveTime();

	if ( ! m_pTarget )
	{
		return defaultTargetStayAliveTime;
	}

	const float stayAliveTime_Movement = CalculateStayAliveTime_Movement( attacker );
	const float stayAliveTime_Stance = CalculateStayAliveTime_Stance( attacker );
	const float stayAliveTime_Direction = CalculateStayAliveTime_Direction( attacker );
	const float stayAliveTime_Other = CalculateStayAliveTime_Zone( attacker );

	const float stayAliveTime = defaultTargetStayAliveTime + stayAliveTime_Movement + stayAliveTime_Stance + stayAliveTime_Direction + stayAliveTime_Other;

	const float adjustedStayAliveTime = m_stayAliveTimeFactor * stayAliveTime;

	return max( 0.0f, adjustedStayAliveTime );
}


//////////////////////////////////////////////////////////////////////////
float CRateOfDeathHelper_Target::CalculateStayAliveTime_Movement( const CRateOfDeathHelper_AttackerInfo& attacker ) const
{
	CRY_ASSERT( m_pTarget );

	const float moveInc = RateOfDeath::GetMoveInc();

	const Vec3& targetVelocity = m_pTarget->GetVelocity();
	const float targetSpeed = targetVelocity.GetLength2D();
	const bool targetRun = ( 6.0f < targetSpeed );

	const bool targetIsPlayer = IsPlayer();
	const bool attackerUsingMelee = attacker.GetUsingMelee();
	if ( targetIsPlayer && ! attackerUsingMelee )
	{
		const bool targetSpeedRun = ( 12.0f < targetSpeed );
		if (targetSpeedRun)
		{
			return moveInc * 2.0f;
		}
		else if ( targetRun )
		{
			return moveInc;
		}
	}
	else if ( targetRun )
	{
		return moveInc;
	}

	return 0.0f;
}


//////////////////////////////////////////////////////////////////////////
float CRateOfDeathHelper_Target::CalculateStayAliveTime_Stance( const CRateOfDeathHelper_AttackerInfo& attacker ) const
{
	const float stanceInc = RateOfDeath::GetStanceInc();

	if ( m_stance == STANCE_CROUCH && AIZONE_KILL < m_zone )
	{
		return stanceInc;
	}
	else if ( m_stance == STANCE_PRONE && AIZONE_COMBAT_FAR <= m_zone )
	{
		return stanceInc * stanceInc;
	}

	return 0.0f;
}


//////////////////////////////////////////////////////////////////////////
float CRateOfDeathHelper_Target::CalculateStayAliveTime_Direction( const CRateOfDeathHelper_AttackerInfo& attacker ) const
{
	CRY_ASSERT( m_pTarget );

	const float directionInc = RateOfDeath::GetDirectionInc();

	const Vec3& attackerPosition = attacker.GetPosition();
	const Vec3& targetPosition = m_pTarget->GetPos();
	Vec3 targetDirection = targetPosition - attackerPosition;
	const float targetDistance = targetDirection.NormalizeSafe();

	const float thr1 = cosf( DEG2RAD( 30.0f ) );
	const float thr2 = cosf( DEG2RAD( 95.0f ) );

	const Vec3& targetViewDirection = m_pTarget->GetViewDir();
	const float dot = -targetDirection.Dot( targetViewDirection );
	if ( dot < thr2 )
	{
		return directionInc * 2.0f;
	}
	else if ( dot < thr1 )
	{
		return directionInc;
	}

	return 0;
}


//////////////////////////////////////////////////////////////////////////
float CRateOfDeathHelper_Target::CalculateStayAliveTime_Zone( const CRateOfDeathHelper_AttackerInfo& attacker ) const
{
	if ( m_zone == AIZONE_KILL )
	{
		if ( ! m_isInVehicle )
		{
			const float killZoneInc = RateOfDeath::GetKillZoneInc();
			return killZoneInc;
		}
	}

	return 0;
}


//////////////////////////////////////////////////////////////////////////
bool CRateOfDeathHelper_Target::IsPlayer() const
{
	CRY_ASSERT( m_pTargetActor );
	return ( m_pTarget->CastToCAIPlayer() != NULL );
}


