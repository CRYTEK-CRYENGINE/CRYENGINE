// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "Settings.h"

#include <CryAudio/IAudioSystem.h>
#include <CryAudioImplFmod/GlobalData.h>
#include <CrySerialization/IArchiveHost.h>

namespace ACE
{
namespace Impl
{
namespace Fmod
{
//////////////////////////////////////////////////////////////////////////
CSettings::CSettings()
	: m_projectPath(AUDIO_SYSTEM_DATA_ROOT "/fmod_project")
	, m_assetsPath(AUDIO_SYSTEM_DATA_ROOT "/" + string(CryAudio::Impl::Fmod::s_szImplFolderName) + "/" + string(CryAudio::s_szAssetsFolderName))
	, m_szUserSettingsFile("%USER%/audiocontrolseditor_fmod.user")
{
	Serialization::LoadJsonFile(*this, m_szUserSettingsFile);
}

//////////////////////////////////////////////////////////////////////////
void CSettings::SetProjectPath(char const* const szPath)
{
	m_projectPath = szPath;
	Serialization::SaveJsonFile(m_szUserSettingsFile, *this);
}

//////////////////////////////////////////////////////////////////////////
void CSettings::Serialize(Serialization::IArchive& ar)
{
	ar(m_projectPath, "projectPath", "Project Path");
}
} // namespace Fmod
} // namespace Impl
} // namespace ACE
