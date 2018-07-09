// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySystem/VR/IHMDDevice.h>

struct IHmdEventListener
{
	// Called when the user requested that the pose is recentered
	virtual void OnRecentered() = 0;
};

//! Main interface to the engine's head-mounted device manager, responsible for maintaining VR devices connected to the system
struct IHmdManager
{
public:

	virtual ~IHmdManager() {}

	enum EHmdSetupAction
	{
		eHmdSetupAction_CreateCvars = 0,
		eHmdSetupAction_Init,
		eHmdSetupAction_PostInit
	};

	enum EHmdAction
	{
		eHmdAction_DrawInfo = 0,
	};

	//! Used to register a HMD headset with the system for later use by the user.
	virtual void RegisterDevice(const char* szDeviceName, IHmdDevice& device) = 0;

	//! Used to unregister the HWD headset
	virtual void UnregisterDevice(const char* szDeviceName) = 0;

	//! Basic functionality needed to setup and destroy an HMD during system init / system shutdown.
	virtual void SetupAction(EHmdSetupAction cmd) = 0;

	//! Trigger an action on the current HMD.
	virtual void Action(EHmdAction action) = 0;

	//! Update the tracking information.
	virtual void UpdateTracking(EVRComponent vrComponent) = 0;

	//! \return Active HMD (or NULL if none have been activated).
	virtual IHmdDevice* GetHmdDevice() const = 0;

	//! \return true if we have an HMD device recognized and r_stereodevice, r_stereooutput and r_stereomode are properly set for stereo rendering.
	virtual bool IsStereoSetupOk() const = 0;

	//! Populates o_info with the asymmetric camera information returned by the current HMD device.
	virtual HMDCameraSetup GetHMDCameraSetup(int nEye, float projRatio, float fnear) const = 0;

	// Notifies all HMD devices of the post being recentered, current position / rotation should be IDENTITY
	virtual void RecenterPose() = 0;

	virtual void AddEventListener(IHmdEventListener *pListener) = 0;
	virtual void RemoveEventListener(IHmdEventListener *pListener) = 0;
};
