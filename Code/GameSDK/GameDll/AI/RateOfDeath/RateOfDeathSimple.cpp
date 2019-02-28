// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// Rate-of-death control: Simple version.

// This class controls how accurate an AI entity will fire at a target.
// When the target takes a lot of damage it will begin to miss more. This
// kind of leniency is especially nice for players.

// This system is a light-weight/helper implementation so that we can have
// cheaper 'semi-AI' entities that do not rely on the more complicated AI systems.

#include "StdAfx.h"
#include "RateOfDeathSimple.h"

#include <CryAISystem/IAIObject.h>
#include <CryAISystem/IAIActor.h>
#include <CryAISystem/IAIActorProxy.h>



//////////////////////////////////////////////////////////////////////////
#define DEFAULT_ROD_SIMPLE_OFFSET_UPDATE_INTERVAL 0.7f
#define DEFAULT_ROD_SIMPLE_HIT_RANGE_MIN 0.1f
#define DEFAULT_ROD_SIMPLE_HIT_RANGE_MAX 0.5f
#define DEFAULT_ROD_SIMPLE_MISS_RANGE_MIN 2.0f
#define DEFAULT_ROD_SIMPLE_MISS_RANGE_MAX 3.0f
#define DEFAULT_ROD_SIMPLE_ATTACK_RANGE 100.0f


namespace RateOfDeathUtil
{

	template< typename T >
	ILINE T GetProperty( SmartScriptTable pPropertiesTable, const char* propertyName, const T& defaultValue )
	{
		if ( ! pPropertiesTable )
		{
			return defaultValue;
		}

		T propertyValue = defaultValue;
		const bool hasProperty = pPropertiesTable->GetValue( propertyName, propertyValue );
		if ( ! hasProperty )
		{
			return defaultValue;
		}

		return propertyValue;
	}

}


//////////////////////////////////////////////////////////////////////////
CRateOfDeathSimple::CRateOfDeathSimple( const IAIObject* const pOwner )
	: m_pOwner( pOwner )
	, m_targetOffset( 0.0f, 0.0f, 0.0f )
	, m_missOffsetIntervalSeconds( DEFAULT_ROD_SIMPLE_OFFSET_UPDATE_INTERVAL )
	, m_missOffsetNextUpdateSeconds( 0.0f )
	, m_hitOffsetIntervalSeconds( DEFAULT_ROD_SIMPLE_OFFSET_UPDATE_INTERVAL )
	, m_hitOffsetNextUpdateSeconds( 0.0f )
	, m_canDamageTarget( true )
	, m_hitMinRange( DEFAULT_ROD_SIMPLE_HIT_RANGE_MIN )
	, m_hitMaxRange( DEFAULT_ROD_SIMPLE_HIT_RANGE_MAX )
	, m_missMinRange( DEFAULT_ROD_SIMPLE_MISS_RANGE_MIN )
	, m_missMaxRange( DEFAULT_ROD_SIMPLE_MISS_RANGE_MAX )
{
	CRY_ASSERT( m_pOwner );
}


//////////////////////////////////////////////////////////////////////////
CRateOfDeathSimple::~CRateOfDeathSimple()
{
}


//////////////////////////////////////////////////////////////////////////
void CRateOfDeathSimple::Init( SmartScriptTable pTable )
{
	m_targetOffset = Vec3( 0, 0, 0 );
	m_missOffsetNextUpdateSeconds = 0;
	m_hitOffsetNextUpdateSeconds = 0;
	m_canDamageTarget = true;

	m_missOffsetIntervalSeconds = RateOfDeathUtil::GetProperty< float >( pTable, "missOffsetIntervalSeconds", DEFAULT_ROD_SIMPLE_OFFSET_UPDATE_INTERVAL );
	m_hitOffsetIntervalSeconds = RateOfDeathUtil::GetProperty< float >( pTable, "hitOffsetIntervalSeconds", DEFAULT_ROD_SIMPLE_OFFSET_UPDATE_INTERVAL );

	m_hitMinRange = RateOfDeathUtil::GetProperty< float >( pTable, "hitMinRange", DEFAULT_ROD_SIMPLE_HIT_RANGE_MIN );
	m_hitMaxRange = RateOfDeathUtil::GetProperty< float >( pTable, "hitMaxRange", DEFAULT_ROD_SIMPLE_HIT_RANGE_MAX );
	m_missMinRange = RateOfDeathUtil::GetProperty< float >( pTable, "missMinRange", DEFAULT_ROD_SIMPLE_MISS_RANGE_MIN );
	m_missMaxRange = RateOfDeathUtil::GetProperty< float >( pTable, "missMaxRange", DEFAULT_ROD_SIMPLE_MISS_RANGE_MAX );

	m_attackerInfo.Reset();

	const float attackRange = RateOfDeathUtil::GetProperty< float >( pTable, "attackRange", DEFAULT_ROD_SIMPLE_ATTACK_RANGE );
	m_attackerInfo.SetAttackRange( attackRange );

	m_rateOfDeathTarget.Reset();
}


