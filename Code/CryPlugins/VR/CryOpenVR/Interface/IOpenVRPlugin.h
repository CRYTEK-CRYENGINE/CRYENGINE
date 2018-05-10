// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySystem/ICryPlugin.h>

#include "IHmdOpenVRDevice.h"

namespace CryVR
{
namespace OpenVR {
struct IOpenVRPlugin : public Cry::IEnginePlugin
{
	CRYINTERFACE_DECLARE_GUID(IOpenVRPlugin, "cd1389a9-b375-47f9-bc45-d382d18b21b1"_cry_guid);

public:
	virtual IOpenVRDevice* CreateDevice() = 0;
	virtual IOpenVRDevice* GetDevice() const = 0;
};

}      // namespace OpenVR
}      // namespace CryVR