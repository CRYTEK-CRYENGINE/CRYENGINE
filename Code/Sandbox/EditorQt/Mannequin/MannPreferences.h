// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include <EditorFramework/Preferences.h>

struct SMannequinGeneralPreferences : public SPreferencePage
{
	SMannequinGeneralPreferences();
	virtual bool Serialize(yasli::Archive& ar) override;

	string defaultPreviewFile;
	int    trackSize;
	float  timelineWheelZoomSpeed;
	bool   bCtrlForScrubSnapping;
};

extern SMannequinGeneralPreferences gMannequinPreferences;

