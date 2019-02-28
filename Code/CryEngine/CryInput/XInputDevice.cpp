// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "XInputDevice.h"
#include <CrySystem/IConsole.h>
#include <CryCore/Platform/platform.h>
#include <CryThreading/IThreadManager.h>

#pragma warning(push)
#pragma warning(disable: 4244)

#if defined(USE_DXINPUT)

	#if defined(USE_DXINPUT)
LINK_SYSTEM_LIBRARY("XINPUT9_1_0.lib")
	#endif // defined(USE_DXINPUT)

GUID CXInputDevice::ProductGUID = {
	0x028e045E, 0x0000, 0x0000, { 0x00, 0x00, 0x50, 0x49, 0x44, 0x56, 0x49, 0x44 }
};
	#define INPUT_MAX                 (32767.0f)
	#define INPUT_DEFAULT_DEADZONE    (0.20f)         // Default to 10% of the range. This is a reasonable default value but can be altered if needed.
	#define INPUT_DEADZONE_MULTIPLIER (FLOAT(0x7FFF)) // Multiply dead zone to cover the +/- 32767 range.

const char* deviceNames[] =
{
	"xinput0",
	"xinput1",
	"xinput2",
	"xinput3"
};

// a few internal ids
	#define _XINPUT_GAMEPAD_LEFT_TRIGGER      (1 << 17)
	#define _XINPUT_GAMEPAD_RIGHT_TRIGGER     (1 << 18)
	#define _XINPUT_GAMEPAD_LEFT_THUMB_X      (1 << 19)
	#define _XINPUT_GAMEPAD_LEFT_THUMB_Y      (1 << 20)
	#define _XINPUT_GAMEPAD_RIGHT_THUMB_X     (1 << 21)
	#define _XINPUT_GAMEPAD_RIGHT_THUMB_Y     (1 << 22)
	#define _XINPUT_GAMEPAD_LEFT_TRIGGER_BTN  (1 << 23) // left trigger usable as a press/release button
	#define _XINPUT_GAMEPAD_RIGHT_TRIGGER_BTN (1 << 24) // right trigger usable as a press/release button
	#define _XINPUT_GAMEPAD_LEFT_THUMB_UP     (1 << 25)
	#define _XINPUT_GAMEPAD_LEFT_THUMB_DOWN   ((1 << 25) + 1)
	#define _XINPUT_GAMEPAD_LEFT_THUMB_LEFT   ((1 << 25) + 2)
	#define _XINPUT_GAMEPAD_LEFT_THUMB_RIGHT  ((1 << 25) + 3)
	#define _XINPUT_GAMEPAD_RIGHT_THUMB_UP    (1 << 26)
	#define _XINPUT_GAMEPAD_RIGHT_THUMB_DOWN  ((1 << 26) + 1)
	#define _XINPUT_GAMEPAD_RIGHT_THUMB_LEFT  ((1 << 26) + 2)
	#define _XINPUT_GAMEPAD_RIGHT_THUMB_RIGHT ((1 << 26) + 3)

volatile bool g_bConnected[4] = { false };

//int g_numControllers = 0;

//////////////////////////////////////////////////////////////////////////
// This is a thread task responsible for periodically checking gamepad
// connection status.
//////////////////////////////////////////////////////////////////////////
class CInputConnectionThreadTask : public IThread
{
	IInput*       m_pInput;
	volatile bool m_bQuit;

public:
	//////////////////////////////////////////////////////////////////////////
	CInputConnectionThreadTask(IInput* pInput)
	{
		m_bQuit = false;
		m_pInput = pInput;
	}

	//////////////////////////////////////////////////////////////////////////
	// Signals the thread that it should not accept anymore work and exit
	void SignalStopWork()
	{
		m_bQuit = true;
	}

	//////////////////////////////////////////////////////////////////////////
	// Start accepting work on thread
	virtual void ThreadEntry()
	{
		IInput* pInput = m_pInput;
		XINPUT_CAPABILITIES caps;

		while (!m_bQuit)
		{
			{
				for (DWORD i = 0; i < 4; ++i)
				{
					DWORD r = XInputGetCapabilities(i, XINPUT_FLAG_GAMEPAD, &caps);
					g_bConnected[i] = r == ERROR_SUCCESS;
				}
			}
			CrySleep(1000);
		}
	}
};

