// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "OculusTouchController.h"

#include <CryRenderer/IRenderer.h>
#include <CryRenderer/IRenderAuxGeom.h>

#define AddSymbolBase(EKI, DEV_KEY_ID, KEY_NAME, KEY_TYPE) m_symbols[EKI - KI_MOTION_BASE] = MapSymbol(DEV_KEY_ID, EKI, KEY_NAME, KEY_TYPE, 0);

#define AddSymbolButton(EKI, DEV_KEY_ID, KEY_NAME, HELPER_FLAGS) \
	AddSymbolBase(EKI, DEV_KEY_ID, KEY_NAME, SInputSymbol::Button) \
	m_buttonSybmolsEx.push_back(SSymbolEx(EKI - KI_MOTION_BASE, HELPER_FLAGS));

#define AddSymbolTrigger(EKI, DEV_KEY_ID, KEY_NAME, HELPER_FLAGS) \
	AddSymbolBase(EKI, DEV_KEY_ID, KEY_NAME, SInputSymbol::Trigger) \
	m_triggerSybmolsEx.push_back(SSymbolEx(EKI - KI_MOTION_BASE, HELPER_FLAGS));

#define AddSymbolStick(EKI, DEV_KEY_ID, KEY_NAME, HELPER_FLAGS) \
	AddSymbolBase(EKI, DEV_KEY_ID, KEY_NAME, SInputSymbol::Axis)  \
	m_stickSybmolsEx.push_back(SSymbolEx(EKI - KI_MOTION_BASE, HELPER_FLAGS));

