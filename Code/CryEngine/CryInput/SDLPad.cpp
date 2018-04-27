// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

/*************************************************************************
   -------------------------------------------------------------------------
   $Id$
   $DateTime$
   Description:	GamePad Input implementation for Linux using SDL
   -------------------------------------------------------------------------
   History:
   - Jan 20,2014:	Created by Leander Beernaert

*************************************************************************/
#include "StdAfx.h"
#if defined(USE_LINUXINPUT)
	#include "SDLPad.h"

// NOTE: The current implementation is modled after the XI input
// Adjustments need to be made if this is not the case!

	#define SDL_GAMEPAD_MAX_PEEP 64

	#define SDL_GAMEPAD_AXIS_MAX (32767)
	#define SDL_GAMEPAD_AXIS_MIN (-32768)

// SDL axis/button/dpad/etc. ids overlap so need to convert each to unique dev value so our CInput
// can create 1:1 mapping to input symbol
	#define SDL_GAMEPAD_SYM_AXIS (1000)
	#define SDL_GAMEPAD_SYM_BTN  (2000)
	#define SDL_GAMEPAD_SYM_DPAD (3000)
	#define SDL_GAMEPAD_SYM(X)  (X)
	#define SDL_GAMEPAD_AXIS(X) (SDL_GAMEPAD_SYM_AXIS + (X))
	#define SDL_GAMEPAD_BTN(X)  (SDL_GAMEPAD_SYM_BTN + (X))

	#define SDL_GAMEPAD_DPAD(X) (SDL_GAMEPAD_SYM_DPAD + (X))
	#define SDL_GAMEPAD_DPAD_UP           (SDL_GAMEPAD_DPAD(SDL_HAT_UP))
	#define SDL_GAMEPAD_DPAD_DOWN         (SDL_GAMEPAD_DPAD(SDL_HAT_DOWN))
	#define SDL_GAMEPAD_DPAD_RIGHT        (SDL_GAMEPAD_DPAD(SDL_HAT_RIGHT))
	#define SDL_GAMEPAD_DPAD_LEFT         (SDL_GAMEPAD_DPAD(SDL_HAT_LEFT))

	#define SDL_GAMEPAD_A                 (SDL_GAMEPAD_SYM(SDL_CONTROLLER_BUTTON_A))
	#define SDL_GAMEPAD_B                 (SDL_GAMEPAD_SYM(SDL_CONTROLLER_BUTTON_B))
	#define SDL_GAMEPAD_X                 (SDL_GAMEPAD_SYM(SDL_CONTROLLER_BUTTON_X))
	#define SDL_GAMEPAD_Y                 (SDL_GAMEPAD_SYM(SDL_CONTROLLER_BUTTON_Y))
	#define SDL_GAMEPAD_LEFT_SHOULDER     (SDL_GAMEPAD_SYM(SDL_CONTROLLER_BUTTON_LEFTSHOULDER))
	#define SDL_GAMEPAD_RIGHT_SHOULDER    (SDL_GAMEPAD_SYM(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER))
	#define SDL_GAMEPAD_LEFT_TRIGGER_BTN  (SDL_GAMEPAD_BTN(SDL_CONTROLLER_AXIS_TRIGGERLEFT))
	#define SDL_GAMEPAD_RIGHT_TRIGGER_BTN (SDL_GAMEPAD_BTN(SDL_CONTROLLER_AXIS_TRIGGERRIGHT))
	#define SDL_GAMEPAD_BACK              (SDL_GAMEPAD_SYM(SDL_CONTROLLER_BUTTON_BACK))
	#define SDL_GAMEPAD_START             (SDL_GAMEPAD_SYM(SDL_CONTROLLER_BUTTON_START))
	#define SDL_GAMEPAD_LEFT_THUMB        (SDL_GAMEPAD_SYM(SDL_CONTROLLER_BUTTON_LEFTSTICK))
	#define SDL_GAMEPAD_RIGHT_THUMB       (SDL_GAMEPAD_SYM(SDL_CONTROLLER_BUTTON_RIGHTSTICK))

