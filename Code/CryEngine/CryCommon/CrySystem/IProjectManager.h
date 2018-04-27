// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryExtension/CryGUID.h>
#include "ICryPluginManager.h"

//! Main interface used to manage the currently run project (known by the .cryproject extension).
struct IProjectManager
{
	virtual ~IProjectManager() {}

	//! Gets the human readable name of the game, for example used for updating the window title on desktop
	virtual const char*  GetCurrentProjectName() const = 0;
	//! Gets the globally unique identifier for this project, used to uniquely identify certain assets with projects
	virtual CryGUID      GetCurrentProjectGUID() const = 0;

	//! Gets the absolute path to the root of the project directory, where the .cryproject resides.
	//! \return Path without trailing separator.
	virtual const char*  GetCurrentProjectDirectoryAbsolute() const = 0;

	//! Gets the path to the assets directory, relative to project root
	virtual const char*  GetCurrentAssetDirectoryRelative() const = 0;
	//! Gets the absolute path to the asset directory
	virtual const char*  GetCurrentAssetDirectoryAbsolute() const = 0;

	virtual const char*  GetProjectFilePath() const = 0;

	//! Adds or updates the value of a CVar in the project configuration
	virtual void         StoreConsoleVariable(const char* szCVarName, const char* szValue) = 0;

	//! Saves the .cryproject file with new values from StoreConsoleVariable
	virtual void         SaveProjectChanges() = 0;

	//! Gets the number of plug-ins for the current project
	virtual const uint16 GetPluginCount() const = 0;
	//! Gets details on a specific plug-in by index
	//! \see GetPluginCount
	virtual void         GetPluginInfo(uint16 index, Cry::IPluginManager::EType& typeOut, string& pathOut, DynArray<EPlatform>& platformsOut) const = 0;
	//! Loads a project template, allowing substition of aliases prefixed by '$' in the provided lambda
	virtual string       LoadTemplateFile(const char* szPath, std::function<string(const char* szAlias)> aliasReplacementFunc) const = 0;
};
