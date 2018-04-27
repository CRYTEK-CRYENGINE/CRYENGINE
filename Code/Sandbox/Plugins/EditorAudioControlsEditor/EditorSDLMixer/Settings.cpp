// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Settings.h"

#include <CryAudio/IAudioSystem.h>
#include <CryAudioImplSDLMixer/GlobalData.h>

namespace ACE
{
namespace Impl
{
namespace SDLMixer
{
//////////////////////////////////////////////////////////////////////////
CSettings::CSettings()
	: m_assetAndProjectPath(AUDIO_SYSTEM_DATA_ROOT "/" +
	                        string(CryAudio::Impl::SDL_mixer::s_szImplFolderName) +
	                        "/"
	                        + string(CryAudio::s_szAssetsFolderName))
{}
} // namespace SDLMixer
} // namespace Impl
} // namespace ACE
