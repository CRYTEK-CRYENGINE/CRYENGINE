// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$
Description: Damage Effect Controller

-------------------------------------------------------------------------
History:
- 02:09:2009   15:00 : Created by Claire Allan

*************************************************************************/

#ifndef __DAMAGE_EFFECT_CONTROLLER_H__
#define __DAMAGE_EFFECT_CONTROLLER_H__

#define MAX_NUM_DAMAGE_EFFECTS (8) //This needs to match the size of activeEffectsBitfield and effectsResetSwitchBitfield

struct HitInfo;
class CDamageEffect;
class CActor;
struct IItemParamsNode;
struct IAttachment;

#include "IGameObject.h"
#include <CryAction/IMaterialEffects.h>

class CDamageEffectController
{
public:
	CDamageEffectController();
	~CDamageEffectController();

	void Init(CActor* m_ownerActor);
	void OnHit(const HitInfo* hitInfo);
	void OnKill(const HitInfo* hitInfo);
	void OnRevive();
	void UpdateEffects(float frameTime);
	void NetSerialiseEffects(TSerialize ser, EEntityAspects aspect);
	uint8 GetActiveEffects() const { return m_activeEffectsBitfield; }
	uint8 GetEffectResetSwitch() const { return m_effectsResetSwitchBitfield; }
	uint8 GetEffectsKilled() const { return m_effectsKillBitfield; }
	void SetActiveEffects(uint8 active);
	void SetEffectResetSwitch(uint8 reset);
	void SetEffectsKilled(uint8 killed);

	static uint32 CreateHash(const char* string);

protected:

	CDamageEffect* m_effectList[MAX_NUM_DAMAGE_EFFECTS];
	int m_associatedHitType[MAX_NUM_DAMAGE_EFFECTS];
	float m_minDamage[MAX_NUM_DAMAGE_EFFECTS];

	CActor* m_ownerActor;

	uint8 m_activeEffectsBitfield;
	uint8 m_effectsResetSwitchBitfield;
	uint8 m_effectsKillBitfield;
	bool  m_allowSerialise;
};

class CDamageEffect
{
public:
	CDamageEffect() { m_ownerActor = NULL; }
	virtual ~CDamageEffect() {};

	virtual void Init(CActor* actor, const IItemParamsNode* params) { m_ownerActor = actor; }
	virtual void Enter() {};
	virtual void Leave() {};
	virtual void Reset() {};
	virtual void OnKill() {};
	virtual bool Update(float frameTime) { return true; };

protected:
	CActor* m_ownerActor;
};

class CKVoltEffect : public CDamageEffect
{
private:
	typedef CDamageEffect inherited;

public:
	static uint32 s_hashId;

	virtual ~CKVoltEffect() { SAFE_RELEASE(m_particleEmitter); SAFE_RELEASE(m_particleEffect); SAFE_RELEASE(m_screenEffect); }

	void Init(CActor* actor, const IItemParamsNode* params);
	void Enter();
	void Leave();
	void Reset(); 
	bool Update(float frameTime);
	
protected:
	void ResetScreenEffect();
	//void FadeCrosshair();
	void DisableScopeReticule();
	IAttachment* GetScreenAttachment();

	IParticleEffect* m_screenEffect;
	IParticleEffect* m_particleEffect;
	IParticleEmitter* m_particleEmitter;
	float m_timer;
	float m_effectTime;
	float m_disabledCrosshairTime;
};

class CTinnitusEffect : public CDamageEffect
{
private:
	typedef CDamageEffect inherited;

public:
	static uint32 s_hashId;

	void Init(CActor* actor, const IItemParamsNode* params);
	void Enter();
	void Reset();
	void Leave();
	bool Update(float frameTime);

protected:
	float m_timer;
	float m_tinnitusTime;
};

class CEntityTimerEffect : public CDamageEffect
{
private:
	typedef CDamageEffect inherited;

public:
	static uint32 s_hashId;

	void Init(CActor* actor, const IItemParamsNode* params);
	void Enter();
	bool Update(float frameTime);

protected:
	int m_entityTimerID;
	float m_initialTime;
	float m_timer;
};

#endif