//////////////////////////////////////////////////////////////////////////
CInputConnectionThreadTask* g_pInputConnectionThreadTask = 0;

void CleanupVibrationAtExit()
{
	// Kill vibration on exit
	XINPUT_VIBRATION vibration;
	ZeroMemory(&vibration, sizeof(XINPUT_VIBRATION));
	for (int i = 0; i < 4; i++)
		XInputSetState(i, &vibration);
}

CXInputDevice::CXInputDevice(IInput& input, int deviceNo) : CInputDevice(input, deviceNames[deviceNo]),
	m_deviceNo(deviceNo), m_connected(false)
{
	memset(&m_state, 0, sizeof(XINPUT_STATE));

	ZeroMemory(&m_currentVibrationSettings, sizeof(XINPUT_VIBRATION));

	m_deviceType = eIDT_Gamepad;
	SetVibration(0, 0, 0);

	m_forceResendSticks = false;

	atexit(CleanupVibrationAtExit);
}

CXInputDevice::~CXInputDevice()
{
	SetVibration(0, 0, 0);

	if (g_pInputConnectionThreadTask)
	{
		g_pInputConnectionThreadTask->SignalStopWork();
		gEnv->pThreadManager->JoinThread(g_pInputConnectionThreadTask, eJM_Join);
		delete g_pInputConnectionThreadTask;
		g_pInputConnectionThreadTask = 0;
	}
}

bool CXInputDevice::Init()
{
	// setup input event names
	MapSymbol(XINPUT_GAMEPAD_DPAD_UP, eKI_XI_DPadUp, "xi_dpad_up");
	MapSymbol(XINPUT_GAMEPAD_DPAD_DOWN, eKI_XI_DPadDown, "xi_dpad_down");
	MapSymbol(XINPUT_GAMEPAD_DPAD_LEFT, eKI_XI_DPadLeft, "xi_dpad_left");
	MapSymbol(XINPUT_GAMEPAD_DPAD_RIGHT, eKI_XI_DPadRight, "xi_dpad_right");
	MapSymbol(XINPUT_GAMEPAD_START, eKI_XI_Start, "xi_start");
	MapSymbol(XINPUT_GAMEPAD_BACK, eKI_XI_Back, "xi_back");
	MapSymbol(XINPUT_GAMEPAD_LEFT_THUMB, eKI_XI_ThumbL, "xi_thumbl");
	MapSymbol(XINPUT_GAMEPAD_RIGHT_THUMB, eKI_XI_ThumbR, "xi_thumbr");
	MapSymbol(XINPUT_GAMEPAD_LEFT_SHOULDER, eKI_XI_ShoulderL, "xi_shoulderl");
	MapSymbol(XINPUT_GAMEPAD_RIGHT_SHOULDER, eKI_XI_ShoulderR, "xi_shoulderr");
	MapSymbol(XINPUT_GAMEPAD_A, eKI_XI_A, "xi_a");
	MapSymbol(XINPUT_GAMEPAD_B, eKI_XI_B, "xi_b");
	MapSymbol(XINPUT_GAMEPAD_X, eKI_XI_X, "xi_x");
	MapSymbol(XINPUT_GAMEPAD_Y, eKI_XI_Y, "xi_y");
	MapSymbol(_XINPUT_GAMEPAD_LEFT_TRIGGER, eKI_XI_TriggerL, "xi_triggerl", SInputSymbol::Trigger);
	MapSymbol(_XINPUT_GAMEPAD_RIGHT_TRIGGER, eKI_XI_TriggerR, "xi_triggerr", SInputSymbol::Trigger);
	MapSymbol(_XINPUT_GAMEPAD_LEFT_THUMB_X, eKI_XI_ThumbLX, "xi_thumblx", SInputSymbol::Axis);
	MapSymbol(_XINPUT_GAMEPAD_LEFT_THUMB_Y, eKI_XI_ThumbLY, "xi_thumbly", SInputSymbol::Axis);
	MapSymbol(_XINPUT_GAMEPAD_LEFT_THUMB_UP, eKI_XI_ThumbLUp, "xi_thumbl_up");
	MapSymbol(_XINPUT_GAMEPAD_LEFT_THUMB_DOWN, eKI_XI_ThumbLDown, "xi_thumbl_down");
	MapSymbol(_XINPUT_GAMEPAD_LEFT_THUMB_LEFT, eKI_XI_ThumbLLeft, "xi_thumbl_left");
	MapSymbol(_XINPUT_GAMEPAD_LEFT_THUMB_RIGHT, eKI_XI_ThumbLRight, "xi_thumbl_right");
	MapSymbol(_XINPUT_GAMEPAD_RIGHT_THUMB_X, eKI_XI_ThumbRX, "xi_thumbrx", SInputSymbol::Axis);
	MapSymbol(_XINPUT_GAMEPAD_RIGHT_THUMB_Y, eKI_XI_ThumbRY, "xi_thumbry", SInputSymbol::Axis);
	MapSymbol(_XINPUT_GAMEPAD_RIGHT_THUMB_UP, eKI_XI_ThumbRUp, "xi_thumbr_up");
	MapSymbol(_XINPUT_GAMEPAD_RIGHT_THUMB_DOWN, eKI_XI_ThumbRDown, "xi_thumbr_down");
	MapSymbol(_XINPUT_GAMEPAD_RIGHT_THUMB_LEFT, eKI_XI_ThumbRLeft, "xi_thumbr_left");
	MapSymbol(_XINPUT_GAMEPAD_RIGHT_THUMB_RIGHT, eKI_XI_ThumbRRight, "xi_thumbr_right");
	MapSymbol(_XINPUT_GAMEPAD_LEFT_TRIGGER_BTN, eKI_XI_TriggerLBtn, "xi_triggerl_btn");  // trigger usable as a button
	MapSymbol(_XINPUT_GAMEPAD_RIGHT_TRIGGER_BTN, eKI_XI_TriggerRBtn, "xi_triggerr_btn"); // trigger usable as a button

	//////////////////////////////////////////////////////////////////////////

	m_basicLeftMotorRumble = m_basicRightMotorRumble = 0.0f;
	m_frameLeftMotorRumble = m_frameRightMotorRumble = 0.0f;
	RestoreDefaultDeadZone();

	return true;
}