//////////////////////////////////////////////////////////////////////////
void CRateOfDeathSimple::SetTarget( IEntity* pTargetEntity )
{
	IAIObject* pTargetEntityAi = ( pTargetEntity == NULL ) ? NULL : pTargetEntity->GetAI();
	m_rateOfDeathTarget.SetTarget( pTargetEntityAi );
}


//////////////////////////////////////////////////////////////////////////
void CRateOfDeathSimple::Update( const float elapsedSeconds )
{
	const Vec3& position = m_pOwner->GetPos();
	m_attackerInfo.SetPosition( position );

	m_rateOfDeathTarget.Update( elapsedSeconds, m_attackerInfo );

	UpdateCanDamageTarget();

	UpdateTargetOffset( elapsedSeconds );
}


//////////////////////////////////////////////////////////////////////////
#ifdef DEBUGDRAW_RATE_OF_DEATH_HELPER
void CRateOfDeathSimple::DebugDraw()
{
	IRenderer* pRenderer = gEnv->pRenderer;
	const float color[ 4 ] = { 1, 1, 1, 1 };

	float drawPosY = 20;

	pRenderer->Draw2dLabel( 5, drawPosY, 1.2f, color, false, "Rate of Death '%s'", m_pOwner->GetName() );
	drawPosY += 15;

	m_rateOfDeathTarget.DebugDraw( drawPosY );
	drawPosY += 15;

	pRenderer->Draw2dLabel( 5, drawPosY, 1.2f, color, false, "Can damage? %s", m_canDamageTarget ? "yes" : "no" );
	drawPosY += 15;

	pRenderer->Draw2dLabel( 10, drawPosY, 1.2f, color, false, "Target Offset <%f, %f, %f>", m_targetOffset.x, m_targetOffset.y, m_targetOffset.z );
	drawPosY += 15;

	pRenderer->Draw2dLabel( 10, drawPosY, 1.2f, color, false, "Miss Offset Update [%f/%f] [time left/ update interval]", m_missOffsetNextUpdateSeconds, m_missOffsetIntervalSeconds );
	drawPosY += 15;

	pRenderer->Draw2dLabel( 10, drawPosY, 1.2f, color, false, "Miss Range [%f-%f]", m_missMinRange, m_missMaxRange );
	drawPosY += 15;

	pRenderer->Draw2dLabel( 10, drawPosY, 1.2f, color, false, "Hit Offset Update [%f/%f] [time left/ update interval]", m_hitOffsetNextUpdateSeconds, m_hitOffsetIntervalSeconds );
	drawPosY += 15;

	pRenderer->Draw2dLabel( 10, drawPosY, 1.2f, color, false, "Hit Range [%f-%f]", m_hitMinRange, m_hitMaxRange );
	drawPosY += 15;
}
#endif


//////////////////////////////////////////////////////////////////////////
void CRateOfDeathSimple::UpdateCanDamageTarget()
{
	const bool canDamageMercyTime = m_rateOfDeathTarget.IsDamageAllowedByMercyTime();
	const bool canDamageHealthThreshold = m_rateOfDeathTarget.IsDamageAllowedByHealthThreshold();

	m_canDamageTarget = canDamageMercyTime && canDamageHealthThreshold;
}


