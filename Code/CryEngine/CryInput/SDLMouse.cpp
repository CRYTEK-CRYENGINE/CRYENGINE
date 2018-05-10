// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description:	Mouse Input implementation for Linux using SDL
   -------------------------------------------------------------------------
   History:
   - Aug 02,2013:	Created by Leander Beernaert

*************************************************************************/
#include "StdAfx.h"

#if defined(USE_LINUXINPUT)

	#include "SDLMouse.h"
	#include <CryRenderer/IRenderer.h>
	#include <CryInput/IHardwareMouse.h>

	#include <SDL.h>

	#define MOUSE_SYM_BASE (1024)
	#define MOUSE_SYM(X) (MOUSE_SYM_BASE + (X))
	#define MOUSE_AXIS_X   (MOUSE_SYM(10))
	#define MOUSE_AXIS_Y   (MOUSE_SYM(11))
	#define MOUSE_AXIS_Z   (MOUSE_SYM(12))
// We need to define custom macros for mouse wheel symbols
// since in SDL2 the mouse wheel event is handles as a single
// occurance
	#define MOUSE_WHEEL_UP   (MOUSE_SYM(13))
	#define MOUSE_WHEEL_DOWN (MOUSE_SYM(14))
	#define MOUSE_MAX_PEEP   64

// Define to enable automatic input grabbing support.  We don't want this
// during development.  If automatic input grabbing support is off, then input
// grabbing can be enforced by typing CTRL+ALT+G.

	#if !defined(_DEBUG)
		#define LINUXINPUT_AUTOGRAB 1
		#define HIDE_MOUSE_POINTER
	#else
		#undef LINUXINPUT_AUTOGRAB
	#endif

CSDLMouse::CSDLMouse(CLinuxInput& input) :
	CLinuxInputDevice(input, "SDL Mouse"),
	m_bGrabInput(false)
{
	m_deviceType = eIDT_Mouse;
	m_pRenderer = gEnv->pRenderer;
}

CSDLMouse::~CSDLMouse()
{

}

bool CSDLMouse::Init()
{
	m_deltas.zero();
	m_oldDeltas.zero();
	m_deltasInertia.zero();

	MapSymbol(MOUSE_SYM(SDL_BUTTON_LEFT), eKI_Mouse1, "mouse1");
	MapSymbol(MOUSE_SYM(SDL_BUTTON_MIDDLE), eKI_Mouse3, "mouse3");
	MapSymbol(MOUSE_SYM(SDL_BUTTON_RIGHT), eKI_Mouse2, "mouse2");
	MapSymbol(MOUSE_WHEEL_UP, eKI_MouseWheelUp, "mwheel_up");
	MapSymbol(MOUSE_WHEEL_DOWN, eKI_MouseWheelDown, "mwheel_down");
	MapSymbol(MOUSE_AXIS_X, eKI_MouseX, "maxis_x", SInputSymbol::RawAxis);
	MapSymbol(MOUSE_AXIS_Y, eKI_MouseY, "maxis_y", SInputSymbol::RawAxis);
	MapSymbol(MOUSE_AXIS_Z, eKI_MouseZ, "maxis_z", SInputSymbol::RawAxis);

	#if defined(HIDE_MOUSE_POINTER)
	SDL_ShowCursor(SDL_DISABLE);
	#endif

	#if defined(LINUXINPUT_AUTOGRAB)
	GrabInput();
	//SDL_SetWindowGrab(static_cast<SDL_Window*>(gEnv->pRenderer->GetHWND()),SDL_TRUE);
	//m_bGrabInput = true;RUE
	#endif
	return true;
}

