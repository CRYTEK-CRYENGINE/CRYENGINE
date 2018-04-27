// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
Description: 

-------------------------------------------------------------------------
History:
- 27:07:2010	11:35 : Created by David Ramos
*************************************************************************/
#pragma once

#ifndef __SPECTACULAR_KILL_H__
#define __SPECTACULAR_KILL_H__

#include "GameParameters.h"
#include <SharedParams/ISharedParams.h>

class CActor;
class CPlayer;
struct ICharacterInstance;
struct AnimEventInstance;
struct IItemParamsNode;
struct IPersistantDebug;

#if !defined(_RELEASE)
	#define SPECTACULAR_KILL_DEBUG
#endif

class CSpectacularKill
{
public:
	CSpectacularKill();
	~CSpectacularKill();

	void															Init(CPlayer* pPlayer);
	void															ReadXmlData(const IItemParamsNode* pRootNode);

	void 															Update(float frameTime);

	void															Reset();
	static void												CleanUp();

	bool 															AnimationEvent(ICharacterInstance *pCharacter, const AnimEventInstance &event);

	const CActor*											GetTarget() const;
	CActor*														GetTarget() { return const_cast<CActor*>(static_cast<const CSpectacularKill*>(this)->GetTarget()); }

	Vec3															GetRelStartOffsetFromTarget() const;
	Vec3															GetStartPos() const;
	bool 															StartOnTarget(EntityId targetId);
	bool 															StartOnTarget(CActor* pTargetActor);
	void 															End(bool bKillerDied = false);

	bool															CanSpectacularKillOn(EntityId targetId) const;
	bool															CanSpectacularKillOn(const CActor* pTarget) const;
	ILINE bool												IsBusy() const { return (m_isBusy); }
	const QuatT&											GetPlayerAnimQuatT() const;

private:
	// Private typedefs
	enum EDeathBlowState
	{
		eDBS_None,
		eDBS_Pending,
		eDBS_Done,
	};

	struct SSpectacularKillAnimation
	{
		SSpectacularKillAnimation() 
			:	killerAnimation("")
			, victimAnimation("")
			, optimalDist(-1.0f)
			, targetToKillerAngle(-1.0f)
			, targetToKillerMinDot(0.342f) // [*DavidR | 27/Aug/2010] FixMe: cos 70º (140º cone), make it more strict when behaviors are ready
			, vKillerObstacleCheckOffset(ZERO)
			, fObstacleCheckLength(-1.0f)
		{
		}

		string	killerAnimation;
		string	victimAnimation;

		float		optimalDist;
		float		targetToKillerAngle;
		float		targetToKillerMinDot;

		Vec3		vKillerObstacleCheckOffset;	// this parameters modify the position of the killer end of the capsule used to check for obstacles
		float		fObstacleCheckLength; // This is the length of the obstacle check primitive, if negative, the distance between actors is used instead
	};
	typedef std::vector<SSpectacularKillAnimation> TSpectacularKillAnimVector;

	struct SSpectacularKillParams
	{
		SSpectacularKillParams()
			: pEnemyClass(NULL)
			, impulseBone(-1)
			, impulseScale(1.0f)
		{
		}

		IEntityClass* pEnemyClass;

		TSpectacularKillAnimVector animations;

		float impulseScale;
		int16 impulseBone;
	};
	typedef std::vector<SSpectacularKillParams> TSpectacularKillParamsVector;

public:
	BEGIN_SHARED_PARAMS(SSharedSpectacularParams)
		TSpectacularKillParamsVector paramsList;
	END_SHARED_PARAMS
private:

	struct SLastKillInfo
	{
		SLastKillInfo() : timeStamp(0.0f) {}

		float timeStamp;
		string killerAnim;
	};

	struct SPredNotValidAnim;

	void															DeathBlow(CActor& targetActor);
	const SSpectacularKillParams* 		GetParamsForClass(IEntityClass* pTargetClass) const;
	bool															GetAnimIndexOn(const SSpectacularKillParams* pTargetParams, int8& animIndex) const;
	bool 															CanExecuteOnTarget(const CActor* pTarget, const SSpectacularKillAnimation& anim) const;
	bool															GetValidAnim(const CActor* pTarget, SSpectacularKillAnimation& anim) const;
	void															ClearState();
	bool															ObstacleCheck(const Vec3& vKillerPos, const Vec3& vTargetPos, const SSpectacularKillAnimation& anim) const;
	IPersistantDebug*									BeginPersistantDebug() const;
	static void												DebugLog(const char* szFormat, ...);



	CPlayer*													m_pOwner;
	EntityId													m_targetId;
	SSharedSpectacularParamsConstPtr	m_pParams;					// Hold the params for this entity class
	EDeathBlowState										m_deathBlowState;
	bool															m_isBusy;

	static SLastKillInfo							s_lastKillInfo;
};

#endif // __SPECTACULAR_KILL_H__
