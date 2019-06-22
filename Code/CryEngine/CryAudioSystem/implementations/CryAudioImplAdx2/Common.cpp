// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "Common.h"

namespace CryAudio
{
namespace Impl
{
namespace Adx2
{
CImpl* g_pImpl = nullptr;

Objects g_constructedObjects;
AcbHandles g_acbHandles;
CriAtomExPlayerConfig g_playerConfig;
CriAtomEx3dSourceConfig g_3dSourceConfig;

uint32 g_numObjectsWithDoppler = 0;

CriAtomExAisacControlId g_absoluteVelocityAisacId = CRIATOMEX_INVALID_AISAC_CONTROL_ID;
CriAtomExAisacControlId g_occlusionAisacId = CRIATOMEX_INVALID_AISAC_CONTROL_ID;

#if defined(CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE)
GameVariableValues g_gameVariableValues;
CategoryValues g_categoryValues;
#endif // CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE
}      // namespace Adx2
}      // namespace Impl
}      // namespace CryAudio