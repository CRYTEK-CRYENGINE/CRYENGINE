// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#ifndef _WEAPON_OFFSET_H_
#define _WEAPON_OFFSET_H_

#include <CrySerialization/IArchive.h>


struct SWeaponOffset
{
	SWeaponOffset() {}
	explicit SWeaponOffset(type_zero)
		:	m_position(ZERO)
		,	m_rotation(ZERO) {}
	explicit SWeaponOffset(QuatT transformation)
		:	m_position(transformation.t)
		,	m_rotation(Ang3(transformation.q)) {}
	SWeaponOffset(Vec3 pos, Ang3 ang)
		:	m_position(pos)
		,	m_rotation(ang) {}

	void Serialize(Serialization::IArchive& ar);

	Vec3 m_position;
	Ang3 m_rotation;
};



inline QuatT ToQuatT(const SWeaponOffset& offset)
{
	QuatT result;
	result.t = offset.m_position;
	result.q = Quat(offset.m_rotation);
	return result;
}



inline SWeaponOffset ToRadians(const SWeaponOffset& offset)
{
	SWeaponOffset result;
	result.m_position = offset.m_position;
	result.m_rotation = DEG2RAD(offset.m_rotation);
	return result;
}



inline Ang3 Lerp(Ang3 a, Ang3 b, float x)
{
	return Ang3(Quat::CreateNlerp(Quat(a), Quat(b), x));
}



class CWeaponOffsetState
{
public:
	CWeaponOffsetState();

	void SetState(const SWeaponOffset& offset, float transitionTime);
	SWeaponOffset Blend(float deltaTime);

private:
	SWeaponOffset ComputeCurrentOffset();

	SWeaponOffset m_current;
	SWeaponOffset m_last;
	float m_transitiontime;
	float m_time;
};



class CWeaponOffsetStack
{
public:
	typedef int TOffsetId;

private:
	struct SWeaponOffsetLayer
	{
		SWeaponOffset m_offset;
		TOffsetId m_id;
		uint32 m_layer;
	};
	typedef std::vector<SWeaponOffsetLayer> TWeaponOffsetLayers;

public:
	CWeaponOffsetStack();
	
	SWeaponOffset Blend(float deltaTime);
	void SetOffset(const SWeaponOffset& offset, float blendTime);
	TOffsetId PushOffset(const SWeaponOffset& offset, uint32 layer, float blendTime);
	void PopOffset(TOffsetId offsetId, float blendTime);

private:
	TWeaponOffsetLayers m_weaponOffsetLayers; 
	CWeaponOffsetState m_state;
	TOffsetId m_nextId;
};



#endif
