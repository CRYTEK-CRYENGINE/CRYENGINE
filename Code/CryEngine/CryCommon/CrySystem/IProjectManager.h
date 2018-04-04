#pragma once

// Interface for CrySystem and Editor to initialize game projects
struct IProjectManager
{
	virtual ~IProjectManager() {}

	//! Gets the name of the game, used for the window title on PC
	virtual const char* GetCurrentProjectName() = 0;

	//! \return Path without trailing separator.
	virtual const char* GetCurrentProjectDirectoryAbsolute() = 0;

	virtual const char* GetCurrentAssetDirectoryRelative() = 0;
	virtual const char* GetCurrentAssetDirectoryAbsolute() = 0;

	//! Adds or updates the value of a CVar in the project configuration
	virtual void        StoreConsoleVariable(const char* szCVarName, const char* szValue) = 0;

	//! Saves the .cryproject file with new values from StoreConsoleVariable
	virtual void        SaveProjectChanges() = 0;
};