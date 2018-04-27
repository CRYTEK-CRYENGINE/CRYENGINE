// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __RATE_OF_DEATH_HELPER__H__
#define __RATE_OF_DEATH_HELPER__H__

#include <CryAISystem/IAgent.h>



//////////////////////////////////////////////////////////////////////////
class CRateOfDeathHelper_HealthThreshold
{
public:
	CRateOfDeathHelper_HealthThreshold();
	~CRateOfDeathHelper_HealthThreshold();

	void Reset();

	void SetStayAliveTime( const float stayAliveTime );
	float GetStayAliveTime() const;

	void SetHealth( const float health, const float maxHealth );
	void Update( const float elapsedSeconds );

	bool IsDamageAllowed() const;

	float GetNormalizedHealth() const;
	float GetNormalizedHealthThreshold() const;

#ifdef DEBUGDRAW_RATE_OF_DEATH_HELPER
	void DebugDraw( float& drawPosY );
#endif

private:
	void UpdateStance();
	void UpdateIsInVehicle();

private:
	float m_stayAliveTime;
	float m_lastMaxHealth;

	float m_normalizedHealth;
	float m_normalizedHealthThreshold;
};



//////////////////////////////////////////////////////////////////////////
class CRateOfDeathHelper_AttackerInfo
{
public:
	CRateOfDeathHelper_AttackerInfo();
	~CRateOfDeathHelper_AttackerInfo();

	void Reset();

	void SetPosition( const Vec3& attackerPosition );
	const Vec3& GetPosition() const;

	void SetAttackRange( const float attackRange );
	float GetAttackRange() const;

	void SetUsingMelee( const bool usingMelee );
	bool GetUsingMelee() const;

	EAITargetZone CalculateZone( const Vec3 position ) const;

private:
	Vec3 m_position;
	float m_attackRange;
	bool m_usingMelee;
};



//////////////////////////////////////////////////////////////////////////
class CRateOfDeathHelper_Target
{
public:
	CRateOfDeathHelper_Target();
	~CRateOfDeathHelper_Target();

	void Reset();

	void SetTarget( const IAIObject* pTarget );

	const IAIObject* GetAIObject() const;
	const IAIActor* GetAIActor() const;

	bool IsActorTarget() const;

	EStance GetStance() const;
	EAITargetZone GetZone() const;
	bool GetIsInVehicle() const;

	void Update( const float elapsedTimeSeconds, const CRateOfDeathHelper_AttackerInfo& attacker );

	bool IsDamageAllowedByHealthThreshold() const;
	bool IsDamageAllowedByMercyTime() const;

	void SetStayAliveTimeFactor( const float stayAliveTimeFactor );
	float GetStayAliveTimeFactor() const;

	float GetStayAliveTime() const;

#ifdef DEBUGDRAW_RATE_OF_DEATH_HELPER
	void DebugDraw( float& drawPosY );
#endif

private:
	void UpdateStance();
	void UpdateIsInVehicle();
	void UpdateZone( const CRateOfDeathHelper_AttackerInfo& attacker );
	void UpdateHealth( const float elapsedTimeSeconds );
	void UpdateStayAliveTime( const CRateOfDeathHelper_AttackerInfo& attacker );

	float CalculateStayAliveTime( const CRateOfDeathHelper_AttackerInfo& attacker ) const;
	float CalculateStayAliveTime_Movement( const CRateOfDeathHelper_AttackerInfo& attacker ) const;
	float CalculateStayAliveTime_Stance( const CRateOfDeathHelper_AttackerInfo& attacker ) const;
	float CalculateStayAliveTime_Direction( const CRateOfDeathHelper_AttackerInfo& attacker ) const;
	float CalculateStayAliveTime_Zone( const CRateOfDeathHelper_AttackerInfo& attacker ) const;

	bool IsPlayer() const;

private:
	const IAIObject* m_pTarget;
	const IAIActor* m_pTargetActor;

	EStance m_stance;
	EAITargetZone m_zone;
	bool m_isInVehicle;

	float m_stayAliveTimeFactor;

	CRateOfDeathHelper_HealthThreshold m_healthThresholdHelper;
};




#endif