// On some controllers, SDL_CONTROLLER_AXIS_TRIGGER* events are accompanied
// by a redundant SDL_CONTROLLER_BUTTON_* event outside the range of SDL_CONTROLLER_BUTTON_MAX
	#define SDL_GAMEPAD_BUTTON_LEFT_TRIGGER_DUPE  (SDL_GAMEPAD_SYM(15))
	#define SDL_GAMEPAD_BUTTON_RIGHT_TRIGGER_DUPE (SDL_GAMEPAD_SYM(16))

	#define SDL_GAMEPAD_LEFT_THUMB_X              (SDL_GAMEPAD_AXIS(SDL_CONTROLLER_AXIS_LEFTX))
	#define SDL_GAMEPAD_LEFT_THUMB_Y              (SDL_GAMEPAD_AXIS(SDL_CONTROLLER_AXIS_LEFTY))
	#define SDL_GAMEPAD_LEFT_TRIGGER              (SDL_GAMEPAD_AXIS(SDL_CONTROLLER_AXIS_TRIGGERLEFT))
	#define SDL_GAMEPAD_LEFT_TRIGGER_DUPE         (SDL_GAMEPAD_AXIS(7)) // Discard duplicate event
	#define SDL_GAMEPAD_RIGHT_THUMB_X             (SDL_GAMEPAD_AXIS(SDL_CONTROLLER_AXIS_RIGHTX))
	#define SDL_GAMEPAD_RIGHT_THUMB_Y             (SDL_GAMEPAD_AXIS(SDL_CONTROLLER_AXIS_RIGHTY))
	#define SDL_GAMEPAD_RIGHT_TRIGGER             (SDL_GAMEPAD_AXIS(SDL_CONTROLLER_AXIS_TRIGGERRIGHT))
	#define SDL_GAMEPAD_RIGHT_TRIGGER_DUPE        (SDL_GAMEPAD_AXIS(6)) // Discard duplicate event

// On some controllers dpad events are not raised as hat events with a
// SDL_HAT_* value, but rather as regular button events with SDL_CONTROLLER_BUTTON_DPAD_*
// values.  These will be mapped/handled similar to our SDL_GAMEPAD_DPAD_* symbols.
	#define SDL_GAMEPAD_LEFT_THUMB_UP    (SDL_GAMEPAD_SYM(SDL_CONTROLLER_BUTTON_DPAD_UP))
	#define SDL_GAMEPAD_LEFT_THUMB_DOWN  (SDL_GAMEPAD_SYM(SDL_CONTROLLER_BUTTON_DPAD_DOWN))
	#define SDL_GAMEPAD_LEFT_THUMB_LEFT  (SDL_GAMEPAD_SYM(SDL_CONTROLLER_BUTTON_DPAD_LEFT))
	#define SDL_GAMEPAD_LEFT_THUMB_RIGHT (SDL_GAMEPAD_SYM(SDL_CONTROLLER_BUTTON_DPAD_RIGHT))

// No SDL support for these
/*#define SDL_GAMEPAD_RIGHT_THUMB_UP (SDL_GAMEPAD_SYM(24))
   #define SDL_GAMEPAD_RIGHT_THUMB_DOWN (SDL_GAMEPAD_SYM(25))
   #define SDL_GAMEPAD_RIGHT_THUMB_LEFT (SDL_GAMEPAD_SYM(26))
 #define SDL_GAMEPAD_RIGHT_THUMB_RIGHT (SDL_GAMEPAD_SYM(27))*/

	#define SDL_GAMEPAD_DEADZONE                   (8000)
	#define SDL_GAMEPAD_DEADZONE_LOW               (-SDL_GAMEPAD_DEADZONE)
	#define SDL_GAMEPAD_DEADZONE_HIGH              (SDL_GAMEPAD_DEADZONE)

	#define SDL_GAMEPAD_TRIGGER_MIN_VAL_FOR_BUTTON 0.1f

CSDLPad::CSDLPad(CLinuxInput& input, int device) :
	CLinuxInputDevice(input, "SDL GamePad"),
	m_pSDLDevice(NULL)
{
	m_deviceType = eIDT_Gamepad;
	m_deviceNo = device;
	m_connected = false;
	m_vibrateTime = 0.0f;
	m_supportsFeedback = false;
	m_curHapticEffect = -1;
}