void CXInputDevice::PostInit()
{
	// Create input connection thread task
	if (g_pInputConnectionThreadTask == NULL)
	{
		g_pInputConnectionThreadTask = new CInputConnectionThreadTask(&GetIInput());

		if (!gEnv->pThreadManager->SpawnThread(g_pInputConnectionThreadTask, "InputWorker"))
		{
			CryFatalError("Error spawning \"InputWorker\" thread.");
		}
	}
}

void FixDeadzone(Vec2& d)
{
	const float INPUT_DEFAULT_DEADZONE_INNER = INPUT_DEFAULT_DEADZONE;
	const float INPUT_DEFAULT_DEADZONE_OUTER = INPUT_DEFAULT_DEADZONE_INNER + 0.1f;

	const float deadZoneInner = 32768.0f * INPUT_DEFAULT_DEADZONE_INNER;
	const float deadZoneOuter = 32768.0f * INPUT_DEFAULT_DEADZONE_OUTER;

	float length = d[0] * d[0] + d[1] * d[1];

	if (length < (deadZoneInner * deadZoneInner))
	{
		// Inside radial deadzone, clamp to 0,0
		d[0] = d[1] = 0.0f;
	}
	else if (length < (deadZoneOuter * deadZoneOuter))
	{
		// Components of d are -32768 to +32767
		d /= 32768.0f;

		float l = sqrtf(length) / 32768.0f;

		d *= ((l - INPUT_DEFAULT_DEADZONE_INNER) / (INPUT_DEFAULT_DEADZONE_OUTER - INPUT_DEFAULT_DEADZONE_INNER));
		d *= 32767.0f;
	}
	// Outside the outer zone, leave the values untouched
}