void CSDLMouse::Update(bool focus)
{
	SDL_Event eventList[MOUSE_MAX_PEEP];

	int nEvents;
	unsigned type = 0;
	// unsigned newX = m_posX, newY = m_posY;

	SInputSymbol* pSymbol = NULL;
	//EInputState newState;

	// Assuming that SDL_PumpEvents is called from CLinuxInput

	nEvents = SDL_PeepEvents(eventList, MOUSE_MAX_PEEP, SDL_GETEVENT, SDL_MOUSEMOTION, SDL_MOUSEWHEEL);

	if (nEvents == -1)
	{
		gEnv->pLog->LogError("SDL_GETEVENT error: %s", SDL_GetError());
		return;
	}
	for (int i = 0; i < nEvents; ++i)
	{
		bool bFlashUI = (gEnv && gEnv->pHardwareMouse);
		type = eventList[i].type;
		if (type == SDL_MOUSEBUTTONDOWN || type == SDL_MOUSEBUTTONUP)
		{
			SDL_MouseButtonEvent* const buttonEvent = &eventList[i].button;
			pSymbol = DevSpecIdToSymbol((uint32)MOUSE_SYM(buttonEvent->button));
			if (pSymbol == NULL)
				continue;
			if (type == SDL_MOUSEBUTTONDOWN)
			{
				pSymbol->state = eIS_Pressed;
				pSymbol->value = 1.f;
				CLinuxInputDevice::PostEvent(pSymbol);
	#if defined(LINUXINPUT_AUTOGRAB)
				GrabInput();
	#endif
				// Update flashUI mouse button
				if (bFlashUI)
				{
					EHARDWAREMOUSEEVENT mouseEvent;
					switch (buttonEvent->button)
					{
					case SDL_BUTTON_LEFT:
						mouseEvent = HARDWAREMOUSEEVENT_LBUTTONDOWN;
						break;
					case SDL_BUTTON_MIDDLE:
						mouseEvent = HARDWAREMOUSEEVENT_MBUTTONDOWN;
						break;
					case SDL_BUTTON_RIGHT:
						mouseEvent = HARDWAREMOUSEEVENT_RBUTTONDOWN;
						break;
					}
					gEnv->pHardwareMouse->Event(buttonEvent->x, buttonEvent->y, mouseEvent);
				}
			}
			else
			{
				pSymbol->state = eIS_Released;
				pSymbol->value = 0.f;
				CLinuxInputDevice::PostEvent(pSymbol);

				// Update flashUI mouse button
				if (bFlashUI)
				{
					EHARDWAREMOUSEEVENT mouseEvent;
					switch (buttonEvent->button)
					{
					case SDL_BUTTON_LEFT:
						mouseEvent = HARDWAREMOUSEEVENT_LBUTTONUP;
						break;
					case SDL_BUTTON_MIDDLE:
						mouseEvent = HARDWAREMOUSEEVENT_MBUTTONUP;
						break;
					case SDL_BUTTON_RIGHT:
						mouseEvent = HARDWAREMOUSEEVENT_RBUTTONUP;
						break;
					}
					gEnv->pHardwareMouse->Event(buttonEvent->x, buttonEvent->y, mouseEvent);
				}
			}
		}
		else if (type == SDL_MOUSEMOTION)
		{
			SDL_MouseMotionEvent* const motionEvent = &eventList[i].motion;
			// If we have grabbed the input exclusively, then we'll pass on the
			// delta values from the events as is.  Without exclusive input, we'll
			// compute the deltas ourselves from the stored position.
			if (m_bGrabInput)
			{
				if (motionEvent->xrel != 0 || motionEvent->yrel != 0)
				{

					float mouseaccel = g_pInputCVars->i_mouse_accel;
					m_deltas.x = (float)motionEvent->xrel;
					m_deltas.y = (float)motionEvent->yrel;

					if (mouseaccel > 0.0f)
					{
						m_deltas.x = m_deltas.x * fabs(m_deltas.x * mouseaccel);
						m_deltas.y = m_deltas.y * fabs(m_deltas.y * mouseaccel);

						CapDeltas(g_pInputCVars->i_mouse_accel_max);
					}

					SmoothDeltas(g_pInputCVars->i_mouse_smooth);

					pSymbol = DevSpecIdToSymbol(MOUSE_AXIS_X);
					assert(pSymbol);
					pSymbol->state = eIS_Changed;
					pSymbol->value = m_deltas.x;
					CLinuxInputDevice::PostEvent(pSymbol);
					pSymbol = DevSpecIdToSymbol(MOUSE_AXIS_Y);
					assert(pSymbol);
					pSymbol->state = eIS_Changed;
					pSymbol->value = m_deltas.y;
					CLinuxInputDevice::PostEvent(pSymbol);
					pSymbol = DevSpecIdToSymbol(MOUSE_AXIS_Z);
					assert(pSymbol);
					pSymbol->state = eIS_Changed;
					pSymbol->value = 0.f;
					CLinuxInputDevice::PostEvent(pSymbol);
				}
			}
			//newX = (int)rintf(motionEvent->x * 800.f / m_pRenderer->GetWidth());
			//newY = (int)rintf(motionEvent->y * 600.f / m_pRenderer->GetHeight());

			float inertia = g_pInputCVars->i_mouse_inertia;

			if (inertia > 0.0f)
			{
				float dt = gEnv->pTimer->GetFrameTime();
				if (dt > 0.1f) dt = 0.1f;
				m_deltas = (m_deltasInertia += (m_deltas - m_deltasInertia) * inertia * dt);
			}

			// Update flashUI mouse:
			if (bFlashUI)
			{
				//gEnv->pHardwareMouse->Event(motionEvent->x, motionEvent->y, HARDWAREMOUSEEVENT_MOVE);
				gEnv->pHardwareMouse->SetHardwareMousePosition((float)motionEvent->x, (float)motionEvent->y);
			}
		}
		else if (type == SDL_MOUSEWHEEL)
		{
			SDL_MouseWheelEvent* pWheelEvent = &eventList[i].wheel;

			if (pWheelEvent->y > 0)
			{
				pSymbol = DevSpecIdToSymbol(MOUSE_WHEEL_UP);
				assert(pSymbol);
				pSymbol->state = eIS_Changed;
				pSymbol->value = (float)pWheelEvent->y;
				CLinuxInputDevice::PostEvent(pSymbol);
			}
			else if (pWheelEvent->y < 0)
			{
				pSymbol = DevSpecIdToSymbol(MOUSE_WHEEL_DOWN);
				assert(pSymbol);
				pSymbol->state = eIS_Changed;
				pSymbol->value = (float)pWheelEvent->y * -1.0f;
				CLinuxInputDevice::PostEvent(pSymbol);
			}

			// Update flashUI mouse wheel
			if (bFlashUI)
			{
				// 120 needs to be added manually to correctly integrate with mouse handling
				// logic which assumes that the value comes in mulitples of 120.
				gEnv->pHardwareMouse->Event(0, 0, HARDWAREMOUSEEVENT_WHEEL, pWheelEvent->y * 120);
			}
		}
		else
		{
			// Unexpected event type.
			abort();
		}
	}
	// Generate mouse motion events when running without exclusive input.
	/*if (!m_bGrabInput)
	   {
	   if (newX != m_posX || newY != m_posY)
	   {
	   pSymbol = DevSpecIdToSymbol(MOUSE_AXIS_X);
	   assert(pSymbol);
	   pSymbol->state = eIS_Changed;
	        pSymbol->value = (int)newX - (int)m_posX;
	   CLinuxInputDevice::PostEvent(pSymbol);
	   pSymbol = DevSpecIdToSymbol(MOUSE_AXIS_Y);
	   assert(pSymbol);
	   pSymbol->state = eIS_Changed;
	        pSymbol->value = (int)newY - (int)m_posY;
	   CLinuxInputDevice::PostEvent(pSymbol);
	   pSymbol = DevSpecIdToSymbol(MOUSE_AXIS_Z);
	   assert(pSymbol);
	   pSymbol->state = eIS_Changed;
	   pSymbol->value = 0.f;
	   CLinuxInputDevice::PostEvent(pSymbol);
	   }
	   }

	   m_posX = newX;
	   m_posY = newY;*/
}