CSDLPad::~CSDLPad()
{
	CloseDevice();
}

bool CSDLPad::Init()
{
	// Setup symbols
	MapSymbol(SDL_GAMEPAD_DPAD_UP, eKI_XI_DPadUp, "xi_dpad_up");
	MapSymbol(SDL_GAMEPAD_DPAD_DOWN, eKI_XI_DPadDown, "xi_dpad_down");
	MapSymbol(SDL_GAMEPAD_DPAD_LEFT, eKI_XI_DPadLeft, "xi_dpad_left");
	MapSymbol(SDL_GAMEPAD_DPAD_RIGHT, eKI_XI_DPadRight, "xi_dpad_right");
	MapSymbol(SDL_GAMEPAD_START, eKI_XI_Start, "xi_start");
	MapSymbol(SDL_GAMEPAD_BACK, eKI_XI_Back, "xi_back");
	MapSymbol(SDL_GAMEPAD_LEFT_THUMB, eKI_XI_ThumbL, "xi_thumbl");
	MapSymbol(SDL_GAMEPAD_RIGHT_THUMB, eKI_XI_ThumbR, "xi_thumbr");
	MapSymbol(SDL_GAMEPAD_LEFT_SHOULDER, eKI_XI_ShoulderL, "xi_shoulderl");
	MapSymbol(SDL_GAMEPAD_RIGHT_SHOULDER, eKI_XI_ShoulderR, "xi_shoulderr");
	MapSymbol(SDL_GAMEPAD_A, eKI_XI_A, "xi_a");
	MapSymbol(SDL_GAMEPAD_B, eKI_XI_B, "xi_b");
	MapSymbol(SDL_GAMEPAD_X, eKI_XI_X, "xi_x");
	MapSymbol(SDL_GAMEPAD_Y, eKI_XI_Y, "xi_y");
	MapSymbol(SDL_GAMEPAD_LEFT_TRIGGER, eKI_XI_TriggerL, "xi_triggerl", SInputSymbol::Trigger);
	MapSymbol(SDL_GAMEPAD_RIGHT_TRIGGER, eKI_XI_TriggerR, "xi_triggerr", SInputSymbol::Trigger);
	MapSymbol(SDL_GAMEPAD_LEFT_THUMB_X, eKI_XI_ThumbLX, "xi_thumblx", SInputSymbol::Axis);
	MapSymbol(SDL_GAMEPAD_LEFT_THUMB_Y, eKI_XI_ThumbLY, "xi_thumbly", SInputSymbol::Axis);
	// Map left thumb dpad button events to corresponing dpad hat events
	/*MapSymbol(SDL_GAMEPAD_LEFT_THUMB_UP, eKI_XI_DPadUp, "xi_dpad_up");
	   MapSymbol(SDL_GAMEPAD_LEFT_THUMB_DOWN, eKI_XI_DPadDown, "xi_dpad_down");
	   MapSymbol(SDL_GAMEPAD_LEFT_THUMB_LEFT, eKI_XI_DPadLeft, "xi_dpad_left");
	   MapSymbol(SDL_GAMEPAD_LEFT_THUMB_RIGHT, eKI_XI_DPadRight, "xi_dpad_right");*/
	MapSymbol(SDL_GAMEPAD_RIGHT_THUMB_X, eKI_XI_ThumbRX, "xi_thumbrx", SInputSymbol::Axis);
	MapSymbol(SDL_GAMEPAD_RIGHT_THUMB_Y, eKI_XI_ThumbRY, "xi_thumbry", SInputSymbol::Axis);
	/*MapSymbol(SDL_GAMEPAD_RIGHT_THUMB_UP, eKI_XI_ThumbRUp, "xi_thumbr_up");
	   MapSymbol(SDL_GAMEPAD_RIGHT_THUMB_DOWN, eKI_XI_ThumbRDown, "xi_thumbr_down");
	   MapSymbol(SDL_GAMEPAD_RIGHT_THUMB_LEFT, eKI_XI_ThumbRLeft, "xi_thumbr_left");
	   MapSymbol(SDL_GAMEPAD_RIGHT_THUMB_RIGHT, eKI_XI_ThumbRRight, "xi_thumbr_right");*/
	MapSymbol(SDL_GAMEPAD_LEFT_TRIGGER_BTN, eKI_XI_TriggerLBtn, "xi_triggerl_btn");  // trigger usable as a button
	MapSymbol(SDL_GAMEPAD_RIGHT_TRIGGER_BTN, eKI_XI_TriggerRBtn, "xi_triggerr_btn"); // trigger usable as a button

	return true;
}

