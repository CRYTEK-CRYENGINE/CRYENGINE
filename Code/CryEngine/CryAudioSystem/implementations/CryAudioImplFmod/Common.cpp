// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "Common.h"
#include "ParameterInfo.h"

namespace CryAudio
{
namespace Impl
{
namespace Fmod
{
FMOD::System* g_pCoreSystem = nullptr;
FMOD::Studio::System* g_pStudioSystem = nullptr;

CImpl* g_pImpl = nullptr;
uint32 g_numObjectsWithDoppler = 0;
bool g_masterBusPaused = false;
Objects g_constructedObjects;
EventToParameterIndexes g_eventToParameterIndexes;
SnapshotEventInstances g_activeSnapshots;

CParameterInfo g_occlusionParameterInfo(g_szOcclusionParameterName);
CParameterInfo g_absoluteVelocityParameterInfo(g_szAbsoluteVelocityParameterName);

#if defined(CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE)
ActiveSnapshots g_activeSnapshotNames;
VcaValues g_vcaValues;
#endif // CRY_AUDIO_IMPL_FMOD_USE_DEBUG_CODE
}      // namespace Fmod
}      // namespace Impl
}      // namespace CryAudio