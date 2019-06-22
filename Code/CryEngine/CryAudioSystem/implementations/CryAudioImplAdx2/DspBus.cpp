// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "DspBus.h"
#include "Object.h"

#include <cri_atom_ex.h>

namespace CryAudio
{
namespace Impl
{
namespace Adx2
{
//////////////////////////////////////////////////////////////////////////
void CDspBus::Set(IObject* const pIObject, float const amount)
{
	auto const pObject = static_cast<CObject const*>(pIObject);

	CriAtomExPlayerHn const pPlayer = pObject->GetPlayer();
	criAtomExPlayer_SetBusSendLevelByName(pPlayer, static_cast<CriChar8 const*>(m_name.c_str()), static_cast<CriFloat32>(amount));
	criAtomExPlayer_UpdateAll(pPlayer);
}
} // namespace Adx2
} // namespace Impl
} // namespace CryAudio
