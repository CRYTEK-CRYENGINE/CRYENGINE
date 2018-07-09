// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#pragma once

#include <CrySchematyc/Services/ISettingsManager.h>

namespace Schematyc
{
	class CSettingsManager : public ISettingsManager
	{
	private:

		typedef std::map<string, ISettingsPtr> Settings;

	public:

		// ISettingsManager
		virtual bool RegisterSettings(const char* szName, const ISettingsPtr& pSettings) override;
		virtual ISettings* GetSettings(const char* szName) const override;
		virtual void VisitSettings(const SettingsVisitor& visitor) const override;
		virtual void LoadAllSettings() override;
		virtual void SaveAllSettings() override;
		// ~ISettingsManager

	private:

		Settings m_settings;
	};
}
