// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "CrySchematyc/Utils/GUID.h"

namespace Schematyc
{

class CCommonFunctionDesc
{
public:

private:

	CryGUID       m_guid;
	const char* m_szName = nullptr;
};

template<typename TYPE, TYPE FUNCTION_PTR> class CFunctionDesc : public CCommonFunctionDesc
{
};

} // Schematyc