void CSDLPad::Update(bool bFocus)
{

	if (m_supportsFeedback && m_vibrateTime > 0.0f)
	{
		m_vibrateTime -= gEnv->pTimer->GetFrameTime();
		if (m_vibrateTime <= 0.0f)
		{
			SDL_HapticDestroyEffect(m_pHapticDevice, m_curHapticEffect);
			m_curHapticEffect = -1;
		}
	}
}

void CSDLPad::ClearAnalogKeyState(TInputSymbols& clearedSymbols)
{
}

void CSDLPad::ClearKeyState()
{
	CInputDevice::ClearKeyState();
}

bool CSDLPad::SetForceFeedback(IFFParams params)
{
	#if defined(SDL_USE_HAPTIC_FEEDBACK)
	if (!m_connected) return false;
	// only submit new one when current has finished
	if (m_supportsFeedback && m_curHapticEffect < 0)
	{
		if (params.strengthA == 0 || params.strengthB == 0)
			return true;

		SDL_HapticEffect effect;
		memset(&effect, 0, sizeof(effect));
		effect.type = SDL_HAPTIC_LEFTRIGHT;
		effect.leftright.large_magnitude = (uint16)(clamp_tpl(params.strengthB, 0.0f, 1.0f) * 65536.0f);
		effect.leftright.small_magnitude = (uint16)(clamp_tpl(params.strengthA, 0.0f, 1.0f) * 65536.0f);
		effect.leftright.length = (params.timeInSeconds == 0.0f) ? 1000 : (Uint32)(params.timeInSeconds * 1000.0f);

		m_curHapticEffect = SDL_HapticNewEffect(m_pHapticDevice, &effect);
		if (m_curHapticEffect < 0)
		{
			gEnv->pLog->LogError("CSDLPad - Gamepad [%d] failed to create feedback effect: %s", m_deviceNo, SDL_GetError());
			return false;
		}
		gEnv->pLog->Log("CSDLPad - Gamepad [%d] submitting create feedback effect (%d,%d,%d)", m_deviceNo, effect.leftright.large_magnitude, effect.leftright.small_magnitude, effect.leftright.length);
		SDL_HapticRunEffect(m_pHapticDevice, m_curHapticEffect, 1);
		m_vibrateTime = params.timeInSeconds;
	}
	#endif
	return true;
}

int CSDLPad::GetInstanceId() const
{
	if (!m_connected) return -1;
	return SDL_JoystickInstanceID(m_pSDLDevice);
}

