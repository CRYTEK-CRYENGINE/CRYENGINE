// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "PluginDll.h"

#include "OsvrResources.h"
#include "OsvrDevice.h"

// Included only once per DLL module.
#include <CryCore/Platform/platform_impl.inl>

#include <CrySystem/VR/IHMDManager.h>

namespace CryVR
{
namespace Osvr {

CPlugin_Osvr::~CPlugin_Osvr()
{
	CryVR::Osvr::Resources::Shutdown();

	GetISystem()->GetISystemEventDispatcher()->RemoveListener(this);
}

bool CPlugin_Osvr::Initialize(SSystemGlobalEnvironment& env, const SSystemInitParams& initParams)
{
	GetISystem()->GetISystemEventDispatcher()->RegisterListener(this, "CPlugin_Osvr");

	return true;
}

void CPlugin_Osvr::OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam)
{
	switch (event)
	{
	case ESYSTEM_EVENT_PRE_RENDERER_INIT:
	{
		// Initialize resources to make sure we query available VR devices
		CryVR::Osvr::Resources::Init();

		if (auto *pDevice = GetDevice())
		{
			gEnv->pSystem->GetHmdManager()->RegisterDevice(GetName(), *pDevice);
		}
	}
	break;
	}
}

IOsvrDevice* CPlugin_Osvr::CreateDevice()
{
	return GetDevice();
}

IOsvrDevice* CPlugin_Osvr::GetDevice() const
{
	return CryVR::Osvr::Resources::GetAssociatedDevice();
}

CRYREGISTER_SINGLETON_CLASS(CPlugin_Osvr)
}      // namespace Osvr
}      // namespace CryVR

#include <CryCore/CrtDebugStats.h>