// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.

//
//	File: SystemCfg.h
//
//	History:
//	-Jan 22,2004:Created
//
//////////////////////////////////////////////////////////////////////

#pragma once

#include <math.h>
#include <map>

typedef string SysConfigKey;
typedef string SysConfigValue;

//////////////////////////////////////////////////////////////////////////
class CSystemConfiguration
{
public:
	CSystemConfiguration(const string& strSysConfigFilePath, CSystem* pSystem, ILoadConfigurationEntrySink* pSink, ELoadConfigurationType configType);
	~CSystemConfiguration();

	string RemoveWhiteSpaces(string& s)
	{
		s.Trim();
		return s;
	}

	bool        IsError() const { return m_bError; }

	static bool OpenFile(const string& filename, CCryFile& file, int flags);

private: // ----------------------------------------

	// Returns:
	//   success
	bool ParseSystemConfig();

	CSystem*                     m_pSystem;
	string                       m_strSysConfigFilePath;
	bool                         m_bError;
	ILoadConfigurationEntrySink* m_pSink;                       // never 0
	ELoadConfigurationType       m_configType;
};
