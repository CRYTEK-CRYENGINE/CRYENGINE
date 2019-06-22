// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "Return.h"
#include "Common.h"
#include "Object.h"

namespace CryAudio
{
namespace Impl
{
namespace Fmod
{
//////////////////////////////////////////////////////////////////////////
CReturn::~CReturn()
{
	for (auto const pObject : g_constructedObjects)
	{
		pObject->RemoveReturn(this);
	}
}

//////////////////////////////////////////////////////////////////////////
void CReturn::Set(IObject* const pIObject, float const amount)
{
	auto const pObject = static_cast<CObject*>(pIObject);
	pObject->SetReturn(this, amount);
}
} // namespace Fmod
} // namespace Impl
} // namespace CryAudio