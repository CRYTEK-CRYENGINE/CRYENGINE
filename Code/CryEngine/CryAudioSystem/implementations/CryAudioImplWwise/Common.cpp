// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "Common.h"

#include "Impl.h"

#if defined(CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE)
	#include "EventInstance.h"
	#include "Object.h"
#endif // CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE

namespace CryAudio
{
namespace Impl
{
namespace Wwise
{
CImpl* g_pImpl = nullptr;

uint32 g_numObjectsWithRelativeVelocity = 0;

#if defined(CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE)
CryCriticalSection g_cs;
std::unordered_map<AkPlayingID, CEventInstance*> g_playingIds;
std::unordered_map<AkGameObjectID, CObject*> g_gameObjectIds;
States g_debugStates;
#endif // CRY_AUDIO_IMPL_WWISE_USE_DEBUG_CODE
}      // namespace Wwise
}      // namespace Impl
}      // namespace CryAudio
