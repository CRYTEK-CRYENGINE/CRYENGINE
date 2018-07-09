// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "CrySchematyc/Utils/GUID.h"

namespace Schematyc
{
// Forward declare interfaces.
struct IRuntimeClass;

// Forward declare shared pointers.
DECLARE_SHARED_POINTERS(IRuntimeClass)

struct IRuntimeRegistry
{
	virtual ~IRuntimeRegistry() {}

	virtual IRuntimeClassConstPtr GetClass(const CryGUID& guid) const = 0;
};
} // Schematyc
