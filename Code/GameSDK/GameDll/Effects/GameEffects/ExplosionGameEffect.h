// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef _EXPLOSION_SCREEN_GAME_EFFECT_
#define _EXPLOSION_SCREEN_GAME_EFFECT_

#pragma once

// Includes
#include "GameEffect.h"
#include "Effects/GameEffectsSystem.h"

// Forward declares
struct SExplosionContainer;

class CDeferredExplosionEffect
{

public:

	enum EDeferredEffectType
	{
		eDET_RadialBlur = 0,
	};

private:

	struct SQueuedRayInfo
	{
		SQueuedRayInfo()
			: explosionPos(ZERO)
			, distance(0.0f)
			, effectMaxDistance(0.0f)
			, rayID(0)
			, effectType(eDET_RadialBlur)
			, request(0)
		{

		}

		Vec3				explosionPos;
		QueuedRayID			rayID;
		EDeferredEffectType effectType;
		float				distance;
		float				effectMaxDistance;
		uint32				request;
	};

public:

	CDeferredExplosionEffect()
		: m_requestCounter(0)
	{

	}

	~CDeferredExplosionEffect();

	void RequestRayCast(CDeferredExplosionEffect::EDeferredEffectType effectType, const Vec3 &startPos, const Vec3 &dir, float distance, float effectMaxDistance, int objTypes, int flags, IPhysicalEntity **pSkipEnts, int nSkipEnts); 
	void OnRayCastDataReceived(const QueuedRayID& rayID, const RayCastResult& result);

private:

	int FindRequestedRay(const QueuedRayID& rayID) const;
	void FreeOldestRequestIfNeeded();

	void TriggerRadialBlur(const Vec3& radialBlurCenter, float maxBlurDistance, float distance);

	const static int kMaxQueuedRays = 4;
	CryFixedArray<SQueuedRayInfo, kMaxQueuedRays> m_queuedRays;

	uint32		m_requestCounter;						
};

//==================================================================================================
// Name: CExplosionGameEffect
// Desc: Explosion game effect
// Author: James Chilvers
//==================================================================================================
class CExplosionGameEffect : public CGameEffect
{
public:
	CExplosionGameEffect();
	~CExplosionGameEffect();

	void	Initialise(const SGameEffectParams* gameEffectParams = NULL) override;
	void	Release() override;
	void	Update(float frameTime) override;

	void	Explode(SExplosionContainer &explosionContainer);
	void	SetCutSceneActive(bool isCutSceneActive) { m_cutSceneActive = isCutSceneActive; }

#if DEBUG_GAME_FX_SYSTEM
	static void  DebugOnInputEvent(int keyId);
	static void	 DebugDisplay(const Vec2& textStartPos,float textSize,float textYStep);
#endif
	
	static void LoadStaticData(IItemParamsNode* pRootNode);
	static void ReloadStaticData(IItemParamsNode* pRootNode);
	static void ReleaseStaticData();

	const char* GetName() const override { return "Explosion"; }

	void		SpawnMaterialEffect(const SExplosionContainer &explosionContainer);

	virtual void GetMemoryUsage( ICrySizer *pSizer ) const override
	{
		pSizer->AddObject(this, sizeof(*this));	
	}
	virtual void ResetRenderParameters() override;
private:

	void		SpawnParticleEffect(const SExplosionContainer &explosionContainer);
	void		SpawnScreenExplosionEffect(const SExplosionContainer &explosionContainer);
	void		SpawnCharacterEffects(const SExplosionContainer &explosionContainer);
	void		QueueMaterialEffect(SExplosionContainer &explosionContainer);

	bool		IsPlayer();

	CDeferredExplosionEffect m_deferredScreenEffects;

	bool		m_cutSceneActive;

};//------------------------------------------------------------------------------------------------

//==================================================================================================
// Name: SExplosionGameEffectParams
// Desc: Explosion game effect parameters
// Author: James Chilvers
//==================================================================================================
struct SExplosionGameEffectParams : public SGameEffectParams
{
	SExplosionGameEffectParams()
	{
		
	}

};//------------------------------------------------------------------------------------------------

#endif // _EXPLOSION_SCREEN_GAME_EFFECT_
