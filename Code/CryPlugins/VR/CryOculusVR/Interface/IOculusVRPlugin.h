// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySystem/ICryPlugin.h>

#include "IHmdOculusRiftDevice.h"

namespace CryVR
{
namespace Oculus {
struct IOculusVRPlugin : public Cry::IEnginePlugin
{
	CRYINTERFACE_DECLARE_GUID(IOculusVRPlugin, "e4b6593f-30ce-4f80-9348-285f842e11c1"_cry_guid);

public:
	virtual IOculusDevice* CreateDevice() = 0;
	virtual IOculusDevice* GetDevice() const = 0;
};

}      // namespace Oculus
}      // namespace CryVR