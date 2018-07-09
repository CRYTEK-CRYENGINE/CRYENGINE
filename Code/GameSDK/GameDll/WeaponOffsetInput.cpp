// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ProceduralWeaponAnimation.h"
#include "WeaponOffsetInput.h"
#include "Player.h"




CWeaponOffsetInput::CWeaponOffsetInput()
	:	m_debugRightOffset(ZERO)
	,	m_debugLeftOffset(ZERO)
	,	m_offset(ZERO)
	,	m_mode(EMode_None)
	,	m_sensibility(ESensibility_Fast)
	,	m_hand(EHand_Right)
{
}



void CWeaponOffsetInput::Update()
{
	float moveSpeed = 0;
	switch (m_sensibility)
	{
		case ESensibility_Slow:
			moveSpeed = 0.001f;
			break;
		case ESensibility_Medium:
			moveSpeed = 0.01f;
			break;
		case ESensibility_Fast:
			moveSpeed = 0.1f;
			break;
	}

	Vec2 offset = m_offset * gEnv->pTimer->GetFrameTime() * moveSpeed;

	switch (m_mode)
	{
		case EMode_PositionOffset:
			AddOffset(
				Vec3(offset.x, 0.0f, -offset.y),
				Ang3(0.0f, 0.0f, 0.0f));
			break;

		case EMode_RotationOffset:
			AddOffset(
				Vec3(0.0f, 0.0f, 0.0f),
				Ang3(-offset.y, 0.0f, -offset.x));
			break;

		case EMode_ZAxisOffset:
			AddOffset(
				Vec3(0.0f, -offset.y, 0.0f),
				Ang3(0.0f, offset.x, 0.0f));
			break;
	}

	m_offset = Vec2(ZERO);
}

bool CWeaponOffsetInput::OnInputEvent(const SInputEvent &inputEvent)
{
	if (inputEvent.keyId == eKI_Mouse1 && inputEvent.state == eIS_Pressed)
		m_mode = EMode_PositionOffset;
	if (inputEvent.keyId == eKI_Mouse1 && inputEvent.state == eIS_Released)
		m_mode = EMode_None;
	if (inputEvent.keyId == eKI_Mouse2 && inputEvent.state == eIS_Pressed)
		m_mode = EMode_RotationOffset;
	if (inputEvent.keyId == eKI_Mouse2 && inputEvent.state == eIS_Released)
		m_mode = EMode_None;
	if (inputEvent.keyId == eKI_Mouse3 && inputEvent.state == eIS_Pressed)
		m_mode = EMode_ZAxisOffset;
	if (inputEvent.keyId == eKI_Mouse3 && inputEvent.state == eIS_Released)
		m_mode = EMode_None;

	if (inputEvent.keyId == eKI_1 && inputEvent.state == eIS_Pressed)
		m_sensibility = ESensibility_Slow;
	if (inputEvent.keyId == eKI_2 && inputEvent.state == eIS_Pressed)
		m_sensibility = ESensibility_Medium;
	if (inputEvent.keyId == eKI_3 && inputEvent.state == eIS_Pressed)
		m_sensibility = ESensibility_Fast;

	if (inputEvent.keyId == eKI_E && inputEvent.state == eIS_Pressed)
		m_hand = EHand_Right;
	if (inputEvent.keyId == eKI_Q && inputEvent.state == eIS_Pressed)
		m_hand = EHand_Left;

	if (inputEvent.keyId == eKI_MouseX)
		m_offset.x = inputEvent.value;
	if (inputEvent.keyId == eKI_MouseY)
		m_offset.y = inputEvent.value;

	return true;
}



void CWeaponOffsetInput::AddOffset(Vec3 pos, Ang3 ang)
{
	if (m_hand == EHand_Right)
		AddRightDebugOffset(SWeaponOffset(pos, ang));
	else if (m_hand == EHand_Left)
		AddLeftDebugOffset(SWeaponOffset(pos, ang));
}




void CWeaponOffsetInput::SetRightDebugOffset(const SWeaponOffset& offset)
{
	m_debugRightOffset = offset;
}



void CWeaponOffsetInput::AddRightDebugOffset(const SWeaponOffset& offset)
{
	m_debugRightOffset.m_position += offset.m_position;
	m_debugRightOffset.m_rotation += offset.m_rotation;
}


void CWeaponOffsetInput::SetLeftDebugOffset(const SWeaponOffset& offset)
{
	m_debugLeftOffset = offset;
}



void CWeaponOffsetInput::AddLeftDebugOffset(const SWeaponOffset& offset)
{
	m_debugLeftOffset.m_position += offset.m_position;
	m_debugLeftOffset.m_rotation += offset.m_rotation;
}



static void NoOpDeleter(void* pPointer) {}


CWeaponOffsetInput::TWeaponOffsetInput CWeaponOffsetInput::Get()
{
	static CWeaponOffsetInput weaponOffsetInput;
	TWeaponOffsetInput pointer = TWeaponOffsetInput(&weaponOffsetInput, NoOpDeleter);
	return pointer;
}