// -------------------------------------------------------------------------
namespace CryVR {
namespace Oculus {

// Current Touch controller hardware is not very accurate in terms of analogue triggers and sticks
// We could expose these values with some Get/Set functions
float COculusController::ms_triggerIndexZeroThreshold = 0.1f;
float COculusController::ms_triggerHandZeroThreshold = 0.1f;
float COculusController::ms_sticksZeroThreshold = 0.1f;

bool COculusController::IsConnected(EHmdController id) const
{
	if (id != eHmdController_OculusLeftHand && id != eHmdController_OculusRightHand)
	{
		CryLogAlways("[Error][COculusController::IsConnected] invalid controller id: %u", id);
		return false;
	}

	return (m_state.m_connected[id] != 0);
}

bool COculusController::IsButtonPressed(EHmdController id, EKeyId button) const
{
	if (button < KI_MOTION_BASE || button > eKI_Motion_OculusTouch_LastButtonIndex)
	{
		CryLogAlways("[Error][COculusController::IsButtonPressed] invalid button id: %u", button);
		return false;
	}

	EHmdControllerButtons eButton = static_cast<EHmdControllerButtons>(OCULUS_BUTTON_TO_ENUM_BIT(button));
	CRY_ASSERT(eButton > 0 && eButton <= eHmdControllerButtons_RTrigger);

	return IsButtonPressed_Internal(eButton);
}

bool COculusController::IsButtonTouched(EHmdController id, EKeyId button) const
{
	if (button < KI_MOTION_BASE || button > eKI_Motion_OculusTouch_LastButtonIndex)
	{
		CryLogAlways("[Error][COculusController::IsButtonTouched] invalid button id: %u", button);
		return false;
	}

	EHmdControllerButtons eButton = static_cast<EHmdControllerButtons>(OCULUS_BUTTON_TO_ENUM_BIT(button));
	CRY_ASSERT(eButton > 0 && eButton <= eHmdControllerButtons_RTrigger);

	return IsButtonTouched_Internal(eButton);
}

bool COculusController::IsGestureTriggered(EHmdController id, EKeyId gesture) const
{
	if (gesture < eKI_Motion_OculusTouch_FirstGestureIndex || gesture > eKI_Motion_OculusTouch_LastGestureIndex)
	{
		CryLogAlways("[Error][COculusController::IsGestureTriggered] invalid gesture id: %u", gesture);
		return false;
	}

	EHmdControllerGestures eGesture = static_cast<EHmdControllerGestures>(OCULUS_GESTURE_TO_ENUM_BIT(gesture));
	CRY_ASSERT(eGesture > 0 && eGesture <= eHmdControllerGesture_RIndexPointing);

	return IsGestureTriggered_Internal(eGesture);
}

float COculusController::GetTriggerValue(EHmdController id, EKeyId trigger) const
{
	if (trigger < eKI_Motion_OculusTouch_FirstTriggerIndex || trigger > eKI_Motion_OculusTouch_LastTriggerIndex)
	{
		CryLogAlways("[Error][COculusController::GetTriggerValue] invalid trigger id: %u", trigger);
		return false;
	}

	EHmdControllerTriggers eTrigger = static_cast<EHmdControllerTriggers>(EKEYID_OCULUS_TRIGGER_TO_ENUM(trigger));
	CRY_ASSERT(eTrigger >= 0 && eTrigger < eHmdControllerTriggers_MaxNumTriggers);

	return GetTriggerValue_Internal(eTrigger);
}

Vec2 COculusController::GetThumbStickValue(EHmdController id, EKeyId stick) const
{
	const bool isLeft = stick == eKI_Motion_OculusTouch_StickL_Y || stick == eKI_Motion_OculusTouch_StickL_X;
	const bool isRight = stick == eKI_Motion_OculusTouch_StickR_Y || stick == eKI_Motion_OculusTouch_StickR_X;
	const bool valid = (isLeft || isRight);
	EHmdControllerThumbSticks index = isLeft ? eHmdControllerThumbSticks_Left : eHmdControllerThumbSticks_Right;

	if (!valid)
	{
		CRY_ASSERT(0);
		CryLogAlways("[Error][COculusController::GetThumbStickValue] invalid stick id: %u", stick);
	}

	return m_state.m_thumbSitcks[index];
}

void COculusController::ApplyForceFeedback(EHmdController id, float freq, float amplitude)
{
	if (m_pSession)
	{
		ovr_SetControllerVibration(m_pSession, m_controllerEnumMap[id], clamp_tpl(freq, 0.f, 1.f), clamp_tpl(amplitude, 0.f, 1.f));
	}
}

// -------------------------------------------------------------------------
COculusController::COculusController() : m_pSession(nullptr)
{
	memset(&m_previousState, 0, sizeof(m_previousState));
	memset(&m_state, 0, sizeof(m_state));
	memset(&m_symbols, 0, sizeof(m_symbols));

	// sanity check on some enums defined elsewhere or constructed from external enums, specially if they are use for indexing arrays
	static_assert(
	  ((eHmdController_OculusLeftHand == 0 && eHmdController_OculusRightHand == 1) ||
	   (eHmdController_OculusLeftHand == 1 && eHmdController_OculusRightHand == 0)) &&
	  eHmdController_MaxNumOculusControllers == 2, "Invalid enum values!");
	static_assert(
	  eHmdControllerTriggers_LT == 0 && eHmdControllerTriggers_RT == 1 &&
	  eHmdControllerTriggers_LT2 == 2 && eHmdControllerTriggers_RT2 == 3 &&
	  eHmdControllerTriggers_MaxNumTriggers == 4, "Invalid enum values!");

	m_controllerEnumMap[eHmdController_OculusLeftHand] = ovrControllerType_LTouch;
	m_controllerEnumMap[eHmdController_OculusRightHand] = ovrControllerType_RTouch;
}

// -------------------------------------------------------------------------
COculusController::~COculusController()
{
	for (SInputSymbol* pSymbol : m_symbols)
	{
		SAFE_DELETE(pSymbol);
	}
}

// -------------------------------------------------------------------------
bool COculusController::Init(ovrSession pSession)
{
	m_pSession = pSession;

	// Button symbols
	AddSymbolButton(eKI_Motion_OculusTouch_A, eHmdControllerButtons_A, "otouch_a", SSymbolEx::eRightController);
	AddSymbolButton(eKI_Motion_OculusTouch_B, eHmdControllerButtons_B, "otouch_b", SSymbolEx::eRightController);
	AddSymbolButton(eKI_Motion_OculusTouch_X, eHmdControllerButtons_X, "otouch_x", SSymbolEx::eLeftController);
	AddSymbolButton(eKI_Motion_OculusTouch_Y, eHmdControllerButtons_Y, "otouch_y", SSymbolEx::eLeftController);
	AddSymbolButton(eKI_Motion_OculusTouch_L3, eHmdControllerButtons_LThumb, "otouch_l3", SSymbolEx::eLeftController);
	AddSymbolButton(eKI_Motion_OculusTouch_R3, eHmdControllerButtons_RThumb, "otouch_r3", SSymbolEx::eRightController);

	AddSymbolButton(eKI_Motion_OculusTouch_TriggerBtnL, eHmdControllerButtons_LTrigger, "otouch_triggerl_btn", SSymbolEx::eLeftController);
	AddSymbolButton(eKI_Motion_OculusTouch_TriggerBtnR, eHmdControllerButtons_RTrigger, "otouch_triggerr_btn", SSymbolEx::eRightController);

	// Trigger symbols
	AddSymbolTrigger(eKI_Motion_OculusTouch_L1, eHmdControllerTriggers_LT, "otouch_l1", SSymbolEx::eLeftController | SSymbolEx::eIndexTrigger);
	AddSymbolTrigger(eKI_Motion_OculusTouch_R1, eHmdControllerTriggers_RT, "otouch_r1", SSymbolEx::eRightController | SSymbolEx::eIndexTrigger);
	AddSymbolTrigger(eKI_Motion_OculusTouch_L2, eHmdControllerTriggers_LT2, "otouch_l2", SSymbolEx::eLeftController | SSymbolEx::eHandTrigger);
	AddSymbolTrigger(eKI_Motion_OculusTouch_R2, eHmdControllerTriggers_RT2, "otouch_r2", SSymbolEx::eRightController | SSymbolEx::eHandTrigger);

	// Sticks
	AddSymbolStick(eKI_Motion_OculusTouch_StickL_Y, eHmdControllerThumbSticks_Left, "otouch_stickly", SSymbolEx::eLeftController | SSymbolEx::eAxisY);
	AddSymbolStick(eKI_Motion_OculusTouch_StickR_Y, eHmdControllerThumbSticks_Right, "otouch_stickry", SSymbolEx::eRightController | SSymbolEx::eAxisY);
	AddSymbolStick(eKI_Motion_OculusTouch_StickL_X, eHmdControllerThumbSticks_Left, "otouch_sticklx", SSymbolEx::eLeftController | SSymbolEx::eAxisX);
	AddSymbolStick(eKI_Motion_OculusTouch_StickR_X, eHmdControllerThumbSticks_Right, "otouch_stickrx", SSymbolEx::eRightController | SSymbolEx::eAxisX);

	return pSession != nullptr;
}

