// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "AnimEventPlayer.h"

#include <CryCore/CryCustomTypes.h>

// A list of all anim events that are exposed by this player
static SCustomAnimEventType g_sdkEvents[] = {
 	{ "swimmingStroke", 0, "Swimming strokes that cause ripples on the water surface." },
	};

const SCustomAnimEventType* AnimEventPlayer_CRYENGINE_SDK::GetCustomType(int customTypeIndex) const
{
	const int count = GetCustomTypeCount();
	if (customTypeIndex < 0 || customTypeIndex >= count)
		return 0;
	return &g_sdkEvents[customTypeIndex];
}

int AnimEventPlayer_CRYENGINE_SDK::GetCustomTypeCount() const
{
	return CRY_ARRAY_COUNT(g_sdkEvents);
}

bool AnimEventPlayer_CRYENGINE_SDK::Play(ICharacterInstance* character, const AnimEventInstance& event)
{
	return false;
}

CRYREGISTER_CLASS(AnimEventPlayer_CRYENGINE_SDK)
