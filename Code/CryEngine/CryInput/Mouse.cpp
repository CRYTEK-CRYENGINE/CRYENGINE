// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   -------------------------------------------------------------------------
   History:
   - Dec 05,2005:	Major rewrite by Marco Koegler

*************************************************************************/
#include "StdAfx.h"
#include "Mouse.h"

#ifdef USE_DXINPUT

SInputSymbol* CMouse::Symbol[MAX_MOUSE_SYMBOLS] = { 0 };

// define a few internal ids
	#define _DIMOFS_WHEEL_UP   (DIMOFS_BUTTON7 + 1)
	#define _DIMOFS_WHEEL_DOWN (DIMOFS_BUTTON7 + 2)

CMouse::CMouse(CDXInput& input)
	: CDXInputDevice(input, "mouse", GUID_SysMouse)
	, m_deltas(ZERO)
	, m_oldDeltas(ZERO)
	, m_deltasInertia(ZERO)
	, m_mouseWheel(0.0f)
{
	m_deviceType = eIDT_Mouse;
}

///////////////////////////////////////////
bool CMouse::Init()
{
	CryLog("Initializing mouse");

	if (!CreateDirectInputDevice(&c_dfDIMouse2, DISCL_NONEXCLUSIVE | DISCL_FOREGROUND, 4096))
		return false;

	// Set mouse axis mode.
	DIPROPDWORD dipdw = { { sizeof(DIPROPDWORD), sizeof(DIPROPHEADER), 0, DIPH_DEVICE }, DIPROPAXISMODE_REL };

	HRESULT hr = GetDirectInputDevice()->SetProperty(DIPROP_AXISMODE, &dipdw.diph);
	if (FAILED(hr))
	{
		CryFatalError("CMouse::Init SetProperty DIPROP_AXISMODE failed.");
		return false;
	}

	m_deltas.zero();
	m_oldDeltas.zero();
	m_deltasInertia.zero();
	m_mouseWheel = 0.0f;

	Acquire();

	// key mapping
	Symbol[eKI_Mouse1 - KI_MOUSE_BASE] = MapSymbol(DIMOFS_BUTTON0, eKI_Mouse1, "mouse1");
	Symbol[eKI_Mouse2 - KI_MOUSE_BASE] = MapSymbol(DIMOFS_BUTTON1, eKI_Mouse2, "mouse2");
	Symbol[eKI_Mouse3 - KI_MOUSE_BASE] = MapSymbol(DIMOFS_BUTTON2, eKI_Mouse3, "mouse3");
	Symbol[eKI_Mouse4 - KI_MOUSE_BASE] = MapSymbol(DIMOFS_BUTTON3, eKI_Mouse4, "mouse4");
	Symbol[eKI_Mouse5 - KI_MOUSE_BASE] = MapSymbol(DIMOFS_BUTTON4, eKI_Mouse5, "mouse5");
	Symbol[eKI_Mouse6 - KI_MOUSE_BASE] = MapSymbol(DIMOFS_BUTTON5, eKI_Mouse6, "mouse6");
	Symbol[eKI_Mouse7 - KI_MOUSE_BASE] = MapSymbol(DIMOFS_BUTTON6, eKI_Mouse7, "mouse7");
	Symbol[eKI_Mouse8 - KI_MOUSE_BASE] = MapSymbol(DIMOFS_BUTTON7, eKI_Mouse8, "mouse8");

	Symbol[eKI_MouseWheelUp - KI_MOUSE_BASE] = MapSymbol(_DIMOFS_WHEEL_UP, eKI_MouseWheelUp, "mwheel_up");
	Symbol[eKI_MouseWheelDown - KI_MOUSE_BASE] = MapSymbol(_DIMOFS_WHEEL_DOWN, eKI_MouseWheelDown, "mwheel_down");
	Symbol[eKI_MouseX - KI_MOUSE_BASE] = MapSymbol(DIMOFS_X, eKI_MouseX, "maxis_x", SInputSymbol::RawAxis);
	Symbol[eKI_MouseY - KI_MOUSE_BASE] = MapSymbol(DIMOFS_Y, eKI_MouseY, "maxis_y", SInputSymbol::RawAxis);
	Symbol[eKI_MouseZ - KI_MOUSE_BASE] = MapSymbol(DIMOFS_Z, eKI_MouseZ, "maxis_z", SInputSymbol::RawAxis);

	return true;
}

