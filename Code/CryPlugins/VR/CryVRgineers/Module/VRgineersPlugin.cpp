#include "StdAfx.h"
#include "VRgineersPlugin.h"
#include "DeviceProvider.h"

#include <CrySchematyc/Env/IEnvRegistry.h>
#include <CrySchematyc/Env/EnvPackage.h>
#include <CrySchematyc/Utils/SharedString.h>

// Included only once per DLL module.
#include <CryCore/Platform/platform_impl.inl>

namespace CryVR
{
	namespace VRgineers
	{
		VRgineersPlugin::~VRgineersPlugin()
		{
			ISystem* pSystem = GetISystem();
			if (pSystem)
			{
				pSystem->GetHmdManager()->UnregisterDevice(GetName());			
				pSystem->GetISystemEventDispatcher()->RemoveListener(this);
			}

			DeviceProvider::GetInstance().Shutdown();
		}

		bool VRgineersPlugin::Initialize(SSystemGlobalEnvironment& env, const SSystemInitParams& initParams)
		{
			gEnv->pSystem->GetISystemEventDispatcher()->RegisterListener(this, "VRgineersPlugin");

			return true;
		}

		void VRgineersPlugin::OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam)
		{
			switch (event)
			{
				case ESYSTEM_EVENT_PRE_RENDERER_INIT:
				{
					DeviceProvider::GetInstance().Init();

					if (auto *pDevice = GetDevice())
					{
						gEnv->pSystem->GetHmdManager()->RegisterDevice(GetName(), *pDevice);
					}
				}
				break;
			}
		}

		IVRgineersDevice* VRgineersPlugin::CreateDevice()
		{
			return GetDevice();
		}

		IVRgineersDevice* VRgineersPlugin::GetDevice() const
		{
			return DeviceProvider::GetInstance().GetDevice();
		}

		CRYREGISTER_SINGLETON_CLASS(VRgineersPlugin)
	}
}