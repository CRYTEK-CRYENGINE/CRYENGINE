// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <ISettings.h>

#include <CrySystem/File/CryFile.h>

namespace ACE
{
namespace Impl
{
namespace SDLMixer
{
class CSettings final : public ISettings
{
public:

	CSettings();

	// ISettings
	virtual char const* GetAssetsPath() const override                    { return m_assetAndProjectPath.c_str(); }
	virtual char const* GetProjectPath() const override                   { return m_assetAndProjectPath.c_str(); }
	virtual void        SetProjectPath(char const* const szPath) override {}
	virtual bool        SupportsProjects() const override                 { return false; }
	// ~ISettings

private:

	string const m_assetAndProjectPath;
};
} // namespace SDLMixer
} // namespace Impl
} // namespace ACE