///////////////////////////////////////////
void CMouse::Update(bool bFocus)
{
	CRY_PROFILE_FUNCTION(PROFILE_INPUT);
	HRESULT hr;

	if (!GetDirectInputDevice())
		return;

	m_deltas.zero();
	m_mouseWheel = 0;

	SInputSymbol* pSymbol = 0;
	EInputState newState;

	if (g_pInputCVars->i_mouse_buffered)
	{
		//memcpy(m_oldEvents, m_Events, sizeof(m_Events));
		//memset(m_Events,0,sizeof(m_Events));

		DIDEVICEOBJECTDATA Events[200];
		// // Buffer mouse input.
		DWORD nElements = 200;
		while (nElements > 0)
		{
			hr = GetDirectInputDevice()->GetDeviceData(sizeof(DIDEVICEOBJECTDATA), Events, &nElements, 0);

			if (FAILED(hr) && Acquire())
			{
				// try again
				hr = GetDirectInputDevice()->GetDeviceData(sizeof(DIDEVICEOBJECTDATA), Events, &nElements, 0);
			}

			if (FAILED(hr))
				return;

			if (nElements > 0)
			{
				for (int i = 0; i < (int)nElements; i++)
				{
					pSymbol = DevSpecIdToSymbol(Events[i].dwOfs);
					if (pSymbol)
					{
						// marcok: should actually process axis events here as` well,
						// but I'm not sure if the game can handle multiple move events

						if (pSymbol->type == SInputSymbol::Button)
						{
							if (Events[i].dwData & 0x80)
							{
								pSymbol->state = eIS_Pressed;
								pSymbol->value = 1.0;
							}
							else
							{
								pSymbol->state = eIS_Released;
								pSymbol->value = 0.0;
							}
							PostEvent(pSymbol);
						}
					}
					switch (Events[i].dwOfs)
					{
					case DIMOFS_X:
						m_deltas.x += float((int)Events[i].dwData);
						break;
					case DIMOFS_Y:
						m_deltas.y += float((int)Events[i].dwData);
						break;
					case DIMOFS_Z:
						m_mouseWheel += float((int)Events[i].dwData);
						break;
					}
				}
			}
		}
	}
	else
	{
		// Not buffered.
		DIMOUSESTATE2 dims;
		memset(&dims, 0, sizeof(dims));

		hr = GetDirectInputDevice()->GetDeviceState(sizeof(DIMOUSESTATE2), &dims);

		if (FAILED(hr) && Acquire())
		{
			// try again
			hr = GetDirectInputDevice()->GetDeviceState(sizeof(DIMOUSESTATE2), &dims);
		}

		if (SUCCEEDED(hr))
		{
			m_deltas.set((float)dims.lX, (float)dims.lY);
			m_mouseWheel = (float)dims.lZ;

			for (int i = 0; i < 8; ++i)
			{
				newState = (dims.rgbButtons[i] & 0x80) ? eIS_Pressed : eIS_Released;
				PostOnlyIfChanged(Symbol[i], newState);
			}
		}
	}

	m_deltas *= g_pInputCVars->i_mouse_sensitivity;

	//marcok: here the raw input already gets cooked a bit ... should be moved
	float mouseaccel = g_pInputCVars->i_mouse_accel;

	if (mouseaccel > 0.0f)
	{
		m_deltas.x = m_deltas.x * (float)fabs(m_deltas.x * mouseaccel);
		m_deltas.y = m_deltas.y * (float)fabs(m_deltas.y * mouseaccel);

		CapDeltas(g_pInputCVars->i_mouse_accel_max);
	}

	SmoothDeltas(g_pInputCVars->i_mouse_smooth);

	const bool hasDeltaChanged = !IsEquivalent(m_deltas, m_oldDeltas, FLT_EPSILON);
	m_oldDeltas = m_deltas; //this needs to happen always. We want to keep the attribute always valid

	//mouse wheel - use custom code instead of PostOnlyWhenChanged because we want the mouseWheel value
	if (m_mouseWheel > 0.0f)
	{
		SInputSymbol* pSymbol = Symbol[eKI_MouseWheelUp - KI_MOUSE_BASE];
		pSymbol->value = m_mouseWheel;
		pSymbol->state = eIS_Pressed;
		PostEvent(pSymbol);
	}
	else if (m_mouseWheel < 0.0f)
	{
		SInputSymbol* pSymbol = Symbol[eKI_MouseWheelDown - KI_MOUSE_BASE];
		pSymbol->value = m_mouseWheel;
		pSymbol->state = eIS_Pressed;
		PostEvent(pSymbol);
	}
	else
	{
		SInputSymbol* pSymbol = Symbol[eKI_MouseWheelUp - KI_MOUSE_BASE];
		if (pSymbol->state != eIS_Released)
		{
			pSymbol->value = 0.0f;
			pSymbol->state = eIS_Released;
			PostEvent(pSymbol);
		}

		SInputSymbol* pSymbol2 = Symbol[eKI_MouseWheelDown - KI_MOUSE_BASE];
		if (pSymbol2->state != eIS_Released)
		{
			pSymbol2->value = 0.0f;
			pSymbol2->state = eIS_Released;
			PostEvent(pSymbol2);
		}
	}

	// mouse movements
	if (m_deltas.GetLength2() > 0.0f || hasDeltaChanged || m_mouseWheel)
	{
		pSymbol = Symbol[eKI_MouseX - KI_MOUSE_BASE];
		pSymbol->state = eIS_Changed;
		pSymbol->value = m_deltas.x;
		PostEvent(pSymbol);

		pSymbol = Symbol[eKI_MouseY - KI_MOUSE_BASE];
		pSymbol->state = eIS_Changed;
		pSymbol->value = m_deltas.y;
		PostEvent(pSymbol);

		pSymbol = Symbol[eKI_MouseZ - KI_MOUSE_BASE];
		pSymbol->state = eIS_Changed;
		pSymbol->value = m_mouseWheel;
		PostEvent(pSymbol);
	}

	//////////////////////////////////////////////////////////////////////////
	float inertia = g_pInputCVars->i_mouse_inertia;

	// mouse inertia
	if (inertia > 0)
	{
		float dt = gEnv->pTimer->GetFrameTime();
		if (dt > 0.1f) dt = 0.1f;
		m_deltas = (m_deltasInertia += (m_deltas - m_deltasInertia) * inertia * dt);
	}
}