void CXInputDevice::Update(bool bFocus)
{
	CRY_PROFILE_FUNCTION(PROFILE_INPUT);

	DEBUG_CONTROLLER_RENDER_BUTTON_ACTION;

	const bool wasConnected = m_connected;
	const bool connected = g_bConnected[m_deviceNo];
	const bool disconnecting = (wasConnected && !connected);

	UpdateConnectedState(connected);

	float frameTime = gEnv->pTimer->GetFrameTime();
	float now = gEnv->pTimer->GetFrameStartTime().GetSeconds();
	if ((m_fVibrationTimer && m_fVibrationTimer < now) || g_pInputCVars->i_forcefeedback == 0 ||
	    gEnv->pSystem->IsPaused() || frameTime < 0.001f)
	{
		m_fVibrationTimer = 0;
		SetVibration(0, 0, 0.0f);
	}

	// Force inputs to get sent out when we're disconnecting or
	// we can get stuck with thumbsticks in a non-neutral state
	if (disconnecting)
	{
		m_forceResendSticks = true;
	}

	// interpret input
	if ((m_connected && bFocus) || disconnecting)
	{
		XINPUT_STATE state;
		memset(&state, 0, sizeof(XINPUT_STATE));
		if (ERROR_SUCCESS != XInputGetState(m_deviceNo, &state) && !disconnecting)
			return;

		if (state.dwPacketNumber != m_state.dwPacketNumber)
		{
			SInputEvent event;
			SInputSymbol* pSymbol = 0;

			event.deviceIndex = (uint8)m_deviceNo;

			if (g_pInputCVars->i_xinput_deadzone_handling > 0)
			{
				{
					Vec2 d(state.Gamepad.sThumbLX, state.Gamepad.sThumbLY);

					FixDeadzone(d);

					state.Gamepad.sThumbLX = d[0];
					state.Gamepad.sThumbLY = d[1];
				}

				{
					Vec2 d(state.Gamepad.sThumbRX, state.Gamepad.sThumbRY);

					FixDeadzone(d);

					state.Gamepad.sThumbRX = d[0];
					state.Gamepad.sThumbRY = d[1];
				}
			}
			else
			{
				const float INV_VALIDRANGE = (1.0f / (INPUT_MAX - m_fDeadZone));

				// make the inputs move smoothly out of the deadzone instead of snapping straight to m_fDeadZone
				float fraction = max((float)abs(state.Gamepad.sThumbLX) - m_fDeadZone, 0.f) * INV_VALIDRANGE;
				state.Gamepad.sThumbLX = fraction * INPUT_MAX * sgn(state.Gamepad.sThumbLX);

				fraction = max((float)abs(state.Gamepad.sThumbLY) - m_fDeadZone, 0.f) * INV_VALIDRANGE;
				state.Gamepad.sThumbLY = fraction * INPUT_MAX * sgn(state.Gamepad.sThumbLY);

				// make the inputs move smoothly out of the deadzone instead of snapping straight to m_fDeadZone
				fraction = max((float)abs(state.Gamepad.sThumbRX) - m_fDeadZone, 0.f) * INV_VALIDRANGE;
				state.Gamepad.sThumbRX = fraction * INPUT_MAX * sgn(state.Gamepad.sThumbRX);

				fraction = max((float)abs(state.Gamepad.sThumbRY) - m_fDeadZone, 0.f) * INV_VALIDRANGE;
				state.Gamepad.sThumbRY = fraction * INPUT_MAX * sgn(state.Gamepad.sThumbRY);

			}

			// compare new values against cached value and only send out changes as new input
			WORD buttonsChange = m_state.Gamepad.wButtons ^ state.Gamepad.wButtons;
			if (buttonsChange)
			{
				for (int i = 0; i < 16; ++i)
				{
					uint32 id = (1 << i);
					if (buttonsChange & id && (pSymbol = DevSpecIdToSymbol(id)))
					{
						pSymbol->PressEvent((state.Gamepad.wButtons & id) != 0);
						pSymbol->AssignTo(event);
						event.deviceType = eIDT_Gamepad;
						GetIInput().PostInputEvent(event);
						DEBUG_CONTROLLER_LOG_BUTTON_ACTION(pSymbol);
					}
				}
			}

			// now we have done the digital buttons ... let's do the analog stuff
			if (m_state.Gamepad.bLeftTrigger != state.Gamepad.bLeftTrigger)
			{
				pSymbol = DevSpecIdToSymbol(_XINPUT_GAMEPAD_LEFT_TRIGGER);
				pSymbol->ChangeEvent(state.Gamepad.bLeftTrigger / 255.0f);
				pSymbol->AssignTo(event);
				event.deviceType = eIDT_Gamepad;
				GetIInput().PostInputEvent(event);
				DEBUG_CONTROLLER_LOG_BUTTON_ACTION(pSymbol);

				//--- Check previous and current trigger against threshold for digital press/release event
				bool bIsPressed = state.Gamepad.bLeftTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD ? true : false;
				bool bWasPressed = m_state.Gamepad.bLeftTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD ? true : false;
				if (bIsPressed != bWasPressed)
				{
					pSymbol = DevSpecIdToSymbol(_XINPUT_GAMEPAD_LEFT_TRIGGER_BTN);
					pSymbol->PressEvent(bIsPressed);
					pSymbol->AssignTo(event);
					event.deviceType = eIDT_Gamepad;
					GetIInput().PostInputEvent(event);
					DEBUG_CONTROLLER_LOG_BUTTON_ACTION(pSymbol);
				}
			}
			if (m_state.Gamepad.bRightTrigger != state.Gamepad.bRightTrigger)
			{
				pSymbol = DevSpecIdToSymbol(_XINPUT_GAMEPAD_RIGHT_TRIGGER);
				pSymbol->ChangeEvent(state.Gamepad.bRightTrigger / 255.0f);
				pSymbol->AssignTo(event);
				event.deviceType = eIDT_Gamepad;
				GetIInput().PostInputEvent(event);
				DEBUG_CONTROLLER_LOG_BUTTON_ACTION(pSymbol);

				//--- Check previous and current trigger against threshold for digital press/release event
				bool bIsPressed = state.Gamepad.bRightTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD;
				bool bWasPressed = m_state.Gamepad.bRightTrigger > XINPUT_GAMEPAD_TRIGGER_THRESHOLD;
				if (bIsPressed != bWasPressed)
				{
					pSymbol = DevSpecIdToSymbol(_XINPUT_GAMEPAD_RIGHT_TRIGGER_BTN);
					pSymbol->PressEvent(bIsPressed);
					pSymbol->AssignTo(event);
					event.deviceType = eIDT_Gamepad;
					GetIInput().PostInputEvent(event);
					DEBUG_CONTROLLER_LOG_BUTTON_ACTION(pSymbol);
				}
			}
			if ((m_state.Gamepad.sThumbLX != state.Gamepad.sThumbLX) || m_forceResendSticks)
			{
				pSymbol = DevSpecIdToSymbol(_XINPUT_GAMEPAD_LEFT_THUMB_X);
				if (state.Gamepad.sThumbLX == 0.f)
					pSymbol->ChangeEvent(0.f);
				else
					pSymbol->ChangeEvent((state.Gamepad.sThumbLX + 32768) / 32767.5f - 1.0f);
				pSymbol->AssignTo(event);
				event.deviceType = eIDT_Gamepad;
				GetIInput().PostInputEvent(event);
				//--- Check previous and current state to generate digital press/release event
				static SInputSymbol* pSymbolLeft = DevSpecIdToSymbol(_XINPUT_GAMEPAD_LEFT_THUMB_LEFT);
				ProcessAnalogStick(pSymbolLeft, m_state.Gamepad.sThumbLX, state.Gamepad.sThumbLX, -25000);
				static SInputSymbol* pSymbolRight = DevSpecIdToSymbol(_XINPUT_GAMEPAD_LEFT_THUMB_RIGHT);
				ProcessAnalogStick(pSymbolRight, m_state.Gamepad.sThumbLX, state.Gamepad.sThumbLX, 25000);
			}
			if ((m_state.Gamepad.sThumbLY != state.Gamepad.sThumbLY) || m_forceResendSticks)
			{
				pSymbol = DevSpecIdToSymbol(_XINPUT_GAMEPAD_LEFT_THUMB_Y);
				if (state.Gamepad.sThumbLY == 0.f)
					pSymbol->ChangeEvent(0.f);
				else
					pSymbol->ChangeEvent((state.Gamepad.sThumbLY + 32768) / 32767.5f - 1.0f);
				pSymbol->AssignTo(event);
				event.deviceType = eIDT_Gamepad;
				GetIInput().PostInputEvent(event);
				//--- Check previous and current state to generate digital press/release event
				static SInputSymbol* pSymbolUp = DevSpecIdToSymbol(_XINPUT_GAMEPAD_LEFT_THUMB_UP);
				ProcessAnalogStick(pSymbolUp, m_state.Gamepad.sThumbLY, state.Gamepad.sThumbLY, 25000);
				static SInputSymbol* pSymbolDown = DevSpecIdToSymbol(_XINPUT_GAMEPAD_LEFT_THUMB_DOWN);
				ProcessAnalogStick(pSymbolDown, m_state.Gamepad.sThumbLY, state.Gamepad.sThumbLY, -25000);
			}
			if ((m_state.Gamepad.sThumbRX != state.Gamepad.sThumbRX) || m_forceResendSticks)
			{
				pSymbol = DevSpecIdToSymbol(_XINPUT_GAMEPAD_RIGHT_THUMB_X);
				if (state.Gamepad.sThumbRX == 0.f)
					pSymbol->ChangeEvent(0.f);
				else
					pSymbol->ChangeEvent((state.Gamepad.sThumbRX + 32768) / 32767.5f - 1.0f);
				pSymbol->AssignTo(event);
				event.deviceType = eIDT_Gamepad;
				GetIInput().PostInputEvent(event);
				//--- Check previous and current state to generate digital press/release event
				static SInputSymbol* pSymbolLeft = DevSpecIdToSymbol(_XINPUT_GAMEPAD_RIGHT_THUMB_LEFT);
				ProcessAnalogStick(pSymbolLeft, m_state.Gamepad.sThumbRX, state.Gamepad.sThumbRX, -25000);
				static SInputSymbol* pSymbolRight = DevSpecIdToSymbol(_XINPUT_GAMEPAD_RIGHT_THUMB_RIGHT);
				ProcessAnalogStick(pSymbolRight, m_state.Gamepad.sThumbRX, state.Gamepad.sThumbRX, 25000);
			}
			if ((m_state.Gamepad.sThumbRY != state.Gamepad.sThumbRY) || m_forceResendSticks)
			{
				pSymbol = DevSpecIdToSymbol(_XINPUT_GAMEPAD_RIGHT_THUMB_Y);
				if (state.Gamepad.sThumbRY == 0.f)
					pSymbol->ChangeEvent(0.f);
				else
					pSymbol->ChangeEvent((state.Gamepad.sThumbRY + 32768) / 32767.5f - 1.0f);
				pSymbol->AssignTo(event);
				event.deviceType = eIDT_Gamepad;
				GetIInput().PostInputEvent(event);
				//--- Check previous and current state to generate digital press/release event
				static SInputSymbol* pSymbolUp = DevSpecIdToSymbol(_XINPUT_GAMEPAD_RIGHT_THUMB_UP);
				ProcessAnalogStick(pSymbolUp, m_state.Gamepad.sThumbRY, state.Gamepad.sThumbRY, 25000);
				static SInputSymbol* pSymbolDown = DevSpecIdToSymbol(_XINPUT_GAMEPAD_RIGHT_THUMB_DOWN);
				ProcessAnalogStick(pSymbolDown, m_state.Gamepad.sThumbRY, state.Gamepad.sThumbRY, -25000);
			}

			// update cache
			m_state = state;
			m_forceResendSticks = false;
		}
	}
}

