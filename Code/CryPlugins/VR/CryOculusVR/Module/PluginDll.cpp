// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "PluginDll.h"

#include "OculusResources.h"
#include "OculusDevice.h"

// Included only once per DLL module.
#include <CryCore/Platform/platform_impl.inl>
#include <CrySystem/VR/IHMDManager.h>

namespace CryVR
{
namespace Oculus {

	int CPlugin_OculusVR::s_hmd_low_persistence = 0;
	int CPlugin_OculusVR::s_hmd_dynamic_prediction = 0;
	float CPlugin_OculusVR::s_hmd_ipd = -1.0f;
	int CPlugin_OculusVR::s_hmd_queue_ahead = 1;
	int CPlugin_OculusVR::s_hmd_projection = 0;
	int CPlugin_OculusVR::s_hmd_perf_hud = 0;
	float CPlugin_OculusVR::s_hmd_projection_screen_dist = 1.0f;

CPlugin_OculusVR::~CPlugin_OculusVR()
{
	if (IConsole* const pConsole = gEnv->pConsole)
	{
		pConsole->UnregisterVariable("hmd_low_persistence");
		pConsole->UnregisterVariable("hmd_dynamic_prediction");
		pConsole->UnregisterVariable("hmd_ipd");
		pConsole->UnregisterVariable("hmd_queue_ahead");
		pConsole->UnregisterVariable("hmd_projection");
		pConsole->UnregisterVariable("hmd_projection_screen_dist");
		pConsole->UnregisterVariable("hmd_perf_hud");
	}

	GetISystem()->GetISystemEventDispatcher()->RemoveListener(this);
}

bool CPlugin_OculusVR::Initialize(SSystemGlobalEnvironment& env, const SSystemInitParams& initParams)
{
	GetISystem()->GetISystemEventDispatcher()->RegisterListener(this, "CPlugin_OculusVR");

	REGISTER_CVAR2("hmd_low_persistence", &s_hmd_low_persistence, s_hmd_low_persistence,
		VF_NULL, "Enables low persistence mode.");

	REGISTER_CVAR2("hmd_dynamic_prediction", &s_hmd_dynamic_prediction, s_hmd_dynamic_prediction,
		VF_NULL, "Enables dynamic prediction based on internally measured latency.");

	REGISTER_CVAR2("hmd_ipd", &s_hmd_ipd, s_hmd_ipd,
		VF_NULL, "HMD Interpupillary distance. If set to a value lower than zero it reads the IPD from the HMD device");

	REGISTER_CVAR2("hmd_queue_ahead", &s_hmd_queue_ahead, s_hmd_queue_ahead,
		VF_NULL, "Enable/Disable Queue Ahead for Oculus");

	REGISTER_CVAR2("hmd_projection", &s_hmd_projection, eHmdProjection_Stereo,
		VF_NULL, "Selects the way the image is projected into the hmd: \n"
		"0 - normal stereoscopic mode\n"
		"1 - monoscopic (cinema-like)\n"
		"2 - monoscopic (head-locked)\n"
	);

	REGISTER_CVAR2("hmd_projection_screen_dist", &s_hmd_projection_screen_dist, 0.f,
		VF_NULL, "If >0 it forces the 'cinema screen' distance to the HMD when using 'monoscopic (cinema-like)' projection");

	REGISTER_CVAR2("hmd_perf_hud", &s_hmd_perf_hud, 0,
		VF_NULL, "Performance HUD Display for HMDs \n"
		"0 - off\n"
		"1 - Summary\n"
		"2 - Latency timing\n"
		"3 - App Render timing\n"
		"4 - Compositor Render timing\n"
		"5 - Version Info\n"
	);

	return true;
}

void CPlugin_OculusVR::OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam)
{
	switch (event)
	{
	case ESYSTEM_EVENT_PRE_RENDERER_INIT:
		{
			// Initialize resources to make sure we query available VR devices
			CryVR::Oculus::Resources::Init();

			if (auto *pDevice = GetDevice())
			{
				gEnv->pSystem->GetHmdManager()->RegisterDevice(GetName(), *pDevice);
			}
		}
		break;
	case ESYSTEM_EVENT_FAST_SHUTDOWN:
	case ESYSTEM_EVENT_FULL_SHUTDOWN:
		{
			gEnv->pSystem->GetHmdManager()->UnregisterDevice(GetName());

			CryVR::Oculus::Resources::Shutdown();
		}
		break;
	}
}

IOculusDevice* CPlugin_OculusVR::CreateDevice()
{
	return GetDevice();
}

IOculusDevice* CPlugin_OculusVR::GetDevice() const
{
	return CryVR::Oculus::Resources::GetAssociatedDevice();
}

CRYREGISTER_SINGLETON_CLASS(CPlugin_OculusVR)

}      // namespace Oculus
}      // namespace CryVR

#include <CryCore/CrtDebugStats.h>