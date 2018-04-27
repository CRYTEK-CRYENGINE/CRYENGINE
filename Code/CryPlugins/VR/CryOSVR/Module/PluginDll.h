// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "../Interface/IOsvrPlugin.h"

namespace CryVR
{
namespace Osvr {
class CPlugin_Osvr : public IOsvrPlugin, public ISystemEventListener
{
	CRYINTERFACE_BEGIN()
	CRYINTERFACE_ADD(IOsvrPlugin)
	CRYINTERFACE_ADD(Cry::IEnginePlugin)
	CRYINTERFACE_END()

	CRYGENERATE_SINGLETONCLASS_GUID(CPlugin_Osvr, "Plugin_OSVR", "655d3252-2a6d-4d09-afe8-2386d4566054"_cry_guid)

	virtual ~CPlugin_Osvr();

	//! This is called to initialize the new plugin.
	virtual bool Initialize(SSystemGlobalEnvironment& env, const SSystemInitParams& initParams) override;

public:
	virtual IOsvrDevice* CreateDevice() override;
	virtual IOsvrDevice* GetDevice() const override;

	// ISystemEventListener
	virtual void OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam) override;
	// ~ISystemEventListener
};

}      // namespace Osvr
}      // namespace CryVR