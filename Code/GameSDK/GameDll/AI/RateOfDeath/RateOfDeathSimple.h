// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// Rate-of-death control: Simple version.

// This class controls how accurate an AI entity will fire at a target.
// When the target takes a lot of damage it will begin to miss more. This
// kind of leniency is especially nice for players.

// This system is a light-weight/helper implementation so that we can have
// cheaper 'semi-AI' entities that do not rely on the more complicated AI systems.

#ifndef __RATE_OF_DEATH_HELPER__SIMPLE__H__
#define __RATE_OF_DEATH_HELPER__SIMPLE__H__

#include "RateOfDeathHelper.h"



class CRateOfDeathSimple
{
public:
	CRateOfDeathSimple( const IAIObject* const pOwner );
	~CRateOfDeathSimple();

	void Init( SmartScriptTable pTable );

	void SetTarget( IEntity* pTargetEntity );

	void Update( const float elapsedSeconds );

#ifdef DEBUGDRAW_RATE_OF_DEATH_HELPER
	void DebugDraw();
#endif

	bool CanDamageTarget() const;

	Vec3 GetTargetOffset() const;

	void SetMissOffsetUpdateIntervalSeconds( const float updateIntervalSeconds );
	float GetMissOffsetUpdateIntervalSeconds() const;

	void SetMissMinRange( const float minRange );
	float GetMissMinRange() const;

	void SetMissMaxRange( const float maxRange );
	float GetMissMaxRange() const;

	void SetHitOffsetUpdateIntervalSeconds( const float updateIntervalSeconds );
	float GetHitOffsetUpdateIntervalSeconds() const;

	void SetHitMinRange( const float minRange );
	float GetHitMinRange() const;

	void SetHitMaxRange( const float maxRange );
	float GetHitMaxRange() const;

private:
	void UpdateCanDamageTarget();
	void UpdateTargetOffset( const float elapsedSeconds );
	Vec3 CalculateTargetOffset( const float minRange, const float maxRange ) const;

private:
	const IAIObject* const m_pOwner;

	CRateOfDeathHelper_AttackerInfo m_attackerInfo;
	CRateOfDeathHelper_Target m_rateOfDeathTarget;

	Vec3 m_targetOffset;

	float m_missOffsetIntervalSeconds;
	float m_missOffsetNextUpdateSeconds;

	float m_hitOffsetIntervalSeconds;
	float m_hitOffsetNextUpdateSeconds;

	float m_hitMinRange;
	float m_hitMaxRange;
	float m_missMinRange;
	float m_missMaxRange;

	bool m_canDamageTarget;
};


#endif
