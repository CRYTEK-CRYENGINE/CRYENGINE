// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "CrySchematyc2/Schematyc_Prerequisites.h"

#include <CrySchematyc2/TemplateUtils/TemplateUtils_StringHashWrapper.h>
#include <CrySchematyc2/TemplateUtils/TemplateUtils_TypeUtils.h>

namespace Schematyc2
{
	typedef TemplateUtils::CStringHashWrapper<TemplateUtils::CFastStringHash, TemplateUtils::CEmptyStringConversion, TemplateUtils::CRawPtrStringStorage> EnvTypeId;

	template <typename TYPE> inline EnvTypeId GetEnvTypeId()
	{
		static const EnvTypeId envTypeId(TemplateUtils::GetTypeName<TYPE>());
		return envTypeId;
	}
}