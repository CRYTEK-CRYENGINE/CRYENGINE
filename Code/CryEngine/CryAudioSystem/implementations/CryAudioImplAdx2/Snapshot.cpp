// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "Snapshot.h"

#if defined(CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE)
	#include "Common.h"
#endif  // CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE

namespace CryAudio
{
namespace Impl
{
namespace Adx2
{
#if defined(CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE)
CryFixedStringT<MaxControlNameLength> g_debugActiveSnapShotName = g_debugNoneSnapshot;
#endif  // CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE

//////////////////////////////////////////////////////////////////////////
ETriggerResult CSnapshot::Execute(IObject* const pIObject, TriggerInstanceId const triggerInstanceId)
{
	switch (m_actionType)
	{
	case EActionType::Start:
		{
			criAtomEx_ApplyDspBusSnapshot(static_cast<CriChar8 const*>(m_name.c_str()), m_changeoverTime);

#if defined(CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE)
			g_debugActiveSnapShotName = m_name;
#endif      // CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE

			break;
		}
	case EActionType::Stop:
		{
			Stop(nullptr);

			break;
		}
	default:
		{
			break;
		}
	}

	return ETriggerResult::DoNotTrack;
}

//////////////////////////////////////////////////////////////////////////
void CSnapshot::Stop(IObject* const pIObject)
{
	criAtomEx_ApplyDspBusSnapshot(nullptr, m_changeoverTime);

#if defined(CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE)
	g_debugActiveSnapShotName = g_debugNoneSnapshot;
#endif        // CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE
}
} // namespace Adx2
} // namespace Impl
} // namespace CryAudio
