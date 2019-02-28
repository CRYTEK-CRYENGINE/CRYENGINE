// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "CrySchematyc/FundamentalTypes.h"

namespace Schematyc
{

struct IQuickSearchOptions
{
	virtual ~IQuickSearchOptions() {}

	virtual uint32      GetCount() const = 0;
	virtual const char* GetLabel(uint32 optionIdx) const = 0;
	virtual const char* GetDescription(uint32 optionIdx) const = 0;
	virtual const char* GetHeader() const = 0;
	virtual const char* GetDelimiter() const = 0;
};

} // Schematyc