	#undef AddSymbolBase
	#undef AddSymbolButton
	#undef AddSymbolTrigger
	#undef AddSymbolStick

	#undef OCULUS_BUTTON_TO_ENUM_BIT
	#undef OCULUS_GESTURE_TO_ENUM_BIT
	#undef EKEYID_OCULUS_TRIGGER_TO_ENUM

// -------------------------------------------------------------------------
inline bool COculusController::HasTriggerChanged(EHmdControllerTriggers id, float threshold) const
{
	return
	  ((m_previousState.m_triggerValue[id] > threshold) || (m_state.m_triggerValue[id] > threshold)) &&
	  (m_previousState.m_triggerValue[id] != m_state.m_triggerValue[id]);
}

// -------------------------------------------------------------------------
inline bool COculusController::HasTriggerZeroed(EHmdControllerTriggers id, float threshold) const
{
	return
	  (m_previousState.m_triggerValue[id] > threshold) &&
	  (m_state.m_triggerValue[id] < threshold);
}

// -------------------------------------------------------------------------
inline COculusController::SStickChangeResult COculusController::HasStickChanged(EHmdControllerThumbSticks id) const
{
	SStickChangeResult res;

	const Vec2 prev = m_previousState.m_thumbSitcks[id];
	const Vec2 current = m_state.m_thumbSitcks[id];
	const float threshold = ms_sticksZeroThreshold;
	const Vec2 prev_abs = Vec2(fabs(prev.x), fabs(prev.y));
	const Vec2 current_abs = Vec2(fabs(current.x), fabs(current.y));

	res.values |= (prev.x != current.x) && ((prev_abs.x > threshold) || (current_abs.x > threshold)) ? SStickChangeResult::eXAxisChanged : 0;
	res.values |= (prev.y != current.y) && ((prev_abs.y > threshold) || (current_abs.y > threshold)) ? SStickChangeResult::eYAxisChanged : 0;
	res.values |= (prev.x != current.x) && ((prev_abs.x > threshold) && (current_abs.x < threshold)) ? SStickChangeResult::eXAxisZeroed : 0;
	res.values |= (prev.y != current.y) && ((prev_abs.y > threshold) && (current_abs.y < threshold)) ? SStickChangeResult::eYAxisZeroed : 0;

	return res;
}

// -------------------------------------------------------------------------
inline bool COculusController::HasConenctionChanged() const
{
	return
	  (IsConnected(eHmdController_OculusLeftHand) != WasConnected(eHmdController_OculusLeftHand)) ||
	  (IsConnected(eHmdController_OculusRightHand) != WasConnected(eHmdController_OculusRightHand));
}

// -------------------------------------------------------------------------
inline bool COculusController::HasDisconnected(EHmdController id) const
{
	return (IsConnected(id) == false && WasConnected(id));
}

// -------------------------------------------------------------------------
void COculusController::PostButtonChanges() const
{
	for (const SSymbolEx button : m_buttonSybmolsEx)
	{
		if (button.symbolIdx >= 0)
		{
			SInputSymbol* pSymbol = m_symbols[button.symbolIdx];
			const EHmdControllerButtons buttonId = static_cast<EHmdControllerButtons>(pSymbol->devSpecId);
			const bool bIsPressed = IsButtonPressed_Internal(buttonId);
			if (WasButtonPressed(buttonId) != bIsPressed)
			{
				SendEventPress(pSymbol, bIsPressed);
			}
		}
	}
}

// -------------------------------------------------------------------------
void COculusController::PostTriggerChanges() const
{
	for (const SSymbolEx trigger : m_triggerSybmolsEx)
	{
		if (trigger.symbolIdx >= 0)
		{
			SInputSymbol* pSymbol = m_symbols[trigger.symbolIdx];
			const EHmdControllerTriggers triggerId = static_cast<EHmdControllerTriggers>(pSymbol->devSpecId);
			const float threshold = trigger.IsSet(SSymbolEx::eIndexTrigger) ? ms_triggerIndexZeroThreshold : ms_triggerHandZeroThreshold;
			if (HasTriggerChanged(triggerId, threshold))
			{
				SendEventChange(pSymbol, GetTriggerValue_Internal(triggerId));
			}
			else if (HasTriggerZeroed(triggerId, threshold))
			{
				SendEventChange(pSymbol, 0.f);
			}
		}
	}
}

// -------------------------------------------------------------------------
void COculusController::PostSticksChanges() const
{
	for (const SSymbolEx stick : m_stickSybmolsEx)
	{
		if (stick.symbolIdx >= 0)
		{
			SInputSymbol* pSymbol = m_symbols[stick.symbolIdx];
			const EHmdControllerThumbSticks stickId = static_cast<EHmdControllerThumbSticks>(pSymbol->devSpecId);
			const SStickChangeResult changeResults = HasStickChanged(stickId);

			if (changeResults.HasChanged())
			{
				if ((stick.IsSet(SSymbolEx::eAxisX) && changeResults.IsSet(SStickChangeResult::eXAxisZeroed)) ||
				    (stick.IsSet(SSymbolEx::eAxisY) && changeResults.IsSet(SStickChangeResult::eYAxisZeroed)))
				{
					SendEventChange(pSymbol, 0.f);
				}
				else
				{
					const Vec2 stickValue = GetThumbStickValue_Internal(stickId);
					if (stick.IsSet(SSymbolEx::eAxisX) && changeResults.IsSet(SStickChangeResult::eXAxisChanged))
					{
						SendEventChange(pSymbol, stickValue.x);
					}
					if (stick.IsSet(SSymbolEx::eAxisY) && changeResults.IsSet(SStickChangeResult::eYAxisChanged))
					{
						SendEventChange(pSymbol, stickValue.y);
					}
				}
			}
		}
	}
}

// -------------------------------------------------------------------------
inline void COculusController::SendEventChange(SInputSymbol* pSymbol, float value) const
{
	SInputEvent event;
	event.deviceType = eIDT_MotionController;
	pSymbol->ChangeEvent(value);
	pSymbol->AssignTo(event);
	gEnv->pInput->PostInputEvent(event);
}

// -------------------------------------------------------------------------
inline void COculusController::SendEventPress(SInputSymbol* pSymbol, bool bPressed) const
{
	SInputEvent event;
	event.deviceType = eIDT_MotionController;
	pSymbol->PressEvent(bPressed);
	pSymbol->AssignTo(event);
	gEnv->pInput->PostInputEvent(event);
}

// -------------------------------------------------------------------------
void COculusController::ClearKeyStateHelper(const TSymolExVector& symbolsExVector, EClearValue flag)
{
	for (SSymbolEx symbolEx : symbolsExVector)
	{
		if (symbolEx.symbolIdx >= 0)
		{
			SInputSymbol* pSymbol = m_symbols[symbolEx.symbolIdx];
			if (pSymbol && pSymbol->value > 0.f)
			{
				const bool bClearAllControllerSymbols = (flag & eCV_FullController) != 0;
				const bool bClearLeftControllerSymbol = ((flag & eCV_LeftController) && (symbolEx.IsSet(SSymbolEx::eLeftController))) != 0;
				const bool bClearRightControllerSymbol = ((flag & eCV_RightController) && (!symbolEx.IsSet(SSymbolEx::eLeftController))) != 0;

				if (bClearAllControllerSymbols || bClearLeftControllerSymbol || bClearRightControllerSymbol)
				{
					switch (pSymbol->type)
					{
					case SInputSymbol::Button:
						{
							pSymbol->state = eIS_Released;
							SendEventPress(pSymbol, false);
						}
						break;
					case SInputSymbol::Axis:   // intentional fall through
					case SInputSymbol::Trigger:
						{
							pSymbol->state = eIS_Changed;
							SendEventChange(pSymbol, 0.f);
						}
						break;
					default:
						assert(0);
						break;
					}
				}
			}
		}
	}
}

// -------------------------------------------------------------------------
void COculusController::ClearKeyState(EClearValue flag)
{
	if (gEnv->pInput)
	{
		ClearKeyStateHelper(m_buttonSybmolsEx, flag);
		ClearKeyStateHelper(m_triggerSybmolsEx, flag);
		ClearKeyStateHelper(m_stickSybmolsEx, flag);
	}
}

// -------------------------------------------------------------------------
SInputSymbol* COculusController::MapSymbol(uint32 deviceSpecificId, EKeyId keyId, const TKeyName& name, SInputSymbol::EType type, uint32 user)
{
	SInputSymbol* pSymbol = new SInputSymbol(deviceSpecificId, keyId, name, type);
	pSymbol->deviceType = eIDT_MotionController;
	pSymbol->user = user;

	return pSymbol;
}

// --------------------------------------------------------------------------------------
void COculusController::ClearControllerState(EClearValue flag)
{
	if (flag & eCV_LeftController)
	{
		const uint32 btMask = eHmdControllerButtons_X | eHmdControllerButtons_Y | eHmdControllerButtons_LThumb | eHmdControllerButtons_LTrigger;
		m_state.m_buttonsState &= ~btMask;
		m_state.m_touchState &= ~btMask;

		const uint32 gestureMask = eHmdControllerGesture_LThumbUp | eHmdControllerGesture_LIndexPointing;
		m_state.m_gesturesState &= ~gestureMask;

		m_state.m_triggerValue[eHmdControllerTriggers_LT] = 0.f;
		m_state.m_triggerValue[eHmdControllerTriggers_LT2] = 0.f;

		m_state.m_thumbSitcks[eHmdControllerThumbSticks_Left].x = 0.f;
		m_state.m_thumbSitcks[eHmdControllerThumbSticks_Left].y = 0.f;
	}

	if (flag & eCV_RightController)
	{
		const uint32 btMask = eHmdControllerButtons_A | eHmdControllerButtons_B | eHmdControllerButtons_RThumb | eHmdControllerButtons_RTrigger;
		m_state.m_buttonsState &= ~btMask;
		m_state.m_touchState &= ~btMask;

		const uint32 gestureMask = eHmdControllerGesture_RThumbUp | eHmdControllerGesture_RIndexPointing;
		m_state.m_gesturesState &= ~gestureMask;

		m_state.m_triggerValue[eHmdControllerTriggers_RT] = 0.f;
		m_state.m_triggerValue[eHmdControllerTriggers_RT2] = 0.f;

		m_state.m_thumbSitcks[eHmdControllerThumbSticks_Right].x = 0.f;
		m_state.m_thumbSitcks[eHmdControllerThumbSticks_Right].y = 0.f;
	}

	ClearKeyState(flag);
}

// -------------------------------------------------------------------------
void COculusController::Update()
{
	bool bHasInputState = false;
	bool bHadInputState = m_state.m_hasInputState;
	if (m_pSession)
	{
		const unsigned int connectedControllerTypes = ovr_GetConnectedControllerTypes(m_pSession);
		ovrInputState inputState;
		if (bHasInputState = (ovr_GetInputState(m_pSession, ovrControllerType_Touch, &inputState) == ovrSuccess))
		{
			m_previousState = m_state;

			// connection
			m_state.m_connected[eHmdController_OculusLeftHand] = (connectedControllerTypes & ovrControllerType_LTouch) != 0;
			m_state.m_connected[eHmdController_OculusRightHand] = (connectedControllerTypes & ovrControllerType_RTouch) != 0;

			// touch state
			uint32 state;
			state = inputState.Touches & ovrTouch_A ? eHmdControllerButtons_A : 0;
			state |= inputState.Touches & ovrTouch_B ? eHmdControllerButtons_B : 0;
			state |= inputState.Touches & ovrTouch_X ? eHmdControllerButtons_X : 0;
			state |= inputState.Touches & ovrTouch_Y ? eHmdControllerButtons_Y : 0;
			state |= inputState.Touches & ovrTouch_LThumb ? eHmdControllerButtons_LThumb : 0;
			state |= inputState.Touches & ovrTouch_RThumb ? eHmdControllerButtons_RThumb : 0;
			state |= inputState.Touches & ovrTouch_LIndexTrigger ? eHmdControllerButtons_LTrigger : 0;
			state |= inputState.Touches & ovrTouch_RIndexTrigger ? eHmdControllerButtons_RTrigger : 0;

			m_state.m_touchState = state;

			// buttons state
			state = inputState.Buttons & ovrButton_A ? eHmdControllerButtons_A : 0;
			state |= inputState.Buttons & ovrButton_B ? eHmdControllerButtons_B : 0;
			state |= inputState.Buttons & ovrButton_X ? eHmdControllerButtons_X : 0;
			state |= inputState.Buttons & ovrButton_Y ? eHmdControllerButtons_Y : 0;
			state |= inputState.Buttons & ovrButton_LThumb ? eHmdControllerButtons_LThumb : 0;
			state |= inputState.Buttons & ovrButton_RThumb ? eHmdControllerButtons_RThumb : 0;
			state |= inputState.Buttons & ovrButton_LShoulder ? eHmdControllerButtons_LTrigger : 0;
			state |= inputState.Buttons & ovrButton_RShoulder ? eHmdControllerButtons_RTrigger : 0;

			m_state.m_buttonsState = state;

			// triggers
			m_state.m_triggerValue[eHmdControllerTriggers_LT] = inputState.IndexTrigger[ovrHand_Left];
			m_state.m_triggerValue[eHmdControllerTriggers_RT] = inputState.IndexTrigger[ovrHand_Right];
			m_state.m_triggerValue[eHmdControllerTriggers_LT2] = inputState.HandTrigger[ovrHand_Left];
			m_state.m_triggerValue[eHmdControllerTriggers_RT2] = inputState.HandTrigger[ovrHand_Right];

			// gestures
			state = inputState.Touches & ovrTouch_LThumbUp ? eHmdControllerGesture_LThumbUp : 0;
			state |= inputState.Touches & ovrTouch_RThumbUp ? eHmdControllerGesture_RThumbUp : 0;
			state |= inputState.Touches & ovrTouch_LIndexPointing ? eHmdControllerGesture_LIndexPointing : 0;
			state |= inputState.Touches & ovrTouch_RIndexPointing ? eHmdControllerGesture_RIndexPointing : 0;

			m_state.m_gesturesState = state;

			// thumbsticks x,y analog positions
			m_state.m_thumbSitcks[eHmdControllerThumbSticks_Left].x = inputState.Thumbstick[ovrHand_Left].x;
			m_state.m_thumbSitcks[eHmdControllerThumbSticks_Left].y = inputState.Thumbstick[ovrHand_Left].y;
			m_state.m_thumbSitcks[eHmdControllerThumbSticks_Right].x = inputState.Thumbstick[ovrHand_Right].x;
			m_state.m_thumbSitcks[eHmdControllerThumbSticks_Right].y = inputState.Thumbstick[ovrHand_Right].y;
		}

		m_state.m_hasInputState = bHasInputState;
	}

	if (bHasInputState == false && bHadInputState)
	{
		// Oculus failed to provide input controllers information
		ClearControllerState(eCV_FullController);
	}
	else
	{
		if (HasDisconnected(eHmdController_OculusLeftHand))
			ClearControllerState(eCV_LeftController);

		if (HasDisconnected(eHmdController_OculusRightHand))
			ClearControllerState(eCV_RightController);
	}

	if (m_pSession && gEnv->pInput)
	{
		PostButtonChanges();
		PostTriggerChanges();
		PostSticksChanges();
	}
}

// -------------------------------------------------------------------------
void COculusController::DebugDraw(float& xPosLabel, float& yPosLabel) const
{
	// Basic info
	const float yPos = yPosLabel, xPosData = xPosLabel, yDelta = 20.f;
	float y = yPos;
	const ColorF fColorLabel(1.0f, 1.0f, 1.0f, 1.0f);
	const ColorF fColorDataConn(1.0f, 1.0f, 0.0f, 1.0f);
	const ColorF fColorDataBt(0.0f, 1.0f, 0.0f, 1.0f);
	const ColorF fColorDataTr(0.0f, 0.0f, 1.0f, 1.0f);
	const ColorF fColorDataTh(1.0f, 0.0f, 1.0f, 1.0f);
	const ColorF fColorDataPose(0.0f, 1.0f, 1.0f, 1.0f);

	IRenderAuxText::Draw2dLabel(xPosLabel, y, 1.3f, fColorLabel, false, "Oculus VR Controller Info: %s", m_pSession ? "Initialized" : "NOT INITIALIZED");
	y += yDelta;

	IRenderAuxText::Draw2dLabel(xPosData, y, 1.3f, fColorDataConn, false, "Left Hand Controller:%s", IsConnected(eHmdController_OculusLeftHand) ? "Connected" : "Disconnected");
	y += yDelta;
	IRenderAuxText::Draw2dLabel(xPosData, y, 1.3f, fColorDataConn, false, "Right Hand Controller:%s", IsConnected(eHmdController_OculusRightHand) ? "Connected" : "Disconnected");
	y += yDelta;

	IRenderAuxText::Draw2dLabel(xPosData, y, 1.3f, fColorDataBt, false, "Bt A:%s", IsButtonPressed_Internal(eHmdControllerButtons_A) ? "Pressed" : "Released");
	y += yDelta;
	IRenderAuxText::Draw2dLabel(xPosData, y, 1.3f, fColorDataBt, false, "Bt B:%s", IsButtonPressed_Internal(eHmdControllerButtons_B) ? "Pressed" : "Released");
	y += yDelta;
	IRenderAuxText::Draw2dLabel(xPosData, y, 1.3f, fColorDataBt, false, "Bt X:%s", IsButtonPressed_Internal(eHmdControllerButtons_X) ? "Pressed" : "Released");
	y += yDelta;
	IRenderAuxText::Draw2dLabel(xPosData, y, 1.3f, fColorDataBt, false, "Bt Y:%s", IsButtonPressed_Internal(eHmdControllerButtons_Y) ? "Pressed" : "Released");
	y += yDelta;
	IRenderAuxText::Draw2dLabel(xPosData, y, 1.3f, fColorDataBt, false, "Bt LThumb:%s", IsButtonPressed_Internal(eHmdControllerButtons_LThumb) ? "Pressed" : "Released");
	y += yDelta;
	IRenderAuxText::Draw2dLabel(xPosData, y, 1.3f, fColorDataBt, false, "Bt LShoulder:%s", IsButtonPressed_Internal(eHmdControllerButtons_LTrigger) ? "Pressed" : "Released");
	y += yDelta;
	IRenderAuxText::Draw2dLabel(xPosData, y, 1.3f, fColorDataBt, false, "Bt RThumb:%s", IsButtonPressed_Internal(eHmdControllerButtons_RThumb) ? "Pressed" : "Released");
	y += yDelta;
	IRenderAuxText::Draw2dLabel(xPosData, y, 1.3f, fColorDataBt, false, "Bt RShoulder:%s", IsButtonPressed_Internal(eHmdControllerButtons_RTrigger) ? "Pressed" : "Released");
	y += yDelta;

	IRenderAuxText::Draw2dLabel(xPosData, y, 1.3f, fColorDataTr, false, "Index Trigger L:%.2f R:%.2f", GetTriggerValue_Internal(eHmdControllerTriggers_LT), GetTriggerValue_Internal(eHmdControllerTriggers_RT));
	y += yDelta;
	IRenderAuxText::Draw2dLabel(xPosData, y, 1.3f, fColorDataTr, false, "Hand  Trigger L:%.2f R:%.2f", GetTriggerValue_Internal(eHmdControllerTriggers_LT2), GetTriggerValue_Internal(eHmdControllerTriggers_RT2));
	y += yDelta;

	Vec2 tsLeft = GetThumbStickValue_Internal(eHmdControllerThumbSticks_Left);
	Vec2 tsRight = GetThumbStickValue_Internal(eHmdControllerThumbSticks_Right);
	IRenderAuxText::Draw2dLabel(xPosData, y, 1.3f, fColorDataTh, false, "Thumbstick L:(%.2f ,  %.2f) R:(%.2f ,  %.2f)", tsLeft.x, tsLeft.y, tsRight.x, tsRight.y);
	y += yDelta;

	Vec3 posLH = GetNativeTrackingState(eHmdController_OculusLeftHand).pose.position;
	Vec3 posRH = GetNativeTrackingState(eHmdController_OculusRightHand).pose.position;
	Vec3 posWLH = GetLocalTrackingState(eHmdController_OculusLeftHand).pose.position;
	Vec3 posWRH = GetLocalTrackingState(eHmdController_OculusRightHand).pose.position;
	IRenderAuxText::Draw2dLabel(xPosData, y, 1.3f, fColorDataPose, false, "Pose   L:(%.2f,%.2f,%.2f) R:(%.2f,%.2f,%.2f)", posLH.x, posLH.y, posLH.z, posRH.x, posRH.y, posRH.z);
	y += yDelta;
	IRenderAuxText::Draw2dLabel(xPosData, y, 1.3f, fColorDataPose, false, "W-Pose L:(%.2f,%.2f,%.2f) R:(%.2f,%.2f,%.2f)", posWLH.x, posWLH.y, posWLH.z, posWRH.x, posWRH.y, posWRH.z);
	y += yDelta;

	Ang3 rotLHAng(GetNativeTrackingState(eHmdController_OculusLeftHand).pose.orientation);
	Vec3 rotLH(RAD2DEG(rotLHAng));
	Ang3 rotRHAng(GetNativeTrackingState(eHmdController_OculusRightHand).pose.orientation);
	Vec3 rotRH(RAD2DEG(rotRHAng));

	Ang3 rotWLHAng(GetLocalTrackingState(eHmdController_OculusLeftHand).pose.orientation);
	Vec3 rotWLH(RAD2DEG(rotWLHAng));
	Ang3 rotWRHAng(GetLocalTrackingState(eHmdController_OculusRightHand).pose.orientation);
	Vec3 rotWRH(RAD2DEG(rotWRHAng));

	IRenderAuxText::Draw2dLabel(xPosData, y, 1.3f, fColorDataPose, false, "Rot[PRY]   L:(%.2f,%.2f,%.2f) R:(%.2f,%.2f,%.2f)", rotLH.x, rotLH.y, rotLH.z, rotRH.x, rotRH.y, rotRH.z);
	y += yDelta;
	IRenderAuxText::Draw2dLabel(xPosData, y, 1.3f, fColorDataPose, false, "W-Rot[PRY] L:(%.2f,%.2f,%.2f) R:(%.2f,%.2f,%.2f)", rotWLH.x, rotWLH.y, rotWLH.z, rotWRH.x, rotWRH.y, rotWRH.z);
	y += yDelta;

	yPosLabel = y;
}

} // namespace Oculus
} // namespace CryVR