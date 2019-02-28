// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Bullet

-------------------------------------------------------------------------
History:
- 12:10:2005   11:15 : Created by Márcio Martins

*************************************************************************/
#pragma once

#ifndef __BULLET_H__
#define __BULLET_H__

#include "Projectile.h"

struct ISkeletonPose;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
//Debug Bullet penetration

#if !defined(_RELEASE)
	#define DEBUG_BULLET_PENETRATION
#endif

#define BULLET_PENETRATION_BACKSIDE_FX_ENABLED_SP	1

#ifdef DEBUG_BULLET_PENETRATION

#define MAX_DEBUG_BULLET_HITS 64
#define DEFAULT_DEBUG_BULLET_HIT_LIFETIME 5.0f

struct SDebugBulletPenetration
{
private:

	struct SDebugBulletHit
	{
		SDebugBulletHit()
			: hitPosition(ZERO)
			, bulletDirection(ZERO)
			, damage(0.0f)
			, lifeTime(0.0f)
			, surfacePierceability(0)
			, isBackFaceHit(false)
			, stoppedBullet(false)
			, tooThick(false)
		{
		}

		Vec3 hitPosition;
		Vec3 bulletDirection;

		float damage;
		float lifeTime;

		int8 surfacePierceability;
		bool isBackFaceHit;
		bool stoppedBullet;
		bool tooThick;
	};

public:

	SDebugBulletPenetration()
		: m_nextHit(0)
	{
	}

	void AddBulletHit(const Vec3& hitPosition, const Vec3& hitDirection, float currentDamage, int8 surfacePierceability, bool isBackFace, bool stoppedBullet, bool tooThick);
	void Update(float frameTime);

private:

	const char* GetPenetrationLevelByPierceability(int8 surfacePierceability) const;

	SDebugBulletHit m_hitsList[MAX_DEBUG_BULLET_HITS];

	uint32 m_nextHit;
};

#endif //DEBUG_BULLET_PENETRATION
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct SPhysicsRayWrapper;

class CBullet : public CProjectile
{
private: 
	typedef CProjectile BaseClass;

	struct SBackHitInfo
	{
		Vec3 pt;	
	};

public:
	static void StaticInit();
	static void StaticShutdown();

public:
	CBullet();
	virtual ~CBullet();

	// CProjectile
	virtual void HandleEvent(const SGameObjectEvent &);
	virtual void SetParams(const SProjectileDesc& projectileDesc);
	virtual void ReInitFromPool();
	virtual bool IsAlive() const;

	virtual void SetDamageCap(float cap) { m_damageCap = cap; };
	virtual void UpdateLinkedDamage(EntityId hitActorId, float totalAccumDamage);
	// ~CProjectile

#ifdef DEBUG_BULLET_PENETRATION
	static void UpdateBulletPenetrationDebug(float frameTime)
	{
		s_debugBulletPenetration.Update(frameTime);
	}
#endif

	static IEntityClass*	EntityClass;

protected:

	virtual void SetUpParticleParams(IEntity* pOwnerEntity, uint8 pierceabilityModifier);
	void ProcessHit(CGameRules& gameRules, const EventPhysCollision& collision, IEntity& target, float damage, int hitMatId, const Vec3& hitDir);
	bool CheckForPreviousHit(EntityId targetId, float& damage);
	bool ShouldSpawnBackSideEffect(IEntity* pHitTarget);
	void DestroyAtHitPosition(const Vec3& hitPosition);
	ILINE int16 GetBulletPierceability() const { return m_bulletPierceability; };

private:

	struct SActorHitInfo
	{
		SActorHitInfo(EntityId _actorId, float _accumDamage, bool _previousHit) :
			linkedAccumDamage(_accumDamage), 
			hitActorId(_actorId),
			previousHit(_previousHit) {};

		float			linkedAccumDamage;
		EntityId	hitActorId;
		bool			previousHit;
	};

	void EmitUnderwaterTracer(const Vec3& pos, const Vec3& destination);
	bool FilterFriendlyAIHit(IEntity* pHitTarget);
	float GetFinalDamage(const Vec3& hitPos) const;
	ILINE float GetDamageAfterPenetrationFallOff() const { return ((float)m_damage - m_accumulatedDamageFallOffAfterPenetration); };

	void HandlePierceableSurface(const EventPhysCollision* pCollision, IEntity* pHitTarget, const Vec3& hitDirection, bool bProcessedCollisionEvent);
	bool ShouldDestroyBullet() const;

	bool RayTraceGeometry(const EventPhysCollision* pCollision, const Vec3& pos, const Vec3& hitDirection, SBackHitInfo* pBackHitInfo);
	int GetRopeBoneId(const EventPhysCollision& collision, IEntity& target, IPhysicalEntity* pRopePhysicalEntity) const;

	static SPhysicsRayWrapper* s_pRayWrapper;

#ifdef DEBUG_BULLET_PENETRATION
	//Bullet penetration debug
	static SDebugBulletPenetration s_debugBulletPenetration;
#endif

	std::vector <SActorHitInfo> m_hitActors;

	float m_damageCap;
	
	float m_damageFallOffStart;
	float m_damageFallOffAmount;
	float m_damageFalloffMin;
	float m_pointBlankAmount;
	float m_pointBlankDistance;
	float m_pointBlankFalloffDistance;

	float m_accumulatedDamageFallOffAfterPenetration;
	int16	m_bulletPierceability;
	int16	m_penetrationCount;

	bool m_alive;
	bool m_ownerIsPlayer;
	bool m_backSideEffectsDisabled;
};


#endif // __BULLET_H__