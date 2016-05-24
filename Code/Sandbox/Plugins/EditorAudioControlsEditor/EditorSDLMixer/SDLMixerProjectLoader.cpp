// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "SDLMixerProjectLoader.h"
#include <CrySystem/File/CryFile.h>
#include <CrySystem/ISystem.h>
#include <CryString/CryPath.h>
#include <IAudioSystemEditor.h>
#include <IAudioSystemItem.h>
#include "AudioSystemControl_sdlmixer.h"
#include "AudioSystemEditor_sdlmixer.h"

using namespace PathUtil;

namespace ACE
{
CSdlMixerProjectLoader::CSdlMixerProjectLoader(const string& sAssetsPath, CAudioSystemEditor_sdlmixer* pAudioSystemImpl)
	: m_pAudioSystemImpl(pAudioSystemImpl)
{
	_finddata_t fd;
	ICryPak* pCryPak = gEnv->pCryPak;
	intptr_t handle = pCryPak->FindFirst(sAssetsPath + CRY_NATIVE_PATH_SEPSTR "*.*", &fd);
	if (handle != -1)
	{
		do
		{
			const string name = fd.name;
			if (name != "." && name != ".." && !name.empty())
			{
				if (name.find(".wav") != string::npos ||
				    name.find(".ogg") != string::npos ||
				    name.find(".mp3") != string::npos)
				{
					// Create the event with the same name as the file
					m_pAudioSystemImpl->CreateControl(SControlDef(name, eSdlMixerTypes_Event));
				}
			}
		}
		while (pCryPak->FindNext(handle, &fd) >= 0);
		pCryPak->FindClose(handle);
	}
}
}
