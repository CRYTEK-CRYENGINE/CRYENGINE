// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "CrySchematyc2/Prerequisites.h"

#include "CrySchematyc2/GUID.h"

namespace Schematyc2
{
	enum class EEnvElementType
	{
		Function
	};

	struct IEnvElement
	{
		virtual ~IEnvElement() {}

		virtual EEnvElementType GetElementType() const = 0;
		virtual SGUID GetGUID() const = 0;
	};
}
