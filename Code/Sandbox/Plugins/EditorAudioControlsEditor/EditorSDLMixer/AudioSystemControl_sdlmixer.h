// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <IAudioConnection.h>
#include <IAudioSystemItem.h>

namespace ACE
{
enum ESdlMixerTypes
{
	eSdlMixerTypes_Invalid    = 0,
	eSdlMixerTypes_Event      = 1,
	eSdlMixerTypes_SampleFile = 2,
};

class IAudioSystemControl_sdlmixer final : public IAudioSystemItem
{
public:
	IAudioSystemControl_sdlmixer() {}
	IAudioSystemControl_sdlmixer(const string& name, CID id, ItemType type);
	virtual ~IAudioSystemControl_sdlmixer() {}
};
}
