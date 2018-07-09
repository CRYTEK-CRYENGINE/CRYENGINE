// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "CrySchematyc/Env/IEnvElement.h"
#include "CrySchematyc/Utils/Assert.h"
#include "CrySchematyc/Utils/Delegate.h"

namespace Schematyc
{
namespace EnvUtils
{

// #SchematycTODO : Do we still need IEnvElement::VisitChildren?
inline void VisitChildren(const IEnvElement& envElement, const std::function<void(const IEnvElement&)>& visitor)
{
	SCHEMATYC_CORE_ASSERT(visitor);
	if (visitor)
	{
		for (const IEnvElement* pEnvChildElement = envElement.GetFirstChild(); pEnvChildElement; pEnvChildElement = pEnvChildElement->GetNextSibling())
		{
			visitor(*pEnvChildElement);
		}
	}
}

template<typename TYPE> inline void VisitChildren(const IEnvElement& envElement, const std::function<void(const TYPE&)>& visitor)
{
	SCHEMATYC_CORE_ASSERT(visitor);
	if (visitor)
	{
		for (const IEnvElement* pEnvChildElement = envElement.GetFirstChild(); pEnvChildElement; pEnvChildElement = pEnvChildElement->GetNextSibling())
		{
			const TYPE* pEnvChildElementImpl = DynamicCast<TYPE>(pEnvChildElement);
			if (pEnvChildElementImpl)
			{
				visitor(*pEnvChildElementImpl);
			}
		}
	}
}

} // EnvUtils
} // Schematyc
