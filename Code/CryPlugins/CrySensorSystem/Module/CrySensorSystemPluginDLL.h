// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "../Interface/ICrySensorSystemPlugin.h"

class CSensorSystem;

class CCrySensorSystemPlugin : public ICrySensorSystemPlugin
{
	CRYINTERFACE_BEGIN()
	CRYINTERFACE_ADD(ICrySensorSystemPlugin)
	CRYINTERFACE_ADD(ICryPlugin)
	CRYINTERFACE_END()

	CRYGENERATE_SINGLETONCLASS_GUID(CCrySensorSystemPlugin, "Plugin_CrySensorSystem", "08a96846-8933-4211-913f-7a64c0bf9822"_cry_guid)

	virtual ~CCrySensorSystemPlugin() {}

	// ICryPlugin
	virtual const char* GetName() const override;
	virtual const char* GetCategory() const override;
	virtual bool        Initialize(SSystemGlobalEnvironment& env, const SSystemInitParams& initParams) override;
	// ~ICryPlugin

public:

	// ICrySensorSystemPlugin
	virtual ISensorSystem& GetSensorSystem() const override;
	// ~ICrySensorSystemPlugin

protected:

	// IPluginUpdateListener
	virtual void OnPluginUpdate(EPluginUpdateType updateType) override;
	// ~IPluginUpdateListener

private:

	std::unique_ptr<CSensorSystem> m_pSensorSystem;
};
