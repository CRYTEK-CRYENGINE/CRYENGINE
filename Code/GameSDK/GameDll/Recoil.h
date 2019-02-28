// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
-------------------------------------------------------------------------
$Id$
$DateTime$

-------------------------------------------------------------------------
History:
- 23:09:2009   : Created by Filipe Amim

*************************************************************************/

#pragma once

#ifndef RECOIL_H
#define RECOIL_H

#include "ItemSystem.h"
#include "ItemParamsRegistration.h"

class CWeapon;
class CActor;
class CFireMode;

struct SRecoilParams
{
	SRecoilParams();
	void Reset(const XmlNodeRef& paramsNode, bool defaultInit = true);

#define RECOIL_PARAMS_MEMBERS(f) \
	f(float,	max_recoil) \
	f(float,	attack) \
	f(float,	first_attack) \
	f(float,	decay) \
	f(float,	end_decay) \
	f(float,	recoil_time) \
	f(Vec2,		max) \
	f(float,	randomness) \
	f(float,	tilt) \
	f(float,	impulse) \
	f(float,	recoil_crouch_m) \
	f(float,	recoil_jump_m) \
	f(float,	recoil_holdBreathActive_m)

	REGISTER_STRUCT(RECOIL_PARAMS_MEMBERS, SRecoilParams)

	void GetMemoryUsage(ICrySizer * s) const;
};

struct SRecoilHints
{
	void Reset(const XmlNodeRef& paramsNode, bool defaultInit = true);

#define RECOIL_HINT_PARAMS(f)
#define RECOIL_HINT_PARAM_VECTORS(f) \
	f(std::vector<Vec2>, hints)

	REGISTER_STRUCT(RECOIL_HINT_PARAM_VECTORS, SRecoilHints)
	void GetMemoryUsage(ICrySizer * s) const;
};

struct SProceduralRecoilParams
{
	SProceduralRecoilParams();

	void Reset(const XmlNodeRef& paramsNode, bool defaultInit = true);

#define PROCEDURALRECOIL_PARAMS_MEMBERS(f) \
	f(float,	duration) \
	f(float,	strength) \
	f(float,	kickIn) \
	f(int,		arms) \
	f(bool,		enabled) \
	f(float,	dampStrength) \
	f(float,	fireRecoilTime) \
	f(float,	fireRecoilStrengthFirst) \
	f(float,	fireRecoilStrength) \
	f(float,	angleRecoilStrength) \
	f(float,	randomness)

	REGISTER_STRUCT(PROCEDURALRECOIL_PARAMS_MEMBERS, SProceduralRecoilParams)

	void GetMemoryUsage(ICrySizer * s) const {}
};


struct SSpreadParams
{
	SSpreadParams();
	void Reset(const XmlNodeRef& paramsNode, bool defaultInit = true);

#define SPREAD_PARAMS_MEMBERS(f) \
	f(float, min) \
	f(float, max) \
	f(float, attack) \
	f(float, decay) \
	f(float, end_decay) \
	f(float, speed_m) \
	f(float, speed_holdBreathActive_m) \
	f(float, rotation_m) \
	f(float, spread_crouch_m) \
	f(float, spread_jump_m) \
	f(float, spread_slide_m) \
	f(float, spread_holdBreathActive_m)

	REGISTER_STRUCT(SPREAD_PARAMS_MEMBERS, SSpreadParams)

	void GetMemoryUsage(ICrySizer * s) const {}
};



struct SSpreadModParams
{
	SSpreadModParams();
	void Reset(const XmlNodeRef& paramsNode, bool defaultInit = true);

	float min_mod;
	float max_mod;
	float attack_mod;
	float decay_mod;
	float end_decay_mod;
	float speed_m_mod;
	float speed_holdBreathActive_m_mod;
	float rotation_m_mod;

	//Stance modifiers
	float spread_crouch_m_mod;
	float spread_jump_m_mod;
	float spread_slide_m_mod;
	float spread_holdBreathActive_m_mod;

	void GetMemoryUsage(ICrySizer* s) const {}
};



struct SRecoilModParams
{
	SRecoilModParams();
	void Reset(const XmlNodeRef& paramsNode, bool defaultInit = true);

	float max_recoil_mod;
	float attack_mod;
	float first_attack_mod;
	float decay_mod;
	float end_decay_mod;
	Vec2 max_mod;
	float impulse_mod;

	//Stance modifiers
	float recoil_crouch_m_mod;
	float recoil_jump_m_mod;

	float recoil_holdBreathActive_m_mod;

	void GetMemoryUsage(ICrySizer* s) const {}
};



class CRecoil
{
public:
	CRecoil();

	void Init(CWeapon* pWeapon, CFireMode* pFiremode);
	void Reset(bool spread);

	void Update(CActor* pOwnerActor, float frameTime, bool weaponFired, int frameId, bool firstShot);
	void SetRecoilMultiplier(float multiplier) { m_recoilMultiplier = multiplier; }
	float GetRecoilmultiplier() const { return m_recoilMultiplier; }
	void SetSpreadMultiplier(float multiplier) { m_spreadMultiplier = multiplier; }
	float GetRecoil() const { return m_recoil; }
	float GetSpread() const;

	ILINE float GetMinSpread() const { return m_spreadParams.min; }
	ILINE float GetMaxSpread() const { return m_spreadParams.max; }

	ILINE void ApplyMaxSpread(float multiplier = 1.f) { m_spread = multiplier * m_spreadParams.max; m_maxSpreadMultiplier = max(multiplier, 1.f); }

	void RecoilImpulse(const Vec3& firingPos, const Vec3& firingDir);

	void GetMemoryUsage(ICrySizer* s) const;


	void PatchSpreadMod(const SSpreadModParams& spreadMod, const SSpreadParams& originalSpreadParams, float modMultiplier);
	void ResetSpreadMod(const SSpreadParams& originalSpreadParams);
	void PatchRecoilMod(const SRecoilModParams& recoilMod, const SRecoilParams& originalRecoilParams, float modMultiplier);
	void ResetRecoilMod(const SRecoilParams& originalRecoilParams, const SRecoilHints* pRecoilHints);

	ILINE float CalculateParamModValue(const float& baseValue, const float& modifierValue, const float& modifierMultiplier)
	{
		return baseValue - (baseValue - baseValue * modifierValue) * modifierMultiplier;
	}

#ifndef _RELEASE
	void DebugUpdate(float frameTime) const;
#endif

private:
	void RecoilShoot(bool firstShot, float maxRecoil);
	void UpdateRecoil(float recoilScale, float maxRecoil, bool weaponFired, bool weaponFiring, float frameTime);
	void SpreadShoot();
	void UpdateSpread(bool weaponFired, bool weaponFiring, float frameTime);
	float GetRecoilScale(const CActor& weaponOwner) const;
	float GetMaxRecoil() const;

	bool IsSingleFireMode() const;
	bool IsHoldingBreath() const;

	void ResetRecoilInternal();
	void ResetSpreadInternal();


	float m_recoil;
	float m_attack;
	float m_recoil_time;
	Vec3 m_recoil_offset;
	Vec3 m_recoil_dir;
	int m_recoil_dir_idx;
	float m_spread;
	float m_recoilMultiplier;
	float m_spreadMultiplier;
	float m_maxSpreadMultiplier;

	SRecoilParams m_recoilParams;
	const SRecoilHints*  m_pRecoilHints;
	SSpreadParams m_spreadParams;

	bool m_singleShot;
	bool m_useSpreadMultiplier;
	bool m_useRecoilMultiplier;

	CFireMode* m_pFireMode;
	CWeapon* m_pWeapon;
};


#endif