void CSDLPad::HandleAxisEvent(const SDL_JoyAxisEvent& axisEvt)
{
	if (!m_connected) return;

	SInputSymbol* pSymbol = NULL;
	SInputEvent inputEvt;
	//CryLogAlways("CSDLPad - AXIS requested(%d) %d on Gamepad [%d]", axisEvt.axis, axisEvt.value, m_deviceNo);
	const int id = SDL_GAMEPAD_AXIS(axisEvt.axis);
	pSymbol = DevSpecIdToSymbol(id);
	if (pSymbol)
	{
		switch (id)
		{
		case SDL_GAMEPAD_LEFT_THUMB_X:
		case SDL_GAMEPAD_RIGHT_THUMB_X:
			pSymbol->value = DeadZoneFilter(axisEvt.value);
			break;
		case SDL_GAMEPAD_LEFT_THUMB_Y:
		case SDL_GAMEPAD_RIGHT_THUMB_Y:
			// Input system seems to expect Y input to be inverted relative to SDL values
			pSymbol->value = -1.0f * DeadZoneFilter(axisEvt.value);
			break;
		case SDL_GAMEPAD_RIGHT_TRIGGER_DUPE:
			// ignore
			break;
		case SDL_GAMEPAD_LEFT_TRIGGER_DUPE:
			// ignore
			break;
		case SDL_GAMEPAD_RIGHT_TRIGGER:
		case SDL_GAMEPAD_LEFT_TRIGGER:
			if (axisEvt.value < 0)
				pSymbol->value = -1.0f * ((float)(SDL_GAMEPAD_AXIS_MIN - axisEvt.value) / ((float) SDL_GAMEPAD_AXIS_MAX * 2.0f));
			else
				pSymbol->value = 0.5f + ((float)axisEvt.value / ((float) SDL_GAMEPAD_AXIS_MAX * 2.0f));

			if (pSymbol->value > -1.0f)
			{
				SInputSymbol* btnSymbol = DevSpecIdToSymbol(id == SDL_GAMEPAD_LEFT_TRIGGER ? SDL_GAMEPAD_LEFT_TRIGGER_BTN : SDL_GAMEPAD_RIGHT_TRIGGER_BTN);
				if (btnSymbol)
				{
					if (pSymbol->value > SDL_GAMEPAD_TRIGGER_MIN_VAL_FOR_BUTTON)
						btnSymbol->state = eIS_Pressed;
					else
						btnSymbol->state = eIS_Released;

					btnSymbol->AssignTo(inputEvt);
					GetLinuxInput().PostInputEvent(inputEvt);
				}
			}
			break;
		default:
			gEnv->pLog->LogError("CSDLPad - Unknown AXIS requested(%d) on Gamepad [%d]", axisEvt.axis, m_deviceNo);
			return;
		}

		pSymbol->state = eIS_Changed;
		pSymbol->AssignTo(inputEvt);
		GetLinuxInput().PostInputEvent(inputEvt);
	}
}

void CSDLPad::HandleHatEvent(const SDL_JoyHatEvent& hatEvt)
{
	if (!m_connected) return;

	SInputSymbol* pSymbol = NULL;
	SInputEvent inputEvt;
	//CryLogAlways( "HAT %x", hatEvt.value);
	#define SET_HAT_STATE(specId, symState) \
	  pSymbol = DevSpecIdToSymbol(specId);  \
	  pSymbol->state = symState;            \
	  pSymbol->AssignTo(inputEvt);          \
	  GetLinuxInput().PostInputEvent(inputEvt);

	if (hatEvt.value == SDL_HAT_CENTERED)
	{
		SET_HAT_STATE(SDL_GAMEPAD_DPAD_UP, eIS_Released);
		SET_HAT_STATE(SDL_GAMEPAD_DPAD_DOWN, eIS_Released);
		SET_HAT_STATE(SDL_GAMEPAD_DPAD_RIGHT, eIS_Released);
		SET_HAT_STATE(SDL_GAMEPAD_DPAD_LEFT, eIS_Released);
	}
	else
	{
		if (hatEvt.value & SDL_HAT_LEFT)
		{
			SET_HAT_STATE(SDL_GAMEPAD_DPAD_LEFT, eIS_Pressed);
		}
		if (hatEvt.value & SDL_HAT_UP)
		{
			SET_HAT_STATE(SDL_GAMEPAD_DPAD_UP, eIS_Pressed);
		}
		if (hatEvt.value & SDL_HAT_DOWN)
		{
			SET_HAT_STATE(SDL_GAMEPAD_DPAD_DOWN, eIS_Pressed);
		}
		if (hatEvt.value & SDL_HAT_RIGHT)
		{
			SET_HAT_STATE(SDL_GAMEPAD_DPAD_RIGHT, eIS_Pressed);
		}
	}

	#undef SET_HAT_STATE
}