void CXInputDevice::ClearKeyState()
{
	m_fVibrationTimer = 0;
	SetVibration(0, 0, 0.0f);

	CInputDevice::ClearKeyState();
}

void CXInputDevice::ClearAnalogKeyState(TInputSymbols& clearedSymbols)
{
	if (m_state.Gamepad.sThumbLX != 0 || m_state.Gamepad.sThumbLY != 0 || m_state.Gamepad.sThumbRX != 0 || m_state.Gamepad.sThumbRY != 0)
	{
		// Only resend if the sticks are actually active. If they aren't then active then sending a resendsticks causes pad inputs to be sent through even if you aren't using the pad - which messes with the UI on PC.
		m_forceResendSticks = true;
	}

	// Reset internal state and decrement packet number so updated again immediately
	// This will force-send the current state of all analog keys on next update
	m_state.Gamepad.sThumbLX = 0;
	m_state.Gamepad.sThumbLY = 0;
	m_state.Gamepad.sThumbRX = 0;
	m_state.Gamepad.sThumbRY = 0;
	m_state.Gamepad.bLeftTrigger = 0;
	m_state.Gamepad.bRightTrigger = 0;
	m_state.dwPacketNumber--;

	//remove the symbols cleared above from the hold list
	static SInputSymbol* pSymbolUpL = DevSpecIdToSymbol(_XINPUT_GAMEPAD_LEFT_THUMB_UP);
	static SInputSymbol* pSymbolDownL = DevSpecIdToSymbol(_XINPUT_GAMEPAD_LEFT_THUMB_DOWN);
	static SInputSymbol* pSymbolLeftL = DevSpecIdToSymbol(_XINPUT_GAMEPAD_LEFT_THUMB_LEFT);
	static SInputSymbol* pSymbolRightL = DevSpecIdToSymbol(_XINPUT_GAMEPAD_LEFT_THUMB_RIGHT);

	static SInputSymbol* pSymbolUpR = DevSpecIdToSymbol(_XINPUT_GAMEPAD_RIGHT_THUMB_UP);
	static SInputSymbol* pSymbolDownR = DevSpecIdToSymbol(_XINPUT_GAMEPAD_RIGHT_THUMB_DOWN);
	static SInputSymbol* pSymbolLeftR = DevSpecIdToSymbol(_XINPUT_GAMEPAD_RIGHT_THUMB_LEFT);
	static SInputSymbol* pSymbolRightR = DevSpecIdToSymbol(_XINPUT_GAMEPAD_RIGHT_THUMB_RIGHT);

	static SInputSymbol* pSymbolTrgBtnL = DevSpecIdToSymbol(_XINPUT_GAMEPAD_LEFT_TRIGGER_BTN);
	static SInputSymbol* pSymbolTrgBtnR = DevSpecIdToSymbol(_XINPUT_GAMEPAD_RIGHT_TRIGGER_BTN);

	clearedSymbols.reserve(10);

	clearedSymbols.push_back(pSymbolUpL);
	clearedSymbols.push_back(pSymbolDownL);
	clearedSymbols.push_back(pSymbolLeftL);
	clearedSymbols.push_back(pSymbolRightL);
	clearedSymbols.push_back(pSymbolUpR);
	clearedSymbols.push_back(pSymbolDownR);
	clearedSymbols.push_back(pSymbolLeftR);
	clearedSymbols.push_back(pSymbolRightR);
	clearedSymbols.push_back(pSymbolTrgBtnL);
	clearedSymbols.push_back(pSymbolTrgBtnR);
}

