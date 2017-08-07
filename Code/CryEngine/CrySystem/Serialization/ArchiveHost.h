// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySerialization/IArchiveHost.h>

namespace Serialization
{

IArchiveHost* CreateArchiveHost();
void RegisterArchiveHostCVars();
}
