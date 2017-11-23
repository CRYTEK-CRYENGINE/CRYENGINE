// Copyright 2001-2017 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CryExtension/CryGUID.h>

// Interface for CrySystem and Editor to initialize game projects
struct IProjectManager
{
	virtual ~IProjectManager() {}

	//! Gets the name of the game, used for the window title on PC
	virtual const char* GetCurrentProjectName() const = 0;
	virtual CryGUID     GetCurrentProjectGUID() const = 0;

	//! \return Path without trailing separator.
	virtual const char* GetCurrentProjectDirectoryAbsolute() const = 0;

	virtual const char* GetCurrentAssetDirectoryRelative() const = 0;
	virtual const char* GetCurrentAssetDirectoryAbsolute() const = 0;

	virtual void        RegenerateCSharpSolution(const char* szDirectory) const = 0;

	//! Adds or updates the value of a CVar in the project configuration
	virtual void        StoreConsoleVariable(const char* szCVarName, const char* szValue) = 0;

	//! Saves the .cryproject file with new values from StoreConsoleVariable
	virtual void        SaveProjectChanges() = 0;
};