void CXInputDevice::UpdateConnectedState(bool isConnected)
{
	if (m_connected != isConnected)
	{
		SInputEvent event;
		event.deviceIndex = (uint8)m_deviceNo;
		event.deviceType = eIDT_Gamepad;
		event.state = eIS_Changed;
		event.deviceUniqueID = m_uniqueId;

		// state has changed, emit event here
		if (isConnected)
		{
			// connect
			event.keyId = eKI_XI_Connect;
			event.keyName = "connect";
		}
		else
		{
			// disconnect
			event.keyId = eKI_XI_Disconnect;
			event.keyName = "disconnect";
		}
		m_connected = isConnected;
		GetIInput().PostInputEvent(event);

		// Send generalized keyId connect/disconnect
		// eKI_XI_Connect & eKI_XI_Disconnect should be deprecated because all devices can be connected/disconnected
		event.keyId = (isConnected) ? eKI_SYS_ConnectDevice : eKI_SYS_DisconnectDevice;
		GetIInput().PostInputEvent(event, true);
	}
}

bool CXInputDevice::SetForceFeedback(IFFParams params)
{
	return SetVibration(min(1.0f, (float)abs(params.strengthA)) * 65535.f, min(1.0f, (float)abs(params.strengthB)) * 65535.f, params.timeInSeconds, params.effectId);
}

