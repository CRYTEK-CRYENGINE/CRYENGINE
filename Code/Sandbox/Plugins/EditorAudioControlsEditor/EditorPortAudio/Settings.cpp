// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Settings.h"

#include <CryAudio/IAudioSystem.h>
#include <CryAudioImplPortAudio/GlobalData.h>

namespace ACE
{
namespace Impl
{
namespace PortAudio
{
//////////////////////////////////////////////////////////////////////////
CSettings::CSettings()
	: m_assetAndProjectPath(AUDIO_SYSTEM_DATA_ROOT "/" +
	                        string(CryAudio::Impl::PortAudio::s_szImplFolderName) +
	                        "/" +
	                        string(CryAudio::s_szAssetsFolderName))
{}
} // namespace PortAudio
} // namespace Impl
} // namespace ACE
