// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "CrySensorSystemPluginDLL.h"

// Included only once per DLL module.
#include <CryCore/Platform/platform_impl.inl>

#include "SensorSystem.h"

CCrySensorSystemPlugin::CCrySensorSystemPlugin()
{
	m_loadPriority = EPluginPriority_CryLoad;
	m_unloadPriority = EPluginPriority_CryUnload;
}

const char* CCrySensorSystemPlugin::GetName() const
{
	return "CrySensorSystem";
}

const char* CCrySensorSystemPlugin::GetCategory() const
{
	return "Plugin";
}

bool CCrySensorSystemPlugin::Initialize(SSystemGlobalEnvironment& env, const SSystemInitParams& initParams)
{
	m_pSensorSystem.reset(new CSensorSystem());
	ICryPlugin::SetUpdateFlags(EUpdateType_Update);
	return true;
}

ISensorSystem& CCrySensorSystemPlugin::GetSensorSystem() const
{
	CRY_ASSERT(m_pSensorSystem);
	return *m_pSensorSystem.get();
}

void CCrySensorSystemPlugin::OnPluginUpdate(EPluginUpdateType updateType)
{
	switch (updateType)
	{
	case IPluginUpdateListener::EUpdateType_Update:
		{
			m_pSensorSystem->Update();
			break;
		}
	}
}

CRYREGISTER_SINGLETON_CLASS(CCrySensorSystemPlugin)

#include <CryCore/CrtDebugStats.h>
