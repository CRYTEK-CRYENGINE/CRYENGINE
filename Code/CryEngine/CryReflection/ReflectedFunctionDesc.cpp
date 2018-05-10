// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "ReflectedFunctionDesc.h"

#include "ReflectedTypeDesc.h"

namespace Cry {
namespace Reflection {

const ITypeDesc* CReflectedFunctionDesc::GetObjectTypeDesc() const
{
	return static_cast<const ITypeDesc*>(m_pObjectType);
}

} // ~Reflection namespace
} // ~Cry namespace
