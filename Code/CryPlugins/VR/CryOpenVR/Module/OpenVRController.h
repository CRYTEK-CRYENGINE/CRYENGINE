// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySystem/VR/IHMDDevice.h>
#include <CryInput/IInput.h>

#define OPENVR_SPECIAL (1 << 8)
#define OPENVR_BASE    eKI_Motion_OpenVR_System

namespace CryVR
{
namespace OpenVR
{
class Controller : public IHmdController
{
public:
	// IHmdController
	virtual bool                    IsConnected(EHmdController id) const override;
	virtual bool                    IsButtonPressed(EHmdController controllerId, EKeyId id) const override { return (id < OPENVR_BASE || id > eKI_Motion_OpenVR_TouchPadBtn) ? false : (m_state[controllerId].buttonsPressed & vr::ButtonMaskFromId((vr::EVRButtonId)(m_symbols[id - OPENVR_BASE]->devSpecId & (~OPENVR_SPECIAL)))) > 0; }
	virtual bool                    IsButtonTouched(EHmdController controllerId, EKeyId id) const override { return (id < OPENVR_BASE || id > eKI_Motion_OpenVR_TouchPadBtn) ? false : (m_state[controllerId].buttonsTouched & vr::ButtonMaskFromId((vr::EVRButtonId)(m_symbols[id - OPENVR_BASE]->devSpecId & (~OPENVR_SPECIAL)))) > 0; }
	virtual bool                    IsGestureTriggered(EHmdController controllerId, EKeyId id) const override { return false; }                           // OpenVR does not have gesture support (yet?)
	virtual float                   GetTriggerValue(EHmdController controllerId, EKeyId id) const override { return m_state[controllerId].trigger; }   // we only have one trigger => ignore trigger id
	virtual Vec2                    GetThumbStickValue(EHmdController controllerId, EKeyId id)const override { return m_state[controllerId].touchPad; }  // we only have one 'stick' (/the touch pad) => ignore thumb stick id

	virtual const HmdTrackingState& GetNativeTrackingState(EHmdController controller) const override { return m_state[controller].nativePose; }
	virtual const HmdTrackingState& GetLocalTrackingState(EHmdController controller) const override { return m_state[controller].localPose; }

	virtual void                    ApplyForceFeedback(EHmdController id, float freq, float amplitude) override;
	virtual void                    SetLightColor(EHmdController id, TLightColor color) override {}
	virtual TLightColor             GetControllerColor(EHmdController id) const override { return 0; }
	virtual uint32                  GetCaps(EHmdController id) const override { return (eCaps_Buttons | eCaps_Tracking | eCaps_Sticks | eCaps_Capacitors); }
	// ~IHmdController

private:
	friend class Device;

	Controller(vr::IVRSystem* pSystem);
	virtual ~Controller() override;

	struct SControllerState
	{
		SControllerState()
			: packetNum(0)
			, buttonsPressed(0)
			, buttonsTouched(0)
			, trigger(0.0f)
			, touchPad(ZERO)
		{
		}

		uint32           packetNum;
		HmdTrackingState nativePose;
		HmdTrackingState localPose;
		uint64           buttonsPressed;
		uint64           buttonsTouched;
		float            trigger;
		Vec2             touchPad;

		inline bool      Pressed(vr::EVRButtonId btn)
		{
			return (buttonsPressed & vr::ButtonMaskFromId(btn)) > 0;
		}

		inline bool Touched(vr::EVRButtonId btn)
		{
			return (buttonsTouched & vr::ButtonMaskFromId(btn)) > 0;
		}
	};

	bool          Init();
	void          Update(vr::TrackedDeviceIndex_t controllerId, HmdTrackingState nativeState, HmdTrackingState localState, vr::VRControllerState_t& vrState);
	void          DebugDraw(float& xPosLabel, float& yPosLabel) const;
	SInputSymbol* MapSymbol(uint32 deviceSpecificId, EKeyId keyId, const TKeyName& name, SInputSymbol::EType type, uint32 user);
	void          OnControllerConnect(vr::TrackedDeviceIndex_t controllerId);
	void          OnControllerDisconnect(vr::TrackedDeviceIndex_t controllerId);
	void          PostButtonIfChanged(EHmdController controllerId, EKeyId keyID);
	void          ClearState();

	SInputSymbol*            m_symbols[eKI_Motion_OpenVR_NUM_SYMBOLS];
	SControllerState         m_state[eHmdController_OpenVR_MaxNumOpenVRControllers],
	                         m_previousState[eHmdController_OpenVR_MaxNumOpenVRControllers];
	vr::TrackedDeviceIndex_t m_controllerMapping[eHmdController_OpenVR_MaxNumOpenVRControllers];

	vr::IVRSystem*           m_pSystem;
};
}
}
