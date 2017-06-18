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

	CRYGENERATE_SINGLETONCLASS(CPlugin_Osvr, "Plugin_OSVR", 0x655D32522A6D4D09, 0xAFE82386D4566054)

	CPlugin_Osvr();
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