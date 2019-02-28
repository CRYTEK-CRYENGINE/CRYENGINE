// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySystem/VR/IHMDManager.h>

class CHmdManager : public IHmdManager
{
public:
	CHmdManager() : m_pHmdDevice(nullptr) {}
	virtual ~CHmdManager();

	virtual void RegisterDevice(const char* szDeviceName, IHmdDevice& device) override;

	virtual void UnregisterDevice(const char* szDeviceName) override;

	// basic functionality needed to setup and destroy an Hmd during system init / system shutdown
	virtual void SetupAction(EHmdSetupAction cmd) override;

	// trigger an action on the current Hmd
	virtual void Action(EHmdAction action) override;

	// update the tracking information
	virtual void UpdateTracking(EVRComponent vrComponent) override;

	// returns the active Hmd (or NULL if none has been activated)
	virtual struct IHmdDevice* GetHmdDevice() const override { return m_pHmdDevice; }

	// returns true if we have an Hmd device recognized and r_stereodevice, r_stereooutput and r_stereomode are properly set for stereo rendering
	virtual bool IsStereoSetupOk() const override;

	// populates o_info with the asymmetric camera information returned by the current Hmd device
	virtual HMDCameraSetup GetHMDCameraSetup(int nEye, float projRatio, float fnear) const override;

	virtual void RecenterPose() override;

	virtual void AddEventListener(IHmdEventListener *pListener) override { stl::push_back_unique(m_listeners, pListener); }
	virtual void RemoveEventListener(IHmdEventListener *pListener) override { stl::find_and_erase(m_listeners, pListener); }

public:
	static void OnVirtualRealityDeviceChanged(ICVar* pCVar);

private:
	// Devices connected to the computer and available for use
	// Key = device / SDK name, value = reference-counted device pointer
	typedef std::unordered_map<string, _smart_ptr<IHmdDevice>, stl::hash_strcmp<string>> TDeviceMap;
	TDeviceMap m_availableDeviceMap;

	struct IHmdDevice* m_pHmdDevice;

	std::vector<IHmdEventListener*> m_listeners;
};
