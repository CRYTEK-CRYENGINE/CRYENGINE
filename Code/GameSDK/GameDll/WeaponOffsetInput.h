// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#ifndef _WEAPON_OFFSET_INPUT_H_
#define _WEAPON_OFFSET_INPUT_H_



#include "IPlayerInput.h"



class CProceduralWeaponAnimation;



class CWeaponOffsetInput : public IInputEventListener
{
private:
	enum Mode
	{
		EMode_None,
		EMode_PositionOffset,
		EMode_RotationOffset,
		EMode_ZAxisOffset,
	};

	enum Sensibility
	{
		ESensibility_Slow,
		ESensibility_Medium,
		ESensibility_Fast,
	};

	enum Hand
	{
		EHand_Right,
		EHand_Left,
	};

public:
	typedef std::shared_ptr<CWeaponOffsetInput> TWeaponOffsetInput;

public:
	CWeaponOffsetInput();

	virtual void Update();
	virtual bool OnInputEvent(const SInputEvent &inputEvent);

	static TWeaponOffsetInput Get();

	void SetRightDebugOffset(const SWeaponOffset& offset);
	void AddRightDebugOffset(const SWeaponOffset& offset);
	SWeaponOffset GetRightDebugOffset() const {return m_debugRightOffset;}

	void SetLeftDebugOffset(const SWeaponOffset& offset);
	void AddLeftDebugOffset(const SWeaponOffset& offset);
	SWeaponOffset GetLeftDebugOffset() const {return m_debugLeftOffset;}

private:
	void AddOffset(Vec3 pos, Ang3 ang);

	SWeaponOffset m_debugRightOffset;
	SWeaponOffset m_debugLeftOffset;

	Vec2 m_offset;
	Mode m_mode;
	Sensibility m_sensibility;
	Hand m_hand;
};


#endif
