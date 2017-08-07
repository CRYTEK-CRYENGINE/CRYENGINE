// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <IAudioConnection.h>
#include <IAudioSystemItem.h>

namespace ACE
{
enum EPortAudioTypes
{
	ePortAudioTypes_Invalid = 0,
	ePortAudioTypes_Event   = 1,
	ePortAudioTypes_Folder  = 2,
};

class IAudioSystemControl final : public IAudioSystemItem
{
public:
	IAudioSystemControl() = default;
	IAudioSystemControl(const string& name, CID id, ItemType type);
	virtual ~IAudioSystemControl() {}
};
}
