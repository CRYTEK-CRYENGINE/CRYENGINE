// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

// #SchematycTODO : Should we be using guids to identify settings?

#pragma once

#include <CrySerialization/Forward.h>

#include "CrySchematyc/FundamentalTypes.h"
#include "CrySchematyc/Utils/Delegate.h"

namespace Schematyc
{
struct ISettings
{
	virtual ~ISettings() {}

	virtual void Serialize(Serialization::IArchive& archive) = 0;
};

DECLARE_SHARED_POINTERS(ISettings)

typedef std::function<EVisitStatus (const char*, const ISettingsPtr&)> SettingsVisitor;

struct ISettingsManager
{
	virtual ~ISettingsManager() {}

	virtual bool       RegisterSettings(const char* szName, const ISettingsPtr& pSettings) = 0;
	virtual ISettings* GetSettings(const char* szName) const = 0;
	virtual void       VisitSettings(const SettingsVisitor& visitor) const = 0;
	virtual void       LoadAllSettings() = 0;
	virtual void       SaveAllSettings() = 0;
};
} // Schematyc
