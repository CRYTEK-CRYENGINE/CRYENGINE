// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "DspBusSetting.h"

#include <cri_atom_ex.h>

namespace CryAudio
{
namespace Impl
{
namespace Adx2
{
#if defined(CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE)
CryFixedStringT<MaxControlNameLength> g_debugCurrentDspBusSettingName = g_debugNoneDspBusSetting;
#endif  // CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE

//////////////////////////////////////////////////////////////////////////
void CDspBusSetting::Load()
{
	criAtomEx_AttachDspBusSetting(static_cast<CriChar8 const*>(m_name.c_str()), nullptr, 0);

#if defined(CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE)
	g_debugCurrentDspBusSettingName = m_name;
#endif  // CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE
}

//////////////////////////////////////////////////////////////////////////
void CDspBusSetting::Unload()
{
	criAtomEx_DetachDspBusSetting();

#if defined(CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE)
	g_debugCurrentDspBusSettingName = g_debugNoneDspBusSetting;
#endif  // CRY_AUDIO_IMPL_ADX2_USE_DEBUG_CODE
}
} // namespace Adx2
} // namespace Impl
} // namespace CryAudio