void CSDLMouse::GrabInput()
{
	SInputSymbol* pSymbol = NULL;
	//unsigned width, height;

	if (m_bGrabInput)
		return;
	SDL_Window* pWindow = *static_cast<SDL_Window**>(m_pRenderer->GetHWND());
	if (!pWindow) return;
	SDL_SetWindowGrab(pWindow, SDL_TRUE);

	#if defined(HIDE_MOUSE_POINTER)
	SDL_ShowCursor(SDL_DISABLE);
	#endif

	if (SDL_SetRelativeMouseMode(SDL_TRUE) != 0)
		CryLogAlways("SDL: Could not set relative mouse mode: %s", SDL_GetError());
	// Warp the cursor to the upper left corner of the screen.

	/*width = gEnv->pRenderer->GetWidth();
	   height = gEnv->pRenderer->GetHeight();
	   pSymbol = DevSpecIdToSymbol(MOUSE_AXIS_X);
	   assert(pSymbol);
	   pSymbol->state = eIS_Changed;
	   pSymbol->value = -width;
	   PostEvent(pSymbol);
	   pSymbol = DevSpecIdToSymbol(MOUSE_AXIS_Y);
	   assert(pSymbol);
	   pSymbol->state = eIS_Changed;
	   pSymbol->value = -height;
	   PostEvent(pSymbol);
	   pSymbol = DevSpecIdToSymbol(MOUSE_AXIS_Z);
	   assert(pSymbol);
	   pSymbol->state = eIS_Changed;
	   pSymbol->value = 0.f;
	   PostEvent(pSymbol);

	   // Warp the cursor to the stored cursor position.
	   if (m_posX != 0 || m_posY != 0)
	   {
	    pSymbol = DevSpecIdToSymbol(MOUSE_AXIS_X);if (accel<0.0001f)
	   {
	    //do nothing ,just like it was before.
	    return;
	   }
	   else if (accel<0.9999f)//mouse smooth, average the old and the actual delta by the delta ammount, less delta = more smooth speed.
	   {
	    Vec2 delta = m_deltas - m_oldDeltas;

	    float len = delta.GetLength();

	    float amt = 1.0f - (min(10.0f,len)/10.0f*min(accel,0.9f));

	    m_deltas = m_oldDeltas + delta*amt;
	   }
	   else if (accel<1.0001f)//mouse smooth, just average the old and the actual delta.
	   {
	    m_deltas = (m_deltas + m_oldDeltas) * 0.5f;
	   }
	   else//mouse smooth with acceleration
	   {
	    float dt = min(gEnv->pTimer->GetFrameTime(),0.1f);

	    Vec2 delta;

	    float amt = 0.0;

	    //if the input want to stop use twice of the acceleration.
	    if (m_deltas.GetLength2()<0.0001f)
	        if (decel>0.0001f)//there is a custom deceleration value? use it.
	            amt = min(1.0f,dt*decel);
	        else//otherwise acceleration * 2 is the default.
	            amt = min(1.0f,dt*accel*2.0f);
	    else
	        amt = min(1.0f,dt*accel);

	    delta = m_deltas - m_oldDeltas;
	    m_deltas = m_oldDeltas + delta*amt;
	   }

	   m_oldDeltas = m_deltas;
	    assert(pSymbol);
	    pSymbol->state = eIS_Changed;
	    pSymbol->value = m_posX;
	    PostEvent(pSymbol);
	    pSymbol = DevSpecIdToSymbol(MOUSE_AXIS_Y);
	    assert(pSymbol);
	    pSymbol->state = eIS_Changed;
	    pSymbol->value = m_posY;
	    PostEvent(pSymbol);
	    pSymbol = DevSpecIdToSymbol(MOUSE_AXIS_Z);
	    assert(pSymbol);
	    pSymbol->state = eIS_Changed;
	    pSymbol->value = 0.f;
	    PostEvent(pSymbol);
	   }*/

	m_bGrabInput = true;
}

