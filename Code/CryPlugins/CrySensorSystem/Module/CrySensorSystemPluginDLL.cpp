// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "CrySensorSystemPluginDLL.h"

// Included only once per DLL module.
#include <CryCore/Platform/platform_impl.inl>

#include "SensorSystem.h"

namespace Cry
{
	namespace SensorSystem
	{
		bool CCrySensorSystemPlugin::Initialize(SSystemGlobalEnvironment& env, const SSystemInitParams& initParams)
		{
			m_pSensorSystem.reset(new CSensorSystem());
			EnableUpdate(EUpdateStep::MainUpdate, true);
			return true;
		}

		ISensorSystem& CCrySensorSystemPlugin::GetSensorSystem() const
		{
			CRY_ASSERT(m_pSensorSystem);
			return *m_pSensorSystem.get();
		}

		void CCrySensorSystemPlugin::MainUpdate(float frameTime)
		{
			m_pSensorSystem->Update();
		}

		CRYREGISTER_SINGLETON_CLASS(CCrySensorSystemPlugin)
	}
}

#include <CryCore/CrtDebugStats.h>
