// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySerialization/Forward.h>
#include <CrySchematyc/FundamentalTypes.h>
#include <CrySchematyc/Reflection/TypeDesc.h>

namespace Schematyc
{

// Forward declare interfaces.
struct IScriptStruct;
// Forward declare classes.
class CAnyValue;
// Forward declare shared pointers.
DECLARE_SHARED_POINTERS(CAnyValue)

// Script structure value.
////////////////////////////////////////////////////////////////////////////////////////////////////
class CScriptStructValue
{
private:

	typedef std::map<string, CAnyValuePtr> FieldMap;   // #SchematycTODO : Replace map with vector to preserve order!

public:

	CScriptStructValue(const IScriptStruct* pStruct);
	CScriptStructValue(const CScriptStructValue& rhs);

	void        Serialize(Serialization::IArchive& archive);

	static void ReflectType(CTypeDesc<CScriptStructValue>& desc);

private:

	void Refresh();

private:

	const IScriptStruct* m_pStruct;   // #SchematycTODO : Wouldn't it be safer to reference by GUID?
	FieldMap             m_fields;
};

} // Schematyc