void CSDLPad::HandleButtonEvent(const SDL_JoyButtonEvent& btEvt)
{
	if (!m_connected) return;

	//CryLogAlways("BTEVENT EVT: bt:%d %s",btEvt.button, (btEvt.state & SDL_PRESSED) ? "PRESSED" : "RELEASED" );
	SInputSymbol* pSymbol = DevSpecIdToSymbol(btEvt.button);
	if (pSymbol)
	{
		SInputEvent inputEvt;
		const EInputState currState = (btEvt.state & SDL_PRESSED) ? eIS_Pressed : eIS_Released;
		//CryLogAlways("BTEVENT SYM: name: %s state:%d=>%d", pSymbol->name.c_str(), int(pSymbol->state), int(currState));
		bool isPost = false;
		switch (btEvt.button)
		{
		// Special handling so they act more like the hat events; events only triggered on
		// initial press and release
		case SDL_GAMEPAD_LEFT_THUMB_UP:
		case SDL_GAMEPAD_LEFT_THUMB_DOWN:
		case SDL_GAMEPAD_LEFT_THUMB_LEFT:
		case SDL_GAMEPAD_LEFT_THUMB_RIGHT:
			isPost = (pSymbol->state == eIS_Unknown)
			         || (currState == eIS_Pressed && (pSymbol->state == eIS_Released))
			         || (currState == eIS_Released && (pSymbol->state == eIS_Pressed || pSymbol->state == eIS_Down));
			break;
		default:
			isPost = true;
			break;
		}
		if (isPost)
		{
			pSymbol->state = currState;
			pSymbol->value = (currState == eIS_Pressed) ? 1.0f : 0.0f;
			pSymbol->AssignTo(inputEvt);
			GetLinuxInput().PostInputEvent(inputEvt);
		}
	}
	else
	{
		switch (btEvt.button)
		{
		// Ignore buttons that are known to be unmapped and we intentionally don't handle
		case SDL_GAMEPAD_BUTTON_LEFT_TRIGGER_DUPE:
		case SDL_GAMEPAD_BUTTON_RIGHT_TRIGGER_DUPE:
			break;
		default:
			gEnv->pLog->LogError("CSDLPad - Gamepad [%d] received unmapped button event %d", m_deviceNo, btEvt.button);
			break;
		}
	}
}

void CSDLPad::HandleConnectionState(const bool connected)
{
	if (m_connected != connected)
	{
		SInputEvent event;
		event.deviceType = eIDT_Gamepad;
		event.deviceIndex = (uint8) m_deviceNo;
		event.state = eIS_Changed;

		if (!connected)
		{
			CloseDevice();
			event.keyId = eKI_XI_Disconnect;
			event.keyName = "disconnect";
			GetLinuxInput().PostInputEvent(event);
		}
		else
		{
			if (OpenDevice())
			{
				event.keyId = eKI_XI_Connect;
				event.keyName = "connect";
				GetLinuxInput().PostInputEvent(event);
				gEnv->pLog->Log("CSDLPad - Gamepad [%d] connected (%s)", m_deviceNo, SDL_JoystickName(m_pSDLDevice));
			}
			else
			{
				gEnv->pLog->LogError("CSDLPad - Could not connect to Gamepad [%d]", m_deviceNo);
			}
		}
	}
}

bool CSDLPad::OpenDevice()
{
	m_pSDLDevice = SDL_JoystickOpen(m_deviceNo);
	if (m_pSDLDevice)
	{
		// SDL_SetHint(SDL_HINT_ACCELEROMETER_AS_JOYSTICK) should skip the accelerometer when
		// enumerating joysticks, but it doesn't seem to work.  Manually check for it here
		// and close it.
		// FIXME: Need a more robust way of detecting this (or get SetHint() to work).
		if (strcmp(SDL_JoystickName(m_pSDLDevice), "Android Accelerometer") == 0)
		{
			SDL_JoystickClose(m_pSDLDevice);
			return false;
		}
		m_connected = true;
	#if defined(SDL_USE_HAPTIC_FEEDBACK)
		m_pHapticDevice = SDL_HapticOpenFromJoystick(m_pSDLDevice);
		if (m_pHapticDevice && (SDL_HapticQuery(m_pHapticDevice) & SDL_HAPTIC_LEFTRIGHT))
		{
			m_supportsFeedback = true;
		}
		else
		{
			if (m_pHapticDevice) SDL_HapticClose(m_pHapticDevice);
			m_supportsFeedback = false;
		}
	#else
		m_supportsFeedback = false;
	#endif
		m_curHapticEffect = -1;
		gEnv->pLog->Log("CSDLPad - Gamepad [%d] supports feedback (Y/N): %s", m_deviceNo, m_supportsFeedback ? "Y" : "N");
	}
	return m_pSDLDevice != NULL;
}

