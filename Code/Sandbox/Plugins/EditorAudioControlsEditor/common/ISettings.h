// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

namespace ACE
{
namespace Impl
{
struct ISettings
{
	//! \cond INTERNAL
	virtual ~ISettings() = default;
	//! \endcond

	//! Returns path to the folder that contains soundbanks and/or audio files.
	virtual char const* GetAssetsPath() const = 0;

	//! Returns path to the folder that contains the middleware project.
	//! If the selected middleware doesn't support projects, the asset path is returned.
	virtual char const* GetProjectPath() const = 0;

	//! Sets the path to the middleware project.
	//! \param szPath - Folder path to the middleware project.
	virtual void SetProjectPath(char const* const szPath) = 0;

	//! Checks if the selected middleware supports projects or not.
	virtual bool SupportsProjects() const = 0;
};
} // namespace Impl
} // namespace ACE
