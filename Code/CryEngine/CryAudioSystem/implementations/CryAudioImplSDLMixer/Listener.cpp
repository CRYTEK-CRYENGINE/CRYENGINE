// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "stdafx.h"
#include "Listener.h"

namespace CryAudio
{
namespace Impl
{
namespace SDL_mixer
{
//////////////////////////////////////////////////////////////////////////
void CListener::SetName(char const* const szName)
{
#if defined(CRY_AUDIO_IMPL_SDLMIXER_USE_DEBUG_CODE)
	m_name = szName;
#endif  // CRY_AUDIO_IMPL_SDLMIXER_USE_DEBUG_CODE
}
} // namespace SDL_mixer
} // namespace Impl
} // namespace CryAudio