void CSDLMouse::UngrabInput()
{
	// If the user forces us to release the input grab, then we'll show
	// the system mouse cursor immediately, even if the cursor is still
	// within our window.  The intention is to give a visible feedback
	// that the release has been performed.
	if (!m_bGrabInput)
		return;
	m_bGrabInput = false;

	SDL_SetWindowGrab(*static_cast<SDL_Window**>(m_pRenderer->GetHWND()), SDL_FALSE);
	if (SDL_SetRelativeMouseMode(SDL_FALSE) != 0)
		CryLogAlways("SDL: Could not unset relative mouse mode: %s", SDL_GetError());
	SDL_ShowCursor(SDL_ENABLE);
}

void CSDLMouse::CapDeltas(float cap)
{
	float temp;

	temp = fabs(m_deltas.x) / cap;
	if (temp > 1.0f) m_deltas.x /= temp;

	temp = fabs(m_deltas.y) / cap;
	if (temp > 1.0f) m_deltas.y /= temp;
}

void CSDLMouse::SmoothDeltas(float accel, float decel)
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
	else  //mouse smooth with acceleration
	{
		float dt = min(gEnv->pTimer->GetFrameTime(), 0.1f);

		Vec2 delta;

		float amt = 0.0;

		//if the input want to stop use twice of the acceleration.
		if (m_deltas.GetLength2() < 0.0001f)
			if (decel > 0.0001f)    //there is a custom deceleration value? use it.
				amt = min(1.0f, dt * decel);
			else      //otherwise acceleration * 2 is the default.
				amt = min(1.0f, dt * accel * 2.0f);
		else
			amt = min(1.0f, dt * accel);

		delta = m_deltas - m_oldDeltas;
		m_deltas = m_oldDeltas + delta * amt;
	}

	m_oldDeltas = m_deltas;
}
#endif
