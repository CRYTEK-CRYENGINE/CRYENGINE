// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryExtension/CryGUID.h>
#include "CrySchematyc/Utils/IString.h"

namespace Schematyc
{
	// Import CryGUID from global namespace so that Schematyc::CryGUID is equal to CryGUID
	using ::CryGUID;

namespace GUID
{
// Get empty GUID.
// SchematycTODO : Replace with constexpr global variable?
////////////////////////////////////////////////////////////////////////////////////////////////////
constexpr CryGUID Empty()
{
	return CryGUID(CryGUID::Null());
}

// Is GUID empty?
////////////////////////////////////////////////////////////////////////////////////////////////////
inline bool IsEmpty(const CryGUID& guid)
{
	return guid == Empty();
}

// Write GUID to string.
////////////////////////////////////////////////////////////////////////////////////////////////////
inline void ToString(IString& output, const CryGUID& guid)
{
	char buf[40];
	guid.ToString(buf);
	output.assign(buf);
}


} // GUID
} // Schematyc
