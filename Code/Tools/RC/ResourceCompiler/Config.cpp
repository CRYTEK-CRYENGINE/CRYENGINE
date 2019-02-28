// Copyright 2001-2018 Crytek GmbH / Crytek Group. All rights reserved.
//
//  Crytek Engine Source File.
//  Copyright (C), Crytek Studios, 2002.
// -------------------------------------------------------------------------
//  File name:   config.cpp
//  Version:     v1.00
//  Created:     4/11/2002 by Timur.
//  Compilers:   Visual Studio.NET
//  Description: 
// -------------------------------------------------------------------------
//  History:
//
////////////////////////////////////////////////////////////////////////////

#include "StdAfx.h"
#include "config.h"
#include "PropertyVars.h"
#include "StringHelpers.h"
#include "IRCLog.h"

//////////////////////////////////////////////////////////////////////////
Config::Config()
	: m_pConfigKeyRegistry(0)
{
}

//////////////////////////////////////////////////////////////////////////
Config::~Config()
{
}

//////////////////////////////////////////////////////////////////////////
void Config::GetUnknownKeys(std::vector<string>& unknownKeys) const
{
	unknownKeys.clear();

	if (!m_pConfigKeyRegistry)
	{
		return;
	}

	const Map::const_iterator end = m_map.end();

	for (Map::const_iterator it = m_map.begin(); it != end; ++it)
	{
		const char* const keyName = it->first.m_sKeyName.c_str();
		if (!m_pConfigKeyRegistry->HasKeyRegistered(keyName))
		{
			unknownKeys.push_back(keyName);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void Config::SetConfigKeyRegistry(IConfigKeyRegistry* pConfigKeyRegistry)
{
	m_pConfigKeyRegistry = pConfigKeyRegistry;
}

//////////////////////////////////////////////////////////////////////////
IConfigKeyRegistry* Config::GetConfigKeyRegistry() const
{
	return m_pConfigKeyRegistry;
}

//////////////////////////////////////////////////////////////////////////
int Config::GetSum(const char* key) const
{
	if (m_pConfigKeyRegistry)
	{
		m_pConfigKeyRegistry->VerifyKeyRegistration(key);
	}

	MapKey mapKey;
	mapKey.m_sKeyName = key;
	mapKey.m_eKeyPri = eCP_PriorityHighest;
	const Map::const_iterator lowerBound = m_map.lower_bound(mapKey);
	mapKey.m_eKeyPri = eCP_PriorityLowest;
	const Map::const_iterator upperBound = m_map.upper_bound(mapKey);

	int ret = 0;

	for (Map::const_iterator it = lowerBound; it != upperBound; ++it)
	{
		const MapKey& foundMapKey = it->first;

		if (!StringHelpers::EqualsIgnoreCase(foundMapKey.m_sKeyName, mapKey.m_sKeyName))
		{
			RCLogError("Unexpected failure in %s", __FUNCTION__);
			break;
		}

		int localvalue;
		if (sscanf (it->second.c_str(), "%d", &localvalue) == 1)
		{
			ret += localvalue;
		}
	}

	return ret;
}

//////////////////////////////////////////////////////////////////////////
bool Config::GetKeyValue(const char* const key, const char* & value, const int ePriMask) const
{
	if (m_pConfigKeyRegistry)
	{
		m_pConfigKeyRegistry->VerifyKeyRegistration(key);
	}

	MapKey mapKey;
	mapKey.m_sKeyName = key;
	mapKey.m_eKeyPri = eCP_PriorityHighest;
	const Map::const_iterator lowerBound = m_map.lower_bound(mapKey);
	mapKey.m_eKeyPri = eCP_PriorityLowest;
	const Map::const_iterator upperBound = m_map.upper_bound(mapKey);

	for (Map::const_iterator it = lowerBound; it != upperBound; ++it)
	{
		const MapKey& foundMapKey = it->first;

		if (ePriMask & foundMapKey.m_eKeyPri)
		{
			if (!StringHelpers::EqualsIgnoreCase(foundMapKey.m_sKeyName, mapKey.m_sKeyName))
			{
				RCLogError("Unexpected failure in %s", __FUNCTION__);
				break;
			}

			value = it->second.c_str();
			return true;
		}
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
void Config::AddConfig(const IConfig* inpConfig)
{
	if (!inpConfig)
	{
		return;
	}

	const Config* const pConfig = inpConfig->GetInternalRepresentation();

	if (!pConfig)
	{
		assert(0);
		return;
	}

	const Map::const_iterator end = pConfig->m_map.end();

	for (Map::const_iterator it = pConfig->m_map.begin(); it != end; ++it)
	{
		const MapKey& mapKey = it->first;
		const string& value = it->second;

		SetKeyValue(mapKey.m_eKeyPri, mapKey.m_sKeyName.c_str(), value.c_str());
	}
}

//////////////////////////////////////////////////////////////////////////
const Config* Config::GetInternalRepresentation() const
{
	return this;	
}

//////////////////////////////////////////////////////////////////////////
bool Config::HasKeyRegistered(const char* szKey) const
{
	assert(szKey);
	return m_pConfigKeyRegistry ? m_pConfigKeyRegistry->HasKeyRegistered(szKey) : false;
}

//////////////////////////////////////////////////////////////////////////
bool Config::HasKeyMatchingWildcards(const char* wildcards) const
{
	if (!wildcards || !wildcards[0])
	{
		return false;
	}

	const string strWildcards(wildcards);

	const Map::const_iterator end = m_map.end();
	for (Map::const_iterator it = m_map.begin(); it != end; ++it)
	{
		if (StringHelpers::MatchesWildcardsIgnoreCase(it->first.m_sKeyName, strWildcards))
		{
			return true;
		}
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
void Config::Clear()
{
	m_map.clear();
}

//////////////////////////////////////////////////////////////////////////
uint32 Config::ClearPriorityUsage(const int ePriMask)
{
	uint32 dwRet = 0;

	Map::const_iterator it = m_map.begin();

	while (it != m_map.end())
	{
		Map::const_iterator itNext = it;
		++itNext;

		const MapKey& mapKey = it->first;

		if (mapKey.m_eKeyPri & ePriMask)
		{
			m_map.erase(mapKey);
			++dwRet;
		}

		it = itNext;
	}

	return dwRet;
}

//////////////////////////////////////////////////////////////////////////
uint32 Config::CountPriorityUsage(const int ePriMask)	const
{
	uint32 dwRet = 0;

	const Map::const_iterator end = m_map.end();

	for (Map::const_iterator it = m_map.begin(); it != end; ++it)
	{
		const MapKey& mapKey = it->first;

		if (mapKey.m_eKeyPri == ePriMask)
		{
			++dwRet;
		}
	}

	return dwRet;
}

//////////////////////////////////////////////////////////////////////////
void Config::SetKeyValue(const EConfigPriority ePri, const char* key, const char* value)
{
	assert(Util::isPowerOfTwo(ePri));

	if (!key || !key[0])
	{
		return;
	}

	MapKey mapKey;
	mapKey.m_sKeyName = key;
	mapKey.m_eKeyPri = ePri;

	if (value == 0)
	{
		m_map.erase(mapKey);
	}
	else
	{
		m_map[mapKey] = value;
	}
}

//////////////////////////////////////////////////////////////////////////
void Config::CopyToConfig(const EConfigPriority ePri, IConfigSink* pDestConfig) const
{
	assert(Util::isPowerOfTwo(ePri));

	const Map::const_iterator end = m_map.end();
	for(Map::const_iterator it=m_map.begin(); it != end; ++it)
	{
		const MapKey& mapKey = it->first;
		if (mapKey.m_eKeyPri == ePri)
		{
			pDestConfig->SetKeyValue(ePri, mapKey.m_sKeyName, it->second);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
void Config::CopyToPropertyVars(CPropertyVars& properties) const
{
	const Map::const_iterator end = m_map.end();
	for(Map::const_iterator it=m_map.begin(); it != end; ++it)
	{
		const MapKey& mapKey = it->first;
		properties.SetProperty(mapKey.m_sKeyName, it->second);
	}
}
