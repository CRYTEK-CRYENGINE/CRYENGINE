#pragma once

#include <CrySystem/ICryPlugin.h>

#include "IVRgineersDevice.h"

namespace CryVR
{
	namespace VRgineers
	{
		struct IVRgineersPlugin : public Cry::IEnginePlugin
		{
			CRYINTERFACE_DECLARE_GUID(IVRgineersPlugin, "F9241DD1-60F5-4CF5-AE8F-D33E8A69D851"_cry_guid);

		public:
			virtual IVRgineersDevice* CreateDevice() = 0;

			virtual IVRgineersDevice* GetDevice() const = 0;
		};

	}      // namespace OpenVR
}      // namespace CryVR