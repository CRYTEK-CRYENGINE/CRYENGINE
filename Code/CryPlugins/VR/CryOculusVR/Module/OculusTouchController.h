// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

	#include <CrySystem/VR/IHMDDevice.h>

namespace CryVR {
namespace Oculus {

	#define OCULUS_BUTTON_TO_ENUM_BIT(BT_ID)          BIT(BT_ID - KI_MOTION_BASE)
	#define OCULUS_GESTURE_TO_ENUM_BIT(GESTURE_ID)    BIT(GESTURE_ID - eKI_Motion_OculusTouch_FirstGestureIndex)
	#define EKEYID_OCULUS_TRIGGER_TO_ENUM(TRIGGER_ID) (TRIGGER_ID - eKI_Motion_OculusTouch_FirstTriggerIndex)

class COculusController : public IHmdController
{
public:
	// IHmdController overrides
	virtual bool                        IsConnected(EHmdController id)                  const override;
	virtual bool                        IsButtonPressed(EHmdController id, EKeyId button)   const override;
	virtual bool                        IsButtonTouched(EHmdController id, EKeyId button)   const override;
	virtual bool                        IsGestureTriggered(EHmdController id, EKeyId gesture)  const override;
	virtual float                       GetTriggerValue(EHmdController id, EKeyId trigger)  const override;
	virtual Vec2                        GetThumbStickValue(EHmdController id, EKeyId stick)    const override;

	virtual const HmdTrackingState&     GetNativeTrackingState(EHmdController controller) const override { return m_state.m_handNativeTrackingState[controller]; }
	virtual const HmdTrackingState&     GetLocalTrackingState(EHmdController controller)  const override { return m_state.m_handLocalTrackingState[controller]; }

	virtual void                        ApplyForceFeedback(EHmdController id, float freq, float amplitude) override;

	virtual void                        SetLightColor(EHmdController id, IHmdController::TLightColor color) override {}
	virtual IHmdController::TLightColor GetControllerColor(EHmdController id) const override                         { return 0; }

	// returns the capabilities of the selected controller
	virtual uint32 GetCaps(EHmdController id) const { return (IHmdController::eCaps_Buttons | IHmdController::eCaps_Tracking | IHmdController::eCaps_Sticks | IHmdController::eCaps_Capacitors | IHmdController::eCaps_Gestures | IHmdController::eCaps_ForceFeedback); }
	// ~IHmdController

private:
	friend class Device;

	COculusController();
	virtual ~COculusController() override;

	// -------------------------------------------------------------------------------------------
	// Enums for buttons, triggers, etc.
	// -------------------------------------------------------------------------------------------
	enum EHmdControllerButtons
	{
		eHmdControllerButtons_A        = OCULUS_BUTTON_TO_ENUM_BIT(eKI_Motion_OculusTouch_A),
		eHmdControllerButtons_B        = OCULUS_BUTTON_TO_ENUM_BIT(eKI_Motion_OculusTouch_B),
		eHmdControllerButtons_X        = OCULUS_BUTTON_TO_ENUM_BIT(eKI_Motion_OculusTouch_X),
		eHmdControllerButtons_Y        = OCULUS_BUTTON_TO_ENUM_BIT(eKI_Motion_OculusTouch_Y),
		eHmdControllerButtons_LThumb   = OCULUS_BUTTON_TO_ENUM_BIT(eKI_Motion_OculusTouch_L3),
		eHmdControllerButtons_RThumb   = OCULUS_BUTTON_TO_ENUM_BIT(eKI_Motion_OculusTouch_R3),
		eHmdControllerButtons_LTrigger = OCULUS_BUTTON_TO_ENUM_BIT(eKI_Motion_OculusTouch_TriggerBtnL),
		eHmdControllerButtons_RTrigger = OCULUS_BUTTON_TO_ENUM_BIT(eKI_Motion_OculusTouch_TriggerBtnR)
	};

	enum EHmdControllerTriggers
	{
		eHmdControllerTriggers_LT  = EKEYID_OCULUS_TRIGGER_TO_ENUM(eKI_Motion_OculusTouch_L1),
		eHmdControllerTriggers_RT  = EKEYID_OCULUS_TRIGGER_TO_ENUM(eKI_Motion_OculusTouch_R1),
		eHmdControllerTriggers_LT2 = EKEYID_OCULUS_TRIGGER_TO_ENUM(eKI_Motion_OculusTouch_L2),
		eHmdControllerTriggers_RT2 = EKEYID_OCULUS_TRIGGER_TO_ENUM(eKI_Motion_OculusTouch_R2),

		eHmdControllerTriggers_MaxNumTriggers,
	};

	enum EHmdControllerGestures
	{
		eHmdControllerGesture_LThumbUp       = OCULUS_GESTURE_TO_ENUM_BIT(eKI_Motion_OculusTouch_Gesture_ThumbUpL),
		eHmdControllerGesture_RThumbUp       = OCULUS_GESTURE_TO_ENUM_BIT(eKI_Motion_OculusTouch_Gesture_ThumbUpR),
		eHmdControllerGesture_LIndexPointing = OCULUS_GESTURE_TO_ENUM_BIT(eKI_Motion_OculusTouch_Gesture_IndexPointingL),
		eHmdControllerGesture_RIndexPointing = OCULUS_GESTURE_TO_ENUM_BIT(eKI_Motion_OculusTouch_Gesture_IndexPointingR),
	};

