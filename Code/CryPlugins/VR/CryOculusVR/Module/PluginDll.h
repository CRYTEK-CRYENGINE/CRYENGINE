// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "../Interface/IOculusVRPlugin.h"

namespace CryVR
{
namespace Oculus {
class CPlugin_OculusVR : public IOculusVRPlugin, public ISystemEventListener
{
	CRYINTERFACE_BEGIN()
	CRYINTERFACE_ADD(IOculusVRPlugin)
	CRYINTERFACE_ADD(Cry::IEnginePlugin)
	CRYINTERFACE_END()

	CRYGENERATE_SINGLETONCLASS_GUID(CPlugin_OculusVR, "Plugin_OculusVR", "4df8241e-2bc2-4ec7-b237-ee5db27265b3"_cry_guid)

	virtual ~CPlugin_OculusVR();

	//! This is called to initialize the new plugin.
	virtual bool Initialize(SSystemGlobalEnvironment& env, const SSystemInitParams& initParams) override;

public:
	virtual IOculusDevice* CreateDevice() override;
	virtual IOculusDevice* GetDevice() const override;

	// ISystemEventListener
	virtual void OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam) override;
	// ~ISystemEventListener

	// Start CVars
public:
	static int   s_hmd_low_persistence;
	static int   s_hmd_dynamic_prediction;
	static float s_hmd_ipd;
	static int   s_hmd_queue_ahead;
	static int   s_hmd_projection;
	static int   s_hmd_perf_hud;
	static float s_hmd_projection_screen_dist;
};

}      // namespace Oculus
}      // namespace CryVR