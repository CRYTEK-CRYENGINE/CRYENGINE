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