	enum EHmdControllerThumbSticks
	{
		eHmdControllerThumbSticks_Left = 0,
		eHmdControllerThumbSticks_Right,

		eHmdControllerThumbSticks_MaxNumThumbSticks
	};

	// -------------------------------------------------------------------------------------------

	enum EClearValue
	{
		eCV_LeftController  = BIT(0),
		eCV_RightController = BIT(1),
		eCV_FullController  = eCV_LeftController | eCV_RightController
	};

	struct SStickChangeResult
	{
		enum EFlags
		{
			eXAxisChanged = BIT(0), eYAxisChanged = BIT(1),
			eXAxisZeroed  = BIT(2), eYAxisZeroed = BIT(3),
		};
		SStickChangeResult() : values(0) {}
		bool HasChanged() const    { return values != 0; }
		bool IsSet(uint32 f) const { return (values & f) != 0; }

		uint32 values;
	};

	// controller state
	struct SState
	{
		uint32           m_buttonsState;
		uint32           m_gesturesState;
		uint32           m_touchState;
		bool             m_connected[eHmdController_MaxNumOculusControllers];
		bool             m_hasInputState;
		float            m_triggerValue[eHmdControllerTriggers_MaxNumTriggers];
		Vec2             m_thumbSitcks[eHmdControllerThumbSticks_MaxNumThumbSticks];
		HmdTrackingState m_handNativeTrackingState[eHmdController_MaxNumOculusControllers];
		HmdTrackingState m_handLocalTrackingState[eHmdController_MaxNumOculusControllers];
	};

	// symbol extended information
	struct SSymbolEx
	{
		enum EFlags
		{
			eAxisX           = BIT(0),
			eAxisY           = BIT(1),
			eIndexTrigger    = BIT(2),
			eHandTrigger     = BIT(3),
			eLeftController  = BIT(4),
			eRightController = BIT(5)
		};

		SSymbolEx() : symbolIdx(-1), flags(0) {}
		SSymbolEx(int idx, uint32 f) : symbolIdx(idx), flags(f) {}
		bool IsSet(EFlags f) const { return (flags & f) != 0; }

		int    symbolIdx;
		uint32 flags;
	};

	typedef std::vector<SSymbolEx> TSymolExVector;

	inline bool               WasConnected(EHmdController id) const { return (m_previousState.m_connected[id] != 0); }
	inline bool               HasConenctionChanged() const;
	inline bool               HasDisconnected(EHmdController id) const;
	inline bool               WasButtonPressed(EHmdControllerButtons id) const { return (m_previousState.m_buttonsState & id) != 0; }
	inline bool               HasTriggerChanged(EHmdControllerTriggers id, float threshold) const;
	inline bool               HasTriggerZeroed(EHmdControllerTriggers id, float threshold) const;
	inline SStickChangeResult HasStickChanged(EHmdControllerThumbSticks id) const;

	inline void               SendEventChange(SInputSymbol* pSymbol, float value) const;
	inline void               SendEventPress(SInputSymbol* pSymbol, bool bPressed) const;

	bool                      Init(ovrSession pSession);
	void                      Update();
	void                      PostButtonChanges() const;
	void                      PostTriggerChanges() const;
	void                      PostSticksChanges() const;
	void                      ClearControllerState(EClearValue flag);
	void                      ClearKeyState(EClearValue flag);
	void                      ClearKeyStateHelper(const TSymolExVector& symbolsExVector, EClearValue flag);
	SInputSymbol*             MapSymbol(uint32 deviceSpecificId, EKeyId keyId, const TKeyName& name, SInputSymbol::EType type, uint32 user);

	inline bool               IsButtonPressed_Internal(EHmdControllerButtons button)   const     { return (m_state.m_buttonsState & button) != 0; }
	inline bool               IsButtonTouched_Internal(EHmdControllerButtons button)   const     { return (m_state.m_touchState & button) != 0; }
	inline bool               IsGestureTriggered_Internal(EHmdControllerGestures gesture)  const { return (m_state.m_gesturesState & gesture) != 0; }
	inline float              GetTriggerValue_Internal(EHmdControllerTriggers trigger)  const    { return m_state.m_triggerValue[trigger]; }
	inline Vec2               GetThumbStickValue_Internal(EHmdControllerThumbSticks stick) const { return m_state.m_thumbSitcks[stick]; }

	void                      DebugDraw(float& xPosLabel, float& yPosLabel) const;

private:

	// static threshold values
	static float ms_triggerIndexZeroThreshold;
	static float ms_triggerHandZeroThreshold;
	static float ms_sticksZeroThreshold;

	// member variables
	ovrSession        m_pSession; // HMD descriptor for this device
	SInputSymbol*     m_symbols[eKI_Motion_OculusTouch_NUM_SYMBOLS];
	ovrControllerType m_controllerEnumMap[eHmdController_MaxNumOculusControllers];  // maps CryEngine controller enum to Oculus controller enum

	SState            m_state, m_previousState;

	TSymolExVector    m_stickSybmolsEx;
	TSymolExVector    m_triggerSybmolsEx;
	TSymolExVector    m_buttonSybmolsEx;
};

} // namespace Oculus
} // namespace CryVR