void CSDLPad::CloseDevice()
{
	if (m_pSDLDevice)
	{
	#if defined(SDL_USE_HAPTIC_FEEDBACK)
		// close haptic, also deletes effect
		if (m_pHapticDevice) SDL_HapticClose(m_pHapticDevice);
	#endif
		// close joystick
		SDL_JoystickClose(m_pSDLDevice);
		m_pSDLDevice = NULL;
		m_curHapticEffect = -1;
		m_connected = false;
		m_supportsFeedback = false;
	}
}

CSDLPadManager::CSDLPadManager(CLinuxInput& input) :
	m_rLinuxInput(input),
	m_gamePads()
{
	m_gamePads.reserve(12);
	for (int i = 0; i < 12; ++i)
	{
		m_gamePads[i] = NULL;
	}
}

CSDLPadManager::~CSDLPadManager()
{
	GamePadVector::iterator it;
	GamePadVector::iterator end = m_gamePads.end();
	for (it = m_gamePads.begin(); it != end; ++it)
	{
		delete *it;
	}
}

bool CSDLPadManager::Init()
{
	#if defined(SDL_HINT_ACCELEROMETER_AS_JOYSTICK)
	// On platforms that have accelerometer, don't return it as a joystick
	const int zero = 0;
	if (!SDL_SetHintWithPriority(SDL_HINT_ACCELEROMETER_AS_JOYSTICK, (const char*)&zero, SDL_HINT_OVERRIDE))
	{
		gEnv->pLog->LogError("CSDLPadManager - SDL_SetHint SDL_HINT_ACCELEROMETER_AS_JOYSTICK failed: %s", SDL_GetError());
	}
	#endif
	// setup devices
	int nGamePads = SDL_NumJoysticks();
	for (int i = 0; i < nGamePads; ++i)
	{
		if (!AddGamePad(i))
		{
			return false;
		}
	}
	return true;
}

void CSDLPadManager::Update(bool bFocus)
{
	SDL_Event eventList[SDL_GAMEPAD_MAX_PEEP];

	int nEvents = SDL_PeepEvents(eventList, SDL_GAMEPAD_MAX_PEEP, SDL_GETEVENT, SDL_JOYAXISMOTION, SDL_JOYDEVICEREMOVED);
	unsigned type = 0;

	if (nEvents == -1)
	{
		gEnv->pLog->LogError("CSDLPadManager - SDL_GetEvent error: %s", SDL_GetError());
		return;
	}
	for (int i = 0; i < nEvents; ++i)
	{
		type = eventList[i].type;

		if (type == SDL_JOYAXISMOTION)
		{
			SDL_JoyAxisEvent& axisEvt = eventList[i].jaxis;
			//CRY_ASSERT(axisEvt.axis < 4);
			int id = axisEvt.which;
			if (id < 0)
			{
				gEnv->pLog->LogError("CSLDPadManager - Received event from invalid Gamepad [%d]", axisEvt.which);
				continue;
			}
			CSDLPad* pad = FindPadByInstanceId(id);
			if (pad != NULL)
			{
				pad->HandleAxisEvent(axisEvt);
			}
		}
		else if (type == SDL_JOYHATMOTION)
		{
			SDL_JoyHatEvent& hatEvt = eventList[i].jhat;
			int id = hatEvt.which;
			if (id < 0)
			{
				gEnv->pLog->LogError("CSLDPadManager - Received event from invalid Gamepad [%d]", hatEvt.which);
				continue;
			}
			CSDLPad* pad = FindPadByInstanceId(id);
			if (pad != NULL)
			{
				pad->HandleHatEvent(hatEvt);
			}
		}
		else if (type == SDL_JOYBUTTONDOWN || type == SDL_JOYBUTTONUP)
		{
			SDL_JoyButtonEvent& btEvt = eventList[i].jbutton;
			int id = btEvt.which;
			if (id < 0)
			{
				gEnv->pLog->LogError("CSLDPadManager - Received event from invalid Gamepad [%d]", btEvt.which);
				continue;
			}
			CSDLPad* pad = FindPadByInstanceId(id);
			if (pad != NULL)
			{
				pad->HandleButtonEvent(btEvt);
			}
		}
		else if (type == SDL_JOYBALLMOTION)
		{
			// do nothing | not applicable
			SDL_JoyBallEvent& ballEvt = eventList[i].jball;
			gEnv->pLog->LogError("CSLDPadManager - Gamepad [%d] triggered a ball motion event which is not supported", ballEvt.which);
		}
		else if (type == SDL_JOYDEVICEREMOVED)
		{
			SDL_JoyDeviceEvent& devEvt = eventList[i].jdevice;
			if (!RemovGamePad(devEvt.which))
			{
				gEnv->pLog->LogError("CSLDPadManager - Failed to remove Gamepad [%d]", devEvt.which);
			}
		}
		else if (type == SDL_JOYDEVICEADDED)
		{
			SDL_JoyDeviceEvent& devEvt = eventList[i].jdevice;
			if (!AddGamePad(devEvt.which))
			{
				gEnv->pLog->LogError("CSLDPadManager - Failed to Add Gamepad [%d]", devEvt.which);
			}
		}
		else
		{
			gEnv->pLog->LogError("CSDLPadManager - Unknown event processed: %x", type);
			break;
		}
	}

}

