// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySerialization/Forward.h>
#include <CrySchematyc2/Schematyc_AggregateTypeId.h>

namespace Schematyc2
{
	bool Serialize(Serialization::IArchive& archive, CAggregateTypeId& value, const char* szName, const char* szLabel);
	bool PatchAggregateTypeIdFromDocVariableTypeInfo(Serialization::IArchive& archive, CAggregateTypeId& value, const char* szName);
}