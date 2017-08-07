// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "../Interface/IOsvrPlugin.h"

namespace CryVR
{
namespace Osvr {
class CPlugin_Osvr : public IOsvrPlugin, public ISystemEventListener
{
	CRYINTERFACE_BEGIN()
	CRYINTERFACE_ADD(IOsvrPlugin)
	CRYINTERFACE_ADD(ICryPlugin)
	CRYINTERFACE_END()

	CRYGENERATE_SINGLETONCLASS_GUID(CPlugin_Osvr, "Plugin_OSVR", "655d3252-2a6d-4d09-afe8-2386d4566054"_cry_guid)

	virtual ~CPlugin_Osvr();

	//! Retrieve name of plugin.
	virtual const char* GetName() const override { return "CryOSVR"; }

	//! Retrieve category for the plugin.
	virtual const char* GetCategory() const override { return "Plugin"; }

	//! This is called to initialize the new plugin.
	virtual bool Initialize(SSystemGlobalEnvironment& env, const SSystemInitParams& initParams) override;

public:
	virtual IOsvrDevice* CreateDevice() override;
	virtual IOsvrDevice* GetDevice() const override;

	// ISystemEventListener
	virtual void OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam) override;
	// ~ISystemEventListener

protected:
	virtual void OnPluginUpdate(EPluginUpdateType updateType) override {}
};

}      // namespace Osvr
}      // namespace CryVR