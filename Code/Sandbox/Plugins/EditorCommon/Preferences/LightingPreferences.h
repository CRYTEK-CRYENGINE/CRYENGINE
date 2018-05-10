// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include <EditorFramework/Preferences.h>

#include "EditorCommonAPI.h"

struct EDITOR_COMMON_API SLightingPreferences : public SPreferencePage
{
	SLightingPreferences();
	virtual bool Serialize(yasli::Archive& ar) override;

	bool bForceSkyUpdate;
	bool bTotalIlluminationEnabled;
};

EDITOR_COMMON_API extern SLightingPreferences gLightingPreferences;