float CSDLPad::DeadZoneFilter(int input)
{
	float retval = 0.0f;
	if (input > SDL_GAMEPAD_DEADZONE_HIGH)
		retval = (input - SDL_GAMEPAD_DEADZONE_HIGH) / (float)(SDL_GAMEPAD_AXIS_MAX - SDL_GAMEPAD_DEADZONE_HIGH);
	else if (input < SDL_GAMEPAD_DEADZONE_LOW)
		retval = -(input - SDL_GAMEPAD_DEADZONE_LOW) / (float)(SDL_GAMEPAD_AXIS_MIN - SDL_GAMEPAD_DEADZONE_LOW);
	return retval;
}

bool CSDLPadManager::AddGamePad(int deviceIndex)
{
	CSDLPad* pad = FindPadByDeviceIndex(deviceIndex);
	if (pad != NULL)
	{
		pad->HandleConnectionState(true);
	}
	else
	{
		CSDLPad* pPad = new CSDLPad(m_rLinuxInput, deviceIndex);
		if (!pPad->Init())
		{
			gEnv->pLog->LogError("CSDLPadManager - Failed to init Gamepad [%d]", deviceIndex);
			delete pPad;
			return false;
		}
		pPad->HandleConnectionState(true);
		m_rLinuxInput.AddInputDevice(pPad);
		m_gamePads.insert(m_gamePads.begin() + deviceIndex, pPad);
	}
	return true;
}

bool CSDLPadManager::RemovGamePad(int instanceId)
{
	CSDLPad* pad = FindPadByInstanceId(instanceId);
	if (pad != NULL)
	{
		pad->HandleConnectionState(false);
		return true;
	}
	return false;
}

CSDLPad* CSDLPadManager::FindPadByInstanceId(int instanceId)
{
	for (GamePadVector::iterator i = m_gamePads.begin(); i != m_gamePads.end(); ++i)
	{
		if (*i == NULL)
			continue;
		if ((*i)->GetInstanceId() == instanceId)
		{
			return *i;
		}
	}
	gEnv->pLog->LogError("CSLDPadManager - Unknown device instance [%d]", instanceId);
	return NULL;
}

CSDLPad* CSDLPadManager::FindPadByDeviceIndex(int deviceIndex)
{
	if (deviceIndex < 0 || deviceIndex >= m_gamePads.size())
		return NULL;
	return m_gamePads[deviceIndex];
}

#endif
