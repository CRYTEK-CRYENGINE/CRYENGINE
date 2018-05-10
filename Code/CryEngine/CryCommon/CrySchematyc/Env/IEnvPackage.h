// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "CrySchematyc/Utils/Delegate.h"
#include "CrySchematyc/Utils/GUID.h"

namespace Schematyc
{

// Forward declare interfaces.
struct IEnvRegistrar;

typedef std::function<void (IEnvRegistrar&)> EnvPackageCallback;

struct IEnvPackage
{
	virtual ~IEnvPackage() {}

	virtual CryGUID              GetGUID() const = 0;
	virtual const char*        GetName() const = 0;
	virtual const char*        GetAuthor() const = 0;
	virtual const char*        GetDescription() const = 0;
	virtual EnvPackageCallback GetCallback() const = 0;
};

} // Schematyc
