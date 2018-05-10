// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "../Interface/IOpenVRPlugin.h"

namespace CryVR
{
namespace OpenVR {
class CPlugin_OpenVR : public IOpenVRPlugin, public ISystemEventListener
{
	CRYINTERFACE_BEGIN()
	CRYINTERFACE_ADD(IOpenVRPlugin)
	CRYINTERFACE_ADD(Cry::IEnginePlugin)
	CRYINTERFACE_END()

	CRYGENERATE_SINGLETONCLASS_GUID(CPlugin_OpenVR, "Plugin_OpenVR", "50a54adb-4bbf-4068-80b9-eb3bffa30c93"_cry_guid)

	virtual ~CPlugin_OpenVR();

	//! This is called to initialize the new plugin.
	virtual bool Initialize(SSystemGlobalEnvironment& env, const SSystemInitParams& initParams) override;

public:
	virtual IOpenVRDevice* CreateDevice() override;
	virtual IOpenVRDevice* GetDevice() const override;

	// ISystemEventListener
	virtual void OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam) override;
	// ~ISystemEventListener

	// Start CVars
public:
	static float s_hmd_quad_distance;
	static float s_hmd_quad_width;
	static int s_hmd_quad_absolute;
};

}      // namespace OpenVR
}      // namespace CryVR