bool CXInputDevice::SetVibration(USHORT leftMotor, USHORT rightMotor, float timing, EFFEffectId effectId)
{
	//if(g_bConnected[m_deviceNo])
	if (m_connected)
	{
		USHORT desiredVibrationLeft = 0;
		USHORT desiredVibrationRight = 0;

		if (effectId == eFF_Rumble_Basic)
		{
			const float now = gEnv->pTimer->GetFrameStartTime().GetSeconds();

			if (m_fVibrationTimer > 0.0f)
			{
				const float oldRumbleRatio = (float)__fsel(-timing, 1.0f, min(1.0f, (fabsf(m_fVibrationTimer - now) * (float)__fres((timing + FLT_EPSILON)))));

				//Store only 'basic', without frame part
				m_basicLeftMotorRumble = max(leftMotor, (USHORT)(m_basicLeftMotorRumble * oldRumbleRatio));
				m_basicRightMotorRumble = max(rightMotor, (USHORT)(m_basicRightMotorRumble * oldRumbleRatio));

				const float newLeftMotor = GetClampedLeftMotorAccumulatedVibration() * oldRumbleRatio;
				const float newRightMotor = GetClampedRightMotorAccumulatedVibration() * oldRumbleRatio;
				desiredVibrationLeft = max(leftMotor, (USHORT)newLeftMotor);
				desiredVibrationRight = max(rightMotor, (USHORT)newRightMotor);
			}
			else
			{
				m_basicLeftMotorRumble = leftMotor;
				m_basicRightMotorRumble = rightMotor;

				desiredVibrationLeft = max(leftMotor, (USHORT)GetClampedLeftMotorAccumulatedVibration());
				desiredVibrationRight = max(rightMotor, (USHORT)GetClampedRightMotorAccumulatedVibration());
			}

			m_fVibrationTimer = (float)__fsel(-timing, 0.0f, now + timing);
		}
		else if (effectId == eFF_Rumble_Frame)
		{
			m_frameLeftMotorRumble = leftMotor;
			m_frameRightMotorRumble = rightMotor;

			desiredVibrationLeft = (USHORT)GetClampedLeftMotorAccumulatedVibration();
			desiredVibrationRight = (USHORT)GetClampedRightMotorAccumulatedVibration();
		}

		if (m_currentVibrationSettings.wLeftMotorSpeed != desiredVibrationLeft ||
		    m_currentVibrationSettings.wRightMotorSpeed != desiredVibrationRight)
		{
			XINPUT_VIBRATION vibration;
			ZeroMemory(&vibration, sizeof(XINPUT_VIBRATION));
			vibration.wLeftMotorSpeed = desiredVibrationLeft;
			vibration.wRightMotorSpeed = desiredVibrationRight;
			DWORD error = XInputSetState(m_deviceNo, &vibration);
			if (error == ERROR_SUCCESS)
			{
				m_currentVibrationSettings.wLeftMotorSpeed = desiredVibrationLeft;
				m_currentVibrationSettings.wRightMotorSpeed = desiredVibrationRight;
			}
		}

		return true;
	}
	return false;
}

