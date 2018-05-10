// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "CryExtension\CryGUID.h"

namespace CryGUIDHelper
{

// To prevent collisions the implementation is a clone of the CryGUID::Create() method from Code/CryEngine/CryCommon/CryExtension/CryGUID.h.
// The CryGUID::Create() can't be used in tools since it refers to engine's gEnv->pSystem.
CryGUID Create();

}
