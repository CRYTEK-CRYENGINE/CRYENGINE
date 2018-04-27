// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySystem/ICryPlugin.h>

#include "IHmdOSVRDevice.h"

namespace CryVR
{
namespace Osvr {
struct IOsvrPlugin : public Cry::IEnginePlugin
{
	CRYINTERFACE_DECLARE_GUID(IOsvrPlugin, "47512e92-6a94-404d-a2c9-a14f0633eebe"_cry_guid);

public:
	virtual IOsvrDevice* CreateDevice() = 0;
	virtual IOsvrDevice* GetDevice() const = 0;
};

}      // namespace Osvr
}      // namespace CryVR