void CXInputDevice::ProcessAnalogStick(SInputSymbol* pSymbol, SHORT prev, SHORT current, SHORT threshold)
{
	bool send = false;

	if (threshold > 0.0f)
	{
		if (prev >= threshold && current < threshold)
		{
			// release
			send = true;
			pSymbol->PressEvent(false);
		}
		else if (prev <= threshold && current > threshold)
		{
			// press
			send = true;
			pSymbol->PressEvent(true);
		}
	}
	else
	{
		if (prev <= threshold && current > threshold)
		{
			// release
			send = true;
			pSymbol->PressEvent(false);
		}
		else if (prev >= threshold && current < threshold)
		{
			// press
			send = true;
			pSymbol->PressEvent(true);
		}
	}

	if (send)
	{
		SInputEvent event;
		event.deviceIndex = (uint8)m_deviceNo;
		pSymbol->AssignTo(event);
		event.deviceType = eIDT_Gamepad;
		GetIInput().PostInputEvent(event);
	}
}

void CXInputDevice::SetDeadZone(float fThreshold)
{
	m_fDeadZone = clamp_tpl(fThreshold, 0.001f, 1.0f) * INPUT_DEADZONE_MULTIPLIER;
}

void CXInputDevice::RestoreDefaultDeadZone()
{
	SetDeadZone(INPUT_DEFAULT_DEADZONE);
}

#endif //defined(USE_DXINPUT)

#pragma warning(pop)
