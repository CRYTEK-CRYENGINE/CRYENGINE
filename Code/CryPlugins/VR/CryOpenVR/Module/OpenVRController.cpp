// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

#include "OpenVRController.h"

#include <CryRenderer/IRenderer.h>
#include <CryRenderer/IRenderAuxGeom.h>

namespace CryVR {
namespace OpenVR {

#define VRC_AXIS_THRESHOLD 0.5f

#define MAPSYMBOL(EKI, DEV_KEY_ID, KEY_NAME, KEY_TYPE) m_symbols[EKI - OPENVR_BASE] = MapSymbol(DEV_KEY_ID, EKI, KEY_NAME, KEY_TYPE, 0);

// -------------------------------------------------------------------------
Controller::Controller(vr::IVRSystem* pSystem)
	: m_pSystem(pSystem)
{
	memset(&m_previousState, 0, sizeof(m_previousState));
	memset(&m_state, 0, sizeof(m_state));

	for (int iController = 0; iController < eHmdController_OpenVR_MaxNumOpenVRControllers; iController++)
		m_controllerMapping[iController] = vr::k_unMaxTrackedDeviceCount;
}

// -------------------------------------------------------------------------
bool Controller::Init()
{
	//				CRYENGINE SYMBOL ID					DEVICE KEY ID									SYMBOL NAME				SYMBOL TYPE
	MAPSYMBOL(eKI_Motion_OpenVR_System, vr::k_EButton_System, "openvr_system", SInputSymbol::Button);
	MAPSYMBOL(eKI_Motion_OpenVR_ApplicationMenu, vr::k_EButton_ApplicationMenu, "openvr_appmenu", SInputSymbol::Button);
	MAPSYMBOL(eKI_Motion_OpenVR_Grip, vr::k_EButton_Grip, "openvr_grip", SInputSymbol::Button);
	MAPSYMBOL(eKI_Motion_OpenVR_TouchPad_X, vr::k_EButton_SteamVR_Touchpad, "openvr_touch_x", SInputSymbol::Axis);
	MAPSYMBOL(eKI_Motion_OpenVR_TouchPad_Y, vr::k_EButton_SteamVR_Touchpad | OPENVR_SPECIAL, "openvr_touch_y", SInputSymbol::Axis);
	MAPSYMBOL(eKI_Motion_OpenVR_Trigger, vr::k_EButton_SteamVR_Trigger, "openvr_trigger", SInputSymbol::Trigger);
	MAPSYMBOL(eKI_Motion_OpenVR_TriggerBtn, vr::k_EButton_SteamVR_Trigger | OPENVR_SPECIAL, "openvr_trigger_btn", SInputSymbol::Button);
	MAPSYMBOL(eKI_Motion_OpenVR_TouchPadBtn, vr::k_EButton_SteamVR_Touchpad, "openvr_touch_btn", SInputSymbol::Button);
	return true;
}

#undef MapSymbol

// -------------------------------------------------------------------------
Controller::~Controller()
{
	for (int iSymbol = 0; iSymbol < eKI_Motion_OpenVR_NUM_SYMBOLS; iSymbol++)
		SAFE_DELETE(m_symbols[iSymbol]);
}

// -------------------------------------------------------------------------
void Controller::ClearState()
{
	for (int i = 0; i < eHmdController_OpenVR_MaxNumOpenVRControllers; i++)
		m_state[i] = SControllerState();
}

// -------------------------------------------------------------------------
SInputSymbol* Controller::MapSymbol(uint32 deviceSpecificId, EKeyId keyId, const TKeyName& name, SInputSymbol::EType type, uint32 user)
{
	SInputSymbol* pSymbol = new SInputSymbol(deviceSpecificId, keyId, name, type);
	pSymbol->deviceType = eIDT_MotionController;
	pSymbol->user = user;
	if (type == SInputSymbol::EType::Axis || type == SInputSymbol::EType::RawAxis)
		pSymbol->state = eIS_Unknown;
	else
		pSymbol->state = eIS_Pressed;
	pSymbol->value = 0.0f;
	return pSymbol;
}

// -------------------------------------------------------------------------
void Controller::Update(vr::TrackedDeviceIndex_t controllerId, HmdTrackingState nativeState, HmdTrackingState localState, vr::VRControllerState_t& vrState)
{
	EHmdController index = eHmdController_OpenVR_MaxNumOpenVRControllers;
	for (int i = 0; i < eHmdController_OpenVR_MaxNumOpenVRControllers; i++)
	{
		if (m_controllerMapping[i] == controllerId)
		{
			index = static_cast<EHmdController>(i);
			break;
		}
	}

	if ((index < eHmdController_OpenVR_MaxNumOpenVRControllers) && (m_state[index].packetNum != vrState.unPacketNum))
	{
		m_previousState[index] = m_state[index];

		m_state[index].packetNum = vrState.unPacketNum;
		m_state[index].buttonsPressed = vrState.ulButtonPressed;
		m_state[index].buttonsTouched = vrState.ulButtonTouched;

		m_state[index].trigger = vrState.rAxis[vr::k_EButton_SteamVR_Trigger & 0x0f].x;
		m_state[index].touchPad.x = vrState.rAxis[vr::k_EButton_SteamVR_Touchpad & 0x0f].x;
		m_state[index].touchPad.y = vrState.rAxis[vr::k_EButton_SteamVR_Touchpad & 0x0f].y;

		// Send button events (if necessary)
		PostButtonIfChanged(index, eKI_Motion_OpenVR_System);
		PostButtonIfChanged(index, eKI_Motion_OpenVR_ApplicationMenu);
		PostButtonIfChanged(index, eKI_Motion_OpenVR_Grip);
		PostButtonIfChanged(index, eKI_Motion_OpenVR_TriggerBtn);
		PostButtonIfChanged(index, eKI_Motion_OpenVR_TouchPadBtn);

		// send trigger event (if necessary)
		if (m_state[index].trigger != m_previousState[index].trigger)
		{
			SInputEvent event;
			SInputSymbol* pSymbol = m_symbols[eKI_Motion_OpenVR_Trigger - OPENVR_BASE];
			pSymbol->ChangeEvent(m_state[index].trigger);
			pSymbol->AssignTo(event);
			event.deviceIndex = controllerId;
			event.deviceType = eIDT_MotionController;
			gEnv->pInput->PostInputEvent(event);
		}

		// send touch pad events (if necessary)
		if (m_state[index].touchPad.x != m_previousState[index].touchPad.x)
		{
			SInputEvent event;
			SInputSymbol* pSymbol = m_symbols[eKI_Motion_OpenVR_TouchPad_X - OPENVR_BASE];
			pSymbol->ChangeEvent(m_state[index].touchPad.x);
			pSymbol->AssignTo(event);
			event.deviceIndex = controllerId;
			event.deviceType = eIDT_MotionController;
			gEnv->pInput->PostInputEvent(event);
		}
		if (m_state[index].touchPad.y != m_previousState[index].touchPad.y)
		{
			SInputEvent event;
			SInputSymbol* pSymbol = m_symbols[eKI_Motion_OpenVR_TouchPad_Y - OPENVR_BASE];
			pSymbol->ChangeEvent(m_state[index].touchPad.y);
			pSymbol->AssignTo(event);
			event.deviceIndex = controllerId;
			event.deviceType = eIDT_MotionController;
			gEnv->pInput->PostInputEvent(event);
		}
	}
	m_state[index].localPose = localState;
	m_state[index].nativePose = nativeState;
}

// -------------------------------------------------------------------------
void Controller::PostButtonIfChanged(EHmdController controllerId, EKeyId keyID)
{
	SInputSymbol* pSymbol = m_symbols[keyID - OPENVR_BASE];
	vr::EVRButtonId vrBtn = static_cast<vr::EVRButtonId>((pSymbol->devSpecId & (~OPENVR_SPECIAL)));

	bool wasPressed = m_previousState[controllerId].Pressed(vrBtn);
	bool isPressed = m_state[controllerId].Pressed(vrBtn);

	if (isPressed != wasPressed)
	{
		SInputEvent event;
		pSymbol->PressEvent(isPressed);
		pSymbol->AssignTo(event);
		event.deviceIndex = controllerId;
		event.deviceType = eIDT_MotionController;
		gEnv->pInput->PostInputEvent(event);
	}
}

// -------------------------------------------------------------------------
void Controller::DebugDraw(float& xPosLabel, float& yPosLabel) const
{
	// Basic info
	const float controllerWidth = 400.0f;
	const float yPos = yPosLabel, xPosData = xPosLabel, yDelta = 20.f;
	float y = yPos;
	const ColorF fColorLabel(1.0f, 1.0f, 1.0f, 1.0f);
	const ColorF fColorDataConn(1.0f, 1.0f, 0.0f, 1.0f);
	const ColorF fColorDataBt(0.0f, 1.0f, 0.0f, 1.0f);
	const ColorF fColorDataTr(0.0f, 0.0f, 1.0f, 1.0f);
	const ColorF fColorDataTh(1.0f, 0.0f, 1.0f, 1.0f);
	const ColorF fColorDataPose(0.0f, 1.0f, 1.0f, 1.0f);

	IRenderAuxText::Draw2dLabel(xPosLabel, y, 1.3f, fColorLabel, false, "OpenVR Controller Info:");
	y += yDelta;

	IRenderAuxText::Draw2dLabel(xPosData, y, 1.3f, fColorDataConn, false, "Left Hand:%s", IsConnected(eHmdController_OpenVR_1) ? "Connected" : "Disconnected");
	IRenderAuxText::Draw2dLabel(xPosData + controllerWidth, y, 1.3f, fColorDataConn, false, "Right Hand:%s", IsConnected(eHmdController_OpenVR_2) ? "Connected" : "Disconnected");
	y += yDelta;

	IRenderAuxText::Draw2dLabel(xPosData, y, 1.3f, fColorDataBt, false, "System:%s", IsButtonPressed(eHmdController_OpenVR_1, eKI_Motion_OpenVR_System) ? "Pressed" : "Released");
	IRenderAuxText::Draw2dLabel(xPosData + controllerWidth, y, 1.3f, fColorDataBt, false, "Bt System:%s", IsButtonPressed(eHmdController_OpenVR_2, eKI_Motion_OpenVR_System) ? "Pressed" : "Released");
	y += yDelta;
	IRenderAuxText::Draw2dLabel(xPosData, y, 1.3f, fColorDataBt, false, "App:%s", IsButtonPressed(eHmdController_OpenVR_1, eKI_Motion_OpenVR_ApplicationMenu) ? "Pressed" : "Released");
	IRenderAuxText::Draw2dLabel(xPosData + controllerWidth, y, 1.3f, fColorDataBt, false, "Bt App:%s", IsButtonPressed(eHmdController_OpenVR_2, eKI_Motion_OpenVR_ApplicationMenu) ? "Pressed" : "Released");
	y += yDelta;
	IRenderAuxText::Draw2dLabel(xPosData, y, 1.3f, fColorDataBt, false, "Grip:%s", IsButtonPressed(eHmdController_OpenVR_1, eKI_Motion_OpenVR_Grip) ? "Pressed" : "Released");
	IRenderAuxText::Draw2dLabel(xPosData + controllerWidth, y, 1.3f, fColorDataBt, false, "Bt Grip:%s", IsButtonPressed(eHmdController_OpenVR_2, eKI_Motion_OpenVR_Grip) ? "Pressed" : "Released");
	y += yDelta;
	IRenderAuxText::Draw2dLabel(xPosData, y, 1.3f, fColorDataBt, false, "Trigger:%s", IsButtonPressed(eHmdController_OpenVR_1, eKI_Motion_OpenVR_TriggerBtn) ? "Pressed" : "Released");
	IRenderAuxText::Draw2dLabel(xPosData + controllerWidth, y, 1.3f, fColorDataBt, false, "Bt Trigger:%s", IsButtonPressed(eHmdController_OpenVR_2, eKI_Motion_OpenVR_TriggerBtn) ? "Pressed" : "Released");
	y += yDelta;
	IRenderAuxText::Draw2dLabel(xPosData, y, 1.3f, fColorDataBt, false, "Pad:%s", IsButtonPressed(eHmdController_OpenVR_1, eKI_Motion_OpenVR_TouchPadBtn) ? "Pressed" : "Released");
	IRenderAuxText::Draw2dLabel(xPosData + controllerWidth, y, 1.3f, fColorDataBt, false, "Bt Pad:%s", IsButtonPressed(eHmdController_OpenVR_2, eKI_Motion_OpenVR_TouchPadBtn) ? "Pressed" : "Released");
	y += yDelta;

	IRenderAuxText::Draw2dLabel(xPosData, y, 1.3f, fColorDataTr, false, "Trigger:%.2f", GetTriggerValue(eHmdController_OpenVR_1, eKI_Motion_OpenVR_Trigger));
	IRenderAuxText::Draw2dLabel(xPosData + controllerWidth, y, 1.3f, fColorDataTr, false, "Trigger:%.2f", GetTriggerValue(eHmdController_OpenVR_2, eKI_Motion_OpenVR_Trigger));
	y += yDelta;

	Vec2 touchLeft = GetThumbStickValue(eHmdController_OpenVR_1, eKI_Motion_OpenVR_TouchPad_X);
	IRenderAuxText::Draw2dLabel(xPosData, y, 1.3f, fColorDataTh, false, "Pad:(%.2f %.2f)", touchLeft.x, touchLeft.y);
	Vec2 touchRight = GetThumbStickValue(eHmdController_OpenVR_2, eKI_Motion_OpenVR_TouchPad_X);
	IRenderAuxText::Draw2dLabel(xPosData + controllerWidth, y, 1.3f, fColorDataTh, false, "Pad:(%.2f %.2f)", touchRight.x, touchRight.y);
	y += yDelta;

	Vec3 posLH = GetNativeTrackingState(eHmdController_OpenVR_1).pose.position;
	Vec3 posRH = GetNativeTrackingState(eHmdController_OpenVR_2).pose.position;
	Vec3 posWLH = GetLocalTrackingState(eHmdController_OpenVR_1).pose.position;
	Vec3 posWRH = GetLocalTrackingState(eHmdController_OpenVR_2).pose.position;
	IRenderAuxText::Draw2dLabel(xPosData, y, 1.3f, fColorDataPose, false, "Pose:(%.2f,%.2f,%.2f)", posLH.x, posLH.y, posLH.z);
	IRenderAuxText::Draw2dLabel(xPosData + controllerWidth, y, 1.3f, fColorDataPose, false, "Pose:(%.2f,%.2f,%.2f)", posRH.x, posRH.y, posRH.z);
	y += yDelta;
	IRenderAuxText::Draw2dLabel(xPosData, y, 1.3f, fColorDataPose, false, "W-Pose:(%.2f,%.2f,%.2f)", posWLH.x, posWLH.y, posWLH.z);
	IRenderAuxText::Draw2dLabel(xPosData + controllerWidth, y, 1.3f, fColorDataPose, false, "W-Pose:(%.2f,%.2f,%.2f)", posWRH.x, posWRH.y, posWRH.z);
	y += yDelta;

	Ang3 rotLHAng(GetNativeTrackingState(eHmdController_OpenVR_1).pose.orientation);
	Vec3 rotLH(RAD2DEG(rotLHAng));
	Ang3 rotRHAng(GetNativeTrackingState(eHmdController_OpenVR_2).pose.orientation);
	Vec3 rotRH(RAD2DEG(rotRHAng));

	Ang3 rotWLHAng(GetLocalTrackingState(eHmdController_OpenVR_1).pose.orientation);
	Vec3 rotWLH(RAD2DEG(rotWLHAng));
	Ang3 rotWRHAng(GetLocalTrackingState(eHmdController_OpenVR_2).pose.orientation);
	Vec3 rotWRH(RAD2DEG(rotWRHAng));

	IRenderAuxText::Draw2dLabel(xPosData, y, 1.3f, fColorDataPose, false, "Rot[PRY]:(%.2f,%.2f,%.2f)", rotLH.x, rotLH.y, rotLH.z);
	IRenderAuxText::Draw2dLabel(xPosData + controllerWidth, y, 1.3f, fColorDataPose, false, "Rot[PRY]:(%.2f,%.2f,%.2f)", rotRH.x, rotRH.y, rotRH.z);
	y += yDelta;
	IRenderAuxText::Draw2dLabel(xPosData, y, 1.3f, fColorDataPose, false, "W-Rot[PRY]:(%.2f,%.2f,%.2f)", rotWLH.x, rotWLH.y, rotWLH.z);
	IRenderAuxText::Draw2dLabel(xPosData + controllerWidth, y, 1.3f, fColorDataPose, false, "W-Rot[PRY]:(%.2f,%.2f,%.2f)", rotWRH.x, rotWRH.y, rotWRH.z);
	y += yDelta;

	yPosLabel = y;
}

// -------------------------------------------------------------------------;
void Controller::OnControllerConnect(vr::TrackedDeviceIndex_t controllerId)
{
	bool added = false;
	for (int i = 0; i < eHmdController_OpenVR_MaxNumOpenVRControllers; i++)
	{
		if (m_controllerMapping[i] >= vr::k_unMaxTrackedDeviceCount)
		{
			m_controllerMapping[i] = controllerId;
			m_state[i] = SControllerState();
			m_previousState[i] = SControllerState();
			added = true;
			break;
		}
	}

	if (!added)
	{
		gEnv->pLog->LogError("[HMD][OpenVR] Maximum number of controllers reached! Ignoring new controller!");
	}
}

// -------------------------------------------------------------------------
void Controller::OnControllerDisconnect(vr::TrackedDeviceIndex_t controllerId)
{
	for (int i = 0; i < eHmdController_OpenVR_MaxNumOpenVRControllers; i++)
	{
		if (m_controllerMapping[i] == controllerId)
		{
			m_controllerMapping[i] = vr::k_unMaxTrackedDeviceCount;
			break;
		}
	}
}

// -------------------------------------------------------------------------
bool Controller::IsConnected(EHmdController id) const
{
	return m_pSystem->IsTrackedDeviceConnected(m_controllerMapping[id]);
}

// -------------------------------------------------------------------------
void Controller::ApplyForceFeedback(EHmdController id, float freq, float amplitude)
{
	auto axisId = vr::k_EButton_SteamVR_Touchpad - vr::k_EButton_Axis0;
	auto durationMicroSeconds = (ushort)clamp_tpl(freq, 0.f, 5000.f);

	m_pSystem->TriggerHapticPulse(m_controllerMapping[id], axisId, durationMicroSeconds);
}

} // namespace OpenVR
} // namespace CryVR
