#pragma once

#include <CrySystem/ICryPlugin.h>
#include <CryGame/IGameFramework.h>
#include <CryEntitySystem/IEntityClass.h>

#include "IVRgineersPlugin.h"

namespace CryVR
{
	namespace VRgineers
	{
		class VRgineersPlugin : public IVRgineersPlugin, public ISystemEventListener
		{
		public:
			CRYINTERFACE_BEGIN()
			CRYINTERFACE_ADD(IVRgineersPlugin)
			CRYINTERFACE_ADD(Cry::IEnginePlugin)
			CRYINTERFACE_END()

			CRYGENERATE_SINGLETONCLASS_GUID(VRgineersPlugin, "VRgineersPlugin", "B41A55E9-A281-4FDA-A41E-0EBE09CF3193"_cry_guid)

			virtual ~VRgineersPlugin();

		private:
			virtual bool Initialize(SSystemGlobalEnvironment& env, const SSystemInitParams& initParams) override;

		public:
			virtual void OnSystemEvent(ESystemEvent event, UINT_PTR wparam, UINT_PTR lparam) override;

		public:
			virtual IVRgineersDevice* CreateDevice() override;

			virtual IVRgineersDevice* GetDevice() const override;
		};
	}
}