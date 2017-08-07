// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"

// Included only once per DLL module.
#include <CryCore/Platform/platform_impl.inl>

class CCryPerceptionSystemPlugin : public ICryPerceptionSystemPlugin
{
	CRYINTERFACE_BEGIN()
		CRYINTERFACE_ADD(ICryPerceptionSystemPlugin)
		CRYINTERFACE_ADD(ICryPlugin)
	CRYINTERFACE_END()

	CRYGENERATE_SINGLETONCLASS_GUID(CCryPerceptionSystemPlugin, "Plugin_CryPerceptionSystem", "a4ee2509-3468-4bad-8472-dc66d91186c6"_cry_guid)

	PLUGIN_FLOWNODE_REGISTER
	PLUGIN_FLOWNODE_UNREGISTER

	CCryPerceptionSystemPlugin() : m_pPerceptionManager(nullptr) {}
	virtual ~CCryPerceptionSystemPlugin() {}

	// ICryPlugin
	virtual const char* GetName() const override { return "CryPerceptionSystem"; }
	virtual const char* GetCategory() const override { return "Plugin"; }
	virtual bool        Initialize(SSystemGlobalEnvironment& env, const SSystemInitParams& initParams) override;
	// ~ICryPlugin

public:

	// ICryPerceptionSystemPlugin
	virtual IPerceptionManager& GetPerceptionManager() const override;
	// ~ICryPerceptionSystemPlugin

protected:

	// IPluginUpdateListener
	virtual void OnPluginUpdate(EPluginUpdateType updateType) override {}
	// ~IPluginUpdateListener

private:
	std::unique_ptr<CPerceptionManager> m_pPerceptionManager;
};

//-----------------------------------------------------------------------------------------------------------
bool CCryPerceptionSystemPlugin::Initialize(SSystemGlobalEnvironment& env, const SSystemInitParams& initParams)
{
	m_pPerceptionManager.reset(new CPerceptionManager);
	return true;
}

//-----------------------------------------------------------------------------------------------------------
IPerceptionManager& CCryPerceptionSystemPlugin::GetPerceptionManager() const
{
	CRY_ASSERT(m_pPerceptionManager);
	return *m_pPerceptionManager.get();
}

//-----------------------------------------------------------------------------------------------------------
CRYREGISTER_SINGLETON_CLASS(CCryPerceptionSystemPlugin)
