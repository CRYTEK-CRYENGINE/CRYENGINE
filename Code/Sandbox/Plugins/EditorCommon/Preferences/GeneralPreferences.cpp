// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
#include <StdAfx.h>

#include "GeneralPreferences.h"

#include <CrySerialization/yasli/decorators/Range.h>
#include <CrySerialization/Color.h>

EDITOR_COMMON_API SEditorGeneralPreferences gEditorGeneralPreferences;
REGISTER_PREFERENCES_PAGE_PTR(SEditorGeneralPreferences, &gEditorGeneralPreferences)

EDITOR_COMMON_API SEditorFilePreferences gEditorFilePreferences;
REGISTER_PREFERENCES_PAGE_PTR(SEditorFilePreferences, &gEditorFilePreferences)

//////////////////////////////////////////////////////////////////////////
// General Preferences
//////////////////////////////////////////////////////////////////////////
SEditorGeneralPreferences::SEditorGeneralPreferences()
	: SPreferencePage("General", "General/General")
	, m_enableSourceControl(false)
	, m_saveOnlyModified(true)
	, m_freezeReadOnly(true)
	, m_showTimeInConsole(false)
	, showWindowsInTaskbar(true)
{
}

bool SEditorGeneralPreferences::Serialize(yasli::Archive& ar)
{
	// General
	ar.openBlock("generalSettings", "General");
	ar(m_enableSourceControl, "enableSourceControl", "Enable Source Control");
	ar(m_saveOnlyModified, "saveOnlyModified", "External layers: Save only Modified");
	ar(m_freezeReadOnly, "freezeReadOnly", "Freeze Read-only external layer on Load");
	ar(showWindowsInTaskbar, "showWindowsInTaskbar", "Show all windows in taskbar (requires restart)");
	ar.closeBlock();

	ar.openBlock("Console", "Console");
	ar(m_showTimeInConsole, "showTimeInConsole", "Show Time In Console");
	ar.closeBlock();

	return true;
}

//////////////////////////////////////////////////////////////////////////
// File Preferences
//////////////////////////////////////////////////////////////////////////
SEditorFilePreferences::SEditorFilePreferences()
	: SPreferencePage("File", "General/File")
	, openCSharpSolution(true)
	, textEditorCSharp("devenv.exe")
	, textEditorScript("notepad++.exe")
	, textEditorShaders("notepad++.exe")
	, textEditorBspaces("notepad++.exe")
	, textureEditor("Photoshop.exe")
	, animEditor("")
	, strStandardTempDirectory("Temp")
	, m_autoSaveTime(10)
	, autoSaveMaxCount(3)
	, autoSaveEnabled(false)
	, filesBackup(true)
{
}

bool SEditorFilePreferences::Serialize(yasli::Archive& ar)
{
	auto prevAutoSavetime = m_autoSaveTime;
	ar.openBlock("files", "Files");
	ar(filesBackup, "filesBackup", "Backup on Save");
	ar.closeBlock();

	ar.openBlock("textEditors", "Text Editors");
	ar(openCSharpSolution, "openCSharSolution", "Open C# solution instead of files");
	ar(textEditorCSharp, "textEditorCSharp", "C# Text Editor");
	ar(textEditorScript, "textEditorScript", "Scripts Text Editor");
	ar(textEditorShaders, "textEditorShaders", "Shaders Text Editor");
	ar(textEditorBspaces, "textEditorBspaces", "BSpace Text Editor");
	ar(strStandardTempDirectory, "strStandardTempDirectory", "Standard Temporary Directory");
	ar(textureEditor, "textureEditor", "Texture Editor");
	ar(animEditor, "animEditor", "Animation Editor");
	ar.closeBlock();

	// Autobackup table.
	ar.openBlock("autoBackup", "Auto Back-up");
	ar(autoSaveEnabled, "autoSaveEnabled", "Enable");
	ar(yasli::Range(m_autoSaveTime, 2, 10000), "autoSaveTime", "Auto Backup Interval (Minutes)");
	ar(yasli::Range(autoSaveMaxCount, 1, 100), "autoSaveMaxCount", "Maximum Auto Backups");
	ar.closeBlock();

	if (ar.isInput() && prevAutoSavetime != m_autoSaveTime)
	{
		autoSaveTimeChanged();
	}

	return true;
}

