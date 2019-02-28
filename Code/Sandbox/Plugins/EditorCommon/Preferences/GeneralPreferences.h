// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#pragma once

#include <EditorFramework/Preferences.h>
#include "EditorCommonAPI.h"

//////////////////////////////////////////////////////////////////////////
// General Preferences
//////////////////////////////////////////////////////////////////////////
struct SEditorGeneralPreferences : public SPreferencePage
{
	SEditorGeneralPreferences();
	virtual bool Serialize(yasli::Archive& ar) override;

	bool showWindowsInTaskbar;

	ADD_PREFERENCE_PAGE_PROPERTY(bool, enableSourceControl, setEnableSourceControl)
	ADD_PREFERENCE_PAGE_PROPERTY(bool, saveOnlyModified, setSaveOnlyModified)
	ADD_PREFERENCE_PAGE_PROPERTY(bool, freezeReadOnly, setFreezeReadOnly)
	ADD_PREFERENCE_PAGE_PROPERTY(bool, showTimeInConsole, setShowTimeInConsole)
};

//////////////////////////////////////////////////////////////////////////
// File Preferences
//////////////////////////////////////////////////////////////////////////
struct SEditorFilePreferences : public SPreferencePage
{
	SEditorFilePreferences();
	virtual bool Serialize(yasli::Archive& ar) override;

	bool   openCSharpSolution;
	string textEditorCSharp;
	string textEditorScript;
	string textEditorShaders;
	string textEditorBspaces;
	string textureEditor;
	string animEditor;
	string strStandardTempDirectory;

	int    autoSaveMaxCount;
	bool   autoSaveEnabled;
	bool   filesBackup;

	ADD_PREFERENCE_PAGE_PROPERTY(int, autoSaveTime, setAutoSaveTime)
};

EDITOR_COMMON_API extern SEditorGeneralPreferences gEditorGeneralPreferences;
EDITOR_COMMON_API extern SEditorFilePreferences gEditorFilePreferences;

