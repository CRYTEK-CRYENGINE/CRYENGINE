// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "IEditor.h"

class EDITOR_COMMON_API LevelEditorSharedState : public IAutoEditorNotifyListener
{
public:
	enum eDisplayInfoLevel
	{
		eDisplayInfoLevel_Low  = 0,
		eDisplayInfoLevel_Med  = 1,
		eDisplayInfoLevel_High = 2
	};

	LevelEditorSharedState();

	bool              IsDisplayInfoEnabled() const;
	eDisplayInfoLevel GetDisplayInfoLevel() const;

	void              ToggleDisplayInfo();
	void              SetDisplayInfoLevel(eDisplayInfoLevel level);

	CCrySignal<void()> showDisplayInfoChanged;
	CCrySignal<void()> displayInfoLevelChanged;

private:
	void OnEditorNotifyEvent(EEditorNotifyEvent aEventId);
	void InitActions();
	void ResetState();
	void SetDisplayInfoCVar();
	void SaveState();

	// Wrappers upon r_displayInfo
	bool              m_showDisplayInfo;
	eDisplayInfoLevel m_displayInfoLevel;
};
