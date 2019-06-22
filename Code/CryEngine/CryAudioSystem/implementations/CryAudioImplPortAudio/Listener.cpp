// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "Listener.h"

namespace CryAudio
{
namespace Impl
{
namespace PortAudio
{
//////////////////////////////////////////////////////////////////////////
void CListener::SetName(char const* const szName)
{
#if defined(CRY_AUDIO_IMPL_PORTAUDIO_USE_DEBUG_CODE)
	m_name = szName;
#endif  // CRY_AUDIO_IMPL_PORTAUDIO_USE_DEBUG_CODE
}
} // namespace PortAudio
} // namespace Impl
} // namespace CryAudio