//////////////////////////////////////////////////////////////////////////
void CRateOfDeathSimple::UpdateTargetOffset( const float elapsedSeconds )
{
	// TODO: For now not doing anything fancy here with the silhouette of the target.
	// TODO: Smaller offsets when missing so that it's not over the top, and use a weapon that does no damage but looks the same... it looks a bit silly if it misses too much on purpose!

	if ( m_canDamageTarget )
	{
		m_missOffsetNextUpdateSeconds = 0;

		m_hitOffsetNextUpdateSeconds -= elapsedSeconds;
		if ( m_hitOffsetNextUpdateSeconds <= 0 )
		{
			const float minRange = min( m_hitMinRange, m_hitMaxRange );
			const float maxRange = max( m_hitMinRange, m_hitMaxRange );
			m_targetOffset = CalculateTargetOffset( minRange, maxRange );

			m_hitOffsetNextUpdateSeconds = m_hitOffsetIntervalSeconds;
		}
	}
	else
	{
		m_hitOffsetNextUpdateSeconds = 0;

		m_missOffsetNextUpdateSeconds -= elapsedSeconds;
		if ( m_missOffsetNextUpdateSeconds <= 0 )
		{
			const float minRange = min( m_missMinRange, m_missMaxRange );
			const float maxRange = max( m_missMinRange, m_missMaxRange );
			m_targetOffset = CalculateTargetOffset( minRange, maxRange );

			m_missOffsetNextUpdateSeconds = m_missOffsetIntervalSeconds;
		}
	}
}


//////////////////////////////////////////////////////////////////////////
Vec3 CRateOfDeathSimple::CalculateTargetOffset( const float minRange, const float maxRange ) const
{
	// TODO: Change this later!
	const float randomRange = cry_random( minRange, maxRange );

	const Vec3 randomOffset = cry_random_unit_vector<Vec3>() * randomRange;

	return randomOffset;
}


//////////////////////////////////////////////////////////////////////////
bool CRateOfDeathSimple::CanDamageTarget() const
{
	return m_canDamageTarget;
}


//////////////////////////////////////////////////////////////////////////
Vec3 CRateOfDeathSimple::GetTargetOffset() const
{
	return m_targetOffset;
}


//////////////////////////////////////////////////////////////////////////
void CRateOfDeathSimple::SetMissOffsetUpdateIntervalSeconds( const float updateIntervalSeconds )
{
	m_missOffsetIntervalSeconds = updateIntervalSeconds;
}


//////////////////////////////////////////////////////////////////////////
float CRateOfDeathSimple::GetMissOffsetUpdateIntervalSeconds() const
{
	return m_missOffsetIntervalSeconds;
}


//////////////////////////////////////////////////////////////////////////
void CRateOfDeathSimple::SetMissMinRange( const float minRange )
{
	m_missMinRange = minRange;
}


//////////////////////////////////////////////////////////////////////////
float CRateOfDeathSimple::GetMissMinRange() const
{
	return m_missMinRange;
}


//////////////////////////////////////////////////////////////////////////
void CRateOfDeathSimple::SetMissMaxRange( const float maxRange )
{
	m_missMaxRange = maxRange;
}


//////////////////////////////////////////////////////////////////////////
float CRateOfDeathSimple::GetMissMaxRange() const
{
	return m_missMaxRange;
}


//////////////////////////////////////////////////////////////////////////
void CRateOfDeathSimple::SetHitOffsetUpdateIntervalSeconds( const float updateIntervalSeconds )
{
	m_hitOffsetIntervalSeconds = updateIntervalSeconds;
}


//////////////////////////////////////////////////////////////////////////
float CRateOfDeathSimple::GetHitOffsetUpdateIntervalSeconds() const
{
	return m_hitOffsetIntervalSeconds;
}


//////////////////////////////////////////////////////////////////////////
void CRateOfDeathSimple::SetHitMinRange( const float minRange )
{
	m_hitMinRange = minRange;
}


//////////////////////////////////////////////////////////////////////////
float CRateOfDeathSimple::GetHitMinRange() const
{
	return m_hitMinRange;
}


//////////////////////////////////////////////////////////////////////////
void CRateOfDeathSimple::SetHitMaxRange( const float maxRange )
{
	m_hitMaxRange = maxRange;
}


//////////////////////////////////////////////////////////////////////////
float CRateOfDeathSimple::GetHitMaxRange() const
{
	return m_hitMaxRange;
}


