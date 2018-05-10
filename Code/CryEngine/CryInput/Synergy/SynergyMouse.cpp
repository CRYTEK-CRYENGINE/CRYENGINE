// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#ifdef USE_SYNERGY_INPUT

	#include "SynergyMouse.h"
	#include "SynergyContext.h"

	#define CONTROL_RELATIVE_INPUT_WITH_MIDDLE_BUTTON

SInputSymbol* CSynergyMouse::Symbol[MAX_SYNERGY_MOUSE_SYMBOLS] = { 0 };

CSynergyMouse::CSynergyMouse(IInput& input, CSynergyContext* pContext)
	: CInputDevice(input, "mouse")
	, m_lastX(0)
	, m_lastY(0)
	, m_lastZ(0)
	, m_lastButtonL(false)
	, m_lastButtonM(false)
	, m_lastButtonR(false)
{
	m_deviceType = eIDT_Mouse;
	m_pContext = pContext;
}

bool CSynergyMouse::Init()
{
	m_lastX = 0;
	m_lastY = 0;
	m_lastZ = 0;
	m_lastButtonL = false;
	m_lastButtonM = false;
	m_lastButtonR = false;
	Symbol[SYNERGY_BUTTON_L] = MapSymbol(SYNERGY_BUTTON_L, eKI_Mouse1, "mouse1");
	Symbol[SYNERGY_BUTTON_R] = MapSymbol(SYNERGY_BUTTON_R, eKI_Mouse2, "mouse2");
	Symbol[SYNERGY_BUTTON_M] = MapSymbol(SYNERGY_BUTTON_M, eKI_Mouse3, "mouse3");
	Symbol[SYNERGY_MOUSE_X] = MapSymbol(SYNERGY_MOUSE_X, eKI_MouseX, "maxis_x", SInputSymbol::RawAxis);
	Symbol[SYNERGY_MOUSE_Y] = MapSymbol(SYNERGY_MOUSE_Y, eKI_MouseY, "maxis_y", SInputSymbol::RawAxis);
	Symbol[SYNERGY_MOUSE_Z] = MapSymbol(SYNERGY_MOUSE_Z, eKI_MouseZ, "maxis_z", SInputSymbol::RawAxis);
	Symbol[SYNERGY_MOUSE_X_ABS] = MapSymbol(SYNERGY_MOUSE_X_ABS, eKI_MouseXAbsolute, "maxis_xabs", SInputSymbol::RawAxis);
	Symbol[SYNERGY_MOUSE_Y_ABS] = MapSymbol(SYNERGY_MOUSE_Y_ABS, eKI_MouseYAbsolute, "maxis_yabs", SInputSymbol::RawAxis);
	return true;
}

///////////////////////////////////////////
void CSynergyMouse::Update(bool bFocus)
{
	SInputEvent event;
	SInputSymbol* pSymbol;
	int16 deltaX, deltaY, deltaZ;
	uint16 x, y, wheelX, wheelY;
	bool buttonL, buttonM, buttonR;

	while (m_pContext->GetMouse(x, y, wheelX, wheelY, buttonL, buttonM, buttonR))
	{
		deltaX = x - m_lastX;
		m_lastX = x;
		deltaY = y - m_lastY;
		m_lastY = y;
		deltaZ = wheelY - m_lastZ;
		m_lastZ = wheelY;

		if (deltaX)
		{
	#ifdef CONTROL_RELATIVE_INPUT_WITH_MIDDLE_BUTTON
			if (buttonM)
	#endif
			{
				pSymbol = Symbol[SYNERGY_MOUSE_X];
				pSymbol->state = eIS_Changed;
				pSymbol->value = deltaX;
				pSymbol->AssignTo(event);
				GetIInput().PostInputEvent(event);
			}

			pSymbol = Symbol[SYNERGY_MOUSE_X_ABS];
			pSymbol->state = eIS_Changed;
			pSymbol->value = x;
			pSymbol->AssignTo(event);
			GetIInput().PostInputEvent(event);
		}

		if (deltaY)
		{
	#ifdef CONTROL_RELATIVE_INPUT_WITH_MIDDLE_BUTTON
			if (buttonM)
	#endif
			{
				pSymbol = Symbol[SYNERGY_MOUSE_Y];
				pSymbol->state = eIS_Changed;
				pSymbol->value = deltaY;
				pSymbol->AssignTo(event);
				GetIInput().PostInputEvent(event);
			}

			pSymbol = Symbol[SYNERGY_MOUSE_Y_ABS];
			pSymbol->state = eIS_Changed;
			pSymbol->value = y;
			pSymbol->AssignTo(event);
			GetIInput().PostInputEvent(event);
		}

		if (deltaZ)
		{
			pSymbol = Symbol[SYNERGY_MOUSE_Z];
			pSymbol->state = eIS_Changed;
			pSymbol->value = deltaZ;
			pSymbol->AssignTo(event);
			GetIInput().PostInputEvent(event);
		}

		if (buttonL != m_lastButtonL)
		{
			pSymbol = Symbol[SYNERGY_BUTTON_L];
			pSymbol->state = buttonL ? eIS_Pressed : eIS_Released;
			pSymbol->value = buttonL ? 1.0f : 0.0f;
			pSymbol->AssignTo(event);
			GetIInput().PostInputEvent(event);
			m_lastButtonL = buttonL;
		}

	#ifndef CONTROL_RELATIVE_INPUT_WITH_MIDDLE_BUTTON
		if (buttonM != m_lastButtonM)
		{
			pSymbol = Symbol[SYNERGY_BUTTON_M];
			pSymbol->state = buttonM ? eIS_Pressed : eIS_Released;
			pSymbol->value = buttonM ? 1.0f : 0.0f;
			pSymbol->AssignTo(event);
			GetIInput().PostInputEvent(event);
			m_lastButtonM = buttonM;
		}
	#endif

		if (buttonR != m_lastButtonR)
		{
			pSymbol = Symbol[SYNERGY_BUTTON_R];
			pSymbol->state = buttonR ? eIS_Pressed : eIS_Released;
			pSymbol->value = buttonR ? 1.0f : 0.0f;
			pSymbol->AssignTo(event);
			GetIInput().PostInputEvent(event);
			m_lastButtonR = buttonR;
		}
	}
}

bool CSynergyMouse::SetExclusiveMode(bool value)
{
	return false;
}

#endif // USE_SYNERGY_INPUT
