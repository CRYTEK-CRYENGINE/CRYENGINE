// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include "ProjectManagement/UI/SelectProjectDialog.h"
#include <CrySerialization/Forward.h>

struct SSystemInitParams;

class CryIcon;
class QWidget;

// Searches for cryengine.cryengine from this EXE-folder, and moving higher
string FindCryEngineRootFolder();

// In the engineFolder, find system.cfg;
// Return true, if any line of this file starts with "sys_project"
bool IsProjectSpecifiedInSystemConfig(const string& engineFolder);

// Find first *.cryproject file in the folder (without search in subfolders)
string FindProjectInFolder(const string& folder);

// Ask user to select a project
string AskUserToSpecifyProject(QWidget* pParent, bool runOnSandboxInit, CSelectProjectDialog::Tab tabToShow);

// Unified way of adding project path to command line
void AppendProjectPathToCommandLine(const string& projectPath, SSystemInitParams& systemParams);

// Engine version description, taken from "cryengine.cryengine"
struct SCryEngineVersion
{
	SCryEngineVersion();
	void   Serialize(Serialization::IArchive& ar);
	int    GetVersionMajor() const;
	int    GetVersionMinor() const;
	int    GetBuild() const;
	string GetVersionShort() const; // "5.5", projects.json format

	// If exactMatch == true, also checks for id equivalence
	bool Equal(const SCryEngineVersion& other, bool exactMatch) const;

	bool IsValid() const;

	string id;      // "engine-5.5", "engine-dev"
	string name;    // "CRYENGINE 5.5",
	string version; // "5.5.0"

	// Calculated values
	int versionMajor;
	int versionMinor;
	int versionBuild;
};

SCryEngineVersion GetCurrentCryEngineVersion();

// User-independent storage folder for settings
string GetCryEngineProgramDataFolder();

// Default root folder for new project
QString GetDefaultProjectsRootFolder();

// Search order:
// 1. <ProjectFolder>\Thumbnail.png
// 2. Default, icon
CryIcon GetProjectThumbnail(const string& projectFolder, const char* szDefaultIconFile);
