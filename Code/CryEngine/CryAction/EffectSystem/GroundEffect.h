// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#ifndef __GROUNDEFFECT_H__
#define __GROUNDEFFECT_H__

#include "../IEffectSystem.h"
#include "CryActionPhysicQueues.h"

struct IParticleEffect;

class CGroundEffect : public IGroundEffect
{
public:

	CGroundEffect(IEntity* pEntity);
	virtual ~CGroundEffect();

	// IGroundEffect
	virtual void SetHeight(float height);
	virtual void SetHeightScale(float sizeScale, float countScale);
	virtual void SetBaseScale(float sizeScale, float countScale, float speedScale = 1.0f);
	virtual void SetInterpolation(float speed);
	virtual void SetFlags(int flags);
	virtual int  GetFlags() const;
	virtual bool SetParticleEffect(const char* pName);
	virtual void SetInteraction(const char* pName);
	virtual void Update();
	virtual void Stop(bool stop);
	// ~IGroundEffect

	void OnRayCastDataReceived(const QueuedRayID& rayID, const RayCastResult& result);

protected:

	void        SetSpawnParams(const SpawnParams& params);
	void        Reset();

	inline bool DebugOutput() const
	{
		static ICVar* pVar = gEnv->pConsole->GetCVar("g_groundeffectsdebug");

		CRY_ASSERT(pVar);

		return pVar->GetIVal() > 0;
	}

	inline bool DeferredRayCasts() const
	{
		static ICVar* pVar = gEnv->pConsole->GetCVar("g_groundeffectsdebug");

		CRY_ASSERT(pVar);

		return pVar->GetIVal() != 2;
	}

	IEntity*         m_pEntity;

	IParticleEffect* m_pParticleEffect;

	int              m_flags, m_slot, m_surfaceIdx, m_rayWorldIntersectSurfaceIdx;
	QueuedRayID      m_raycastID;

	string           m_interaction;

	bool             m_active                 : 1;

	bool             m_stopped                : 1;

	bool             m_validRayWorldIntersect : 1;

	float            m_height, m_rayWorldIntersectHeight, m_ratio;

	float            m_sizeScale, m_sizeGoal;

	float            m_countScale;

	float            m_speedScale, m_speedGoal;

	float            m_interpSpeed;

	float            m_maxHeightCountScale, m_maxHeightSizeScale;
};
#endif //__GROUNDEFFECT_H__
