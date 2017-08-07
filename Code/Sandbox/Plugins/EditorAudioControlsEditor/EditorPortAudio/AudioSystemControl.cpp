// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "StdAfx.h"
#include "AudioSystemControl.h"

namespace ACE
{
IAudioSystemControl::IAudioSystemControl(const string& name, CID id, ItemType type)
	: IAudioSystemItem(name, id, type)
{
}
}