///////////////////////////////////////////
bool CMouse::SetExclusiveMode(bool value)
{
	if (!GetDirectInputDevice())
		return false;

	HRESULT hr;

	Unacquire();

	if (value)
	{
		hr = GetDirectInputDevice()->SetCooperativeLevel(GetDXInput().GetHWnd(), DISCL_EXCLUSIVE | DISCL_FOREGROUND);
	}
	else
	{
		hr = GetDirectInputDevice()->SetCooperativeLevel(GetDXInput().GetHWnd(), DISCL_NONEXCLUSIVE | DISCL_FOREGROUND);
	}

	if (FAILED(hr))
	{
		gEnv->pLog->Log("Mouse:SetExclusiveMode(%s) failed\n", value ? "true" : "false");
		return false;
	}

	return Acquire();
}

//////////////////////////////////////////////////////////////////////////
void CMouse::PostEvent(SInputSymbol* pSymbol)
{
	// Post Input events.
	SInputEvent event;
	pSymbol->AssignTo(event, GetDXInput().GetModifiers());
	GetDXInput().PostInputEvent(event);
}

void CMouse::PostOnlyIfChanged(SInputSymbol* pSymbol, EInputState newState)
{
	if (pSymbol->state != eIS_Released && newState == eIS_Released)
	{
		pSymbol->state = newState;
		pSymbol->value = 0.0f;
	}
	else if (pSymbol->state == eIS_Released && newState == eIS_Pressed)
	{
		pSymbol->state = newState;
		pSymbol->value = 1.0f;
	}
	else
	{
		return;
	}

	PostEvent(pSymbol);
}

void CMouse::SmoothDeltas(float accel, float decel)
{
	if (accel < 0.0001f)
	{
		//do nothing ,just like it was before.
		return;
	}
	else if (accel < 0.9999f)//mouse smooth, average the old and the actual delta by the delta ammount, less delta = more smooth speed.
	{
		Vec2 delta = m_deltas - m_oldDeltas;

		float len = delta.GetLength();

		float amt = 1.0f - (min(10.0f, len) / 10.0f * min(accel, 0.9f));

		m_deltas = m_oldDeltas + delta * amt;
	}
	else if (accel < 1.0001f)//mouse smooth, just average the old and the actual delta.
	{
		m_deltas = (m_deltas + m_oldDeltas) * 0.5f;
	}
	else//mouse smooth with acceleration
	{
		float dt = min(gEnv->pTimer->GetFrameTime(), 0.1f);

		Vec2 delta;

		float amt = 0.0;

		//if the input want to stop use twice of the acceleration.
		if (m_deltas.GetLength2() < 0.0001f)
			if (decel > 0.0001f)//there is a custom deceleration value? use it.
				amt = min(1.0f, dt * decel);
			else//otherwise acceleration * 2 is the default.
				amt = min(1.0f, dt * accel * 2.0f);
		else
			amt = min(1.0f, dt * accel);

		delta = m_deltas - m_oldDeltas;
		m_deltas = m_oldDeltas + delta * amt;
	}

	m_oldDeltas = m_deltas;
}

void CMouse::CapDeltas(float cap)
{
	/*float len = GETLEN2D(m_Deltas);

	   if (len < cap)
	   return;

	   float mult;

	   if (len > 0.0f)
	   mult = 1.0f/len;
	   else
	   mult = 0.0001f;

	   m_Deltas[0] *= mult * cap;
	   m_Deltas[1] *= mult * cap;*/

	//just cap deltas at certain value, used for mouse acceleration
	float temp;

	temp = fabs(m_deltas[0]) / cap;
	if (temp > 1.0f) m_deltas[0] /= temp;

	temp = fabs(m_deltas[1]) / cap;
	if (temp > 1.0f) m_deltas[1] /= temp;
}

#endif //USE_DXINPUT
