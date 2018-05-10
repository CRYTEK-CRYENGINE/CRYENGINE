// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description:
	Part of Player that controls stealth killing.

-------------------------------------------------------------------------
History:
- 15:10:2009: Created by Sven Van Soom

*************************************************************************/

#pragma once

#ifndef __STEALTH_KILL_H__
#define __STEALTH_KILL_H__

class CActor;
class CPlayer;
struct ICharacterInstance;
struct AnimEventInstance;
struct IItemParamsNode;

#if !defined(_RELEASE)
	#define STEALTH_KILL_DEBUG
#endif

class CStealthKill
{
private:

	friend class CActionStealthKill;

	struct SStealthKillAnimation
	{
		SStealthKillAnimation() 
			:	playerAnimation("")
			, enemyAnimation("")
			,	useKnife(true)
		{
		}

		string	playerAnimation;
		string	enemyAnimation;
		bool		useKnife;

	};

	typedef std::vector<CStealthKill::SStealthKillAnimation> TStealthKillAnimVector;

	struct SStealthKillParams
	{
		SStealthKillParams()
			: pEnemyClass(NULL)
			, impulseBone("")
			, impulseScale(1.0f)
			, extendedMaxDist(1.4f)
			, maxDist(1.4f)
			, maxHeightDiff(0.75f)
			, angleRange(70.f)
			, requiresCloak(true)
			, cloakInterference(false)
		{
		}

		IEntityClass* pEnemyClass;

		TStealthKillAnimVector animations;
		
		string impulseBone;
		float impulseScale;
		float extendedMaxDist;
		float maxDist;
		float maxHeightDiff;
		float angleRange;
		bool  requiresCloak;
		bool  cloakInterference;
	};

	typedef std::vector<CStealthKill::SStealthKillParams> TStealthKillParamsVector;

public:
	CStealthKill();

	void Init(CPlayer* pPlayer);
	ILINE bool IsBusy() const { return (m_isBusy); }
	void Enter(int enemyEntityId, int animIndex = -1);
	void Leave(CActor* pTarget);
	void Update(float frameTime);
	bool AnimationEvent(ICharacterInstance *pCharacter, const AnimEventInstance &event);
	enum ECanExecuteOn { CE_YES, CE_NO, CE_NO_BUT_DONT_DO_OTHER_ACTIONS_ON_IT };
	ECanExecuteOn CanExecuteOn(CActor* pTarget, bool checkExtendedRange) const;
	CActor* GetTarget();
	EntityId GetTargetId() const;
	void SetTargetForAttackAttempt(EntityId targetId);
	QuatT GetPlayerAnimQuatT();
	uint8	GetAnimIndex();

	void ReadXmlData(const IItemParamsNode* pRootNode, bool reloading = false);

	void Serialize(TSerialize ser);
	
	static void CleanUp();

	void Abort(EntityId victim = 0, EntityId victimMountedWeapon = 0);
	void ForceFinishKill();
	static void ConstructHitInfo(EntityId killerId, EntityId victimId, const Vec3 &forwardDir, HitInfo &result);

private:

	bool IsKillerAbleToExecute() const;

	void DeathBlow(CActor* pTarget);
	void DoDeathBlow(CActor* pTarget);
	void ResetState(IActor* pTarget);

	void DisableActorCollisions();
	void EnableActorCollisions();

	void SetStatsViewMode(bool followCameraBone);

	static const CStealthKill::SStealthKillParams* GetParamsForClass(IEntityClass* pTargetClass);
	static void AddPhysics(IPhysicalEntity **pArr, int &pos, CActor *pPlayer);

	QuatT			m_playerAnimQuatT;
	CPlayer*	m_pPlayer;

	float			m_attemptTimer;
	
	EntityId	m_targetId;
	uint8			m_targetPhysCounter;
	uint8			m_animIndex;
	bool			m_isBusy;
	bool			m_isDeathBlowDone;
	bool			m_bCollisionsIgnored;

	static TStealthKillParamsVector s_stealthKillParams;
	static float s_stealthKill_attemptRange;
	static float s_stealthKill_attemptTime;
	static float s_stealthKill_attemptSpeed;
	static bool s_dataLoaded;
};


#endif // __STEALTH_KILL_H__
