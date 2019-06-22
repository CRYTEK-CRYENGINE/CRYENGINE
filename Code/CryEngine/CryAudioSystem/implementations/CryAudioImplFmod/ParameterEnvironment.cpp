// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "ParameterEnvironment.h"
#include "Common.h"
#include "Object.h"

namespace CryAudio
{
namespace Impl
{
namespace Fmod
{
//////////////////////////////////////////////////////////////////////////
CParameterEnvironment::~CParameterEnvironment()
{
	for (auto const pObject : g_constructedObjects)
	{
		pObject->RemoveParameter(m_parameterInfo);
	}
}

//////////////////////////////////////////////////////////////////////////
void CParameterEnvironment::Set(IObject* const pIObject, float const amount)
{
	auto const pObject = static_cast<CObject*>(pIObject);
	pObject->SetParameter(m_parameterInfo, m_multiplier * amount + m_shift);
}
} // namespace Fmod
} // namespace Impl
} // namespace CryAudio