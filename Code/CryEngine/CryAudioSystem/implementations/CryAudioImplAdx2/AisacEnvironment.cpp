// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "AisacEnvironment.h"
#include "Object.h"

namespace CryAudio
{
namespace Impl
{
namespace Adx2
{
//////////////////////////////////////////////////////////////////////////
void CAisacEnvironment::Set(IObject* const pIObject, float const amount)
{
	auto const pObject = static_cast<CObject const*>(pIObject);

	CriAtomExPlayerHn const pPlayer = pObject->GetPlayer();
	criAtomExPlayer_SetAisacControlById(pPlayer, m_id, static_cast<CriFloat32>(m_multiplier * amount + m_shift));
	criAtomExPlayer_UpdateAll(pPlayer);
}
} // namespace Adx2
} // namespace Impl
} // namespace CryAudio
