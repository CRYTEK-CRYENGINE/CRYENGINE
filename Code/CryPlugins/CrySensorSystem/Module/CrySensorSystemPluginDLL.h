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

	CRYGENERATE_SINGLETONCLASS(CCrySensorSystemPlugin, "Plugin_CrySensorSystem", 0x08a9684689334211, 0x913f7a64c0bf9822)

	CCrySensorSystemPlugin();
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
