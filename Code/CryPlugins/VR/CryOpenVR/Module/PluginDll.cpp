// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "PluginDll.h"

#include "OpenVRResources.h"
#include "OpenVRDevice.h"

// Included only once per DLL module.
#include <CryCore/Platform/platform_impl.inl>

namespace CryVR
{
namespace OpenVR {

CPlugin_OpenVR::~CPlugin_OpenVR()
{
	ISystem* pSystem = GetISystem();
	if (pSystem)
	{
		pSystem->GetHmdManager()->UnregisterDevice(GetName());
	}

	CryVR::OpenVR::Resources::Shutdown();

	if (pSystem)
	{
		pSystem->GetISystemEventDispatcher()->RemoveListener(this);
	}
}

bool CPlugin_OpenVR::Initialize(SSystemGlobalEnvironment& env, const SSystemInitParams& initParams)
{
	GetISystem()->GetISystemEventDispatcher()->RegisterListener(this,"CPlugin_OpenVR");

	return true;
}

void CPlugin_OpenVR::OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam)
{
	switch (event)
	{
	case ESYSTEM_EVENT_PRE_RENDERER_INIT:
		{
			// Initialize resources to make sure we query available VR devices
			CryVR::OpenVR::Resources::Init();

			if (auto *pDevice = GetDevice())
			{
				gEnv->pSystem->GetHmdManager()->RegisterDevice(GetName(), *pDevice);
			}
		}
	break;
	}
}

IOpenVRDevice* CPlugin_OpenVR::CreateDevice()
{
	return GetDevice();
}

IOpenVRDevice* CPlugin_OpenVR::GetDevice() const
{
	return CryVR::OpenVR::Resources::GetAssociatedDevice();
}

CRYREGISTER_SINGLETON_CLASS(CPlugin_OpenVR)
}      // namespace OpenVR
}      // namespace CryVR

#include <CryCore/CrtDebugStats.h>