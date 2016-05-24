// Copyright 2001-2016 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <IAudioConnection.h>
#include <IAudioSystemItem.h>

namespace ACE
{
enum EWwiseItemTypes
{
	eWwiseItemTypes_Invalid     = 0,
	eWwiseItemTypes_Event       = BIT(0),
	eWwiseItemTypes_Rtpc        = BIT(1),
	eWwiseItemTypes_Switch      = BIT(2),
	eWwiseItemTypes_AuxBus      = BIT(3),
	eWwiseItemTypes_SoundBank   = BIT(4),
	eWwiseItemTypes_State       = BIT(5),
	eWwiseItemTypes_SwitchGroup = BIT(6),
	eWwiseItemTypes_StateGroup  = BIT(7),
};

class IAudioSystemControl_wwise final : public IAudioSystemItem
{
public:
	IAudioSystemControl_wwise() {}
	IAudioSystemControl_wwise(const string& name, CID id, ItemType type);
	virtual ~IAudioSystemControl_wwise() {}
};
}
