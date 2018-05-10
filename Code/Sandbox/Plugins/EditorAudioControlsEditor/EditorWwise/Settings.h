// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <ISettings.h>

#include <CrySystem/File/CryFile.h>

namespace ACE
{
namespace Impl
{
namespace Wwise
{
class CSettings final : public ISettings
{
public:

	CSettings();

	// ISettings
	virtual char const* GetAssetsPath() const override    { return m_assetsPath.c_str(); }
	virtual char const* GetProjectPath() const override   { return m_projectPath.c_str(); }
	virtual void        SetProjectPath(char const* const szPath) override;
	virtual bool        SupportsProjects() const override { return true; }
	// ~ISettings

	void Serialize(Serialization::IArchive& ar);

private:

	string            m_projectPath;
	string const      m_assetsPath;
	char const* const m_szUserSettingsFile;
};
} // namespace Wwise
} // namespace Impl
} // namespace ACE
