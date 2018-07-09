// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

#include "StdAfx.h"
#include "SettingsManager.h"

#include <CrySerialization/IArchiveHost.h>
#include <CrySchematyc/Utils/Assert.h>
#include <CrySchematyc/Utils/StackString.h>

namespace Schematyc
{
	bool CSettingsManager::RegisterSettings(const char* szName, const ISettingsPtr& pSettings)
	{
		SCHEMATYC_CORE_ASSERT_FATAL(szName && szName[0] && pSettings);
		if(szName && szName[0] && pSettings)
		{
			// #SchematycTODO : Validate name and check for collision/overlap!
			return m_settings.insert(Settings::value_type(szName, pSettings)).second;
		}
		return false;
	}

	ISettings* CSettingsManager::GetSettings(const char* szName) const
	{
		Settings::const_iterator itSettings = m_settings.find(szName);
		return itSettings != m_settings.end() ? itSettings->second.get() : nullptr;
	}

	void CSettingsManager::VisitSettings(const SettingsVisitor& visitor) const
	{
		SCHEMATYC_CORE_ASSERT(visitor);
		if(visitor)
		{
			for(const Settings::value_type& settings : m_settings)
			{
				if(visitor(settings.first, settings.second) == EVisitStatus::Stop)
				{
					break;
				}
			}
		}
	}

	void CSettingsManager::LoadAllSettings()
	{
		const char* szSettingsFolder = gEnv->pSchematyc->GetSettingsFolder();
		for(const Settings::value_type& settings : m_settings)
		{
			const char*  szName = settings.first.c_str();
			CStackString fileName = szSettingsFolder;
			fileName.append("/");
			fileName.append(szName);
			fileName.append(".sc_settings");
			fileName.MakeLower();

			Serialization::LoadXmlFile(*settings.second, fileName);
		}
	}

	void CSettingsManager::SaveAllSettings()
	{
		const char* szSettingsFolder = gEnv->pSchematyc->GetSettingsFolder();
		for(Settings::value_type& settings : m_settings)
		{
			const char*  szName = settings.first.c_str();
			CStackString fileName = szSettingsFolder;
			fileName.append("/");
			fileName.append(szName);
			fileName.append(".sc_settings");
			fileName.MakeLower();
			
			Serialization::SaveXmlFile(fileName, *settings.second, szName);
		}
	}
}