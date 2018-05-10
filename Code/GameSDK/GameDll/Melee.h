// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Beam Fire Mode Implementation

-------------------------------------------------------------------------
History:
- 23:3:2006   13:02 : Created by Márcio Martins

*************************************************************************/

#pragma once

#ifndef __MELEE_H__
#define __MELEE_H__

#include "IGameRulesSystem.h"
#include "GameParameters.h"
#include "MeleeCollisionHelper.h"
#include "FireModeParams.h"
#include "ICryMannequin.h"
#include "GameTypeInfo.h"

class CWeapon;
class CActor;

class CMelee :
	public IMeleeCollisionHelperListener
{
	struct StopAttackingAction;
	struct DelayedImpulse;

public:
	class CMeleeAction : public TAction<SAnimationContext>
	{
	public:
		DEFINE_ACTION("MeleeAction");

		typedef TAction<SAnimationContext> BaseClass;

		CMeleeAction( int priority, CWeapon* pWeapon, FragmentID fragmentID);

		virtual void Exit() override;
		void OnHitResult(CActor* pOwnerActor, bool hit);
		void NetEarlyExit();
		void StopAttackAction();

	private:
		EntityId m_weaponId;
		bool m_FinalStage;
		bool m_bCancelled;
	};

public:
	CRY_DECLARE_GTI_BASE(CMelee);

	CMelee();
	virtual ~CMelee();

	void Release();

	void InitMeleeMode(CWeapon* pWeapon, const SMeleeModeParams* pParams);
	void InitFragmentData();

	void Update(float frameTime, uint32 frameId);
	void GetMemoryUsage(ICrySizer * s) const;

	void Activate(bool activate);

	bool CanAttack() const;
	void StartAttack();
	bool IsAttacking() const { return m_attacking || m_netAttacking; };

	void NetAttack();
	int GetDamage() const;
	float GetRange()  const;
	ILINE void SetDelayTimer( float timer ) { m_delayTimer = timer; }
	
	//IMeleeCollisionHelperListener
	virtual void OnSuccesfulHit(const ray_hit& hitResult);
	virtual void OnFailedHit(); 
	//~IMeleeCollisionHelperListener

	void CloseRangeAttack(bool closeRangeAttack);

	EntityId GetNearestTarget();
	ILINE static const char* GetWeaponComponentType() { return "Melee"; }
	void OnMeleeHitAnimationEvent();

	ILINE const SMeleeParams& GetMeleeParams() const { return m_pMeleeParams->meleeparams; }
	ILINE const SMeleeModeParams* GetMeleeModeParams() const { return m_pMeleeParams; }

	static void PlayHitMaterialEffect(const Vec3 &position, const Vec3 &normal, bool bBoostedMelee, int surfaceIdx);
	ILINE static void SetMeleeDelay(float fDelay) { s_fNextAttack = fDelay; }

	float GetDuration() const;
	float GetDelay() const;

	ILINE bool IsMeleeWeapon() const { return m_pMeleeParams->meleeparams.is_melee_weapon; }

	float GetImpulseStrength();

private:

	float GetImpulseAiToPlayer() const;
	void RequestAlignmentToNearestTarget();
	void PerformMelee(const Vec3 &pos, const Vec3 &dir, bool remote);
	bool PerformCylinderTest(const Vec3 &pos, const Vec3 &dir, bool remote);
	int Hit(const Vec3 &pt, const Vec3 &dir, const Vec3 &normal, IPhysicalEntity *pCollider, EntityId collidedEntityId, int partId, int ipart, int surfaceIdx, bool remote);
	void Impulse(const Vec3 &pt, const Vec3 &dir, const Vec3 &normal, IPhysicalEntity *pCollider, EntityId collidedEntityId, int partId, int ipart, int surfaceIdx, int hitTypeID, int iPrim);

	void ApplyMeleeDamageHit( const SCollisionTestParams& collisionParams, const ray_hit& hitResult );
	void ApplyMeleeDamage(const Vec3& point, const Vec3& dir, const Vec3& normal, IPhysicalEntity* physicalEntity,
		EntityId entityID, int partId, int ipart, int surfaceIdx, bool remote, int iPrim);

	void ApplyMeleeEffects(bool hit);
	bool IsFriendlyHit(IEntity* pShooter, IEntity* pTarget);

	bool DoSlideMeleeAttack(CActor* pOwnerActor);
	float GetMeleeDamage() const;

	const ItemString &SelectMeleeAction() const;
	void GenerateAndQueueMeleeAction() const;
	void GenerateAndQueueMeleeActionForStatus( const SMeleeTags::TTagParamsContainer& tagContainer ) const;
	SMeleeTags::SMeleeFragData GenerateFragmentData( const SMeleeTags::TTagParamsContainer& tagContainer ) const;

	bool IsMeleeFilteredOnEntity(const IEntity& targetEntity) const;

	bool SwitchToMeleeWeaponAndAttack();

	bool StartMultiAnimMeleeAttack(CActor* pActor);

protected:

	enum EHitStatus
	{
		EHitStatus_Invalid,
		EHitStatus_ReceivedAnimEvent,
		EHitStatus_HaveHitResult
	};

	CWeapon* m_pWeapon;
	const SMeleeModeParams* m_pMeleeParams;

	float		m_useMeleeWeaponDelay;
	float		m_delayTimer;
	float		m_attackTurnAmount; //Camera lock/turn progression value [0-1]
	float		m_attackTurnAmountSmoothRate;
	float		m_attackTime;	//How long the melee attack has been active for
	mutable int	m_hitTypeID;
	bool		m_attacked;
	bool		m_netAttacking;
	bool		m_attacking;
	bool		m_slideKick;
	bool		m_shortRangeAttack;

	EHitStatus m_hitStatus;
	IActionController* m_piActionController;

	static float s_fNextAttack;

	static EntityId s_meleeSnapTargetId;
	static bool s_bMeleeSnapTargetCrouched;

	CMeleeCollisionHelper m_collisionHelper;

	ray_hit m_lastRayHit;
	SCollisionTestParams m_lastCollisionTest;

	CMeleeAction* m_pMeleeAction;
};

#endif //__MELEE_H__
