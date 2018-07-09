// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "HMDManager.h"
#include "HMDCVars.h"

#include <CryRenderer/IStereoRenderer.h>

#include <CrySystem/ICryPluginManager.h>

// Note:
//  We support a single HMD device at a time.
// This manager will be updated accordingly as other HMDs come, based on their system's init/shutdown, etc requirements.
// That may imply changes in the interface.

CHmdManager::~CHmdManager()
{
}

// ------------------------------------------------------------------------
void CHmdManager::RegisterDevice(const char* szDeviceName, IHmdDevice& device)
{
	// Reference counting will be handled inside the vector
	m_availableDeviceMap.insert(TDeviceMap::value_type(szDeviceName, &device));
}

// ------------------------------------------------------------------------
void CHmdManager::UnregisterDevice(const char* szDeviceName)
{
	auto it = m_availableDeviceMap.find(szDeviceName);
	if (it == m_availableDeviceMap.end())
		return;

	// If we lost selected device, nullify it
	if (m_pHmdDevice == it->second)
	{
		m_pHmdDevice = nullptr;
		if (gEnv->pRenderer != nullptr)
			gEnv->pRenderer->GetIStereoRenderer()->OnHmdDeviceChanged(m_pHmdDevice);
	}

	m_availableDeviceMap.erase(it);
}

// ------------------------------------------------------------------------
void CHmdManager::OnVirtualRealityDeviceChanged(ICVar *pCVar)
{
	gEnv->pSystem->GetHmdManager()->SetupAction(EHmdSetupAction::eHmdSetupAction_Init);
}

// ------------------------------------------------------------------------
void CHmdManager::SetupAction(EHmdSetupAction cmd)
{
	LOADING_TIME_PROFILE_SECTION;
	switch (cmd)
	{
	case EHmdSetupAction::eHmdSetupAction_CreateCvars:
		CryVR::CVars::Register();
		break;
	// ------------------------------------------------------------------------
	case EHmdSetupAction::eHmdSetupAction_PostInit: // Nothing to do for Oculus after SDK 0.6.0
		break;
	// ------------------------------------------------------------------------
	case EHmdSetupAction::eHmdSetupAction_Init:
		{
			if (gEnv->pConsole)
			{
				ICVar* pVrSupportVar = gEnv->pConsole->GetCVar("sys_vr_support");

				if (pVrSupportVar->GetIVal() > 0)
				{
					m_pHmdDevice = nullptr;

					const char *selectedHmdName = CryVR::CVars::pSelectedHmdNameVar->GetString();
					TDeviceMap::iterator hmdIt;

					if (strlen(selectedHmdName) > 0)
					{
						hmdIt = m_availableDeviceMap.find(selectedHmdName);
						if (hmdIt == m_availableDeviceMap.end())
						{
							pVrSupportVar->Set(0);

							if (gEnv->pRenderer != nullptr)
							{
								gEnv->pRenderer->GetIStereoRenderer()->OnHmdDeviceChanged(m_pHmdDevice);
							}

							CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "Tried to select unavailable VR device %s!", selectedHmdName);
							return;
						}
					}
					else // No HMD explicitly selected, opt for first available (since sys_vr_support was 1)
					{
						hmdIt = m_availableDeviceMap.begin();
						if (hmdIt == m_availableDeviceMap.end())
						{
							pVrSupportVar->Set(0);

							if (gEnv->pRenderer != nullptr)
							{
								gEnv->pRenderer->GetIStereoRenderer()->OnHmdDeviceChanged(m_pHmdDevice);
							}

							CryWarning(VALIDATOR_MODULE_SYSTEM, VALIDATOR_ERROR, "VR support was enabled, but no VR device was detected!");
							return;
						}
					}

					m_pHmdDevice = hmdIt->second;

					if (gEnv->pRenderer != nullptr)
					{
						gEnv->pRenderer->GetIStereoRenderer()->OnHmdDeviceChanged(m_pHmdDevice);
					}

					gEnv->pSystem->LoadConfiguration("vr.cfg", 0, eLoadConfigGame);
				}
				else if(m_pHmdDevice != nullptr)
				{
					m_pHmdDevice = nullptr;

					gEnv->pRenderer->GetIStereoRenderer()->OnHmdDeviceChanged(m_pHmdDevice);
				}
			}
		}
		break;

	default:
		assert(0);
	}

}

// ------------------------------------------------------------------------
void CHmdManager::Action(EHmdAction action)
{
	if (m_pHmdDevice)
	{
		switch (action)
		{
		case EHmdAction::eHmdAction_DrawInfo:
			m_pHmdDevice->UpdateInternal(IHmdDevice::eInternalUpdate_DebugInfo);
			break;
		default:
			assert(0);
		}
	}
}

// ------------------------------------------------------------------------
void CHmdManager::UpdateTracking(EVRComponent updateType)
{
	if (m_pHmdDevice)
	{
		IRenderer* pRenderer = gEnv->pRenderer;
		const int frameId = pRenderer->GetFrameID(false);

		m_pHmdDevice->UpdateTrackingState(updateType, frameId);
	}
}

// ------------------------------------------------------------------------
bool CHmdManager::IsStereoSetupOk() const
{
	if (m_pHmdDevice)
	{
		if (IStereoRenderer* pStereoRenderer = gEnv->pRenderer->GetIStereoRenderer())
		{
			EStereoDevice device = EStereoDevice::STEREO_DEVICE_NONE;
			EStereoMode mode = EStereoMode::STEREO_MODE_NO_STEREO;
			EStereoOutput output = EStereoOutput::STEREO_OUTPUT_STANDARD;

			pStereoRenderer->GetInfo(&device, &mode, &output, 0);

			return (
			  (device == EStereoDevice::STEREO_DEVICE_DEFAULT || device == EStereoDevice::STEREO_DEVICE_FRAMECOMP) &&
			  (mode == EStereoMode::STEREO_MODE_POST_STEREO || mode == EStereoMode::STEREO_MODE_DUAL_RENDERING || mode == EStereoMode::STEREO_MODE_MENU) &&
			  (output == EStereoOutput::STEREO_OUTPUT_SIDE_BY_SIDE || output == EStereoOutput::STEREO_OUTPUT_HMD)
			  );
		}
	}
	return false;
}

// ------------------------------------------------------------------------
HMDCameraSetup CHmdManager::GetHMDCameraSetup(int nEye, float projRatio, float fnear) const
{
	if (m_pHmdDevice)
		return m_pHmdDevice->GetHMDCameraSetup(nEye, projRatio, fnear);

	return HMDCameraSetup{};
}

// ------------------------------------------------------------------------
void CHmdManager::RecenterPose()
{
	// All HMDs subscribe to our events, so simply notify listeners and reaction will occur
	for (auto it = m_listeners.begin(); it != m_listeners.end(); ++it)
	{
		(*it)->OnRecentered();
	}
}