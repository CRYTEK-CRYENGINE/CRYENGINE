// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "../Interface/IOculusVRPlugin.h"

namespace CryVR
{
namespace Oculus {
class CPlugin_OculusVR : public IOculusVRPlugin, public ISystemEventListener
{
	CRYINTERFACE_BEGIN()
	CRYINTERFACE_ADD(IOculusVRPlugin)
	CRYINTERFACE_ADD(ICryPlugin)
	CRYINTERFACE_END()

	CRYGENERATE_SINGLETONCLASS(CPlugin_OculusVR, "Plugin_OculusVR", 0x4DF8241E2BC24EC7, 0xB237EE5DB27265B3)

	CPlugin_OculusVR();
	virtual ~CPlugin_OculusVR();

	//! Retrieve name of plugin.
	virtual const char* GetName() const override { return "CryOculusVR"; }

	//! Retrieve category for the plugin.
	virtual const char* GetCategory() const override { return "Plugin"; }

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
	static int   s_hmd_post_inject_camera;

protected:
	virtual void OnPluginUpdate(EPluginUpdateType updateType) override {}
};

}      // namespace Oculus
}      // namespace CryVR